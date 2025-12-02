#!/bin/bash
uv run runner.py -b .. -l results -t testcases.json -p .
uv run grader.py -l results -t testcases.json
uv run report.py results
