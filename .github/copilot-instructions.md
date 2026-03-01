# WebCams Project Guidelines

## Project Overview

Multi-component distributed camera system: ESP32-S3 firmware ([EspCamPicPusher/](EspCamPicPusher/)), PHP web server ([WebCamPics/](WebCamPics/)), and Raspberry Pi scripts ([RasPiCam/](RasPiCam/)). Cameras push JPEG images via HTTPS POST to a PHP server that stores and displays them. No database—uses JSON config and filesystem.

## Architecture

**Data Flow**: Camera Devices → HTTPS POST → [upload.php](WebCamPics/upload.php) → Process/Store → Web Display ([index.php](WebCamPics/index.php), [location.php](WebCamPics/location.php))

**ESP32 State Machine**: `MODE_CONFIG` (web UI active) → `MODE_CAPTURE` (wake, capture, upload, sleep) → `MODE_WAIT` (next capture <5 min). See [main.cpp](EspCamPicPusher/src/main.cpp) state handling.

**Library Pattern (ESP32)**: Modular manager classes in separate [lib/](EspCamPicPusher/lib/) directories—`ConfigManager` (NVS persistence), `ScheduleManager` (wake timing), `SleepManager` (deep sleep + RTC memory + WiFi retry tracking), `WebConfigServer` (async REST API + AP/STA mode awareness), `CameraMutex` (thread-safe camera access for dual-core ESP32-S3).

**PHP Structure**: Modular [lib/](WebCamPics/lib/)—`auth.php` (token validation with fallback headers), `storage.php` (sanitized filesystem ops), `image.php` (GD processing), `path.php` (installation-agnostic URL generation).

## API Conventions

