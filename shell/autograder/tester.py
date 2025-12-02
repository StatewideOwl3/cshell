import io
import logging
import re
import shutil
import tempfile
import textwrap
import time
from datetime import datetime, timedelta, timezone
from enum import StrEnum
from pathlib import Path
from typing import List, Literal, Optional, Union
import uuid

import docker
import docker.errors
import pexpect
import typer
from pydantic import BaseModel, ConfigDict, Field

LINESEP = "\r\n"
INDENT = "    "
RAW_LOG_HEADER = "##### START RAW OUTPUT #####\n"
RESTART_LOG_HEADER = "##### RESTARTED SHELL #####\n"

DOCKERFILE_CONTENT_TESTER = """FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
# Install build-essential for C runtime libraries, and gosu for privilege dropping
RUN apt-get update && apt-get install -y build-essential gosu screen && rm -rf /var/lib/apt/lists/*
COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh
ENTRYPOINT ["entrypoint.sh"]
"""

ENTRYPOINT_CONTENT_TESTER = """#!/bin/bash
set -e
HOST_UID=${HOST_UID:-1000}
HOST_GID=${HOST_GID:-1000}
HOST_USER=${HOST_USER:-osnuser}
groupadd -g $HOST_GID $HOST_USER 2>/dev/null || true
useradd -u $HOST_UID -g $HOST_GID -s /bin/bash -d /app $HOST_USER 2>/dev/null || true
chown -R $HOST_USER:$HOST_USER /app 2>/dev/null || true
if [ -t 0 ]; then
    stty -echo 2>/dev/null || true
fi
echo $HOST_UID > /proc/self/loginuid
exec gosu $HOST_USER "$@"
"""

IMAGE_NAME_TESTER = "shell_tester:latest"
logger = logging.getLogger(__name__)

class FlexibleIO(io.StringIO):
    def write(self, s: str | bytes, /) -> int:
        if isinstance(s, bytes):
            s = s.decode()
        return super().write(s)

def ensure_docker_image_tester(client, force_rebuild: bool = False):
    if not force_rebuild:
        try:
            client.images.get(IMAGE_NAME_TESTER)
            logger.debug(f"Tester image {IMAGE_NAME_TESTER} already exists")
            return
        except docker.errors.ImageNotFound:
            pass

    logger.info(f"Building tester image {IMAGE_NAME_TESTER}...")

    build_dir = Path.cwd() / ".docker_build_temp_tester"
    build_dir.mkdir(exist_ok=True)

    (build_dir / "Dockerfile").write_text(DOCKERFILE_CONTENT_TESTER)
    (build_dir / "entrypoint.sh").write_text(ENTRYPOINT_CONTENT_TESTER)

    try:
        client.images.build(
            path=str(build_dir), tag=IMAGE_NAME_TESTER, rm=True
        )
        logger.info(f"Tester image {IMAGE_NAME_TESTER} built successfully")
    finally:
        for file in build_dir.iterdir():
            file.unlink()
        build_dir.rmdir()


class Line(BaseModel):
    text: str = Field(description="The expected content of a line of output.")
    is_re: bool = Field(
        default=False,
        description="Whether or not to interpret the text field as regex.",
    )
    negative_match: bool = Field(
        default=False,
        description="If True, a match is considered a fail and no match is considered a pass.",
    )


class TestCaseInput(BaseModel):
    expect_exit: bool = Field(
        default=False,
        description="Whether or not the shell is expected to exit after this input. For example, this would happen when inputting Ctrl+D.",
    )
    skip_prompt_check: bool = Field(
        default=False,
        description="Skip the prompt check."
    )


class ControlSignal(TestCaseInput):
    code: str = Field(
        max_length=1,
        pattern="[a-z]?",
        description="A single lowercase letter denoting the specific control signal to send.",
    )
    output: Optional[Line] = None


