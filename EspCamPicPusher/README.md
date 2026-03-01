# EspCamPicPusher

This project is a Seeeduino XIAO ESP32S3 Sense application that captures images from an attached camera and pushes them with HTTP POST to a specified server endpoint. Features ultra-low-power operation using deep sleep between captures, with a web-based configuration interface on startup.

## Features

- **Web Configuration Mode**: On power-up, provides a 15-minute web interface for runtime configuration
  - Configure WiFi credentials, server URL, authentication token
  - Modify capture schedule times
  - Adjust timezone settings and power management
  - Manual image capture with live preview
  - Activity-based timeout (resets on any HTTP request)
- **Deep Sleep Power Management**: Ultra-low power consumption (~10-150 µA) between scheduled captures
  - Automatically wakes ~60 seconds before scheduled capture time
  - Smart waiting mode when captures are close together (<5 minutes)
  - ~99% power reduction compared to always-on operation
- **NVS Configuration Storage**: All settings stored persistently in ESP32 non-volatile memory
- **WiFi Infrastructure Mode**: Connects to your WiFi network using configurable credentials
- **NTP Time Synchronization**: Automatically updates time from NTP servers (minimized during sleep)
- **Scheduled Image Capture**: Captures images at configured times throughout the day (up to 24 times)
- **HTTPS Upload**: Posts captured images to a server endpoint with token-based authentication
- **OV2640 Camera Support**: Optimized for the XIAO ESP32S3 Sense built-in camera
- **Error Recovery**: Automatically enters configuration mode after 3 consecutive capture failures

## Hardware Requirements

- Seeeduino XIAO ESP32S3 Sense
- OV2640 Camera Module (included with Sense variant)
- USB-C cable for programming and power

## Software Setup

### Quick Start (Recommended)

1. **Flash the firmware** to your XIAO ESP32S3 Sense
2. **Power on** - Device enters CONFIG mode for 15 minutes
3. **Connect** to the same WiFi network or note device IP from serial monitor
4. **Browse** to `http://<DEVICE_IP>/` and configure via web UI:
   - WiFi credentials
   - Server URL and authentication token
   - Capture schedule times
   - Timezone settings
5. **Save** configuration - Device enters deep sleep mode and wakes for scheduled captures

### Advanced: Pre-Configure Defaults (Optional)

You can optionally set factory defaults by editing header files before compilation:

#### 1. WiFi Credentials (Optional)

Edit `include/wifi_credentials.h` with default WiFi information:

```cpp
const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";
```

**Note**: This file is not tracked by git. These values serve as defaults but can be overridden via web UI.

#### 2. Server Authentication (Optional)

Edit `include/auth_token.h` with default server settings:

```cpp
#define SERVER_URL "https://your-server.com/api/upload"
#define AUTH_TOKEN "your_secret_token_here"
```

**Note**: These defaults can be changed at runtime via the web configuration interface.

#### 3. Application Settings

Edit `include/config.h` to customize default values:

- **Power Management**:
  ```cpp
  const int DEFAULT_WEB_TIMEOUT_MIN = 15;      // Web server timeout (1-240 min)
  const int DEFAULT_SLEEP_MARGIN_SEC = 60;     // Wake N seconds before capture
  const int MIN_SLEEP_THRESHOLD_SEC = 300;     // Don't sleep if capture <5 min away
  ```

- **Timezone** (can be changed via web UI):
  ```cpp
  const long GMT_OFFSET_SEC = 3600;            // Adjust for your timezone
  const int DAYLIGHT_OFFSET_SEC = 3600;        // Adjust for daylight saving
  ```

- **Capture Schedule** (can be modified via web UI):
  ```cpp
  const CaptureTime CAPTURE_TIMES[] = {
      {8, 0},    // 08:00 AM
      {11, 0},   // 11:00 AM
      {15, 0},   // 03:00 PM
      {17, 0}    // 05:00 PM
  };
  ```

- **Camera Settings**:
  ```cpp
  #define CAMERA_FRAME_SIZE FRAMESIZE_UXGA  // Resolution: 1600x1200
  #define CAMERA_JPEG_QUALITY 10            // Quality: 0-63 (lower = better)
  ```

**Note**: Once configured via web UI, settings are stored in NVS and override these defaults.

### 4. Build and Upload

Using PlatformIO:

```bash
# Build the project
pio run

# Upload to device
pio run --target upload

# Monitor serial output
pio device monitor
```

Or use the PlatformIO buttons in VS Code.

## Server Endpoint

The application sends HTTPS POST requests with the following headers:

- `Content-Type: image/jpeg`
- `Authorization: Bearer <AUTH_TOKEN>`
- `X-Device-ID: <MAC_ADDRESS>`
- `X-Timestamp: <YYYY-MM-DD HH:MM:SS>`

The request body contains the JPEG image data.

### Example Server Implementation (Node.js/Express)

