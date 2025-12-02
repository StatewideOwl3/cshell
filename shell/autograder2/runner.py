import json

import docker
from pydantic import ValidationError
from tqdm import tqdm
from tester import Tester, TestCase, TestCaseList, ensure_docker_image_tester
from builder import load_roster
import typer
from pathlib import Path
from typing import List, Optional
import shutil
import concurrent.futures

import logging

app = typer.Typer()

logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")
logger = logging.getLogger(__name__)

def run_tests_on_student(binary_dir: Path, log_dir: Path, testcases: List[TestCase], test_dir: Path, test_case: Optional[int] = None, section: Optional[str] = None):
    test_log_dir = log_dir / "tests"
    if section is None and test_case is None:
        if test_log_dir.exists():
            shutil.rmtree(test_log_dir)
        test_log_dir.mkdir()

    tester = Tester(binary_dir / 'shell.out', test_dir)
    for id, testcase in enumerate(tqdm(testcases, f"Running testcases for {binary_dir.name.replace('mini-project-1-', '')}", leave=False)):
        if test_case is not None and test_case != id:
            continue
        if section is not None and section not in testcase.section:
            continue
        result = tester.run(testcase)

        log_file = test_log_dir / f"test_{id}.log"
        log_file.write_text(result.model_dump_json(indent=4))

@app.command()
def main(
    binaries_dir: Path = typer.Option(
        ...,
        "--binaries-dir",
        "-b",
        help="Path to folder with built binaries",
    ),
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
    test_dir: Path = typer.Option(
        ...,
        "--test-dir",
        "-p",
        help="Path to folder used in testing",
    ),
    section: Optional[str] = typer.Option(
        None,
        "--section",
        "-s",
        help="Section to test"
    ),
    roster_path: Optional[Path] = typer.Option(
        None, "--roster-path", help="Path to GitHub Classroom roster CSV"
    ),
    roll_no: Optional[str] = typer.Option(
        None, "--roll-no", help="Specific roll number to run"
    ),
    test_case: Optional[int] = typer.Option(
        None, "--test-case", help="Specific testcase to run"
    ),
    force_rebuild: bool = typer.Option(
        False, "--force-rebuild", help="Force rebuild Docker image"
    ),
    threads: int = typer.Option(
        16,
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
        for path in [binaries_dir, roster_path, testcases_path]
    ):
        logger.error("Invalid path provided")
        raise typer.Exit(code=1)

    try:
        testcases = TestCaseList.model_validate_json(testcases_path.read_text()).testcases
    except json.JSONDecodeError as e:
        logger.exception("Unable to decode testcase file", exc_info=e)
        raise typer.Exit(code=1)
    except ValidationError as e:
        logger.exception("Unable to validate loaded testcases", exc_info=e)
        raise typer.Exit(code=1)

    logging_dir.mkdir(exist_ok=True)
    if not binaries_dir.exists():
        logger.error(
            f"Binary directory not found in {binaries_dir}"
        )
        raise typer.Exit(code=1)

    run_tests_on_student(binaries_dir, logging_dir, testcases, test_dir)

if __name__ == "__main__":
    app()