class StartShell(TestCaseInput):
    """Starts the shell following an exit. Note that the shell is restarted in the same environment."""

    reason: str = Field(
        default="", description="Short reason for restarting the shell"
    )


class GenericCmd(TestCaseInput):
    """A command in a testcase."""

    cmd: str = Field(
        description=(
            "The command to run. Note that this should include the arguments to"
            " be passed to the command."
        )
    )
    sequential_outputs: List[Line] = Field(
        default_factory=list,
        description=(
            "A list of output lines that the command is expected to produce, in"
            " order. Note that if the command is expected to output nothing,"
            " this must be left empty and ignore_output must not be set."
        ),
    )
    nonsequential_outputs: List[Line] = Field(
        default_factory=list,
        description=(
            "A list of output lines that the command may produce, in any order."
            "These lines will be searched for and extracted before checking the"
            "sequential outputs. Useful for checking outputs whose order is non"
            "-deterministic, like the job creation output."
        ),
    )
    ignore_output: bool = Field(
        default=False,
        description=(
            "If true, the output is ignored. This should be set when checking, "
            "for example, if the shell accepts a command as valid, but the"
            " output itself is not being checked."
        ),
    )
    cwd_after: Optional[str] = Field(
        default=None,
        description=(
            "The expected current working directory after running this command."
            "Leave empty if it is expected to be unchanged. This may either be an"
            "absolute path like '/app/test' or a relative one like '~/test'."
        ),
    )


CmdType = Union[StartShell, GenericCmd, ControlSignal]


class TestCase(BaseModel):
    section: Literal[
        "A.1",
        "A.2",
        "A.3",
        "B.1",
        "B.2",
        "B.3",
        "C.1",
        "C.2",
        "C.3",
        "C.4",
        "D.1",
        "D.2",
        "E.1",
        "E.2",
        "E.3",
        "E.4",
        "Misc",
    ] = Field(description="The section that the test case is testing.")
    description: str = Field(
        description="A short description of the test case."
    )
    cmds: List[CmdType] = Field(
        description="A list of commands to be run sequentially in the testcase."
    )
    timeout: float = Field(
        default=2.0,
        description="Timeout for execution of each command. Change only when necessary.",
        gt=0.0,
    )
    strict_prompt: bool = Field(
        default=False,
        description="If set, checks whether the prompt prints the correct username, hostname, and current working directory. Only set to True when checking the prompt is necessary.",
    )
    requires_test_folder: bool = Field(
        default=False,
        description="""If set, adds a skeleton test folder at /app/test whose structure is as below,
```
> tree
.
├── file4.txt
├── file5.txt
├── folder1
│   ├── file1.txt
│   └── folder4
│       ├── file2.txt
│       └── folder5
│           └── file3.txt
├── folder2
│   └── file6.txt
└── folder3
    ├── file7.txt
    ├── file8.txt
    └── file9.txt

6 directories, 9 files
```

Each file's contents follows the same format. For example, file3.txt contains,
```
> cat file3.txt
This is file3.txt! I have 3 lines. This is line 1.
This is line 2.
This is line 3.
```
""",
    )


class TestCaseList(BaseModel):
    testcases: List[TestCase]


class EventType(StrEnum):
    INPUT = "input"
    OUTPUT = "output"
    TIMEOUT = "timeout"
    SIGNAL = "signal"
    ERROR = "error"
    EOF = "eof"


class Event(BaseModel):
    # model_config = ConfigDict(use_enum_values=True)
    time: datetime = Field(
        default_factory=lambda: datetime.now(timezone(timedelta(hours=+5.50))),
        init=False,
    )
    type: EventType
    details: str

class Result(BaseModel):
    testcase: TestCase
    events: List[Event]
    raw_log: str = Field()
    py_log: str = Field()

class ChildProcessExited(Exception):
    def __init__(self, *args: object) -> None:
        super().__init__(*args)


