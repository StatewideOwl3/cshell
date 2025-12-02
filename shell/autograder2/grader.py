from datetime import timedelta
import re
import logging
from pathlib import Path
from typing import Optional, List, Any
import os
import concurrent.futures

import typer
from pydantic import BaseModel
from tqdm import tqdm

from tester import ControlSignal, GenericCmd, StartShell, TestCaseList, Result, EventType
from builder import load_roster

app = typer.Typer()

logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")
logger = logging.getLogger(__name__)

class FailureReason(BaseModel):
    step_index: int
    command: str
    reason: str
    expected: Any = None
    actual: Any = None

class GradeReport(BaseModel):
    test_id: str
    section: str
    description: str
    passed: bool
    failure: Optional[FailureReason] = None
    score: float

class GradeSummary(BaseModel):
    results: List[GradeReport]
    total_tests: int
    passed_tests: int
    failed_tests: int

def check_line_match(line: str, expected: str, is_regex: bool) -> bool:
    if is_regex:
        try:
            return re.search(expected.encode("utf-8").decode("unicode_escape"), line) is not None
        except re.error:
            return False
    return line == expected

def parse_prompt(prompt_text: str):
    pattern = r"^<([^@]+)@([^:]+):([^>]+)>\s*$"
    match = re.match(pattern, prompt_text.strip())
    if match:
        return match.groups() # (user, host, cwd)
    return None

