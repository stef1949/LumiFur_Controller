Place converted `.lfv` video assets in this folder before running `pio run -t uploadfs`.

The firmware looks for `/videos/bad_apple.lfv` when `VIEW_VIDEO_PLAYER` is selected.

You can also drop supported source media here, including `.gif`, `.mp4`, `.webm`, `.mov`, `.mkv`, and `.avi`. The build converts them into lowercase `.lfv` files automatically during `buildfs` or `uploadfs`.

By default, source media is converted into a `64x32` LFV clip using `cover` fit, so each 64x32 panel is fully filled while keeping the source aspect ratio. The firmware mirrors that clip onto both panels.

`video_sources/` is also watched for source media if you prefer to keep the raw files out of the FATFS data folder.

Example:

```powershell
py tools/video_to_lfv.py "C:\path\to\bad_apple.gif" --output data/videos/bad_apple.lfv
C:\Users\stef1\.platformio\penv\Scripts\pio.exe run -e adafruit_matrixportal_esp32s3 -t uploadfs
```
