# EspCamPicPusher

This project is a Seeeduino XIAO ESP32S3 Sense application that captures images from an attached camera and pushes them with HTTP POST to a specified server endpoint.

## Features

- **WiFi Infrastructure Mode**: Connects to your WiFi network using configurable credentials
- **NTP Time Synchronization**: Automatically updates time from NTP servers periodically
- **Scheduled Image Capture**: Captures images at configured times throughout the day
- **HTTPS Upload**: Posts captured images to a server endpoint with token-based authentication
- **OV2640 Camera Support**: Optimized for the XIAO ESP32S3 Sense built-in camera

## Hardware Requirements

- Seeeduino XIAO ESP32S3 Sense
- OV2640 Camera Module (included with Sense variant)
- USB-C cable for programming and power

## Software Setup

### 1. Configure WiFi Credentials

Edit `include/wifi_credentials.h` and update with your WiFi information:

```cpp
const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";
```

**Note**: This file is not tracked by git for security.

### Configure Server Authentication

Edit `include/auth_token.h` and set your server authentication token:

- **Server URL and Authentication Token**:
  ```cpp
  const char* SERVER_URL = "https://your-server.com/api/upload";
  const char* AUTH_TOKEN = "your_secret_token_here";
  ```

### 2. Configure Application Settings

Edit `include/config.h` to customize:

- **Timezone**:
  ```cpp
  const long GMT_OFFSET_SEC = 0;        // Adjust for your timezone
  const int DAYLIGHT_OFFSET_SEC = 0;    // Adjust for daylight saving
  ```

- **Capture Schedule** (times in 24-hour format):
  ```cpp
  const CaptureTime CAPTURE_TIMES[] = {
      {8, 0},    // 08:00 AM
      {12, 0},   // 12:00 PM
      {16, 0},   // 04:00 PM
      {20, 0}    // 08:00 PM
  };
  ```

- **Camera Settings**:
  ```cpp
  #define CAMERA_FRAME_SIZE FRAMESIZE_UXGA  // Resolution: 1600x1200
  #define CAMERA_JPEG_QUALITY 10            // Quality: 0-63 (lower = better)
  ```

### 3. Build and Upload

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

1. **Startup**: Device connects to WiFi, initializes camera, and syncs time with NTP
2. **Scheduled Capture**: At configured times, the device:
   - Captures a JPEG image
   - Uploads it to the configured server via HTTPS POST
   - Provides visual feedback via LED blinks (if available)
3. **Time Updates**: NTP sync occurs every hour to maintain accurate time

## Troubleshooting

- **WiFi Connection Issues**: Check credentials in `wifi_credentials.h`
- **Camera Init Failed**: Verify camera is properly connected and PSRAM is enabled
- **Upload Failures**: Check server URL, network connectivity, and authentication token
- **Certificate Errors**: The code uses `setInsecure()` for testing; implement proper certificate validation for production

## Serial Monitor Output

The device provides detailed logging at 115200 baud over USB serial, including:
- WiFi connection status
- Camera initialization
- Time synchronization
- Scheduled capture events
- Upload success/failure

## Notes, References, and Resources

- [Seeeduino XIAO ESP32S3 Sense](https://wiki.seeedstudio.com/XIAO_ESP32S3_Sense/)
- [DroneBot Workshop: XIAO ESP32S3 Sense](https://dronebotworkshop.com/xiao-esp32s3-sense/)

### XIAO Sense default firmware

Starting from June 2025, the factory firmware of XIAO ESP32S3 Sense enables a default AP Wi‑Fi with the following credentials:

SSID: XIAO_ESP32S3_Sense
Password: seeedstudio
