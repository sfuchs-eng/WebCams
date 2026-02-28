# WebCam Pics - PHP Web Application

A PHP-based web application for receiving, processing, and displaying images from ESP32-CAM or Raspberry Pi cameras sent via HTTP POST in push manner.

A simple solution independent from camera manufacturers' cloud services, running on basic web hosting with PHP support. No database required - uses JSON config and filesystem for storage.

The push approach allows for sporadic image uploads out of private IP networks without needing port forwarding or dynamic DNS.

## Features

- **Image Reception**: Receives JPEG images via HTTP POST with authentication
- **Image Processing**: 
  - Rotate images (0°, 90°, 180°, 270°)
  - Add camera title overlay
  - Add timestamp overlay
  - Configurable text styling
- **Multi-Camera Support**: Handles multiple cameras identified by MAC address
- **Location-Based Organization**: Group cameras by location
- **Responsive Web Interface**: Works on mobile and desktop
- **Image History**: View images from the last 14 days
- **File-Based Storage**: No database required - uses JSON config and filesystem
- **Flexible Installation**: Works in any directory - automatically detects its path (root, subdirectory, etc.)
- **Legacy API Support**: Compatible with legacy multipart/form-data POST interface

## Installation

1. **Requirements**:
   - PHP 7.4 or higher
   - GD extension for image processing
   - Apache or Nginx web server

2. **Setup**:
   ```bash
   # Copy files to any web server directory - path is auto-detected
   # Example 1: Subdirectory
   cp -r WebCamPics /var/www/html/webcams
   
   # Example 2: Different subdirectory
   # cp -r WebCamPics /var/www/html/cams
   
   # Example 3: Root directory
   # cp -r WebCamPics/* /var/www/html/
   
   # Run setup script to configure permissions and paths
   cd /var/www/html/webcams
   ./setup.sh
   ```
   
   **Note**: The application automatically detects its installation path. The setup script
   will configure the `.htaccess` file with the correct absolute path for HTTP Basic Auth.

3. **Configuration**:
   
   Copy `config/config_template.json` to `config/config.json` and update settings:
   ```json
   {
     "auth_tokens": [
       "your_secret_token_here",
       "additional_token_if_needed"
     ],
     "timezone": "Europe/Zurich",
     "image_retention_days": 14,
     "upload_max_size_mb": 10
   }
   ```
   
   **Important**: 
   - Change the tokens in `auth_tokens` array to secure random strings!
   - All tokens work for both the current API (Bearer authentication) and legacy API (form-data)
   - You can have multiple tokens for different cameras or for token rotation

4. **ESP32-CAM Configuration**:
   
   The upload URL is automatically shown in the admin panel.
   
   Update your ESP32-CAM's `auth_token.h`:
   ```cpp
   // Get the correct URL from the admin panel, e.g.:
   #define SERVER_URL "https://your-domain.com/webcams/upload.php"
   // or if installed at root:
   #define SERVER_URL "https://your-domain.com/upload.php"
   // or if installed elsewhere:
   #define SERVER_URL "https://your-domain.com/cams/upload.php"
   
   #define AUTH_TOKEN "your_secret_token_here"
   ```

## HTTP Basic Authentication

The admin panel (`admin.php`) is protected with HTTP Basic Authentication to prevent unauthorized access to camera configuration and sensitive tokens.

### Managing Users

Users are stored in `config/.htpasswd` using Apache's htpasswd format.

**Create the first user:**
```bash
cd /path/to/WebCamPics
htpasswd -c config/.htpasswd username
# You'll be prompted to enter a password
```

**Add additional users:**
```bash
htpasswd config/.htpasswd newuser
# Note: No -c flag when adding to existing file
```

**Change a user's password:**
```bash
htpasswd config/.htpasswd username
# Enter new password when prompted
```

**Remove a user:**
```bash
htpasswd -D config/.htpasswd username
```

**List all users:**
```bash
cat config/.htpasswd | cut -d ':' -f 1
```

**Security Notes:**
- All authenticated users have full admin access (no role separation)
- The `.htpasswd` file is protected by existing `.htaccess` rules
- Use strong passwords - htpasswd uses bcrypt hashing by default
- The `config/.htpasswd` file should have `600` permissions (set by setup.sh)
- After creating users, test admin access: `http://your-domain.com/webcams/admin.php`
- ESP32 camera uploads to `upload.php` are NOT affected by Basic Auth (they use Bearer token)

### Troubleshooting Basic Auth

If the admin panel doesn't prompt for authentication:

1. **Check .htaccess absolute path**: Open `.htaccess` and verify the `AuthUserFile` path is correct
   ```bash
   # Verify the path exists
   ls -l /full/path/to/config/.htpasswd
   ```

2. **Verify Apache allows .htaccess overrides**: 
   ```apache
   # In your Apache config (e.g., /etc/apache2/sites-enabled/000-default.conf)
   <Directory /var/www/html>
       AllowOverride All
   </Directory>
   ```

3. **Check Apache auth modules are enabled**:
   ```bash
   sudo a2enmod auth_basic
   sudo a2enmod authn_file
   sudo systemctl restart apache2
   ```

