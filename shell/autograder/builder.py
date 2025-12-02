import concurrent.futures
import getpass
import json
import logging
import os
import subprocess
import tempfile
from pathlib import Path
from typing import Dict, Optional, Tuple

import docker
import docker.errors
import typer
from tqdm import tqdm

app = typer.Typer()

logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")
logger = logging.getLogger(__name__)

DOCKERFILE_CONTENT = """FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y build-essential libgdbm-compat-dev coreutils gosu && rm -rf /var/lib/apt/lists/*
COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh
COPY gcc_wrapper.sh /usr/local/bin/gcc_wrapper
RUN chmod +x /usr/local/bin/gcc_wrapper
RUN mv /usr/bin/gcc /usr/bin/gcc.real && ln -s /usr/local/bin/gcc_wrapper /usr/bin/gcc
ENTRYPOINT ["entrypoint.sh"]
WORKDIR /repo
CMD ["bash"]
"""

ENTRYPOINT_CONTENT = """#!/bin/bash
set -e
HOST_UID=${HOST_UID:-1000}
HOST_GID=${HOST_GID:-1000}
HOST_USER=${HOST_USER:-user}
groupadd -g $HOST_GID $HOST_USER 2>/dev/null || true
useradd -u $HOST_UID -g $HOST_GID -s /bin/bash -m $HOST_USER 2>/dev/null || true
chown -R $HOST_USER:$HOST_USER /repo/ 2>/dev/null || true
chown -R $HOST_USER:$HOST_USER /output/ 2>/dev/null || true
exec gosu $HOST_USER "$@"
"""

GCC_WRAPPER_CONTENT = R"""#!/bin/sh
for arg in "$@"; do
    if [ "$arg" = "-static" ] || [ "$arg" = "--static" ]; then
        echo "Static linking is not allowed." >&2
        exit 1
    fi
done
if echo "$@" | grep -q '\-Wl,\-static'; then
    echo "Linker flag -static not allowed." >&2
    exit 1
fi

NEW_ARGS=""
SKIP_NEXT=0
for arg in "$@"; do
    if [ "$SKIP_NEXT" -eq 1 ]; then
        SKIP_NEXT=0
        continue
    fi
    
    case "$arg" in
        -Werror)
            continue
            ;;
        -std=*)
            continue
            ;;
        -Werror=*)
            continue
            ;;
        *_POSIX_C_SOURCE*)
            continue
            ;;
        *_XOPEN_SOURCE*)
            continue
            ;;
        *)
            NEW_ARGS="$NEW_ARGS $arg"
            ;;
    esac
done

EXTRA_FLAGS="-std=c99 -Wno-error -no-pie -fPIC -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700"

# echo "Running GCC wrapper:" >&2
# echo "    gcc $EXTRA_FLAGS $NEW_ARGS" >&2

exec /usr/bin/gcc.real $EXTRA_FLAGS $NEW_ARGS
"""

IMAGE_NAME = "shell_builder:latest"


def ensure_docker_image(force_rebuild: bool = False):
    client = docker.from_env()

    if not force_rebuild:
        try:
            client.images.get(IMAGE_NAME)
            logger.info(f"Docker image {IMAGE_NAME} already exists")
            return
        except docker.errors.ImageNotFound:
            pass

    logger.info(f"Building Docker image {IMAGE_NAME}...")

    build_dir = Path.cwd() / ".docker_build_temp"
    build_dir.mkdir(exist_ok=True)

    (build_dir / "Dockerfile").write_text(DOCKERFILE_CONTENT)
    (build_dir / "entrypoint.sh").write_text(ENTRYPOINT_CONTENT)
    (build_dir / "gcc_wrapper.sh").write_text(GCC_WRAPPER_CONTENT)

    try:
        image, build_logs = client.images.build(
            path=str(build_dir), tag=IMAGE_NAME, rm=True
        )
        logger.info(f"Docker image {IMAGE_NAME} built successfully")
    finally:
        for file in build_dir.iterdir():
            file.unlink()
        build_dir.rmdir()


def revert_git_repo(repo_path: Path):
    try:
        subprocess.run(
            ["git", "clean", "-fdx"],
            cwd=repo_path,
            check=True,
            capture_output=True,
        )
        subprocess.run(
            ["git", "checkout", "--", "."],
            cwd=repo_path,
            check=True,
            capture_output=True,
        )
    except subprocess.CalledProcessError as e:
        logger.warning(f"Could not fully revert repo {repo_path.name}: {e}")

