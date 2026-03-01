# EspCamPicPusher - Deep Sleep & Web Configuration Implementation

## Overview

This implementation adds two major features to EspCamPicPusher:

1. **Web Configuration Mode**: A 15-minute (configurable) web server that starts on power-up, allowing runtime configuration of all settings via a user-friendly web interface.

2. **Deep Sleep Power Management**: Ultra-low-power operation between scheduled captures by entering deep sleep and waking automatically ~60 seconds before each scheduled image capture.

## Architecture Changes

### Operating Modes

The ESP32 now operates in three distinct modes:

#### 1. **CONFIG MODE** (Web Server Active)
- Triggers on: Fresh boot/power cycle
- Duration: 15 minutes (configurable), resets on any HTTP request
- Features:
  - Full web UI at `http://<ESP_IP>/`
  - Change WiFi credentials
  - Update server URL and auth token
  - Modify capture schedule
  - Manual image capture with preview
  - View device status
  - Factory reset option
- Exit: After timeout (if next capture >5 min away) → enters sleep mode
- Exit: After timeout (if next capture <5 min away) → enters wait mode

#### 2. **CAPTURE MODE** (Quick Capture)
- Triggers on: Timer wake from deep sleep
- Actions:
  1. Connect to WiFi (skip if already connected)
  2. Sync NTP if >24 hours since last sync
  3. Initialize camera
  4. Capture and upload image
  5. Calculate next wake time
  6. Enter deep sleep
- LED Indication: 2 slow blinks on success, 5 fast on error
- Failure Handling: After 3 consecutive failures, stays awake in CONFIG mode

#### 3. **WAIT MODE** (Active Waiting)
- Triggers on: When next capture is <5 minutes away
- Purpose: Avoid sleep/wake cycling for imminent captures
- Actions: Poll time every 10 seconds, execute capture when scheduled
- Exit: After capture, checks if should sleep or continue waiting

### New Library Components

#### ConfigManager (`lib/ConfigManager/`)
- Manages all configuration using ESP32 NVS (non-volatile storage)
- Persists settings across reboots and power cycles
- Validation of all configuration parameters
- JSON import/export for web API
- Factory defaults from compiled-in values

**Stored Configuration:**
```cpp
- WiFi SSID and password
- Server URL and authentication token
- NTP timezone offsets (GMT + daylight)
- Capture schedule (up to 24 times/day)
- Web server timeout (1-240 minutes)
- Sleep margin (0-600 seconds)
```

#### ScheduleManager (`lib/ScheduleManager/`)
- Calculates next wake time from current time and schedule
- Handles midnight rollovers
- Determines seconds until next scheduled event
- Checks if current time matches any scheduled capture time
- Time formatting utilities

**Key Algorithm:**
```
Current Time: 14:30
Schedule: [08:00, 11:00, 15:00, 17:00]
Sleep Margin: 60 seconds
→ Next capture: 15:00
→ Wake time: 14:59:00
→ Sleep duration: 29 minutes
```

#### SleepManager (`lib/SleepManager/`)
- Manages ESP32 deep sleep operations
- Tracks wake reason (power-on, timer, external)
- Stores persistent data in RTC memory:
  - Boot counter
  - Last NTP sync timestamp
  - Failed capture counter
- Automatic WiFi disconnect before sleep
- Detailed sleep/wake logging

**RTC Memory Persistence:**
- Survives deep sleep (NOT power cycle)
- Used to minimize NTP syncs (only every 24h)
- Tracks consecutive failures

#### WebConfigServer (`lib/WebConfigServer/`)
- Async web server (ESPAsyncWebServer)
- Activity-based timeout with auto-reset
- REST API endpoints:
  - `GET /` - Main configuration UI (HTML)
  - `GET /config` - Current config as JSON
  - `POST /config` - Save new configuration
  - `GET /status` - Device status (IP, MAC, heap, timeout)
  - `GET /preview` - Capture and return JPEG image
  - `POST /reset` - Factory reset and reboot
- Responsive HTML/CSS/JavaScript interface
- Real-time countdown timer display
- Image preview functionality

### Power Consumption

**Estimated Current Draw:**

| Mode | Current | Notes |
|------|---------|-------|
| **Deep Sleep** | 10-150 µA | Lowest power, only RTC active |
| **Config Mode** (WiFi on) | 100-160 mA | Web server + WiFi STA |
| **Capture Mode** (active) | 150-250 mA | Camera + WiFi + processing |

