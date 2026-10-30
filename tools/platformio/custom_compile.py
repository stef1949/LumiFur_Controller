"""Inject Git identity without overriding the configured firmware version.

SOURCE_DATE_EPOCH can supply a reproducible UTC build timestamp in release jobs.
"""
from datetime import datetime, timezone
from pathlib import Path
import os
import subprocess


def git_value(project_dir, *arguments):
    try:
        return subprocess.check_output(
            ["git", "-C", str(project_dir), *arguments],
            stderr=subprocess.DEVNULL, text=True,
        ).strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def inject_git_info(env):
    project_dir = Path(env["PROJECT_DIR"])
    commit = git_value(project_dir, "rev-parse", "--short=12", "HEAD")
    branch = git_value(project_dir, "rev-parse", "--abbrev-ref", "HEAD")
    definitions = [("GIT_COMMIT", env.StringifyMacro(commit)),
                   ("GIT_BRANCH", env.StringifyMacro(branch))]
    if "SOURCE_DATE_EPOCH" in os.environ:
        timestamp = datetime.fromtimestamp(int(os.environ["SOURCE_DATE_EPOCH"]), timezone.utc)
        # Pre-scripts run before PlatformIO parses build_flags. Remove its
        # defaults during the later unflag pass, after they have been parsed.
        env.Append(BUILD_UNFLAGS=["-DBUILD_DATE=__DATE__", "-DBUILD_TIME=__TIME__"])
        definitions += [("BUILD_DATE", env.StringifyMacro(timestamp.strftime("%Y-%m-%d"))),
                        ("BUILD_TIME", env.StringifyMacro(timestamp.strftime("%H:%M:%S UTC")))]
    env.Append(CPPDEFINES=definitions)


if "Import" in globals():
    Import("env")
    inject_git_info(env)
