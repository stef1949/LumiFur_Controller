#!/usr/bin/env python3
"""Convert a standard video file into LumiFur's monochrome LFV1 format."""

from __future__ import annotations

import argparse
import os
import shutil
import struct
import subprocess
from dataclasses import dataclass
from fractions import Fraction
from pathlib import Path
from typing import Optional


MAGIC = b"LFV1"
HEADER_STRUCT = struct.Struct("<4sHHHHII")
FRAME_HEADER_STRUCT = struct.Struct("<BBH")
RUN_HEADER_STRUCT = struct.Struct("<HH")
FRAME_KEYFRAME = 0
FRAME_DELTA = 1


@dataclass
class Options:
    input_path: Path
    output_path: Path
    width: int
    height: int
    fps: Optional[float]
    threshold: int
    fit: str
    invert: bool
    keyframe_interval: int
    merge_gap: int
    use_source_fps: bool


def parse_args() -> Options:
    parser = argparse.ArgumentParser(description="Convert MP4/GIF/WebM/etc. into LumiFur LFV1 video files.")
    parser.add_argument("input", type=Path, help="Source video file.")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("data/videos/bad_apple.lfv"),
        help="Destination LFV file. Defaults to data/videos/bad_apple.lfv.",
    )
    parser.add_argument("--width", type=int, default=64, help="Output width in pixels. Must be a multiple of 8.")
    parser.add_argument("--height", type=int, default=32, help="Output height in pixels.")
    parser.add_argument(
        "--fps",
        default=None,
        help="Output frame rate. Use a number or 'source'. Defaults to source timing for GIFs and 15 FPS otherwise.",
    )
    parser.add_argument("--threshold", type=int, default=128, help="Monochrome threshold from 0-255.")
    parser.add_argument(
        "--fit",
        choices=("stretch", "contain", "cover"),
        default="cover",
        help="Resize strategy before thresholding.",
    )
    parser.add_argument("--invert", action="store_true", help="Invert black/white after thresholding.")
    parser.add_argument(
        "--keyframe-interval",
        type=int,
        default=120,
        help="Insert a raw keyframe every N frames for robust looping.",
    )
    parser.add_argument(
        "--merge-gap",
        type=int,
        default=2,
        help="Merge diff runs across short unchanged byte gaps to reduce metadata overhead.",
    )
    args = parser.parse_args()

    if args.width <= 0 or args.height <= 0:
        parser.error("width and height must be positive")
    if args.width % 8 != 0:
        parser.error("width must be a multiple of 8")
    if not (0 <= args.threshold <= 255):
        parser.error("threshold must be between 0 and 255")
    if args.keyframe_interval <= 0:
        parser.error("keyframe interval must be positive")
    if args.merge_gap < 0:
        parser.error("merge gap cannot be negative")

    fps: Optional[float] = None
    use_source_fps = False
    if args.fps is not None:
        raw_fps = str(args.fps).strip().lower()
        if raw_fps == "source":
            use_source_fps = True
        else:
            try:
                fps = float(raw_fps)
            except ValueError as exc:
                raise SystemExit(f"Invalid fps value: {args.fps}") from exc
            if fps <= 0:
                parser.error("fps must be positive")

    return Options(
        input_path=args.input,
        output_path=args.output,
        width=args.width,
        height=args.height,
        fps=fps,
        threshold=args.threshold,
        fit=args.fit,
        invert=args.invert,
        keyframe_interval=args.keyframe_interval,
        merge_gap=args.merge_gap,
        use_source_fps=use_source_fps,
    )


def require_tool(name: str) -> str:
    resolved = shutil.which(name)
    if not resolved:
        raise SystemExit(f"{name} was not found in PATH")
    return resolved


def build_filter(opts: Options) -> str:
    assert opts.fps is not None
    if opts.fit == "stretch":
        scale_filter = f"scale={opts.width}:{opts.height}:flags=lanczos"
    elif opts.fit == "contain":
        scale_filter = (
            f"scale={opts.width}:{opts.height}:force_original_aspect_ratio=decrease:flags=lanczos,"
            f"pad={opts.width}:{opts.height}:(ow-iw)/2:(oh-ih)/2:black"
        )
    else:
        scale_filter = (
            f"scale={opts.width}:{opts.height}:force_original_aspect_ratio=increase:flags=lanczos,"
            f"crop={opts.width}:{opts.height}"
        )

    return f"fps={opts.fps},{scale_filter},format=gray"


def parse_frame_rate(rate_text: str) -> Optional[float]:
    value = rate_text.strip()
    if not value or value == "0/0":
        return None

    try:
        if "/" in value:
            rate = float(Fraction(value))
        else:
            rate = float(value)
    except (ValueError, ZeroDivisionError):
        return None

    if rate <= 0:
        return None
    return rate


def probe_source_fps(input_path: Path, ffprobe_path: str) -> Optional[float]:
    command = [
        ffprobe_path,
        "-v",
        "error",
        "-select_streams",
        "v:0",
        "-show_entries",
        "stream=avg_frame_rate,r_frame_rate",
        "-of",
        "default=noprint_wrappers=1:nokey=1",
        str(input_path),
    ]
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        return None

    for line in result.stdout.splitlines():
        fps = parse_frame_rate(line)
        if fps is not None:
            return fps

    return None


def resolve_output_fps(opts: Options, ffprobe_path: Optional[str]) -> float:
    if opts.fps is not None:
        return opts.fps

    should_probe_source = opts.use_source_fps or opts.input_path.suffix.lower() == ".gif"
    if should_probe_source:
        if not ffprobe_path:
            if opts.use_source_fps:
                raise SystemExit("ffprobe was not found in PATH; --fps source requires ffprobe")
        else:
            source_fps = probe_source_fps(opts.input_path, ffprobe_path)
            if source_fps is not None:
                return source_fps
            if opts.use_source_fps:
                raise SystemExit(f"Could not determine source frame rate for {opts.input_path}")

    return 15.0