def grade_student(log_dir: Path):
    test_logs_dir = log_dir / "tests"
    grade_logs_dir = log_dir / "grade"
    
    if grade_logs_dir.exists():
        import shutil
        shutil.rmtree(grade_logs_dir)
    grade_logs_dir.mkdir()

    summary_results = []

    log_files = sorted(
        test_logs_dir.glob("*.log"), 
        key=lambda p: int(p.stem.split('_')[1]) if '_' in p.stem else 0
    )

    for log_file in log_files:
        try:
            result = Result.model_validate_json(log_file.read_text())
        except Exception as e:
            logger.error(f"Failed to parse {log_file}: {e}")
            continue

        testcase = result.testcase
        events = result.events
        
        passed = True
        failure = None

        if any(e.type == EventType.ERROR for e in events):
            passed = False
            failure = FailureReason(
                step_index=-1, 
                command="Global Execution", 
                reason="Test runner encountered an error (see raw logs)."
            )

        if passed:
            stream_events = [e for e in events if e.type in [EventType.INPUT, EventType.OUTPUT, EventType.TIMEOUT, EventType.EOF]]
            
            input_indices = [i for i, e in enumerate(stream_events) if e.type == EventType.INPUT]
            
            if len(input_indices) != len(testcase.cmds):
                passed = False
                failure = FailureReason(
                    step_index=len(input_indices), 
                    command="Sequence Check", 
                    reason=f"Expected {len(testcase.cmds)} commands, but executed {len(input_indices)}."
                )

            if passed:
                for cmd_idx, cmd in enumerate(testcase.cmds):
                    if not passed:
                        break
                    if isinstance(cmd, StartShell):
                        continue
                    
                    start_idx = input_indices[cmd_idx]
                    end_idx = input_indices[cmd_idx+1] if cmd_idx + 1 < len(input_indices) else len(stream_events)
                    
                    command_events = stream_events[start_idx:end_idx]
                    
                    prompt_output = None

                    outputs_only = [e for e in command_events[1:] if e.type == EventType.OUTPUT]
                    
                    if outputs_only:
                        last_out = outputs_only[-1].details
                        if re.match(r"(<[^@]*?@[^:]*?:[^>]*?>\s|<[^@]*?@[^:]*?:[^>]*?>)", last_out):
                            prompt_output = last_out
                            output_content_events = outputs_only[:-1]
                        else:
                            prompt_output = None
                            output_content_events = outputs_only
                    else:
                        output_content_events = []

                    actual_lines = [e.details for e in output_content_events]

                    if isinstance(cmd, ControlSignal):
                        errcmd = f"Ctrl+{cmd.code}"
                        if cmd.code == 'd':
                            expected_eof = stream_events[-1]
                            if expected_eof.type != EventType.EOF:
                                passed = False
                                failure = FailureReason(
                                    step_index=cmd_idx,
                                    command=errcmd,
                                    reason="Expected EOF from child.",
                                    expected="EventType.EOF",
                                    actual=f"EventType.{expected_eof.type.name}"
                                )
                                break
                            if (expected_eof.time - command_events[0].time) > timedelta(seconds=0.5):
                                passed = False
                                failure = FailureReason(
                                    step_index=cmd_idx,
                                    command=errcmd,
                                    reason="Expected EOF from child.",
                                    expected="EventType.EOF",
                                    actual=f"EventType.{expected_eof.type.name}"
                                )
                                break
                            continue
                    else:
                        errcmd = cmd.cmd

                    if not cmd.expect_exit and not cmd.skip_prompt_check and not prompt_output:
                        passed = False
                        failure = FailureReason(step_index=cmd_idx, command=errcmd, reason="Prompt not found after command execution.")

                    if testcase.strict_prompt and not cmd.expect_exit and not cmd.skip_prompt_check:
                        if not prompt_output:
                            passed = False
                            failure = FailureReason(step_index=cmd_idx, command=errcmd, reason="Prompt not found after command execution.")
                        else:
                            parsed = parse_prompt(prompt_output)
                            if not parsed:
                                passed = False
                                failure = FailureReason(step_index=cmd_idx, command=errcmd, reason="Prompt malformed.", actual=prompt_output)
                            else:
                                user, host, cwd = parsed
                                if isinstance(cmd, GenericCmd) and cmd.cwd_after:
                                    expected_cwd = cmd.cwd_after
                                    if expected_cwd != cwd:
                                        passed = False
                                        failure = FailureReason(
                                            step_index=cmd_idx, 
                                            command=cmd.cmd, 
                                            reason="Wrong CWD in prompt.", 
                                            expected=expected_cwd, 
                                            actual=cwd
                                        )

                    if not passed:
                        break

                    lines_to_check = list(map(lambda x: x.strip(), actual_lines))

                    if isinstance(cmd, ControlSignal):
                        if len(lines_to_check) != (1 if cmd.output is not None else 0):
                            passed = False
                            failure = FailureReason(step_index=cmd_idx, command=errcmd, reason="Incorrect number of output lines for sequential check.")
                        else:
                            if not cmd.output:
                                continue

                            actual_line = lines_to_check[0]
                            seq_line = cmd.output
                            is_match = check_line_match(actual_line, seq_line.text, seq_line.is_re)
                            
                            if seq_line.negative_match:
                                if is_match:
                                    passed = False
                                    failure = FailureReason(
                                        step_index=cmd_idx,
                                        command=errcmd,
                                        reason="Found forbidden sequential output at line 0",
                                        actual=actual_line
                                    )
                                    break
                            else:
                                if not is_match:
                                    passed = False
                                    failure = FailureReason(
                                        step_index=cmd_idx,
                                        command=errcmd,
                                        reason="Mismatch at sequential line 0",
                                        expected=seq_line.text,
                                        actual=actual_line
                                    )
                                    break
                        if not passed:
                            break
                        else:
                            continue

                    for ns_line in cmd.nonsequential_outputs:
                        found_index = -1
                        for i, line in enumerate(lines_to_check):
                            if check_line_match(line, ns_line.text, ns_line.is_re):
                                found_index = i
                                break
                        
                        if ns_line.negative_match:
                            if found_index != -1:
                                passed = False
                                failure = FailureReason(
                                    step_index=cmd_idx, 
                                    command=cmd.cmd, 
                                    reason=f"Found forbidden output: '{ns_line.text}'",
                                    actual=lines_to_check[found_index]
                                )
                                break
                        else:
                            if found_index == -1:
                                passed = False
                                failure = FailureReason(
                                    step_index=cmd_idx, 
                                    command=cmd.cmd, 
                                    reason=f"Missing required non-sequential output: '{ns_line.text}'",
                                    actual="\r\n".join(actual_lines)
                                )
                                break
                            else:
                                lines_to_check.pop(found_index)

                    if not passed:
                        break
                    if cmd.ignore_output:
                        continue

                    if cmd.sequential_outputs:
                        if len(lines_to_check) != len(cmd.sequential_outputs):
                            passed = False
                            failure = FailureReason(
                                step_index=cmd_idx,
                                command=cmd.cmd,
                                reason="Incorrect number of output lines for sequential check.",
                                expected=len(cmd.sequential_outputs),
                                actual=len(lines_to_check)
                            )
                        else:
                            for seq_idx, seq_line in enumerate(cmd.sequential_outputs):
                                actual_line = lines_to_check[seq_idx]
                                is_match = check_line_match(actual_line, seq_line.text, seq_line.is_re)
                                
                                if seq_line.negative_match:
                                    if is_match:
                                        passed = False
                                        failure = FailureReason(
                                            step_index=cmd_idx,
                                            command=cmd.cmd,
                                            reason=f"Found forbidden sequential output at index {seq_idx}",
                                            actual=actual_line
                                        )
                                        break
                                else:
                                    if not is_match:
                                        passed = False
                                        failure = FailureReason(
                                            step_index=cmd_idx,
                                            command=cmd.cmd,
                                            reason=f"Mismatch at sequential line {seq_idx}",
                                            expected=seq_line.text,
                                            actual=actual_line
                                        )
                                        break
                    
                    if not passed:
                        break

        report = GradeReport(
            test_id=log_file.stem,
            section=testcase.section,
            description=testcase.description,
            passed=passed,
            failure=failure,
            score=1.0 if passed else 0.0
        )
        
        summary_results.append(report)

        (grade_logs_dir / f"{log_file.stem}_grade.json").write_text(
            report.model_dump_json(indent=4)
        )

    summary = GradeSummary(
        results=summary_results,
        total_tests=len(summary_results),
        passed_tests=sum(1 for r in summary_results if r.passed),
        failed_tests=sum(1 for r in summary_results if not r.passed)
    )
    
    (log_dir / "summary.json").write_text(summary.model_dump_json(indent=4))
    logger.info(f"Grading complete. Passed: {summary.passed_tests}/{summary.total_tests}. Logs in {grade_logs_dir}")


