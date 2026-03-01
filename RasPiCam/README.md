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

Generally it is recommended to use NetworkManager for WiFi management on Raspberry Pi OS, as it provides better stability and easier configuration. However, if you prefer to use the built-in `wpa_supplicant` and manual WiFi configuration, you can run the following script to apply optimizations and set up the WiFi watchdog:

```bash
cd RasPiCam
./install-stability-tools.sh
```

This will:

- Make all scripts executable
- Apply WiFi stability optimizations
- Install WiFi watchdog service
- Configure power management settings

Wifi stability improvements when using NetworkManager:

- run `sudo nmcli connection modify <YourSSID> 802-11-wireless.band bg` to enforce 2.4GHz band. Better range than 5GHz and sufficient for camera uploads.
- run `./setup-wifi-stability.sh` to apply additional optimizations (disable power management, set regulatory domain, etc.).

### 2. Configure Credentials

**Option A: Using Environment Variables (Recommended)**

Copy and configure the environment template:

```bash
cp env.template env
nano env
```

Edit the variables in `env`:

```bash
CAM_NAME="CoolCam"                                    # Your camera ID
CAM_AUTH_TOKEN="your-secret-token-here"              # Server authentication token
CAM_POSTHANDLER="https://yourdomain.com/cams/upload.php"  # Server upload URL
```

Then run the script with environment variables loaded:

```bash
source env && ./capture-upload-image.sh
```

**Option B: Direct Script Editing**

Edit `capture-upload-image.sh` and modify the default values:

```bash
CAM="${CAM_NAME:-CoolCam}"                           # Your camera ID
AUTH_TOKEN="${CAM_AUTH_TOKEN:-your-secret-token-here}"   # Server authentication token
POSTHANDLER="${CAM_POSTHANDLER:-https://yourdomain.com/cams/upload.php}"  # Server upload URL
```

### 3. Set Up Cron Job

Add scheduled captures:

```bash
crontab -e
```

**Option A: Using environment variables (recommended):**

```cron
# Load environment and capture every hour from 07:00 to 17:00
0 7-17 * * * cd /home/pi/RasPiCam && source env && ./capture-upload-image.sh
```

**Option B: Using script defaults:**

```cron
# Capture every hour from 07:00 to 17:00
0 7-17 * * * /home/pi/RasPiCam/capture-upload-image.sh
```

Note: The script now includes built-in log rotation (daily rotation, keeps 30 days of logs in `/var/log/webcam/`).

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

**With environment variables:**
```bash
source env && ./capture-upload-image.sh
```

**With script defaults:**
```bash
./capture-upload-image.sh
```

### View Capture Logs
```bash
sudo tail -f /var/log/webcam/capture.log
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

**Script Configuration:**
```bash
MAX_RETRIES=3          # Number of upload attempts
RETRY_DELAY=5          # Seconds between retries
CAPTURE_TIMEOUT=10     # Camera capture timeout (seconds)
CONNECTION_TIMEOUT=10  # curl connection timeout (seconds)
MAX_TIME=30           # curl maximum operation time (seconds)
```

**Logging:**
```bash
LOG_DIR="/var/log/webcam"      # Log directory
LOG_FILE="${LOG_DIR}/capture.log"  # Current log file
```

The script automatically rotates logs daily and keeps 30 days of history. Rotated logs are named `capture.log.YYYY-MM-DD`.

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
2. Verify server URL and token in `env` file (or script defaults)
3. Test manually: `source env && ./capture-upload-image.sh` (if using environment variables)
4. Check logs: `sudo tail -f /var/log/webcam/capture.log`

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

- `capture-upload-image.sh` - Main capture and upload script with retry logic and built-in log rotation
- `env.template` - Environment variable template for configuration (copy to `/etc/environment` and customize)
- `wifi-watchdog.sh` - Connection monitoring daemon
- `setup-wifi-stability.sh` - WiFi optimization setup
- `install-stability-tools.sh` - Complete installation script
- `systemd/wifi-watchdog.service` - Systemd service for watchdog (in systemd/ subdirectory)
- `test-upload.sh` - Test script for upload functionality

## License

See [LICENSE.md](../LICENSE.md) in project root.
