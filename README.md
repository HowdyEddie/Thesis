# SmartLockerStorage

ESP32 DevKit V4 locker firmware using PlatformIO. See [`src/PROJECT_PROGRESS.txt`](src/PROJECT_PROGRESS.txt) for the current project handoff and hardware-validation status.

## GitHub workflow

- Track physical verification with the **Physical hardware acceptance** issue template. Record the firmware commit, setup, actual results, and failures; use test data only.
- Track CH376S work with the **USB export implementation** issue template. Firmware-side export code is present, but USB export is not considered hardware-verified until it has been tested with the prototype and a known-good drive.
- Make focused changes on a short-lived branch and open a pull request targeting `main`. Link the issue in the pull request and update `src/PROJECT_PROGRESS.txt` when project status or validation results change.
- Pushes to any branch and pull requests targeting `main` run the **PlatformIO build / esp32dev** GitHub Actions check (`pio run -e esp32dev`). This lets the workflow validate its first branch before it has been merged to `main`. It verifies compilation only; it does not validate hardware or replace the physical acceptance checklist.
- In repository **Settings → Branches → Add branch protection rule**, protect `main`, require pull requests before merging, and require the **PlatformIO build / esp32dev** status check after its first successful run. For a solo project, reviews can remain optional; keep the pull request as the documented checkpoint.
- Never commit real student rosters, account data, credentials, or other private test data. Back up SD data before reset or migration testing.

## Build locally

Install PlatformIO Core, then run:

```sh
pio run -e esp32dev
```

The firmware build is not a substitute for testing with the actual ESP32, display, SD card, and locker hardware.
