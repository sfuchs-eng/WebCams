#!/bin/bash
# Test Legacy Upload Script
# Tests the legacy POST interface with multipart/form-data

echo "==================================="
echo "  Legacy Upload Endpoint Test"
echo "==================================="
echo ""

# Configuration
SERVER_URL="http://localhost/webcams/upload.php"
LEGACY_TOKEN="token1"
CAMERA_ID="test_cam"

# Check if a test image is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <image.jpg> [camera_id] [auth_token]"
    echo ""
    echo "Example:"
    echo "  $0 test_image.jpg"
    echo "  $0 test_image.jpg my_camera token1"
    echo ""
    exit 1
fi

IMAGE_FILE="$1"

# Override defaults if provided
if [ ! -z "$2" ]; then
    CAMERA_ID="$2"
fi

if [ ! -z "$3" ]; then
    LEGACY_TOKEN="$3"
fi

if [ ! -f "$IMAGE_FILE" ]; then
    echo "ERROR: Image file '$IMAGE_FILE' not found!"
    exit 1
fi

echo "Test Configuration:"
echo "  Server URL:    $SERVER_URL"
echo "  Legacy Token:  $LEGACY_TOKEN"
echo "  Camera ID:     $CAMERA_ID"
echo "  Image File:    $IMAGE_FILE"
echo ""

# Prompt for confirmation
read -p "Proceed with legacy upload test? (y/n) " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Test cancelled."
    exit 0
fi

echo ""
echo "Uploading using legacy POST interface..."
echo ""

# Perform upload using curl with multipart/form-data
# Legacy interface expects: auth, cam, pic parameters
RESPONSE=$(curl -s -w "\nHTTP_STATUS:%{http_code}" \
    -F "auth=$LEGACY_TOKEN" \
    -F "cam=$CAMERA_ID" \
    -F "pic=@$IMAGE_FILE" \
    "$SERVER_URL")

# Extract HTTP status code
HTTP_STATUS=$(echo "$RESPONSE" | grep HTTP_STATUS | cut -d ':' -f 2)
BODY=$(echo "$RESPONSE" | sed '/HTTP_STATUS/d')

echo "HTTP Status: $HTTP_STATUS"
echo ""
echo "Response:"
echo "$BODY" | python3 -m json.tool 2>/dev/null || echo "$BODY"
echo ""

if [ "$HTTP_STATUS" = "200" ]; then
    echo "✓ Legacy upload successful!"
    echo ""
    echo "The image should be stored in: images/$CAMERA_ID/"
    echo "Check config/cameras.json for the auto-created camera entry."
else
    echo "✗ Legacy upload failed!"
    echo ""
    echo "Common issues:"
    echo "  - Check if SERVER_URL is correct"
    echo "  - Verify LEGACY_TOKEN is in config.json's legacy_tokens array"
    echo "  - Ensure web server is running"
    echo "  - Check file permissions on server"
    echo "  - Verify PHP extensions (GD, fileinfo) are installed"
fi

echo ""
