---
name: Physical hardware acceptance
about: Record ESP32 locker hardware acceptance results
title: "Hardware acceptance: "
---

## Test setup

- Firmware commit:
- Board / display / peripheral revisions:
- SD card and test-data backup:
- Tester and date:

## Results

- [ ] GPIO14 SD initialization, write, and card-removal behavior
- [ ] Daily/monthly CSV creation, quoted fields, reboot history reload, and legacy root-file migration
- [ ] Multiple-slot borrow/return and independent output relocking
- [ ] RFID returns on the Locked Screen and both Home Dashboard pages
- [ ] Power-cycle recovery of today's current logs and slot state
- [ ] RTC midnight rollover refreshes current logs without deleting prior-day files
- [ ] QR/manual borrower entry and optional roster lookup (use test data only)
- [ ] Admin access, limit-switch safety, and display idle/wake behavior
- [ ] Limit-switch interruption clears selection; after release, a zero-valued slot VP and confirm work
- [ ] Dashboard slot tap selects, repeat tap deselects, and tapping another available slot moves selection

## Evidence and failures

Record each result, observed behavior, and any relevant sanitized logs or photos. Do not attach real student rosters, credentials, or personal data.