**Upload Protocol** (see [upload.php](WebCamPics/upload.php)):
- Headers: `Content-Type: image/jpeg`, `Authorization: Bearer {token}` (not passing some web hosters' nginx-apache config) OR `X-Auth-Token: {token}` (works for most hosting providers), `X-Device-ID: {MAC}`
- Body: Raw JPEG binary (no multipart encoding for current API)
- Response: JSON with `success`, `device_id`, `timestamp`, `size`, `filename`
- **Legacy API also supported**: multipart/form-data with `auth` and `cam` POST params

**Authentication Layers**:
1. Upload API: Token array in [config/config.json](WebCamPics/config/config.json) (multiple tokens for different devices/rotation)
2. Admin panel: HTTP Basic Auth (`.htpasswd` file)
3. ESP32 web config: Optional Basic Auth (stored in NVS)

**Path Handling (PHP)**: Always use `baseUrl()` from [lib/path.php](WebCamPics/lib/path.php) for internal links—auto-detects root/subdirectory installation. Never hardcode paths.

## Build and Test

**ESP32 (PlatformIO)**:
```bash
cd EspCamPicPusher
~/.platformio/penv/bin/pio run                    # Build
~/.platformio/penv/bin/pio run --target upload    # Flash
~/.platformio/penv/bin/pio device monitor -b 115200  # Serial debug
```

**PHP Server Setup**:
```bash
cd WebCamPics
./setup.sh                 # Configure permissions, .htaccess paths
./testing/test_upload.sh test.jpg      # Test current API
./testing/test_legacy_upload.sh test.jpg  # Test legacy API
```

**Raspberry Pi**:
```bash
cd RasPiCam
./capture-upload-image.sh  # Manual capture & upload
```

## Project Conventions

**Camera Status (3-state)**: `disabled` (discard uploads), `hidden` (process but don't display), `enabled` (public). New cameras auto-register as `hidden` on first upload.

**Thread Safety**: **Always** use [CameraMutex](EspCamPicPusher/lib/CameraMutex/) when accessing ESP32 camera (dual-core race conditions). Example pattern:
```cpp
if (!CameraMutex::lock(5000)) { /* timeout */ }
camera_fb_t* fb = esp_camera_fb_get();
// ... use fb ...
esp_camera_fb_return(fb);
CameraMutex::unlock();
```

**RTC Memory Pattern** (ESP32): Store boot count, NTP sync timestamp, failure counters, WiFi retry count in [RTC data struct](EspCamPicPusher/lib/SleepManager/SleepManager.h) to survive deep sleep (not power cycles). Minimizes NTP requests and enables error recovery. WiFi retry counter persists across 5-minute retry intervals during timer wake.

**Power Management**: Default to deep sleep (~10-150 µA). Wake `sleepMarginSec` (default 60s) before capture. Don't sleep if next capture <5 min away. See [SleepManager](EspCamPicPusher/lib/SleepManager/).

**File Organization**:
- PHP: Modular `lib/` utilities (snake_case functions)
- C++: Library classes in `lib/{ComponentName}/` (PascalCase classes, camelCase methods)
- Config: PHP uses JSON, ESP32 uses NVS with compiled defaults
- Specs and reports per project built with AI in `../<project>/AI/Specs/` and `../<project>/AI/Reports/` (markdown format)

**Path Sanitization**: Device IDs converted to filesystem-safe names (MAC colons→hyphens, whitelist `[a-zA-Z0-9_-]`, 3-64 chars). See [storage.php](WebCamPics/lib/storage.php#L14-L49).

**Testing Strategy**: Bash scripts with curl for API testing. Serial monitor + LED feedback for ESP32. Manual web UI testing before deployment.

## Integration Points

**ESP32 → Server** ([main.cpp upload logic](EspCamPicPusher/src/main.cpp)):
```cpp
HTTPClient http;
http.begin(client, serverUrl);
http.addHeader("X-Auth-Token", token);
http.addHeader("X-Device-ID", WiFi.macAddress());
http.POST(imageBuffer, imageLength);
```

**WiFi Modes** (ESP32):
- **STA only**: `WiFi.mode(WIFI_STA)` for normal operation after successful config
- **AP+STA**: `WiFi.mode(WIFI_AP_STA)` + `WiFi.softAP()` when WiFi fails on boot
- AP SSID format: `ESP32-CAM-{last4MAC}` (e.g., `ESP32-CAM-A1B2`)
- AP IP: 192.168.4.1 (unprotected, no password)
- Both modes accessible simultaneously (web UI on both networks)

**Raspberry Pi → Server** ([capture-upload-image.sh](RasPiCam/capture-upload-image.sh)):
```bash
libcamera-jpeg -o - | curl -X POST \
  -H "X-Auth-Token: ${TOKEN}" \
  -H "X-Device-ID: ${CAM}" \
  --data-binary @- "${URL}"
```

**Header Fallback** (PHP): Checks `X-Auth-Token`, `Authorization`, `$_SERVER['HTTP_AUTHORIZATION']`, `REDIRECT_HTTP_AUTHORIZATION` for nginx/PHP-FPM compatibility. See [lib/auth.php](WebCamPics/lib/auth.php#L167-L186).

## Security

**HTTPS Required**: ESP32 uses `WiFiClientSecure` (currently `.setInsecure()`—*should implement cert validation for production*).

**Input Validation**: MIME type checks (`image/jpeg` only), size limits (configurable `upload_max_size_mb`), device ID sanitization (path traversal prevention), schedule time range checks (0-23 hours, 0-59 minutes).

**Token Management**: Multiple tokens supported in array format. Same tokens work for current + legacy APIs. Admin panel shows tokens (HTTP Basic Auth protected).

## Recent Implementations

**WiFi AP+STA Fallback Mode** (2026-03): Automatic WiFi recovery and configuration resilience. On boot failure, ESP32 creates unprotected AP (`ESP32-CAM-{last4MAC}`) at 192.168.4.1 while continuing STA connection attempts. Web UI includes live WiFi credential testing (`/config/test` endpoint) before save, automatic reboot on successful WiFi change, and visual countdown. Timer wake implements 5-retry logic at 5-minute intervals before sleeping until next capture. RTC memory tracks retry count across deep sleep. Status endpoint returns both AP and STA connection details. See [main.cpp](EspCamPicPusher/src/main.cpp) `setupWiFiAPSTA()`, [SleepManager](EspCamPicPusher/lib/SleepManager/) retry tracking, [WebConfigServer](EspCamPicPusher/lib/WebConfigServer/) test endpoint.

**Path-Agnostic Installation** (see [PATH_AGNOSTIC_IMPLEMENTATION.md](WebCamPics/AI/Reports/PATH_AGNOSTIC_IMPLEMENTATION.md)): Auto-detects installation path. All PHP files updated to use `baseUrl()` instead of hardcoded paths.

**Deep Sleep + Web Config v2.0** (see [IMPLEMENTATION_SUMMARY.md](EspCamPicPusher/AI/Reports/IMPLEMENTATION_SUMMARY.md)): 4 new library components (~2,575 lines), state machine architecture, 99% power reduction, activity-based timeout with schedule awareness.

## Key Differences from Defaults

- No database—filesystem + JSON
- ESP32 deep sleep by default (not continuous)
- Mutex protection for dual-core camera access
- Multiple authentication tokens (array, not single)
- Path auto-detection (not hardcoded)
- 3-state camera visibility
- Legacy API backward compatibility maintained
- RTC memory for persistent data across sleeps
- WiFi AP+STA fallback mode on boot failure
- Live WiFi credential testing before save
- Timer wake retry logic (5 attempts at 5-min intervals)
