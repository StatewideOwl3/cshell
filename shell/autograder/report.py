#!/usr/bin/env python3

import json
import sys
import html
from pathlib import Path
from typing import Dict, Any, Optional, Tuple


def get_css() -> str:
    # this function returns the css styles as a string. we include the css
    # directly in the python script so the generated html file is standalone
    # and doesn't require an external stylesheet to look good.
    return """
        :root {
            --bg-body: #f8fafc;
            --bg-card: #ffffff;
            --text-main: #334155;
            --text-muted: #64748b;
            --primary: #3b82f6;
            --success: #10b981;
            --danger: #ef4444;
            --warning: #f59e0b;
            --warning-bg: #fffbeb;
            --warning-text: #92400e;
            --border: #e2e8f0;
            --code-bg: #1e293b;
            --font-mono: 'JetBrains Mono Regular', Consolas, 'Liberation Mono', Menlo, monospace;
        }

        * { margin: 0; padding: 0; box-sizing: border-box; }
        
        body {
            font-family: var(--font-mono);
            background-color: var(--bg-body);
            color: var(--text-main);
            line-height: 1.5;
            padding-bottom: 60px;
            font-size: 14px;
        }
        
        .header {
            background-color: white;
            border-bottom: 1px solid var(--border);
            padding: 1.5rem 0;
            margin-bottom: 2rem;
        }

        .header-content {
            max-width: 1200px;
            margin: 0 auto;
            padding: 0 20px;
        }
        
        .header h1 { font-size: 1.5em; font-weight: 700; color: #0f172a; }
        .header p { color: var(--text-muted); font-size: 0.9em; }
        
        .container { max-width: 1200px; margin: 0 auto; padding: 0 20px; }
        
        /* stats grid layout */
        .stats {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 16px;
            margin-bottom: 40px;
        }
        
        .stat-card {
            background: var(--bg-card);
            padding: 20px;
            border-radius: 4px;
            border: 1px solid var(--border);
            box-shadow: 0 1px 2px rgba(0,0,0,0.05);
        }
        
        .stat-card h2 { 
            font-size: 2em; 
            font-weight: 700; 
            line-height: 1; 
            margin-bottom: 4px; 
        }
        
        .stat-card p { 
            color: var(--text-muted); 
            font-size: 0.8em; 
            text-transform: uppercase; 
            font-weight: 700; 
        }
        
        .stat-card.total h2 { color: var(--primary); }
        .stat-card.passed h2 { color: var(--success); }
        .stat-card.failed h2 { color: var(--danger); }
        
        /* test card styling */
        .section-title {
            font-size: 1.25em;
            color: #0f172a;
            margin-bottom: 16px;
            font-weight: 700;
            border-bottom: 2px solid var(--border);
            padding-bottom: 8px;
        }
        
        .test-card {
            background: var(--bg-card);
            border-radius: 4px;
            box-shadow: 0 1px 2px rgba(0,0,0,0.05);
            margin-bottom: 20px;
            border: 1px solid var(--border);
            overflow: hidden;
        }
        
        .test-header {
            padding: 10px 20px;
            background: #f1f5f9;
            border-bottom: 1px solid var(--border);
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        
        .test-section-info { font-weight: 700; color: var(--text-muted); }
        
        .test-header-right { display: flex; align-items: center; gap: 12px; }
        
        .test-id-badge {
            background: var(--code-bg);
            color: #e2e8f0;
            padding: 3px 8px;
            border-radius: 4px;
            font-size: 0.9em;
        }

        .badge {
            padding: 3px 8px;
            border-radius: 4px;
            font-size: 0.8em;
            font-weight: 700;
            text-transform: uppercase;
        }
        .badge-failed { background: #fee2e2; color: #991b1b; }
        .badge-passed { background: #dcfce7; color: #166534; }
        
        .test-body { padding: 20px; }
        .test-description { margin-bottom: 15px; color: var(--text-main); }

        /* copy button styling */
        .copy-btn {
            background: white;
            border: 1px solid var(--border);
            padding: 4px 10px;
            border-radius: 4px;
            cursor: pointer;
            font-family: var(--font-mono);
            font-size: 0.8em;
            color: var(--text-muted);
            transition: all 0.2s;
            display: inline-flex;
            align-items: center;
        }
        .copy-btn:hover { 
            background: #f1f5f9; 
            color: var(--text-main); 
            border-color: #cbd5e1; 
        }
        .copy-btn:active { transform: translateY(1px); }

        /* code block styling */
        .code-block {
            background: var(--code-bg);
            color: #e2e8f0;
            padding: 12px;
            border-radius: 4px;
            font-size: 0.9em;
            overflow-x: auto;
            white-space: pre-wrap;
        }

        .diff-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 20px;
            margin-top: 16px;
        }
        @media (max-width: 768px) { 
            .diff-grid { grid-template-columns: 1fr; } 
        }

        .label {
            font-size: 0.8em;
            text-transform: uppercase;
            font-weight: 700;
            color: var(--text-muted);
            margin-bottom: 6px;
            display: block;
        }

        .reason-box {
            background: #fef2f2;
            border-left: 4px solid var(--danger);
            padding: 12px;
            color: #7f1d1d;
            margin-bottom: 16px;
            font-size: 0.95em;
        }

        .runner-error {
            background: var(--warning-bg);
            border-left: 4px solid var(--warning);
            padding: 12px;
            color: var(--warning-text);
            margin-bottom: 16px;
            font-size: 0.95em;
            border-radius: 0 4px 4px 0;
        }

        /* passed list styling */
        .passed-list { list-style: none; }
        .passed-item {
            padding: 10px 14px;
            background: white;
            border-bottom: 1px solid var(--border);
            font-size: 0.95em;
            display: flex;
            align-items: center;
        }
        .passed-item:before {
            content: "+";
            color: var(--success);
            font-weight: bold;
            margin-right: 10px;
        }
        .passed-item:last-child { border-bottom: none; }
        .passed-group { 
            border: 1px solid var(--border); 
            border-radius: 4px; 
            overflow: hidden; 
            margin-bottom: 20px; 
        }
        .passed-header { 
            background: #f8fafc; 
            padding: 10px 14px; 
            font-weight: 700; 
            border-bottom: 1px solid var(--border); 
        }
    """