## Usage

### Viewing Images

- **Main View**: `http://your-domain.com/webcams/` 
  Shows the latest image from each camera, grouped by location

- **Location History**: Click on any location to view historical images
  Filter by time range (1, 3, 7, or 14 days)

### Camera Administration

- Access: `http://your-domain.com/webcams/admin.php`
- **Authentication Required**: Admin panel is protected with HTTP Basic Authentication
- Configure each camera:
  - Set camera title and location
  - Enable/disable camera
  - Set rotation angle
  - Configure text overlays
  - Customize font size and color

### Adding New Locations

Edit `config/config.json` to add new locations:
```json
{
  "locations": {
    "living_room": {
      "title": "Living Room",
      "description": "Indoor cameras"
    }
  }
}
```

## API Endpoint

### Current Interface (Recommended)

#### POST /upload.php

Upload an image from an ESP32-CAM device or any other web cam supporting the current interface.

**Headers**:
- `Authorization: Bearer {AUTH_TOKEN}` (required)
- `Content-Type: image/jpeg` (required)
- `X-Device-ID: {MAC_ADDRESS}` (required)
- `X-Timestamp: {YYYY-MM-DD HH:MM:SS}` (optional)

**Body**: Raw JPEG image data

**Response**:
```json
{
  "success": true,
  "device_id": "AA:BB:CC:DD:EE:FF",
  "timestamp": "2024-02-24 10:30:00",
  "size": 123456,
  "filename": "2024-02-24_10-30-00.jpg"
}
```

### Legacy Interface (Backward Compatibility)

For existing cameras using the legacy POST interface with multipart/form-data.

#### POST /upload.php (Legacy)

**Content-Type**: `multipart/form-data`

**Parameters**:
- `auth` (string, required): Authentication token from `auth_tokens` array in config
- `cam` (string, required): Camera identifier (e.g., "front_door", "garden_cam")
- `pic` (file, required): JPEG image file
- `picname` (string, optional): Ignored - filename is auto-generated from timestamp

**Example using curl**:
```bash
curl -F "auth=token1" -F "cam=test_cam" -F "pic=@image.jpg" \
  http://your-domain.com/webcams/upload.php
```

**Response**: Same JSON format as current interface

**Notes**:
- Legacy cameras are identified by the `cam` parameter value (sanitized for filesystem use)
- Camera identifiers are sanitized: only alphanumeric, hyphens, and underscores allowed
- Minimum identifier length: 3 characters (shorter IDs are padded with a hash)
- Maximum identifier length: 64 characters (longer IDs are truncated)
- Images are stored in `images/{cam_name}/` directory
- Full image processing pipeline is applied (rotation, overlays, etc.)
- New camera entries are auto-created in `cameras.json` with status "hidden"
- Run [test_legacy_upload.sh](test_legacy_upload.sh) to verify legacy interface

### Testing

**Test current interface**:
```bash
./test_upload.sh test_image.jpg
```

**Test legacy interface**:
```bash
./test_legacy_upload.sh test_image.jpg my_camera token1
```

## File Structure

```
WebCamPics/
├── index.php           # Main viewer
├── location.php        # Location/camera history view
├── admin.php           # Camera administration
├── upload.php          # Image upload endpoint
├── config/
│   ├── config.json     # Main configuration
│   └── cameras.json    # Per-camera settings
├── lib/
│   ├── auth.php        # Authentication functions
│   ├── storage.php     # File storage functions
│   └── image.php       # Image processing functions
├── assets/
│   └── style.css       # Responsive styles
├── images/             # Stored images (auto-created)
│   └── {MAC}/          # Per-camera folders
└── logs/               # Upload logs (auto-created)
```

## Maintenance

### Cleanup Old Images

Images older than the configured retention period are automatically cleaned up.
You can manually trigger cleanup by calling:

```php
require_once 'lib/storage.php';
$deleted = cleanupOldImages(14); // 14 days
```

Consider setting up a cron job:
```bash
0 3 * * * /usr/bin/php /var/www/html/webcams/cleanup.php
```

### View Logs

Upload logs are stored in `logs/upload.log`:
```bash
tail -f /var/www/html/webcams/logs/upload.log
```

## Security Notes

1. **Change the default auth token** in `config/config.json`
2. Use HTTPS for production deployments
3. Consider adding HTTP Basic Auth to the admin interface
4. Regularly update PHP to the latest version
5. Set appropriate file permissions (755 for directories, 644 for files)

## Troubleshooting

### Images not uploading
- Check auth token matches between ESP32 and server
- Verify upload.php is accessible
- Check PHP error log: `tail -f /var/log/apache2/error.log`
- Ensure images directory is writable

### Images not displaying
- Verify GD extension is installed: `php -m | grep gd`
- Check file permissions on images directory
- Look for errors in browser console

### Text overlay not working
- Ensure GD extension is installed
- Check font settings in camera configuration
- Verify PHP has write permissions to process images

## Support

For issues related to the ESP32-CAM firmware, see the EspCamPicPusher project.