class Tester:
    def __init__(self, bin: Path, test_folder: Optional[Path] = None):
        self.child: pexpect.spawn
        self.events: List[Event]
        self.timeout: float
        self.raw_logfiles: List[FlexibleIO]
        self.py_logfile: io.StringIO
        self.logger: logging.Logger
        self.temp_dir: Optional[str] = None

        self.bin = bin
        self.test_folder = test_folder

        self.docker_client = docker.from_env()

        self._re_prompt = re.compile(
            r"(<[^@]*?@[^:]*?:[^>]*?>\s|<[^@]*?@[^:]*?:[^>]*?>)\s?"
        )

        self.logger = logging.getLogger(f"{__name__}.{uuid.uuid4()}")
        self.logger.setLevel(logging.DEBUG)

    def _last_event(self):
        if len(self.events) == 0:
            return None
        return self.events[-1]

    def _prompt_event(self):
        if event := self._last_event():
            return (
                event.type == EventType.OUTPUT
                and self._re_prompt.fullmatch(event.details) is not None
            )
        return False

    def _eof_event(self):
        if event := self._last_event():
            return event.type == EventType.EOF
        return False

    def _timeout_event(self):
        if event := self._last_event():
            return event.type == EventType.TIMEOUT
        return False

    def _exit_event(self):
        return self._eof_event() or self._timeout_event()

    def _raise_if_exit(self):
        if self._exit_event():
            self.logger.info("Exit event received, raising ChildProcessExited")
            raise ChildProcessExited()

    def _add_event(
        self, type: EventType, details: str, raise_if_exit: bool = True
    ):
        event = Event(type=type, details=details)
        self.events.append(event)
        self.logger.debug("Received event:\n%s", self._pretty_print(event))

        if raise_if_exit:
            self._raise_if_exit()

    def _consume_event(self, raise_if_exit: bool = True, timeout: Optional[float] = None):
        result = self.child.expect(
            [
                self._re_prompt,
                rf".*?{LINESEP}",
                pexpect.EOF,
                pexpect.TIMEOUT,
            ],
            timeout=(timeout or -1)
        )

        if result == 0 or result == 1:
            if isinstance(self.child.match, re.Match) and (
                text := self.child.match.group(0).rstrip(LINESEP)
            ):
                if result == 0 and self.child.before:
                    self._add_event(EventType.OUTPUT, self.child.before)
                self._add_event(EventType.OUTPUT, text)
            else:
                # empty line
                return
        elif result == 2:
            if isinstance(self.child.after, pexpect.EOF):
                trace = (
                    self.child.after.get_trace()
                    if hasattr(self.child.after, "get_trace")
                    else str(self.child.after)
                )
                self._add_event(
                    EventType.EOF, f"EOF received from child: {trace}", raise_if_exit=raise_if_exit
                )
            else:
                self._add_event(
                    EventType.EOF, f"EOF received from child:\n{self._pretty_print(self.child)}", raise_if_exit=raise_if_exit
                )
        elif result == 3:
            if isinstance(self.child.after, pexpect.TIMEOUT):
                trace = (
                    self.child.after.get_trace()
                    if hasattr(self.child.after, "get_trace")
                    else str(self.child.after)
                )
                self._add_event(EventType.TIMEOUT, f"Child timed out: {trace}", raise_if_exit=raise_if_exit)
            else:
                self._add_event(
                    EventType.TIMEOUT, f"Child timed out:\n{self._pretty_print(self.child)}", raise_if_exit=raise_if_exit
                )
        else:
            self._add_event(
                EventType.ERROR,
                f"Unexpected spawn.expect result: '{result}'",
            )

    def _pretty_print(self, obj):
        if isinstance(obj, pexpect.spawn):
            return textwrap.indent(str(obj), INDENT)
        if isinstance(obj, BaseModel):
            return textwrap.indent(obj.model_dump_json(indent=4), INDENT)
        self.logger.warning(
            "Object of unexpected type '%s' passed to _pretty_print, returning repr(obj)...",
            type(obj),
        )
        return repr(obj)

    def _setup_logging(self):
        py_logfile = io.StringIO()

        handler = logging.StreamHandler(py_logfile)
        formatter = logging.Formatter(
            "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
        )

        handler.setFormatter(formatter)
        self.logger.addHandler(handler)
        self.logger.propagate = False

        return py_logfile
    
    def _kill_container(self):
        if hasattr(self, "container_name"):
            try:
                container = self.docker_client.containers.get(self.container_name)
                container.remove(force=True) 
                self.logger.debug(f"Force removed container {self.container_name}")
            except docker.errors.NotFound:
                pass
            except Exception as e:
                self.logger.warning(f"Failed to remove container {self.container_name}: {e}")
            finally:
                delattr(self, "container_name")

    def _setup_shell(
        self, use_test_folder: bool = True, restart: bool = False
    ):
        if restart and hasattr(self, "container_name"):
            self._kill_container()
        if not restart:
            self.temp_dir = tempfile.mkdtemp()
            test_path = Path(self.temp_dir) / "test"
            test_path.mkdir()

            shutil.copy(self.bin, self.temp_dir)

            if (
                use_test_folder
                and self.test_folder
                and Path(self.test_folder).is_dir()
            ):
                shutil.copytree(
                    self.test_folder, test_path, dirs_exist_ok=True
                )

        self.container_name = f"tester_{uuid.uuid4().hex}"

        command = "docker"
        args = [
            "run",
            "--name",
            self.container_name,
            "-i",
            "-t",
            "--rm",
            "--init",
            "-h",
            "osntesting",
            "-v",
            f"{self.temp_dir}:/app",
            "-w",
            "/app",
            IMAGE_NAME_TESTER,
            f"/app/{self.bin.name}",
        ]

        if not restart:
            self.raw_logfiles = [FlexibleIO()]
        else:
            self.raw_logfiles.append(FlexibleIO())

        child = pexpect.spawn(
            command=command,
            args=args,
            timeout=self.timeout,
            encoding="utf-8",
            echo=False,
            logfile=self.raw_logfiles[-1],
        )

        self.logger.info("Spawned child:\n%s", self._pretty_print(child))

        self.child = child

    def _await_prompt(self, timeout: Optional[float] = None):
        while not self._exit_event() and not self._prompt_event():
            self._consume_event(timeout=timeout)

    def _await_exit(self):
        start = time.time()
        while not self._exit_event():
            if time.time() - start > self.timeout:
                # no return (raises ChildProcessExited)
                self._add_event(
                    EventType.TIMEOUT,
                    "Timed out waiting for exit",
                    raise_if_exit=False,
                )
            self._consume_event(raise_if_exit=False)

    def _send_cmd(self, cmd: CmdType):
        self._add_event(EventType.INPUT, repr(cmd))
        if isinstance(cmd, GenericCmd):
            self.child.sendline(cmd.cmd)
        elif isinstance(cmd, ControlSignal) and cmd.code:
            signal_map = {
                'c': 'SIGINT',
                'z': 'SIGTSTP',
                '\\': 'SIGQUIT'
            }
            
            if cmd.code in signal_map:
                try:
                    container = self.docker_client.containers.get(self.container_name)
                    container.kill(signal=signal_map[cmd.code])
                    self.logger.debug(f"Sent {signal_map[cmd.code]} to container {self.container_name}")
                except docker.errors.NotFound:
                    self.logger.error(f"Container {self.container_name} not found when sending signal")
                except Exception as e:
                    self.logger.error(f"Failed to send signal: {e}")
            else:
                self.child.sendcontrol(cmd.code)
        elif isinstance(cmd, StartShell):
            self.logger.info("Restarting shell")
            self._setup_shell(restart=True)
        else:
            self.logger.error(
                self._pretty_print(cmd),
            )
            # no return (raises ChildProcessExited)
            self._add_event(
                EventType.ERROR,
                "Encountered empty command in TestCase.cmds:\n" + self._pretty_print(cmd),
            )

        if not cmd.expect_exit and not cmd.skip_prompt_check:
            self._await_prompt()
        elif cmd.expect_exit:
            self._await_exit()

    def run(self, testcase: TestCase, force_rebuild: bool = False) -> Result:
        self.events = []
        self.timeout = testcase.timeout
        self.raw_logfiles = []
        self.testcase = testcase

        ensure_docker_image_tester(self.docker_client, force_rebuild=force_rebuild)

        try:
            self.py_logfile = self._setup_logging()
            self._setup_shell(testcase.requires_test_folder)

            self._await_prompt(timeout=5.0)

            for cmd in testcase.cmds:
                self._send_cmd(cmd)
            try:
                if hasattr(self, 'child') and self.child.isalive():
                    self.child.close()
            except Exception as e:
                self._add_event(
                    type=EventType.ERROR,
                    details=f"Failed to close child: {e}",
                    raise_if_exit=False
                )
        except ChildProcessExited:
            try:
                if text := self.child.read(1024):
                    self._add_event(type=EventType.OUTPUT, details=text)
            except pexpect.TIMEOUT:
                pass
            except KeyboardInterrupt as e:
                raise e
            except Exception as e:
                self._add_event(
                    type=EventType.ERROR,
                    details=f"Error while reading child's final output: {e}",
                    raise_if_exit=False,
                )
        except KeyboardInterrupt as e:
            raise e
        except Exception as e:
            self.logger.error("Unexpected error:", exc_info=e)
            self._add_event(
                type=EventType.ERROR, details=f"Unexpected error: {e}"
            )
        finally:
            self._kill_container()
            raw_log = ""
            py_log = ""

            if hasattr(self, "child"):
                self.logger.info(
                    "Finished running testcase. Child info at exit:\n%s",
                    self._pretty_print(self.child),
                )

                if self.temp_dir and Path(self.temp_dir).is_dir():
                    shutil.rmtree(self.temp_dir, ignore_errors=True)
                    self.logger.info("Cleaned up temp_dir: %s", self.temp_dir)
                    self.temp_dir = None

                self.logger.removeHandler(self.logger.handlers[0])

                del self.child

                raw_log = ""
                for idx,raw_logfile in enumerate(self.raw_logfiles):
                    raw_logfile.flush()
                    raw_logfile.seek(0)
                    if idx == 0:
                        raw_log += raw_logfile.read()
                    else:
                        raw_log += RESTART_LOG_HEADER + raw_logfile.read()
                    raw_logfile.close()

                self.py_logfile.flush()
                self.py_logfile.seek(0)
                py_log = self.py_logfile.read()
                self.py_logfile.close()

                del self.raw_logfiles
                del self.py_logfile

            return Result(
                testcase=testcase,
                events=self.events,
                raw_log=RAW_LOG_HEADER + raw_log,
                py_log=py_log,
            )


def main(bin: Path, test_folder: Optional[Path] = None, force_rebuild: bool = False):
    logging.basicConfig(
        format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
        level=logging.DEBUG,
    )

    tester = Tester(bin, test_folder)
    result = tester.run(
        TestCase(
            section="A.1",
            description="meow",
            cmds=[
                GenericCmd(
                    cmd="echo hi", sequential_outputs=[Line(text="hello")]
                ),
                ControlSignal(expect_exit=True, code="d"),
                StartShell(),
                GenericCmd(
                    cmd="echo meow", sequential_outputs=[Line(text="meow")]
                ),
            ],
        ),
        force_rebuild=force_rebuild
    )
    print(result.model_dump_json(indent=4))


if __name__ == "__main__":
    typer.run(main)