def apply_git_diff(repo_path: Path, git_diff: Path):
    try:
        subprocess.run(
            ["git", "apply", str(git_diff.resolve())],
            cwd=repo_path,
            check=True,
            capture_output=True,
        )
    except subprocess.CalledProcessError as e:
        logger.warning(f"Could not apply patch file {git_diff} to {repo_path.name}: {e}")


def find_shell_directories(repo_path: Path) -> list[Path]:
    shell_dirs = []
    for path in repo_path.rglob("*"):
        if path.is_dir() and path.name.lower() == "shell":
            if "llm" not in str(path).lower():
                makefile = path / "Makefile"
                alt_makefile = path / "makefile"
                if makefile.exists() or alt_makefile.exists():
                    shell_dirs.append(path)
    return shell_dirs


def clean_build_artifacts(directory: Path):
    for pattern in ["*.o", "*.out", "shell.out", "a.out"]:
        for file in directory.rglob(pattern):
            if file.is_file():
                try:
                    file.unlink()
                except Exception:
                    pass


def find_binary_in_repo(repo_path: Path) -> Optional[Path]:
    for binary_name in ["shell.out", "shell", "a.out"]:
        for file in repo_path.rglob(binary_name):
            if file.is_file() and os.access(file, os.X_OK):
                return file
    return None


def build_in_container(
    repo_path: Path, shell_dir: Path, log_path: Path
) -> Tuple[bool, Optional[Path]]:
    client = docker.from_env()

    repo_path_abs = repo_path.resolve()

    relative_shell = shell_dir.relative_to(repo_path)

    container = None
    with tempfile.TemporaryDirectory(
        prefix=".docker_output_temp_"
    ) as temp_dir:
        output_dir_abs = Path(temp_dir).resolve()
        try:
            host_username = getpass.getuser()

            container = client.containers.create(
                image=IMAGE_NAME,
                command=[
                    "bash",
                    "-c",
                    f"cd /repo/{relative_shell} && make clean 2>&1; make all 2>&1 || make 2>&1",
                ],
                volumes={
                    str(repo_path_abs): {"bind": "/repo", "mode": "rw"},
                    str(output_dir_abs): {"bind": "/output", "mode": "rw"},
                },
                environment={
                    "HOST_UID": str(os.getuid()),
                    "HOST_GID": str(os.getgid()),
                    "HOST_USER": host_username,
                },
                detach=True,
            )

            container.start()
            result = container.wait()

            logs = container.logs(stdout=True, stderr=True).decode(
                "utf-8", errors="replace"
            )
            log_path.parent.mkdir(parents=True, exist_ok=True)
            log_path.write_text(logs)

            exit_code = result.get("StatusCode", 1)

            if exit_code == 0:
                binary = find_binary_in_repo(repo_path)
                return True, binary
            else:
                return False, None

        except Exception as e:
            logger.error(f"Container execution failed: {e}")
            if container:
                try:
                    logs = container.logs(stdout=True, stderr=True).decode(
                        "utf-8", errors="replace"
                    )
                    log_path.parent.mkdir(parents=True, exist_ok=True)
                    log_path.write_text(logs)
                except Exception:
                    pass
            return False, None
        finally:
            if container:
                try:
                    container.remove(force=True)
                except Exception:
                    pass

def process_repo(repo_path: Path, git_diff: Optional[Path] = None) -> Tuple[str, Dict]:
    repo_name = repo_path.name
    log_path = Path.cwd() / "logs" / repo_name / "build.log"
    binary_output_path = Path.cwd() / "binaries" / repo_name / "shell.out"

    result = {"binary_path": "", "log_path": str(log_path), "success": False}

    revert_git_repo(repo_path)

    shell_dirs = find_shell_directories(repo_path)

    if not shell_dirs:
        logger.warning(
            f"No shell directory with Makefile found in {repo_name}"
        )
        log_path.parent.mkdir(parents=True, exist_ok=True)
        log_path.write_text(
            "No shell directory with Makefile found in repository"
        )
        revert_git_repo(repo_path)
        return (repo_name, result)
    
    if git_diff:
        apply_git_diff(repo_path, git_diff)

    for shell_dir in shell_dirs:
        clean_build_artifacts(shell_dir)

        success, binary = build_in_container(repo_path, shell_dir, log_path)

        if success and binary:
            binary_output_path.parent.mkdir(parents=True, exist_ok=True)

            with open(binary, "rb") as src:
                binary_output_path.write_bytes(src.read())

            binary_output_path.chmod(0o755)

            result["binary_path"] = str(binary_output_path)
            result["success"] = True
            revert_git_repo(repo_path)
            return (repo_name, result)

    logger.warning(f"Failed to build binary for {repo_name}")
    revert_git_repo(repo_path)
    return (repo_name, result)


