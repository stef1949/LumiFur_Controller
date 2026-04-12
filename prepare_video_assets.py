from pathlib import Path
import sys

Import("env")

PROJECT_DIR = Path(env["PROJECT_DIR"])
VIDEO_SOURCE_DIR = PROJECT_DIR / "video_sources"
VIDEO_OUTPUT_DIR = PROJECT_DIR / "data" / "videos"
DISCOVERY_DIRS = (
    VIDEO_SOURCE_DIR,
    VIDEO_OUTPUT_DIR,
)
CONVERTER_PATH = PROJECT_DIR / "tools" / "video_to_lfv.py"
DEFAULT_VIDEO_WIDTH = 64
DEFAULT_VIDEO_HEIGHT = 32
DEFAULT_VIDEO_FIT = "cover"
SUPPORTED_SOURCE_EXTENSIONS = {
    ".gif",
    ".mp4",
    ".webm",
    ".mov",
    ".mkv",
    ".avi",
}

sys.path.insert(0, str(CONVERTER_PATH.parent))
from video_to_lfv import Options, convert  # noqa: E402


def should_convert(source_path: Path, output_path: Path) -> bool:
    if not output_path.exists():
        return True

    output_mtime = output_path.stat().st_mtime
    if source_path.stat().st_mtime > output_mtime:
        return True
    if CONVERTER_PATH.stat().st_mtime > output_mtime:
        return True

    return False


def output_path_for_source(source_path: Path) -> Path:
    return VIDEO_OUTPUT_DIR / f"{source_path.stem.lower()}.lfv"


def discover_video_sources(source_dirs: tuple[Path, ...]) -> list[Path]:
    found: dict[str, Path] = {}
    for source_dir in source_dirs:
        if not source_dir.exists():
            continue

        for candidate in sorted(source_dir.iterdir()):
            if not candidate.is_file():
                continue
            if candidate.suffix.lower() not in SUPPORTED_SOURCE_EXTENSIONS:
                continue

            stem = candidate.stem.lower()
            if stem in found:
                raise RuntimeError(
                    f"Multiple video sources share the same output name '{stem}': "
                    f"{found[stem].relative_to(PROJECT_DIR)} and {candidate.relative_to(PROJECT_DIR)}"
                )
            found[stem] = candidate

    return list(found.values())


def convert_sources() -> None:
    sources = discover_video_sources(DISCOVERY_DIRS)
    if not sources:
        return

    VIDEO_OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    print("Preparing video assets...")

    for source_path in sources:
        output_path = output_path_for_source(source_path)
        if not should_convert(source_path, output_path):
            print(f"  up to date: {output_path.relative_to(PROJECT_DIR)}")
            continue

        print(f"  converting: {source_path.relative_to(PROJECT_DIR)} -> {output_path.relative_to(PROJECT_DIR)}")
        convert(
            Options(
                input_path=source_path,
                output_path=output_path,
                width=DEFAULT_VIDEO_WIDTH,
                height=DEFAULT_VIDEO_HEIGHT,
                fps=None,
                threshold=128,
                fit=DEFAULT_VIDEO_FIT,
                invert=False,
                keyframe_interval=120,
                merge_gap=2,
                use_source_fps=False,
            )
        )


convert_sources()