```javascript
app.post('/api/upload', (req, res) => {
    const token = req.headers.authorization?.replace('Bearer ', '');
    
    if (token !== 'your_secret_token_here') {
        return res.status(401).json({ error: 'Unauthorized' });
    }
    
    const deviceId = req.headers['x-device-id'];
    const timestamp = req.headers['x-timestamp'];
    
    // Save image
    const filename = `${deviceId}_${Date.now()}.jpg`;
    fs.writeFileSync(`uploads/${filename}`, req.body);
    
    res.json({ success: true, filename });
});
```

## Operation

### Operating Modes

The device operates in three distinct modes:

#### 1. CONFIG Mode (Web Server Active)
- **Triggers**: Fresh boot / power cycle
- **Duration**: 15 minutes (configurable, resets on HTTP activity)
- **Access**: Browse to `http://<DEVICE_IP>/`
- **Features**:
  - Full configuration interface
  - Manual image capture with preview
  - Real-time device status
  - Factory reset option
- **Exit**: After timeout, transitions to CAPTURE or WAIT mode depending on next scheduled capture time

#### 2. CAPTURE Mode (Quick Capture)
- **Triggers**: Timer wake from deep sleep
- **Actions**:
  1. Connect to WiFi
  2. Sync NTP (if >24 hours since last sync)
  3. Initialize camera
  4. Capture and upload image
  5. Calculate next wake time
  6. Enter deep sleep
- **LED**: 2 slow blinks on success, 5 fast blinks on error
- **Recovery**: After 3 consecutive failures, stays awake in CONFIG mode

#### 3. WAIT Mode (Active Waiting)
- **Triggers**: When next capture is <5 minutes away
- **Purpose**: Avoid sleep/wake cycling for imminent captures
- **Actions**: Poll time every 10 seconds, execute capture when scheduled
- **Exit**: After capture, checks if should sleep or continue waiting

### Normal Operation Flow

1. **Power On**: Device enters CONFIG mode, starts web server
2. **Configuration**: User accesses web UI at `http://<DEVICE_IP>/` and configures settings
3. **Timeout**: After 15 minutes of inactivity, web server stops
4. **Deep Sleep**: Device calculates time until next capture (minus 60-second margin) and sleeps
5. **Wake & Capture**: Device wakes, captures image, uploads to server
6. **Repeat**: Returns to deep sleep, cycle continues

### Power Consumption

- **Deep Sleep**: 10-150 µA (99%+ reduction)
- **CONFIG Mode**: ~120 mA (WiFi active)
- **CAPTURE Mode**: ~200 mA peak (camera + WiFi)
- **Battery Life Example**: ~68 days on 2000 mAh battery with 4 captures/day (vs. 16 hours always-on)

## Web Configuration API

The web server provides these endpoints:

- `GET /` - Main configuration interface (HTML)
- `GET /config` - Current configuration as JSON
- `POST /config` - Save new configuration (JSON body)
- `GET /status` - Device status (IP, MAC, heap, timeout)
- `GET /preview` - Capture and return JPEG image
- `POST /reset` - Factory reset and reboot

## Troubleshooting

- **Can't Access Web UI**: Power cycle device to re-enter CONFIG mode, check IP in serial output
- **WiFi Connection Issues**: Update credentials via web UI or check `wifi_credentials.h`
- **Camera Init Failed**: Verify XIAO ESP32S3 **Sense** variant (has camera), check PSRAM enabled
- **Upload Failures**: Verify server URL, network connectivity, and authentication token via web UI
- **Wrong Capture Times**: Check timezone settings in web UI, verify NTP sync in serial logs
- **High Power Consumption**: Ensure device enters deep sleep (check serial logs), disconnect USB cable
- **Device Won't Sleep**: Wait for full 15-minute timeout, or check if next capture is <5 minutes away
- **Repeated Failures**: Device will stay in CONFIG mode after 3 consecutive failed captures for troubleshooting
- **Certificate Errors**: The code uses `setInsecure()` for testing; implement proper certificate validation for production

## Serial Monitor Output

The device provides detailed logging at 115200 baud over USB serial, including:
- Wake reason (power-on, timer, etc.)
- Operating mode (CONFIG, CAPTURE, WAIT)
- WiFi connection status and IP address
- Camera initialization status
- NTP time synchronization
- Web server URL and timeout countdown
- Scheduled capture events
- Upload success/failure with HTTP response codes
- Deep sleep duration and next wake time
- RTC data (boot count, failed captures, last NTP sync)

## Additional Documentation

- **[IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md)** - Comprehensive technical documentation, architecture details, and troubleshooting
- **[IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md)** - Quick reference of implementation details

## Notes, References, and Resources

- [Seeeduino XIAO ESP32S3 Sense](https://wiki.seeedstudio.com/XIAO_ESP32S3_Sense/)
- [DroneBot Workshop: XIAO ESP32S3 Sense](https://dronebotworkshop.com/xiao-esp32s3-sense/)

### XIAO Sense default firmware

Starting from June 2025, the factory firmware of XIAO ESP32S3 Sense enables a default AP Wi‑Fi with the following credentials:

SSID: XIAO_ESP32S3_Sense
Password: seeedstudio