@app.command()
def main(
    logging_dir: Path = typer.Option(
        ...,
        "--logging-dir",
        "-l",
        help="Path to folder to save logs in",
    ),
    testcases_path: Path = typer.Option(
        ...,
        "--testcases",
        "-t",
        help="Path to json file containing testcases",
    ),
    roster_path: Optional[Path] = typer.Option(
        None, "--roster-path", help="Path to GitHub Classroom roster CSV"
    ),
    roll_no: Optional[str] = typer.Option(
        None, "--roll-no", help="Specific roll number to grade"
    ),
    threads: int = typer.Option(
        os.cpu_count(),
        "--threads",
        "-j",
        help="Number of threads to use for processing",
    ),
):
    if (roster_path is None) != (roll_no is None):
        logger.error(
            "Both --roster-path and --roll-no must be provided together"
        )
        raise typer.Exit(code=1)

    if not all(
        path is None or path.exists()
        for path in [logging_dir, roster_path, testcases_path]
    ):
        logger.error("Invalid path provided")
        raise typer.Exit(code=1)

    try:
        TestCaseList.model_validate_json(testcases_path.read_text())
    except Exception as e:
        logger.exception("Unable to validate loaded testcases file", exc_info=e)
        raise typer.Exit(code=1)

    grade_student(logging_dir)


if __name__ == "__main__":
    app()