**Example Power Savings:**

Previous (always-on): ~120 mA × 24h = 2880 mAh/day

New (4 captures/day):
- Sleep: 0.1 mA × 23h 40m = 2.37 mAh
- Capture: 200 mA × 4 × 2min = 26.67 mAh
- **Total: ~29 mAh/day** (99% reduction!)

With 2000 mAh battery:
- Previous: ~16 hours
- **New: ~68 days!**

## Configuration

### Initial Setup

1. **First Boot**: ESP enters CONFIG mode for 15 minutes
2. **Connect**: Join ESP's WiFi network or connect to same network
3. **Browse**: Navigate to `http://<ESP_IP>/`
4. **Configure**: 
   - WiFi credentials
   - Server URL (e.g., `https://myserver.com/upload.php`)
   - Authentication token
   - Capture schedule times
   - Timezone settings
5. **Save**: Click "Save Configuration"
6. **Wait**: Countdown expires → ESP enters sleep mode

### Runtime Configuration Changes

**Option 1: Power Cycle**
- Disconnect and reconnect power
- ESP boots into CONFIG mode
- Make changes via web UI

**Option 2: Serial Console** (for debugging)
- Monitor serial output at 115200 baud
- Device will self-diagnose issues

### Web Interface Features

#### Status Dashboard
- IP address and MAC address
- WiFi signal strength (RSSI)
- Free heap memory
- Remaining config timeout (live countdown)

#### WiFi Configuration
- SSID input field
- Password field (masked)
- Auto-connects on save

#### Server Configuration
- Full HTTPS URL input
- Auth token (masked in display)

#### Schedule Management
- Dynamic list of capture times
- Add/remove times with buttons
- Hour (0-23) and minute (0-59) inputs
- Supports up to 24 captures per day

#### Timezone Settings
- GMT offset in seconds (e.g., 3600 for GMT+1)
- Daylight saving offset (typically 3600 or 0)

#### Power Management
- Web timeout: 1-240 minutes (how long server stays active)
- Sleep margin: 0-600 seconds (wake up N seconds early)

#### Manual Operations
- **Capture & Preview**: Immediately capture image and display in browser
- **Reload**: Refresh configuration from NVS
- **Factory Reset**: Erase all settings and reboot

## Configuration Files

### config.h
Default compile-time values:
```cpp
DEFAULT_WEB_TIMEOUT_MIN = 15      // Web server timeout
MAX_WEB_TIMEOUT_MIN = 240         // Maximum allowed timeout
DEFAULT_SLEEP_MARGIN_SEC = 60     // Wake N seconds early
MIN_SLEEP_THRESHOLD_SEC = 300     // Don't sleep if <5 min to capture
```

### wifi_credentials.h (optional)
Factory default WiFi credentials:
```cpp
#define WIFI_SSID "YourSSID"
#define WIFI_PASSWORD "YourPassword"
```

If file doesn't exist, CONFIG mode starts with empty credentials.

### auth_token.h (optional)
Factory default server settings:
```cpp
#define SERVER_URL "https://example.com/upload.php"
#define AUTH_TOKEN "your-secret-token"
```

If file doesn't exist, CONFIG mode starts with empty values.

**Note**: All settings stored in NVS override compiled defaults.

## LED Indicators

| Blink Pattern | Meaning |
|--------------|---------|
| 3 fast blinks | Startup/boot |
| 2 slow blinks | Successful capture & upload |
| 5 fast blinks | Capture/upload error |
| 10 fast blinks | Critical error (config failure) |

*Note: Seeeduino XIAO ESP32S3 may not have built-in LED.*

## Schedule Examples

### Example 1: Four Times Daily
```
08:00 - Morning
11:00 - Late morning
15:00 - Afternoon
17:00 - Evening
```

### Example 2: Hourly (9am-5pm)
```
09:00, 10:00, 11:00, 12:00, 
13:00, 14:00, 15:00, 16:00, 17:00
```

### Example 3: On the Hour
```
00:00, 03:00, 06:00, 09:00,
12:00, 15:00, 18:00, 21:00
```

**Important**: Times must be sorted and unique (enforced by validation).

## Error Handling & Recovery

