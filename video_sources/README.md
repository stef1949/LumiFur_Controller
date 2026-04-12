Place source media files here for automatic conversion during `buildfs` and `uploadfs`.

Supported source formats:

- `.gif`
- `.mp4`
- `.webm`
- `.mov`
- `.mkv`
- `.avi`

Examples:

- `video_sources/bad_apple.gif`
- `video_sources/bad_apple.mp4`

The build script converts each source into `data/videos/<lowercase-name>.lfv`. By default the output is a `64x32` per-panel clip with `cover` fit, so each panel is filled while preserving aspect ratio, and the firmware mirrors it onto both 64x32 panels.

Source media placed directly in `data/videos/` is also converted automatically.