def load_roster(roster_path: Path) -> Dict[str, str]:
    mapping = {}
    with open(roster_path) as f:
        lines = f.readlines()[1:]
        for line in lines:
            items = line.strip().split(",")
            if len(items) >= 2:
                mapping[items[0][1:-1]] = items[1][1:-1]
    return mapping


@app.command()
def main(
    submissions_dir: Path = typer.Option(
        ...,
        "--submissions-dir",
        "-s",
        help="Path to parent folder with student repos",
    ),
    roster_path: Optional[Path] = typer.Option(
        None, "--roster-path", help="Path to GitHub Classroom roster CSV"
    ),
    roll_no: Optional[str] = typer.Option(
        None, "--roll-no", help="Specific roll number to rebuild"
    ),
    git_diff: Optional[Path] = typer.Option(
        None, "--git-diff", help="Optional git diff to apply to student's repo before building"
    ),
    force_rebuild: bool = typer.Option(
        False, "--force-rebuild", help="Force rebuild Docker image"
    ),
    threads: int = typer.Option(
        os.cpu_count(),
        "--threads",
        "-t",
        help="Number of threads to use for processing",
    ),
):
    if (roster_path is None) != (roll_no is None):
        logger.error(
            "Both --roster-path and --roll-no must be provided together"
        )
        raise typer.Exit(code=1)
    
    if git_diff and (roster_path is None):
        logger.error("Can only apply git diff on single student")

    ensure_docker_image(force_rebuild)

    results_file = Path.cwd() / "build-results.json"
    existing_results = {}
    if results_file.exists():
        with open(results_file) as f:
            existing_results = json.load(f)

    if roster_path and roll_no:
        roster = load_roster(roster_path)
        if roll_no not in roster:
            logger.error(f"Roll number {roll_no} not found in roster")
            raise typer.Exit(code=1)

        github_username = roster[roll_no]
        repo_name = f"mini-project-1-{github_username}"
        repo_path = submissions_dir / repo_name

        if not repo_path.exists():
            logger.error(
                f"Repository {repo_name} not found in {submissions_dir}"
            )
            raise typer.Exit(code=1)

        binary_dir = Path.cwd() / "binaries" / repo_name
        log_dir = Path.cwd() / "logs" / repo_name

        if binary_dir.exists():
            for file in binary_dir.iterdir():
                file.unlink()
            binary_dir.rmdir()

        if log_dir.exists():
            for file in log_dir.iterdir():
                file.unlink()
            log_dir.rmdir()

        logger.info(f"Processing single student: {repo_name}")
        repo_name, result = process_repo(repo_path, git_diff)
        existing_results[repo_name] = result

    else:
        if not submissions_dir.exists() or not submissions_dir.is_dir():
            logger.error(f"Submissions directory not found: {submissions_dir}")
            raise typer.Exit(code=1)

        repos = [
            p
            for p in submissions_dir.iterdir()
            if p.is_dir() and (p / ".git").exists()
        ]

        if not repos:
            logger.warning(
                "No git repositories found in submissions directory"
            )
            raise typer.Exit(code=0)

        logger.info(f"Found {len(repos)} repositories to process")

        with concurrent.futures.ThreadPoolExecutor(
            max_workers=threads
        ) as executor:
            future_to_repo = {
                executor.submit(process_repo, repo): repo for repo in repos
            }

            for future in tqdm(
                concurrent.futures.as_completed(future_to_repo),
                total=len(repos),
                desc="Processing repositories",
            ):
                try:
                    repo_name, result = future.result()
                    existing_results[repo_name] = result
                except Exception as e:
                    repo = future_to_repo[future]
                    logger.error(f"Error processing {repo.name}: {e}")

    with open(results_file, "w") as f:
        json.dump(existing_results, f, indent=2)

    logger.info(f"Build results saved to {results_file}")


if __name__ == "__main__":
    app()
