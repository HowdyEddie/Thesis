---
name: USB export implementation
about: Define and implement the CH376S USB export workflow
title: "Implement USB export"
---

## Hardware and requirements

- Exact CH376S module/board and interface:
- Required export behavior and file formats:
- Expected handling when no drive is connected or copying fails:
- Test setup and data-safety plan:

## Acceptance criteria

- [ ] Hardware/interface is confirmed against the actual device
- [ ] USB drive detection is verified on hardware
- [ ] Required files are copied and verified on the drive
- [ ] Errors are reported accurately; the UI never reports success for a failed copy
- [ ] Existing SD data remains intact
- [ ] Physical test results are recorded before this feature is described as working

Until these criteria pass on the physical device, USB export remains a placeholder.
