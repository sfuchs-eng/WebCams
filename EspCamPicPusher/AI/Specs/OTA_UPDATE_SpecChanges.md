# Summary of changes to the original OTA update spec

## Details

- Added a new design principle emphasizing deep sleep compatibility, specifying that OTA operations should occur during CONFIG or WAIT modes after image-pushing only.
- Validate the new firmware immediately after the first successful capture post-boot, even if no timeslot is due, to ensure the device can operate correctly with the new firmware before marking it as valid.

## Fundamental

- Add a mechanism to prevent iterating unsuccessful OTA attempts in a loop, which could lead to rapid battery drain. Fix the OTA retries to max 2 for the same firmware version.
