# OTA Build and Upload Script

Automated build and upload tool for EspCamPicPusher firmware OTA updates.

## Setup

1. **Copy the credentials template:**
   ```bash
   cp .ota-credentials.template .ota-credentials
   ```

2. **Edit `.ota-credentials` with your server details:**
   ```bash
   # OTA Server Credentials
   OTA_SERVER_URL=https://your-server.com/cams
   OTA_USERNAME=admin
   OTA_PASSWORD=your_admin_password
   OTA_DESCRIPTION="Automated build"
   ```

   The `.ota-credentials` file is gitignored for security.

## Usage

### Basic Usage

Build and upload firmware:
```bash
./build-and-upload-ota.sh
```

### Options

**Set custom description:**
```bash
./build-and-upload-ota.sh -d "Fixed camera timeout bug"
```

**Skip build (upload existing firmware):**
```bash
./build-and-upload-ota.sh -s
```

**Help:**
```bash
./build-and-upload-ota.sh -h
```

## What It Does

1. **Reads credentials** from `.ota-credentials`
2. **Extracts firmware version** from `platformio.ini`
3. **Compiles firmware** using PlatformIO (unless `-s` flag)
4. **Uploads to server** via HTTP Basic Auth
5. **Reports success/failure** with detailed output

## Workflow

```
┌─────────────────┐
│ Edit Code       │
└────────┬────────┘
         │
┌────────▼────────┐
│ Run Script      │  ./build-and-upload-ota.sh -d "Description"
└────────┬────────┘
         │
┌────────▼────────┐
│ Build Firmware  │  PlatformIO compiles .bin
└────────┬────────┘
         │
┌────────▼────────┐
│ Upload to       │  curl → ota-upload.php
│ Server          │
└────────┬────────┘
         │
┌────────▼────────┐
│ Go to Admin     │  Schedule OTA for cameras
│ Panel           │
└────────┬────────┘
         │
┌────────▼────────┐
│ Cameras Auto-   │  On next image upload
│ Update          │
└─────────────────┘
```

## File Locations

- **Firmware binary**: `.pio/build/seeed_xiao_esp32s3/firmware.bin`
- **Upload filename**: `firmware_v{VERSION}.bin` (e.g., `firmware_v1.1.0.bin`)
- **Server endpoint**: `{OTA_SERVER_URL}/ota-upload.php`

## Authentication

Uses HTTP Basic Authentication with credentials from `.ota-credentials`:
- Username and password sent via curl `-u` flag
- Matches `.htpasswd` configuration on server
- Same auth as admin panel access

## Error Handling

The script will exit with error if:
- `.ota-credentials` file not found → Copy from template
- Credentials incomplete → Check OTA_SERVER_URL, OTA_USERNAME, OTA_PASSWORD
- PlatformIO not found → Install PlatformIO
- Build fails → Check compilation errors
- Upload fails (HTTP != 200) → Check server URL, auth credentials, network

## Example Output

```
========================================
Building firmware v1.1.0
========================================
... PlatformIO build output ...
✓ Build successful

========================================
Uploading to OTA Server
========================================
Server:      https://example.com
Version:     v1.1.0
Size:        1.23 MB
Description: Fixed deep sleep bug

Uploading...
✓ Upload successful!

Server response:
{
  "success": true,
  "filename": "firmware_v1.1.0.bin",
  "size": 1289456,
  "sha256": "a94a8fe5ccb19ba61c4c0873...",
  "version": "1.1.0",
  "message": "Firmware uploaded successfully"
}

========================================
OTA firmware ready for deployment
========================================

Next steps:
1. Go to your server admin panel
2. Edit the target camera configuration
3. Schedule the firmware update
4. Camera will update on next image upload
```

## Troubleshooting

**"Credentials file not found"**
- Run: `cp .ota-credentials.template .ota-credentials`
- Edit the file with your server details

**"PlatformIO not found"**
- Install: `pip install platformio`
- Or use full path in script

**"HTTP 401 Unauthorized"**
- Check OTA_USERNAME and OTA_PASSWORD
- Verify `.htpasswd` file on server
- Test credentials in browser at `/ota-upload.php`

**"HTTP 400 Invalid file type"**
- Ensure firmware file has `.bin` extension
- Check FIRMWARE_VERSION in platformio.ini

**"Build failed"**
- Check compilation errors in output
- Verify all libraries installed
- Check `platformio.ini` configuration

## Security Notes

- **Never commit `.ota-credentials`** - It's gitignored
- Store passwords securely (consider using env vars or secret management)
- Use HTTPS for OTA_SERVER_URL in production
- Rotate passwords regularly
- Limit `.htpasswd` access to admin users only

## Integration with CI/CD

Can be integrated into automated workflows:

```yaml
# Example GitHub Actions
- name: Build and Upload OTA
  env:
    OTA_SERVER_URL: ${{ secrets.OTA_SERVER_URL }}
    OTA_USERNAME: ${{ secrets.OTA_USERNAME }}
    OTA_PASSWORD: ${{ secrets.OTA_PASSWORD }}
  run: |
    echo "OTA_SERVER_URL=$OTA_SERVER_URL" > .ota-credentials
    echo "OTA_USERNAME=$OTA_USERNAME" >> .ota-credentials
    echo "OTA_PASSWORD=$OTA_PASSWORD" >> .ota-credentials
    ./build-and-upload-ota.sh -d "CI Build ${{ github.sha }}"
```
