#!/bin/bash

# Test script to debug upload issues
AUTH_TOKEN='your-secret-token-here'
CAM="CoolCam"
POSTHANDLER='https://yourdomain.com/cams/upload.php'

echo "Testing upload with verbose curl output..."
echo "---"

# Create a small test image (1x1 pixel JPEG) (this pic didn't work in my tests while real images did, so maybe it's not a valid JPEG? But I got to testing auth and headers)
echo -e '\xff\xd8\xff\xe0\x00\x10JFIF\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00\xff\xdb\x00C\x00\x08\x06\x06\x07\x06\x05\x08\x07\x07\x07\t\t\x08\n\x0c\x14\r\x0c\x0b\x0b\x0c\x19\x12\x13\x0f\x14\x1d\x1a\x1f\x1e\x1d\x1a\x1c\x1c $.\x27 $\x1c\x1c(7),01444\x1f\x27=@37<=\xff\xc0\x00\x0b\x08\x00\x01\x00\x01\x01\x01\x11\x00\xff\xc4\x00\x14\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\t\xff\xc4\x00\x14\x10\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xda\x00\x08\x01\x01\x00\x00?\x00\x7f\x00\xff\xd9' > /tmp/test.jpg

# Show what we're sending
echo "X-Auth-Token: ${AUTH_TOKEN}"
echo "X-Device-ID: ${CAM}"
echo "X-Timestamp: $(date +"%Y-%m-%d %H:%M:%S")"
echo "URL: ${POSTHANDLER}"
echo "---"
echo ""

# Send test request with verbose output
# Using X-Auth-Token instead of Authorization header (Apache/PHP-FPM compatibility)
curl -v -X POST \
	-H "X-Auth-Token: ${AUTH_TOKEN}" \
	-H "Content-Type: image/jpeg" \
	-H "X-Device-ID: ${CAM}" \
	-H "X-Timestamp: $(date +"%Y-%m-%d %H:%M:%S")" \
	--data-binary @/tmp/test.jpg \
	"${POSTHANDLER}"

echo ""
echo "---"
echo "If you see '401 Unauthorized', check that:"
echo "1. AUTH_TOKEN in this script matches 'auth_token' in config/config.json"
echo "2. The config.json file exists at: /path/to/NewCams/config/config.json"
echo "3. The auth_token value in config.json is not the default placeholder"
