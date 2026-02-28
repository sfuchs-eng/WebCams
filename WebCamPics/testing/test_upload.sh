#!/bin/bash
# Test Upload Script
# Tests the image upload endpoint with a sample image

echo "==================================="
echo "  Upload Endpoint Test"
echo "==================================="
echo ""

# Configuration
SERVER_URL="http://localhost/webcams/upload.php"
AUTH_TOKEN="your_secret_token_here"
DEVICE_ID="AA:BB:CC:DD:EE:FF"

# Check if a test image is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <image.jpg>"
    echo ""
    echo "Example:"
    echo "  $0 test_image.jpg"
    echo ""
    exit 1
fi

IMAGE_FILE="$1"

if [ ! -f "$IMAGE_FILE" ]; then
    echo "ERROR: Image file '$IMAGE_FILE' not found!"
    exit 1
fi

echo "Test Configuration:"
echo "  Server URL: $SERVER_URL"
echo "  Auth Token: $AUTH_TOKEN"
echo "  Device ID:  $DEVICE_ID"
echo "  Image File: $IMAGE_FILE"
echo ""

# Prompt for confirmation
read -p "Proceed with upload test? (y/n) " -n 1 -r
echo ""

if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Test cancelled."
    exit 0
fi

echo ""
echo "Uploading..."
echo ""

# Perform upload using curl
TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')

RESPONSE=$(curl -s -w "\nHTTP_STATUS:%{http_code}" \
    -X POST \
    -H "Content-Type: image/jpeg" \
    -H "Authorization: Bearer $AUTH_TOKEN" \
    -H "X-Device-ID: $DEVICE_ID" \
    -H "X-Timestamp: $TIMESTAMP" \
    --data-binary "@$IMAGE_FILE" \
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
    echo "✓ Upload successful!"
else
    echo "✗ Upload failed!"
    echo ""
    echo "Common issues:"
    echo "  - Check if SERVER_URL is correct"
    echo "  - Verify AUTH_TOKEN matches server configuration"
    echo "  - Ensure web server is running"
    echo "  - Check file permissions on server"
fi

echo ""