### Consecutive Capture Failures
- **Threshold**: 3 consecutive failed captures
- **Action**: Stay awake in CONFIG mode instead of sleeping
- **Recovery**: User can diagnose via web UI (check WiFi, server, camera)
- **Reset**: Successfully capture → counter resets to 0

### WiFi Connection Failure
- **In CONFIG mode**: Logs error, user can fix credentials
- **In CAPTURE mode**: Counts as failure, increments counter
- **After 3 failures**: Stays in CONFIG mode

### Camera Initialization Failure
- **In CONFIG mode**: Logs error, camera marked as not ready
- **In CAPTURE mode**: Counts as failure, may enter CONFIG mode

### NTP Sync Failure
- **On first boot**: Retries 20 times, then continues with potentially wrong time
- **After deep sleep**: Uses RTC clock (drifts ~5% per hour)
- **Recovery**: Re-syncs automatically if >24h since last successful sync

### Configuration Corruption
- **Detection**: Magic number validation in NVS
- **Action**: Load factory defaults from compiled code
- **User action**: Reconfigure via web UI

### Unexpected Wake
- **Unknown wake reason**: Enter CONFIG mode to be safe
- **User action**: Check serial logs, reconfigure if needed

## Development & Testing

### Serial Monitor Output

Connect USB cable and monitor at **115200 baud**:

```
=== EspCamPicPusher ===
Starting...

=== Wake Reason: Power-On/Reset ===
=== Entering CONFIGURATION MODE ===

--- WiFi Setup ---
Connecting to: MyNetwork
..........
WiFi connected!
IP address: 192.168.1.100
Signal strength: -45 dBm

--- Camera Setup ---
Camera initialized successfully

--- Time Setup ---
NTP Server: pool.ntp.org
Waiting for NTP time sync.....
Time synchronized!
2026-03-01 14:23:45

=== EspCamPicPusher Ready - Config Mode ===
Configuration URL: http://192.168.1.100/
Web timeout: 15 minutes
===========================================
```

### Testing Checklist

- [ ] Fresh boot enters CONFIG mode
- [ ] Web UI accessible at `http://<IP>/`
- [ ] Configuration saves to NVS
- [ ] Settings persist after reboot
- [ ] Timeout countdown resets on activity
- [ ] Image preview works
- [ ] After timeout, enters sleep (if >5 min to capture)
- [ ] After timeout, stays awake (if <5 min to capture)
- [ ] Wakes at correct time (±30 sec)
- [ ] Captures and uploads successfully
- [ ] Re-enters sleep after capture
- [ ] Factory reset works
- [ ] After 3 failures, stays in CONFIG mode
- [ ] NTP sync works (check serial output)

### Debugging Tips

