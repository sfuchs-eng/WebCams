#!/bin/bash
#
# Build and Upload OTA Firmware Script
# Compiles ESP32 firmware and uploads to OTA server
#
# Usage: ./build-and-upload-ota.sh [options]
# Options:
#   -d, --description "text"  Set firmware description
#   -s, --skip-build          Skip compilation, upload existing firmware
#   -h, --help                Show this help message
#

set -e  # Exit on error

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
SKIP_BUILD=false
CUSTOM_DESCRIPTION=""

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--description)
            CUSTOM_DESCRIPTION="$2"
            shift 2
            ;;
        -s|--skip-build)
            SKIP_BUILD=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  -d, --description \"text\"  Set firmware description"
            echo "  -s, --skip-build          Skip compilation, upload existing firmware"
            echo "  -h, --help                Show this help message"
            echo ""
            echo "Configuration:"
            echo "  Copy .ota-credentials.template to .ota-credentials and configure your server details"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# Load credentials
CREDENTIALS_FILE=".ota-credentials"

if [ ! -f "$CREDENTIALS_FILE" ]; then
    echo -e "${RED}Error: Credentials file not found: $CREDENTIALS_FILE${NC}"
    echo -e "${YELLOW}Please copy .ota-credentials.template to .ota-credentials and configure it${NC}"
    exit 1
fi

# Source the credentials file
source "$CREDENTIALS_FILE"

# Validate required variables
if [ -z "$OTA_SERVER_URL" ]; then
    echo -e "${RED}Error: OTA_SERVER_URL not set in $CREDENTIALS_FILE${NC}"
    exit 1
fi

if [ -z "$OTA_USERNAME" ] || [ -z "$OTA_PASSWORD" ]; then
    echo -e "${RED}Error: OTA_USERNAME and OTA_PASSWORD must be set in $CREDENTIALS_FILE${NC}"
    exit 1
fi

# Set description
if [ -n "$CUSTOM_DESCRIPTION" ]; then
    FIRMWARE_DESCRIPTION="$CUSTOM_DESCRIPTION"
elif [ -n "$OTA_DESCRIPTION" ]; then
    FIRMWARE_DESCRIPTION="$OTA_DESCRIPTION"
else
    FIRMWARE_DESCRIPTION="Build $(date +%Y-%m-%d_%H:%M:%S)"
fi

# Extract firmware version from platformio.ini
FIRMWARE_VERSION=$(grep "FIRMWARE_VERSION" platformio.ini | grep -oP '\d+\.\d+\.\d+' || echo "unknown")

# PlatformIO paths
PLATFORMIO_BIN="$HOME/.platformio/penv/bin/pio"
if [ ! -f "$PLATFORMIO_BIN" ]; then
    # Try alternative location
    PLATFORMIO_BIN="platformio"
    if ! command -v $PLATFORMIO_BIN &> /dev/null; then
        echo -e "${RED}Error: PlatformIO not found${NC}"
        echo "Please install PlatformIO: https://platformio.org/"
        exit 1
    fi
fi

BUILD_DIR=".pio/build/seeed_xiao_esp32s3"
FIRMWARE_FILE="$BUILD_DIR/firmware.bin"

# Build firmware
if [ "$SKIP_BUILD" = false ]; then
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}Building firmware v${FIRMWARE_VERSION}${NC}"
    echo -e "${BLUE}========================================${NC}"
    
    "$PLATFORMIO_BIN" run
    
    if [ $? -ne 0 ]; then
        echo -e "${RED}Build failed!${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}✓ Build successful${NC}"
else
    echo -e "${YELLOW}Skipping build (using existing firmware)${NC}"
fi

# Check if firmware file exists
if [ ! -f "$FIRMWARE_FILE" ]; then
    echo -e "${RED}Error: Firmware file not found: $FIRMWARE_FILE${NC}"
    echo "Please build the firmware first or remove --skip-build flag"
    exit 1
fi

# Get firmware file size
FIRMWARE_SIZE=$(stat -f%z "$FIRMWARE_FILE" 2>/dev/null || stat -c%s "$FIRMWARE_FILE" 2>/dev/null)
FIRMWARE_SIZE_MB=$(echo "scale=2; $FIRMWARE_SIZE / 1024 / 1024" | bc)

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Uploading to OTA Server${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "Server:      ${YELLOW}$OTA_SERVER_URL${NC}"
echo -e "Version:     ${YELLOW}v$FIRMWARE_VERSION${NC}"
echo -e "Size:        ${YELLOW}${FIRMWARE_SIZE_MB} MB${NC}"
echo -e "Description: ${YELLOW}$FIRMWARE_DESCRIPTION${NC}"
echo ""

# Generate firmware filename
FIRMWARE_FILENAME="XiaoPicPusher_v${FIRMWARE_VERSION}.bin"

# Upload to server using curl
UPLOAD_URL="${OTA_SERVER_URL}/ota-upload.php"

echo -e "${BLUE}Uploading...${NC}"

RESPONSE=$(curl -s -w "\n%{http_code}" \
    -u "${OTA_USERNAME}:${OTA_PASSWORD}" \
    -F "firmware=@${FIRMWARE_FILE};filename=${FIRMWARE_FILENAME}" \
    -F "description=${FIRMWARE_DESCRIPTION}" \
    "$UPLOAD_URL")

# Split response into body and status code
HTTP_BODY=$(echo "$RESPONSE" | head -n -1)
HTTP_CODE=$(echo "$RESPONSE" | tail -n 1)

# Check response
if [ "$HTTP_CODE" -eq 200 ]; then
    echo -e "${GREEN}✓ Upload successful!${NC}"
    echo ""
    echo "Server response:"
    echo "$HTTP_BODY" | python3 -m json.tool 2>/dev/null || echo "$HTTP_BODY"
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}OTA firmware ready for deployment${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo "Next steps:"
    echo "1. Go to your server admin panel"
    echo "2. Edit the target camera configuration"
    echo "3. Schedule the firmware update"
    echo "4. Camera will update on next image upload"
    exit 0
else
    echo -e "${RED}✗ Upload failed!${NC}"
    echo -e "HTTP Status: ${RED}$HTTP_CODE${NC}"
    echo ""
    echo "Server response:"
    echo "$HTTP_BODY"
    exit 1
fi