def pack_frame(frame_bytes: bytes, width: int, height: int, threshold: int, invert: bool) -> bytes:
    bytes_per_row = width // 8
    packed = bytearray(bytes_per_row * height)
    src_index = 0

    for y in range(height):
        for x_byte in range(bytes_per_row):
            packed_value = 0
            for _ in range(8):
                pixel_on = frame_bytes[src_index] >= threshold
                if invert:
                    pixel_on = not pixel_on
                packed_value = (packed_value << 1) | int(pixel_on)
                src_index += 1
            packed[y * bytes_per_row + x_byte] = packed_value

    return bytes(packed)


def encode_delta(current: bytes, previous: bytes, merge_gap: int) -> bytes:
    payload = bytearray()
    idx = 0
    frame_size = len(current)

    while idx < frame_size:
        if current[idx] == previous[idx]:
            idx += 1
            continue

        run_start = idx
        last_diff = idx
        idx += 1

        while idx < frame_size:
            if current[idx] != previous[idx]:
                last_diff = idx
                idx += 1
                continue

            gap_start = idx
            while idx < frame_size and current[idx] == previous[idx]:
                idx += 1
                if idx - gap_start > merge_gap:
                    break
            if idx < frame_size and current[idx] != previous[idx] and (idx - gap_start) <= merge_gap:
                last_diff = idx
                idx += 1
                continue

            break

        run_end = last_diff + 1
        run_data = current[run_start:run_end]
        payload.extend(RUN_HEADER_STRUCT.pack(run_start, len(run_data)))
        payload.extend(run_data)
        idx = run_end

    return bytes(payload)


def write_frame(out_file, encoding: int, payload: bytes) -> None:
    out_file.write(FRAME_HEADER_STRUCT.pack(encoding, 0, len(payload)))
    out_file.write(payload)


def read_exact(stream, size: int) -> bytes:
    chunks = bytearray()
    while len(chunks) < size:
        chunk = stream.read(size - len(chunks))
        if not chunk:
            break
        chunks.extend(chunk)
    return bytes(chunks)


def convert(opts: Options) -> None:
    ffmpeg = require_tool("ffmpeg")
    ffprobe = shutil.which("ffprobe")

    if not opts.input_path.exists():
        raise SystemExit(f"Input file not found: {opts.input_path}")

    opts.fps = resolve_output_fps(opts, ffprobe)
    opts.output_path.parent.mkdir(parents=True, exist_ok=True)
    frame_interval_micros = max(1, round(1_000_000 / opts.fps))
    bytes_per_frame = (opts.width * opts.height) // 8
    raw_frame_bytes = opts.width * opts.height
    filter_graph = build_filter(opts)

    command = [
        ffmpeg,
        "-v",
        "error",
        "-i",
        str(opts.input_path),
        "-an",
        "-sn",
        "-vf",
        filter_graph,
        "-f",
        "rawvideo",
        "-pix_fmt",
        "gray",
        "pipe:1",
    ]

    print(f"Converting {opts.input_path} -> {opts.output_path}")
    print(f"FPS: {opts.fps:.3f}")
    print(f"Filter: {filter_graph}")

    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    previous_frame = bytes(bytes_per_frame)
    frame_count = 0

    with opts.output_path.open("wb+") as out_file:
        out_file.write(
            HEADER_STRUCT.pack(
                MAGIC,
                opts.width,
                opts.height,
                bytes_per_frame,
                0,
                0,
                frame_interval_micros,
            )
        )

        assert process.stdout is not None
        while True:
            raw_frame = read_exact(process.stdout, raw_frame_bytes)
            if not raw_frame:
                break
            if len(raw_frame) != raw_frame_bytes:
                raise SystemExit("ffmpeg returned an incomplete frame")

            packed_frame = pack_frame(raw_frame, opts.width, opts.height, opts.threshold, opts.invert)
            use_keyframe = frame_count == 0 or (frame_count % opts.keyframe_interval) == 0

            if use_keyframe:
                write_frame(out_file, FRAME_KEYFRAME, packed_frame)
            else:
                delta_payload = encode_delta(packed_frame, previous_frame, opts.merge_gap)
                if len(delta_payload) >= len(packed_frame):
                    write_frame(out_file, FRAME_KEYFRAME, packed_frame)
                else:
                    write_frame(out_file, FRAME_DELTA, delta_payload)

            previous_frame = packed_frame
            frame_count += 1
            if frame_count % 300 == 0:
                print(f"Encoded {frame_count} frames...")

        return_code = process.wait()
        stderr_output = process.stderr.read().decode("utf-8", errors="replace") if process.stderr else ""
        if return_code != 0:
            raise SystemExit(stderr_output.strip() or "ffmpeg conversion failed")
        if frame_count == 0:
            raise SystemExit("The source video produced no frames")

        out_file.seek(0)
        out_file.write(
            HEADER_STRUCT.pack(
                MAGIC,
                opts.width,
                opts.height,
                bytes_per_frame,
                0,
                frame_count,
                frame_interval_micros,
            )
        )

    output_size = os.path.getsize(opts.output_path)
    print(f"Wrote {frame_count} frames, {output_size / 1024:.1f} KiB")
    print("Upload with: pio run -e adafruit_matrixportal_esp32s3 -t uploadfs")


def main() -> None:
    convert(parse_args())


if __name__ == "__main__":
    main()