**ESP won't connect to WiFi:**
- Check SSID/password in web UI or serial logs
- Verify 2.4 GHz network (ESP32 doesn't support 5 GHz)
- Check signal strength (RSSI in status page)

**Web server not accessible:**
- Check IP address in serial output
- Ensure device and computer on same network
- Try `ping <ESP_IP>` from terminal
- Check firewall settings

**Camera errors:**
- Verify XIAO ESP32S3 **Sense** variant (has camera)
- Check GPIO pins in [config.h](include/config.h)
- Look for `Camera init failed` in serial logs

**Time/schedule issues:**
- Verify NTP sync succeeded (`Time synchronized!` in logs)
- Check timezone offsets (GMT + daylight)
- Ensure schedule times are valid (0-23 hours, 0-59 minutes)

**Deep sleep not working:**
- Check that next capture is >5 min away
- Verify serial output shows `Entering Deep Sleep for X seconds`
- USB connection may keep ESP awake (disconnect USB)

**High power consumption:**
- Disconnect USB cable (it keeps some circuits active)
- Measure after ESP enters sleep (takes ~10 seconds)
- Check for CONFIG mode (LED should be off in sleep)

## API Reference

### HTTP Endpoints

#### GET `/`
Returns: HTML configuration interface

#### GET `/config`
Returns: JSON object with current configuration
```json
{
  "wifiSsid": "MyNetwork",
  "wifiPassword": "********",
  "serverUrl": "https://example.com/upload.php",
  "authToken": "********",
  "gmtOffsetSec": 3600,
  "daylightOffsetSec": 3600,
  "schedule": [
    {"hour": 8, "minute": 0},
    {"hour": 11, "minute": 0},
    {"hour": 15, "minute": 0},
    {"hour": 17, "minute": 0}
  ],
  "webTimeoutMin": 15,
  "sleepMarginSec": 60
}
```

#### POST `/config`
Body: JSON configuration object
Returns: `{"success": true/false, "message": "..."}`

#### GET `/status`
Returns: Device status
```json
{
  "macAddress": "AA:BB:CC:DD:EE:FF",
  "ipAddress": "192.168.1.100",
  "rssi": -45,
  "uptime": 12345,
  "remainingTimeout": 789,
  "freeHeap": 234567,
  "cameraReady": true
}
```

#### GET `/preview`
Returns: JPEG image (binary)
Content-Type: `image/jpeg`

#### POST `/reset`
Action: Factory reset and reboot
Returns: `{"success": true, "message": "Resetting..."}`

## Upgrade Path

### From Previous Version

The new code is **not directly compatible** with the previous always-on version due to fundamental architecture changes. To upgrade:

1. **Flash new firmware** via PlatformIO
2. **First boot**: ESP enters CONFIG mode
3. **Reconfigure** all settings via web UI:
   - WiFi credentials (previously in `wifi_credentials.h`)
   - Server URL and token (previously in `auth_token.h`)
   - Schedule (previously in `config.h` as `CAPTURE_TIMES[]`)
4. **Save configuration** → stored in NVS
5. **Wait for timeout** → ESP enters sleep mode

**Migration Notes:**
- Old compile-time configs are used as defaults only
- All runtime settings stored in NVS (survives firmware updates)
- To preserve settings across firmware updates: don't erase flash

### Reverting to Previous Version

If you need to go back to always-on mode:

1. Checkout previous Git commit
2. Flash previous firmware
3. Configuration hardcoded in header files again
4. No deep sleep, always-on operation restored

## Troubleshooting

### Problem: ESP keeps restarting
**Cause**: Configuration validation failure or hardware issue
**Solution**: 
- Check serial logs for error messages
- Try factory reset via web UI
- Reflash firmware if corrupted

### Problem: Images not uploading
**Cause**: Server URL, auth token, or network issue
**Solution**:
- Check server URL in web UI (must include `https://`)
- Verify auth token matches server expectation
- Test upload manually using preview button
- Check server logs

### Problem: Wrong capture times
**Cause**: Timezone misconfiguration or NTP failure
**Solution**:
- Verify GMT offset (seconds east of UTC)
- Check daylight saving offset
- Look for `Time synchronized!` in serial logs
- If NTP fails, ESP may use RTC (drifts ~5%)

### Problem: Battery drains quickly
**Cause**: Not entering sleep, or USB connected
**Solution**:
- Disconnect USB cable (keeps circuits active)
- Check serial logs for `Entering Deep Sleep`
- Verify timeout expires (wait 15+ minutes)
- Ensure next capture is >5 minutes away

### Problem: Can't access web UI
**Cause**: Wrong IP, network issue, or timeout expired
**Solution**:
- Get IP from serial logs
- Ensure computer on same network
- Power cycle ESP to re-enter CONFIG mode
- Check DHCP lease in router

## Future Enhancements

Possible improvements for future versions:

- [ ] GPIO button to force CONFIG mode
- [ ] mDNS support (access via `http://espcam.local/`)
- [ ] OTA firmware updates via web UI
- [ ] Image quality adjustment in web UI
- [ ] WiFi AP mode if STA connection fails
- [ ] Battery voltage monitoring
- [ ] SD card storage for offline operation
- [ ] Multiple server upload support
- [ ] Camera settings (brightness, contrast) via web UI
- [ ] External RTC for accurate timekeeping
- [ ] Certificate pinning for HTTPS

## Credits

**Original Author**: Stefan Fuchs
**Implementation Date**: March 2026
**Board**: Seeeduino XIAO ESP32S3 Sense
**Framework**: Arduino ESP32 + PlatformIO

**Libraries Used**:
- `esp32-camera` v2.0.4 - Camera driver
- `ESPAsyncWebServer` v1.2.3 - Async web server
- `AsyncTCP` v1.1.1 - TCP async support
- `ArduinoJson` v6.21.3 - JSON parsing
- Built-in: `WiFi`, `Preferences`, `esp_sleep.h`

## License

See [LICENSE.md](../../LICENSE.md) in project root.
