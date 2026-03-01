# RasPiCam - Raspberry Pi Camera Upload System

Upload images from Raspberry Pi cameras to the WebCamPics server with enhanced WiFi stability.

## Features

- Automatic image capture and upload via HTTPS POST
- WiFi stability improvements with retry logic
- Connection monitoring and automatic recovery
- Compatible with libcamera (Raspberry Pi Camera Module 3)
- Cron-based scheduling for regular captures

## Installation

### 1. Quick Setup

```bash
cd RasPiCam
./install-stability-tools.sh
```

This will:
- Make all scripts executable
- Apply WiFi stability optimizations
- Install WiFi watchdog service
- Configure power management settings

### 2. Configure Credentials

Edit `capture-upload-image.sh` with your settings:

```bash
CAM="CoolCam"                                    # Your camera ID
AUTH_TOKEN='your-secret-token-here'              # Server authentication token
POSTHANDLER='https://yourdomain.com/cams/upload.php'  # Server upload URL
```

### 3. Set Up Cron Job

Add scheduled captures:

```bash
crontab -e
```

Add line for captures every 5 minutes:
```cron
*/5 * * * * /home/pi/RasPiCam/capture-upload-image.sh >> /var/log/webcam-upload.log 2>&1
```

## WiFi Stability Features

### Retry Logic (`capture-upload-image.sh`)
- 3 automatic retry attempts on upload failure
- Network connectivity check before each attempt
- Configurable timeouts and retry delays
- Detailed logging with timestamps

### WiFi Watchdog (`wifi-watchdog.sh`)
- Continuous network connectivity monitoring
- Automatic WiFi interface restart on connection loss
- Logs all events to `/var/log/wifi-watchdog.log`
- Runs as systemd service for reliability

### Power Management Optimizations
- Disables WiFi power saving (reduces dropouts)
- Optional IPv6 disable (reduces connection issues)
- Persists across reboots via `/etc/rc.local`

## Manual Operations

### Test Upload
```bash
./capture-upload-image.sh
```

### Check WiFi Watchdog Status
```bash
systemctl status wifi-watchdog.service
sudo journalctl -u wifi-watchdog -f
```

### View Watchdog Logs
```bash
sudo tail -f /var/log/wifi-watchdog.log
```

### Apply WiFi Optimizations Only
```bash
./setup-wifi-stability.sh
```

## Configuration

### capture-upload-image.sh Settings
```bash
MAX_RETRIES=3          # Number of upload attempts
RETRY_DELAY=5          # Seconds between retries
CAPTURE_TIMEOUT=10     # Camera capture timeout (seconds)
CONNECTION_TIMEOUT=10  # curl connection timeout (seconds)
MAX_TIME=30           # curl maximum operation time (seconds)
```

### wifi-watchdog.sh Settings
```bash
PING_HOST="8.8.8.8"    # Host to ping for connectivity check
CHECK_INTERVAL=30      # Seconds between checks
FAIL_THRESHOLD=3       # Failed pings before restart
```

## Troubleshooting

### WiFi Keeps Dropping
1. Check power supply (use 5V 3A minimum)
2. Verify `iw dev wlan0 get power_save` shows "off"
3. Check watchdog logs: `sudo tail -f /var/log/wifi-watchdog.log`
4. Consider USB WiFi dongle if onboard WiFi is problematic

### Upload Failures
1. Check network: `ping 8.8.8.8`
2. Verify server URL and token in `capture-upload-image.sh`
3. Test manually: `./capture-upload-image.sh`
4. Check cron logs: `tail -f /var/log/webcam-upload.log`

### Camera Not Working
1. Enable camera: `sudo raspi-config` → Interface Options → Camera
2. Test camera: `libcamera-jpeg -o test.jpg`
3. Check permissions: User must be in `video` group

## Hardware Recommendations

- **Power Supply**: Quality 5V 3A adapter (insufficient power causes WiFi issues)
- **WiFi**: Use 2.4GHz band (better range than 5GHz)
- **Placement**: Keep Pi close to router or use WiFi repeater
- **Cooling**: Add heat sinks for Pi 4/5 (overheating can cause WiFi dropouts)

## Integration with WebCamPics Server

This component works with the WebCamPics PHP server:
- Sends JPEG images via HTTPS POST
- Uses token authentication (`X-Auth-Token` header)
- Includes device ID and timestamp metadata
- Server handles image storage and display

See main [WebCams README](../README.md) for complete system architecture.

## Files

- `capture-upload-image.sh` - Main capture and upload script with retry logic
- `wifi-watchdog.sh` - Connection monitoring daemon
- `setup-wifi-stability.sh` - WiFi optimization setup
- `install-stability-tools.sh` - Complete installation script
- `systemd/wifi-watchdog.service` - Systemd service for watchdog
- `test-upload.sh` - Test script for upload functionality

## License

See [LICENSE.md](../LICENSE.md) in project root.
