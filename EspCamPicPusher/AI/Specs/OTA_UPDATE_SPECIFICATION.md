# OTA Firmware Update System Specification

**Document Version:** 1.1  
**Date:** March 7, 2026  
**Project:** EspCamPicPusher / WebCamPics  
**Author:** AI Specification Agent

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Component Specifications](#component-specifications)
4. [API Contracts](#api-contracts)
5. [State Machine Integration](#state-machine-integration)
6. [Security & Validation](#security--validation)
7. [Error Handling & Rollback](#error-handling--rollback)
8. [Implementation Phases](#implementation-phases)

---

## Overview

### Purpose

Implement over-the-air (OTA) firmware updates for ESP32-S3 camera devices using ESP-IDF core functions. Enables remote firmware deployment without physical access to devices while maintaining safety through validation and rollback mechanisms.

### Design Principles

1. **ESP-IDF Native**: Use `esp_ota_*` and `esp_partition_*` APIs directly (no Arduino HTTPUpdate library)
2. **Deep Sleep Compatible**: OTA operations occur after image-pushing in any mode
3. **Fail-Safe**: Automatic rollback on validation failure
4. **Minimal Memory**: Stream firmware data, don't buffer entire binary
5. **Admin-Controlled**: Server-side scheduling prevents unwanted updates
6. **Per-Device Targeting**: Different cameras can have different firmware versions
7. **Battery Protection**: Maximum 3 client-side retry attempts per firmware file to prevent battery drain loops
8. **Immediate Validation**: Force capture+validation after OTA boot even when no timeslot is due
9. **Dedicated OTA Mode**: OTA flashing happens in a minimal boot (no camera, no AsyncWebServer) to avoid watchdog conflicts and memory pressure

### Update Lifecycle

```
┌─────────────────┐
│ 1. Admin Upload │  Admin stores firmware.bin on server
└────────┬────────┘
         │
┌────────▼────────┐
│ 2. Association  │  Admin links firmware to camera(s) in cameras.json
└────────┬────────┘
         │
┌────────▼────────┐
│ 3. Discovery    │  Camera uploads image → receives "ota_available" in JSON response
└────────┬────────┘
         │
┌────────▼────────┐
│ 4. Save & Reboot│  OTA metadata saved to NVS → RemoteLogger flush → ESP.restart()
└────────┬────────┘  (safe: RemoteLogger still healthy, no async_tcp conflict)
         │
┌────────▼────────┐
│ 5. OTA Mode     │  Minimal boot: WiFi only, no camera, no AsyncWebServer
│    Boot         │  NVS flag cleared → performUpdate() → download → flash
└────────┬────────┘
         │
┌────────▼────────┐
│ 6. Flash/Reboot │  Write to inactive partition, set boot partition, esp_restart()
└────────┬────────┘
         │
┌────────▼────────┐
│ 7. Validation   │  New firmware confirms success → mark valid → confirm to server
└────────┬────────┘  OR fails validation → rollback to previous partition
         │
┌────────▼────────┐
│ 8. Confirmation │  POST to /ota-confirm.php with status → clear scheduled update
└─────────────────┘
```

---

## Architecture

### Partition Scheme

ESP32 uses dual OTA partition layout for safe updates:

```
┌─────────────────────────────────────┐
│ nvs          (20KB)  - NVS storage  │
│ otadata      (8KB)   - OTA metadata │
│ app0 (ota_0) (1.5MB) - Partition A  │  ← Currently running
│ app1 (ota_1) (1.5MB) - Partition B  │  ← Update target
│ spiffs       (varies) - Optional    │
└─────────────────────────────────────┘
```

**Update Process:**
1. Firmware writes to inactive partition (ota_1)
2. On success, set ota_1 as boot partition
3. Reboot → ESP32 boots from ota_1
4. Validate → Mark partition valid OR rollback to ota_0
5. Swap roles for next update

### Directory Structure

**PHP Server (WebCamPics):**

```
WebCamPics/
├── firmware/                    # NEW: Firmware storage
│   ├── firmware_v1.0.0.bin
│   ├── firmware_v1.1.0.bin
│   └── checksums.json          # SHA256 checksums for validation
├── ota-upload.php              # NEW: Firmware upload endpoint
├── ota-download.php            # NEW: Firmware download endpoint  
├── ota-confirm.php             # NEW: Update confirmation endpoint
├── config/
│   ├── cameras.json            # MODIFIED: Add ota_scheduled field
│   └── config.json
├── lib/
│   ├── ota.php                 # NEW: OTA helper functions
│   └── ...
```

**ESP32 Firmware:**

```
EspCamPicPusher/
├── lib/
│   ├── OTAManager/             # NEW: OTA update manager
│   │   ├── OTAManager.h
│   │   └── OTAManager.cpp
│   └── ...
├── src/
│   └── main.cpp                # MODIFIED: Add OTA handling
├── partitions.csv              # NEW: OTA partition table
```

---

## Component Specifications

### 1. PHP Server Components

#### 1.1 Firmware Storage (`firmware/` directory)

**Purpose:** Centralized storage for firmware binaries with versioning.

**Naming Convention:**
```
firmware_v{MAJOR}.{MINOR}.{PATCH}.bin
Example: firmware_v1.2.3.bin
```

**Checksums File (`firmware/checksums.json`):**
```json
{
  "firmware_v1.0.0.bin": {
    "sha256": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
    "size": 1245632,
    "uploaded": "2026-03-01T10:30:00Z",
    "description": "Initial release"
  },
  "firmware_v1.1.0.bin": {
    "sha256": "a94a8fe5ccb19ba61c4c0873d391e987982fbbd3e7e4e4f5c1e5c6e7f8e9f0a1",
    "size": 1256448,
    "uploaded": "2026-03-15T14:20:00Z",
    "description": "Added OTA support"
  }
}
```

#### 1.2 Camera Configuration Extension (`config/cameras.json`)

**Add OTA Fields:**
```json
{
  "cam_aabbccddeeff": {
    "device_id": "AA:BB:CC:DD:EE:FF",
    "location": "garden",
    "title": "Garden Camera",
    "status": "enabled",
    "firmware_version": "1.0.0",           // NEW: Current firmware (reported by camera)
    "ota_scheduled": null,                 // NEW: Firmware file to push (or null)
    "ota_retry_count": 0,                  // NEW: Failed attempts counter (0-2, clears on success)
    "ota_last_attempt": null,              // NEW: ISO timestamp of last attempt
    "ota_last_status": null,               // NEW: "success", "failed", "pending", "rollback"
    "ota_last_error": null,                // NEW: Error message if failed
    ...existing fields...
  }
}
```

### 2. ESP32 Firmware Components

#### 2.1 OTA Manager Library

Core OTA functionality using ESP-IDF functions:
- `esp_ota_begin()` - Start OTA update
- `esp_ota_write()` - Stream firmware data
- `esp_ota_end()` - Finalize update
- `esp_ota_set_boot_partition()` - Switch boot partition
- `esp_ota_mark_app_valid_cancel_rollback()` - Confirm update

**NVS Namespace `"ota"` — Key Layout:**

| Key | Type | Purpose |
|-----|------|---------|
| `pending` | bool | Pending OTA boot flag |
| `fwFile` | string | Firmware filename to flash |
| `fwVersion` | string | Firmware version string |
| `dlUrl` | string | Download URL/path |
| `size` | uint32 | Expected size in bytes |
| `sha256` | string | Expected SHA256 checksum |
| `mandatory` | bool | Mandatory update flag |
| `confFile` | string | Firmware file for post-reboot confirmation |
| `failFile` | string | Firmware file associated with failure counter |
| `failCount` | uint32 | Number of failed attempts for `failFile` |

`pending`/`fwFile`/etc. are cleared **before** `performUpdate()` to prevent reboot loops.  
`confFile` is written alongside them and only cleared after successful server confirmation.  
`failFile`/`failCount` persist across reboots; auto-reset when a different firmware file is offered.

#### 2.2 Integration Points

**Upload Response Extension:**
```json
{
  "success": true,
  "device_id": "AA:BB:CC:DD:EE:FF",
  "ota": {
    "available": true,
    "firmware_file": "firmware_v1.1.0.bin",
    "firmware_version": "1.1.0",
    "download_url": "https://server.com/ota-download.php?file=firmware_v1.1.0.bin",
    "size": 1256448,
    "sha256": "a94a8fe5...",
    "mandatory": false
  }
}
```

---

## API Contracts

### 1. Upload Response Extension

**Endpoint:** `POST /upload.php`

**New Request Header:**
```
X-Firmware-Version: 1.0.0
```

**Extended Response:**
- Includes `ota` object when update scheduled
- `ota.available` indicates if update is ready
- Contains all info needed for download/validation

### 2. Firmware Download

**Endpoint:** `GET /ota-download.php?file={filename}`

**Headers:**
```
X-Auth-Token: {token}
X-Device-ID: {device_id}
```

**Response:**
- Status: 200 OK
- Content-Type: application/octet-stream
- X-Firmware-SHA256: {checksum}
- Body: Binary firmware data

### 3. OTA Confirmation

**Endpoint:** `POST /ota-confirm.php`

**Request Body (Success):**
```json
{
  "success": true,
  "firmware_file": "firmware_v1.1.0.bin",
  "firmware_version": "1.1.0",
  "message": "OTA update successful"
}
```

**Request Body (Failure):**
```json
{
  "success": false,
  "firmware_file": "firmware_v1.1.0.bin",
  "error": "Checksum mismatch",
  "rollback": true
}
```

---

## State Machine Integration

### Operating Modes

Four modes are defined in `OperatingMode`:

| Mode | Description | OTA Trigger |
|------|-------------|-------------|
| `MODE_CONFIG` | Web server active | ✅ After successful image upload |
| `MODE_CAPTURE` | Timer-wake capture cycle | ✅ After successful image upload |
| `MODE_WAIT` | Waiting for imminent capture | ✅ After successful image upload |
| `MODE_OTA` | **Dedicated OTA flash mode** | N/A — executes OTA then reboots |

### OTA is Mode-Agnostic

OTA is triggered identically from any mode: after a successful image upload, if the server response contains `ota.available = true`, `handleOtaUpdate()` saves metadata to NVS and calls `ESP.restart()`. This avoids all mode-specific complications.

### MODE_OTA Boot Path

Checked in `setup()` **before** camera, web server, NTP, or RemoteLogger initialization:

```
setup() begins
  │
  ├─ configManager.begin()        ← Only NVS, no network
  │
  ├─ otaManager.hasPendingUpdate()?
  │     YES → WiFi only → otaManager.begin() → return (→ loop → runOtaMode())
  │     NO  → normal init continues
  │
  └─ normal mode selection (WAKE reason)
```

**Resources initialized in OTA mode:** WiFi, OTAManager  
**Resources NOT initialized:** camera, AsyncWebServer (no `async_tcp` task), NTP, RemoteLogger

This gives ~283 KB free heap vs. ~180 KB in full CONFIG mode, and eliminates the `async_tcp` watchdog crash that previously occurred when AsyncWebServer was stopped inline before OTA.

### First Boot After OTA

1. Device boots from new partition
2. Partition state: `ESP_OTA_IMG_PENDING_VERIFY`
3. `otaManager.isFirstBootAfterOta()` → true in `setup()`
4. `pendingOtaFirmwareFile` loaded from NVS key `confFile` (persisted before the OTA flash)
5. **Force immediate capture+validation** even if no timeslot is due
6. On successful capture: call `esp_ota_mark_app_valid_cancel_rollback()`
7. Send confirmation POST to `/ota-confirm.php` with the firmware filename
8. Clear `confFile`, failure tracking, and any leftover pending keys from NVS
9. If capture/validation fails, device auto-rolls back on next reboot

### Client-Side Retry Limiting

In addition to server-side retry tracking, the firmware enforces its own limit:

- NVS stores `failFile` (firmware filename) and `failCount` (integer)
- `handleOtaUpdate()` checks `failCount >= 3` **before** saving and rebooting
- If limit reached: log warning, skip OTA, continue normal operation
- Counter auto-resets when the server offers a **different** firmware filename
- Counter is cleared on successful OTA validation
- This prevents camera lock-out if server-side retry tracking fails or network communication is intermittent

---

## Security & Validation

### Authentication Layers

1. **Firmware Upload:** HTTP Basic Auth (admin only)
2. **Firmware Download:** X-Auth-Token (camera device token)
3. **Download Authorization:** Server verifies OTA scheduled for specific device

### Checksum Validation

**Server-Side:**
- Calculate SHA256 on upload
- Store in `checksums.json`

**ESP32-Side:**
- Calculate SHA256 during download (streaming with mbedtls)
- Compare with expected value
- Abort if mismatch

### Partition Validation

**ESP-IDF Mechanisms:**
1. New partition marked `PENDING_VERIFY` after flash
2. First boot from new partition
3. Application validates and calls `esp_ota_mark_app_valid_cancel_rollback()`
4. Auto-rollback if validation not confirmed

---

## Error Handling & Rollback

### Error Categories

1. **Download Errors:** Retry on next upload (up to 3 client-side attempts)
2. **Flash Errors:** Abort, keep current firmware
3. **Validation Errors:** Abort before flashing
4. **Boot Errors:** ESP-IDF auto-rollback
5. **WiFi Unavailable in OTA Mode:** Clear pending update, reboot normally

### Confirmation Protocol

**Success:** Server clears `ota_scheduled`, updates `firmware_version`, resets `ota_retry_count` to 0

**Failure:** Server increments `ota_retry_count`. If count reaches server-side limit, clears `ota_scheduled` (no further retries)

**Rollback:** Server clears `ota_scheduled`, resets `ota_retry_count` (firmware incompatible)

### Client-Side Retry Limit Mechanism

**Purpose:** Prevent battery drain from repeated failed OTA cycles, independent of server communication

**Implementation:**
1. `OTAManager` tracks `failCount` per firmware filename in NVS namespace `"ota"`
2. Before saving a new pending update, `handleOtaUpdate()` checks `getOtaFailureCount(firmwareFile) >= 3`
3. If limit reached: skip OTA, log warning via RemoteLogger, return without rebooting
4. Counter tracks the filename: offering a different firmware resets it automatically
5. Counter cleared on successful OTA validation (`clearOtaFailures()`)
6. Attempt number included in RemoteLogger context on each initiation

**Battery Protection:**
- Max 3 download+flash attempts per firmware file
- Each failed cycle: ~30-60s download + WiFi reconnect time
- Server-side limit provides additional guard; client-side limit adds defense-in-depth

### Logging Strategy

OTA operations span 2-3 reboots. RemoteLogger (synchronous HTTP) is only used when safe:

| Event | Logging method |
|-------|---------------|
| OTA detected in upload response | RemoteLogger (still in normal boot, healthy state) |
| Before OTA reboot | `RemoteLogger::flush()` called to drain buffer |
| During OTA mode boot | Serial only (RemoteLogger not initialized) |
| OTA failure in dedicated mode | `sendConfirmation()` to server; Serial |
| First boot after success | RemoteLogger available (normal boot) |
| Validation confirmed | RemoteLogger::info + sendConfirmation |
| Validation failed / rollback | RemoteLogger::error |

---

## Implementation Phases

### Phase 1: Server Infrastructure (8-12 hours)
1. Create firmware storage structure
2. Implement OTA helper functions
3. Create API endpoints (upload, download, confirm)
4. Extend cameras.json schema
5. Modify upload.php response

### Phase 2: Admin UI (4-6 hours)
1. Firmware upload form
2. Firmware list table
3. OTA scheduling interface
4. Status display

### Phase 3: ESP32 OTA Library (12-16 hours)
1. Create OTAManager library
2. Implement ESP-IDF functions
3. SHA256 streaming validation
4. Partition management

### Phase 4: Main Integration (6-8 hours)
1. Integrate OTAManager with main.cpp
2. Parse OTA from upload response
3. Mode-aware triggering
4. First-boot validation

### Phase 5: Testing (8-12 hours)
1. Happy path testing
2. Error scenario testing
3. Rollback testing
4. Multi-camera testing

---

## Partition Table

**File:** `partitions.csv`

```csv
# Name,     Type, SubType, Offset,  Size,     Flags
nvs,        data, nvs,     0x9000,  0x5000,
otadata,    data, ota,     0xe000,  0x2000,
app0,       app,  ota_0,   0x10000, 0x180000,
app1,       app,  ota_1,   0x190000,0x180000,
spiffs,     data, spiffs,  0x310000,0xF0000,
```

**Sizes:**
- nvs: 20KB
- otadata: 8KB  
- app0: 1.5MB
- app1: 1.5MB
- spiffs: 960KB (optional)

---

## Version Numbering

**Semantic Versioning:** `MAJOR.MINOR.PATCH`

- **MAJOR:** Breaking changes
- **MINOR:** New features (backward compatible)
- **PATCH:** Bug fixes

**Examples:**
- `1.0.0`: Initial release
- `1.1.0`: Added OTA support
- `2.0.0`: Breaking changes

---

## Testing Checklist

### Critical Tests

- [ ] Firmware upload works
- [ ] Checksums calculated correctly
- [ ] OTA appears in upload response when scheduled
- [ ] ESP32 saves OTA info to NVS and reboots into MODE_OTA
- [ ] OTA mode boot: no AsyncWebServer, no camera initialized
- [ ] ESP32 downloads firmware via HTTPS (~283 KB free heap)
- [ ] SHA256 validation works
- [ ] Partition write successful
- [ ] NVS pending flag cleared before flash (no reboot loop on power loss)
- [ ] confFile persisted and loaded correctly after OTA reboot
- [ ] Device boots from new partition
- [ ] First-boot validation works
- [ ] Confirmation sent to server with correct firmware filename
- [ ] Rollback works on failure
- [ ] OTA works from CONFIG mode (was broken: async_tcp WDT crash)
- [ ] OTA works from CAPTURE mode (was broken: dual TLS + camera DMA conflict)
- [ ] OTA works from WAIT mode
- [ ] Multiple cameras update independently

### Edge Cases

- [ ] Power loss during download (NVS pending cleared before flash — no loop; retries normally)
- [ ] WiFi fails in OTA mode (pending cleared, reboots to normal operation)
- [ ] Client retry limit: 3 failures for same firmware → OTA skipped, normal operation resumes
- [ ] Different firmware offered after failures → retry counter resets
- [ ] Invalid firmware rejected (checksum mismatch)
- [ ] Corrupted download (checksum fails)
- [ ] Firmware too large (error handled)

---

## ESP-IDF OTA Functions Reference

**Key Functions:**

```cpp
// Partition discovery
const esp_partition_t* esp_ota_get_running_partition(void);
const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t*);

// OTA operations
esp_err_t esp_ota_begin(const esp_partition_t*, size_t, esp_ota_handle_t*);
esp_err_t esp_ota_write(esp_ota_handle_t, const void*, size_t);
esp_err_t esp_ota_end(esp_ota_handle_t);
esp_err_t esp_ota_abort(esp_ota_handle_t);

// Boot management
esp_err_t esp_ota_set_boot_partition(const esp_partition_t*);
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void);
esp_err_t esp_ota_get_state_partition(const esp_partition_t*, esp_ota_img_states_t*);
```

**OTA States:**
- `ESP_OTA_IMG_NEW`: Not booted yet
- `ESP_OTA_IMG_PENDING_VERIFY`: First boot, needs validation
- `ESP_OTA_IMG_VALID`: Validated
- `ESP_OTA_IMG_INVALID`: Will rollback

---

## Troubleshooting

**Camera doesn't download OTA:**
- Check `ota_scheduled` in cameras.json
- Verify auth token
- Check firmware file exists

**OTA downloads but doesn't flash:**
- Verify partition size
- Check SHA256 checksum
- Review serial output for errors

**Device boots old firmware:**
- Check validation was called
- Verify no crashes during first boot
- Check camera/WiFi initialized successfully

---

**End of Specification**
