# customCompile.py
import subprocess
from datetime import datetime

def get_git_tag():
    try:
        return subprocess.check_output(["git", "describe", "--tags", "--always"]).decode().strip()
    except:
        return "unknown"

def get_git_commit():
    try:
        return subprocess.check_output(["git", "rev-parse", "--short", "HEAD"]).decode().strip()
    except:
        return "unknown"

def get_git_branch():
    try:
        return subprocess.check_output(["git", "rev-parse", "--abbrev-ref", "HEAD"]).decode().strip()
    except:
        return "unknown"

def inject_git_info(env):
    fw_tag = get_git_tag()
    commit = get_git_commit()
    branch = get_git_branch()
    date_str = datetime.now().strftime("%Y-%m-%d")
    time_str = datetime.now().strftime("%H:%M:%S")

    env.Append(
        BUILD_FLAGS=[
            f'-DFIRMWARE_VERSION="{fw_tag}"',
            f'-DGIT_COMMIT="{commit}"',
            f'-DGIT_BRANCH="{branch}"',
            f'-DBUILD_DATE="{date_str}"',
            f'-DBUILD_TIME="{time_str}"'
        ]
    )

def before_build(env):
    inject_git_info(env)

Import("env")
before_build(env)