def parse_raw_log(log_path: Path) -> Tuple[Optional[str], Optional[str], str]:
    # this function parses the raw .log file generated by the test runner.
    # we need to do this because the main summary.json might not contain the
    # full output if the test crashed or timed out mid-execution.
    
    # if the log file doesn't exist, we can't extract any extra info.
    if not log_path.exists():
        print(f"warning: log file not found at {log_path}")
        return None, None, ""

    error_html = None
    error_text = None
    raw_output_buffer = []

    try:
        # we open the file in utf-8 to ensure special chars are handled correctly.
        with open(log_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        events = data.get('events', [])
        
        # first, we reconstruct the output stream. we iterate through all events
        # and collect anything labeled 'output'.
        for event in events:
            if event.get('type') == 'output':
                details = event.get('details', '')
                if details:
                    raw_output_buffer.append(details)
        
        # next, we look for explicit error events. these usually contain python
        # stack traces from the test runner itself.
        for event in events:
            if event.get('type') == 'error':
                details = event.get('details', 'Unknown error')
                # we only want the first line for the summary to avoid clutter.
                summary = details.split('\n')[0] 
                
                # we create a plain text version for the clipboard.
                error_text = f"Test Runner Error: {summary}"
                # and an html version for the display.
                error_html = (
                    f"<strong>Test Runner Error:</strong> "
                    f"{html.escape(summary)}"
                )
                break
        
        # if we didn't find an explicit error, we check for timeouts.
        if not error_html:
            for event in events:
                if event.get('type') == 'timeout':
                    text_msg = (
                        "Timeout Exceeded: The shell process hung or did not "
                        "respond within the time limit."
                    )
                    error_text = text_msg
                    
                    # we split the message to make the title bold in html.
                    title, body = text_msg.split(':', 1)
                    error_html = f"<strong>{title}:</strong>{body}"
                    break

    except Exception as e:
        # if the log is corrupt, we fail silently but log it to console.
        print(f"error parsing log {log_path}: {e}")
        return None, None, ""
        
    return error_html, error_text, "".join(raw_output_buffer)


def format_expectations(failure: Dict[str, Any]) -> str:
    # this function converts the expected output structure into html.
    # it handles different types of expectations like sequential outputs,
    # non-sequential outputs, and exit codes.
    
    if not failure:
        return ""
    
    expected = failure.get('expected')
    if expected is None:
        return ""
    
    # if expectation is a simple type (int/str), return it directly.
    if not isinstance(expected, dict):
        return f'<div class="code-block">{html.escape(str(expected))}</div>'
    
    html_parts = []
    
    # helper function to create consistent rows for expectations.
    # we use different colors for positive (must appear) vs negative
    # (must not appear) expectations.
    def render_row(label, content, is_negative=False):
        color = "#ef4444" if is_negative else "#10b981"
        safe_content = html.escape(str(content))
        return (
            f'<div style="margin-bottom: 10px;">'
            f'<div style="font-size: 0.9em; color: {color}; font-weight: 700; '
            f'margin-bottom: 4px;">{label}</div>'
            f'<div class="code-block" style="border-left: 3px solid {color};">'
            f'{safe_content}</div>'
            f'</div>'
        )

    # process sequential output requirements.
    if 'sequential_outputs' in expected and \
       isinstance(expected['sequential_outputs'], list):
        for i, output in enumerate(expected['sequential_outputs'], 1):
            should_appear = output.get('should_appear', True)
            rule = "MUST APPEAR" if should_appear else "MUST NOT APPEAR"
            label = f"#{i} Sequential ({rule})"
            content = output.get('content', '')
            html_parts.append(render_row(label, content, not should_appear))
    
    # process non-sequential output requirements (order doesn't matter).
    if 'nonsequential_outputs' in expected and \
       isinstance(expected['nonsequential_outputs'], list):
        for output in expected['nonsequential_outputs']:
            should_appear = output.get('should_appear', True)
            rule = "MUST APPEAR" if should_appear else "MUST NOT APPEAR"
            label = f"Any Order ({rule})"
            content = output.get('content', '')
            html_parts.append(render_row(label, content, not should_appear))
    
    # check if the working directory is being verified.
    if 'cwd_after' in expected and expected['cwd_after']:
        html_parts.append(
            render_row("Expected Directory", expected['cwd_after'])
        )
    
    # check if the process was expected to exit.
    if 'expect_exit' in expected and expected['expect_exit']:
         html_parts.append(
             '<div style="margin-top:8px; font-weight:700; color:#0369a1;">'
             '[!] Process expected to exit</div>'
         )
    
    # fallback: if no specific keys were found but expected isn't empty,
    # just dump the json so the user sees something.
    if not html_parts:
        safe_dump = html.escape(json.dumps(expected, indent=2))
        return f'<div class="code-block">{safe_dump}</div>'
        
    return ''.join(html_parts)


def format_actual_output(content: Any) -> str:
    # this function handles the actual output received from the shell.
    # it is critical to escape the html here because shell output often
    # contains characters like < and > (e.g. in prompts) which would
    # otherwise be interpreted as html tags by the browser.
    
    if content is None:
        return "<em style='color:#94a3b8'>No output captured</em>"
    
    s_content = str(content)
    if not s_content:
        return "<em style='color:#94a3b8'>Empty output</em>"
    
    safe_content = html.escape(s_content)
    return f'<div class="code-block">{safe_content}</div>'


def escape_js_string(text) -> str:
    # this function prepares text to be embedded inside a javascript string.
    # we allow json.dumps to handle quotes and backslashes, then we replace
    # single quotes manually because we are placing this inside '' in js.
    s = json.dumps(text, default=str)
    return s.replace('"', '&quot;').replace("'", "\\'")


def generate_html_report(data: Dict, input_file_path: Path) -> str:
    # this is the main function that assembles the html report.
    print("generating html report structure...")
    
    results = data.get('results', [])
    total = data.get('total_tests', len(results))
    passed_count = data.get('passed_tests', 0)
    failed_count = data.get('failed_tests', 0)
    
    print(f"stats: {total} total, {passed_count} passed, {failed_count} failed")
    
    # we process failed tests first because they are the most important.
    failed_tests = [r for r in results if not r.get('passed', False)]
    failed_html = ""
    
    if failed_tests:
        print(f"processing {len(failed_tests)} failed tests...")
        
        # header for the failed section.
        failed_html += (
            f'<div style="margin-bottom: 40px;">'
            f'<h2 class="section-title" style="color:#ef4444">'
            f'Failed Tests ({len(failed_tests)})</h2>'
        )
        
        for i, test in enumerate(failed_tests, 1):
            section = test.get('section', 'N/A')
            desc = test.get('description', 'No description')
            test_id = test.get('test_id', '')
            failure = test.get('failure', {})
            
            # we try to find the raw log file to see if the runner crashed.
            runner_error_msg_html = None
            runner_error_msg_text = None
            captured_raw_output = ""
            
            if test_id:
                raw_log_path = input_file_path.parent / 'tests' / f"{test_id}.log"
                # checking if we need to pull raw data from the logs.
                if raw_log_path.exists():
                    runner_error_msg_html, runner_error_msg_text, \
                    captured_raw_output = parse_raw_log(raw_log_path)

            # by default, we use the output from the json summary.
            actual_content_to_show = failure.get('actual')
            
            # however, if the runner crashed, the summary json might be empty.
            # in that case, we prioritize the raw output we captured above.
            if runner_error_msg_html and captured_raw_output:
                print(f"using raw output for {test_id} due to runner error")
                actual_content_to_show = captured_raw_output
            
            # we prepare the expected data for the clipboard copy.
            expected_raw = failure.get('expected', {})
            
            # we format the clipboard text differently depending on whether
            # it was a logic failure or a system crash.
            if runner_error_msg_text:
                clipboard_text = (
                    f"In {test_id.replace('_', ' ')}, my output is "
                    f"`{str(actual_content_to_show)}`, but the test runner "
                    f"produces the following error: `{runner_error_msg_text}`"
                )
            else:
                clipboard_text = (
                    f"In test {test_id.replace('_', ' ')}, the expected output is "
                    f"`{json.dumps(expected_raw)}`, and my output is "
                    f"`{str(actual_content_to_show)}`."
                )
            
            clipboard_safe = escape_js_string(clipboard_text)

            # basic info for display.
            cmd = failure.get('command', '') if isinstance(failure, dict) else ''

            # if the runner crashed (timeout/error), the assertion failure
            # in the summary is usually just a side effect, so we hide it.
            reason = failure.get('reason', '') if isinstance(failure, dict) else ''
            if runner_error_msg_html:
                reason = "" 

            # we construct the html card for this test.
            failed_html += (
                f'<div class="test-card">'
                f'<div class="test-header">'
                f'<span class="test-section-info">Section {section}</span>'
                f'<div class="test-header-right">'
                f'<button class="copy-btn" '
                f'onclick="copyIssue({clipboard_safe}, this)">'
                f'Copy Issue Info</button>'
                f'<span class="test-id-badge">{test_id}</span>'
                f'<span class="badge badge-failed">Failed</span>'
                f'</div>'
                f'</div>'
                f'<div class="test-body">'
                f'<div class="test-description">{desc}</div>'
            )
            
            # conditionally add the runner error box.
            if runner_error_msg_html:
                failed_html += (
                    f'<div class="runner-error">{runner_error_msg_html}</div>'
                )
            
            # conditionally add the assertion reason box.
            if reason:
                failed_html += (
                    f'<div class="reason-box"><strong>Assertion Failure:'
                    f'</strong> {html.escape(reason)}</div>'
                )
            
            # conditionally add the command box.
            if cmd and cmd != "Global Execution":
                failed_html += (
                    f'<span class="label">Command</span>'
                    f'<div class="code-block" style="margin-bottom:16px">'
                    f'$ {html.escape(cmd)}</div>'
                )
            
            # finally, add the grid showing expected vs actual.
            failed_html += (
                f'<div class="diff-grid">'
                f'<div>'
                f'<span class="label">Expected</span>'
                f'{format_expectations(failure)}'
                f'</div>'
                f'<div>'
                f'<span class="label">Actual</span>'
                f'{format_actual_output(actual_content_to_show)}'
                f'</div>'
                f'</div>'
                f'</div>'
                f'</div>'
            )
        failed_html += '</div>'
    
    # add all the passed tests.
    passed_tests = [r for r in results if r.get('passed', False)]
    passed_html = ""
    
    if passed_tests:
        print(f"processing {len(passed_tests)} passed tests...")
        
        passed_html += (
            '<div style="margin-top: 40px;">'
            '<h2 class="section-title">Passed Tests</h2>'
        )
        
        # we group passed tests by section for better readability.
        sections = {}
        for test in passed_tests:
            sec = test.get('section', 'Misc')
            if sec not in sections: sections[sec] = []
            sections[sec].append(test)
            
        for sec in sorted(sections.keys()):
            passed_html += (
                f'<div class="passed-group">'
                f'<div class="passed-header">Section {sec}</div>'
                f'<ul class="passed-list">'
            )
            
            for test in sections[sec]:
                desc = test.get("description", "No description")
                passed_html += (
                    f'<li class="passed-item">{html.escape(desc)}</li>'
                )
                
            passed_html += '</ul></div>'
        passed_html += '</div>'

    # we assemble the full html document.
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OSN Mini Project 1 Autograder Summary</title>
    <style>{get_css()}</style>
    <script>
        function copyIssue(text, btn) {{
            try {{
                 // we create a temporary textarea to hold the text we want
                 // to copy to the clipboard.
                 var tempInput = document.createElement("textarea");
                 tempInput.value = text;
                 document.body.appendChild(tempInput);
                 tempInput.select();
                 document.execCommand("copy");
                 document.body.removeChild(tempInput);
                 
                 // provide visual feedback to the user.
                 var originalText = btn.innerText;
                 btn.innerText = "Copied!";
                 btn.style.color = "#10b981";
                 btn.style.borderColor = "#10b981";
                 
                 // reset the button state after 2 seconds.
                 setTimeout(function() {{
                     btn.innerText = originalText;
                     btn.style.color = "";
                     btn.style.borderColor = "";
                 }}, 2000);
            }} catch (err) {{
                console.error('Failed to copy', err);
                alert('Failed to copy to clipboard');
            }}
        }}
    </script>
</head>
<body>
    <div class="header">
        <div class="header-content">
            <h1>OSN Mini Project 1 Autograder Summary</h1>
            <p>Automated Analysis Report</p>
        </div>
    </div>
    
    <div class="container">
        <div class="stats">
            <div class="stat-card total"><h2>{total}</h2><p>Total</p></div>
            <div class="stat-card passed"><h2>{passed_count}</h2><p>Passed</p></div>
            <div class="stat-card failed"><h2>{failed_count}</h2><p>Failed</p></div>
        </div>
        
        {failed_html}
        {passed_html}
    </div>
</body>
</html>"""


def main():
    # we need at least one argument (the summary file).
    if len(sys.argv) < 2:
        print("usage: uv run report.py <logs folder>")
        sys.exit(1)
    
    input_file = Path(sys.argv[1]) / "summary.json"
    print(f"starting analysis of {input_file}...")
    
    if not input_file.exists():
        print(f"error: file '{input_file}' not found")
        sys.exit(1)
    
    try:
        # we load the summary json data.
        with open(input_file, 'r') as f:
            data = json.load(f)
        
        # pass input_file path so we can find relative logs.
        html_content = generate_html_report(data, input_file)
        
        # we save the output to 'summary.html' in the same directory.
        output_file = input_file.parent / 'summary.html'
        print(f"writing html report to {output_file}...")
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(html_content)
        
        print("success: html report generated.")
        
    except json.JSONDecodeError:
        print(f"error: invalid json in {input_file}")
        sys.exit(1)
    except Exception as e:
        print(f"error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
