#!/bin/bash

# Configuration
CAM="CoolCam"
AUTH_TOKEN='your-secret-token-here'
POSTHANDLER='https://yourdomain.com/cams/upload.php'

# Retry settings
MAX_RETRIES=3
RETRY_DELAY=5
CAPTURE_TIMEOUT=10
CONNECTION_TIMEOUT=10
MAX_TIME=30

# Function to check internet connectivity
check_connectivity() {
    ping -c 1 -W 2 8.8.8.8 > /dev/null 2>&1
    return $?
}

# Function to capture and upload with retries
capture_and_upload() {
    local attempt=1
    
    while [ $attempt -le $MAX_RETRIES ]; do
        echo "$(date '+%Y-%m-%d %H:%M:%S') - Attempt $attempt/$MAX_RETRIES"
        
        # Check connectivity first
        if ! check_connectivity; then
            echo "No network connectivity, waiting..."
            sleep $RETRY_DELAY
            ((attempt++))
            continue
        fi
        
        # Capture and upload
        if libcamera-jpeg -n --immediate=1 --autofocus-on-capture=1 --metering=average \
                          --timeout ${CAPTURE_TIMEOUT}000 -o - 2>/dev/null | \
           curl -X POST \
                -H "X-Auth-Token: ${AUTH_TOKEN}" \
                -H "Content-Type: image/jpeg" \
                -H "X-Device-ID: ${CAM}" \
                -H "X-Timestamp: $(date +"%Y-%m-%d %H:%M:%S")" \
                --connect-timeout ${CONNECTION_TIMEOUT} \
                --max-time ${MAX_TIME} \
                --retry 2 \
                --retry-delay 3 \
                --data-binary @- \
                -o "-" \
                "${POSTHANDLER}" 2>&1; then
            echo -e "\n$(date '+%Y-%m-%d %H:%M:%S') - Upload successful"
            return 0
        fi
        
        echo "Upload failed, waiting before retry..."
        sleep $RETRY_DELAY
        ((attempt++))
    done
    
    echo "$(date '+%Y-%m-%d %H:%M:%S') - All $MAX_RETRIES attempts failed" >&2
    return 1
}

# Main execution
echo 'Capturing and transmitting image...'
capture_and_upload

## Example for scaling, rotating and adding a label to the image on camera side. Requires ImageMagick's "convert" tool.
## The new API of WebCamPics allows to configure rotation and overlay text on the server side, so this is not needed anymore. But it can be used for more complex image processing on camera side if desired.
# libcamera-jpeg -n --immediate=1 --autofocus-on-capture=1 --metering=average -o - | \
#	convert - -scale "50%" -rotate -90 \
#		-gravity NorthWest -background '#333' -fill White label:"${CAM}, $(date +"%Y-%m-%d %H:%M:%S")" -composite - | \
#	curl -X POST \
#		-H "X-Auth-Token: ${AUTH_TOKEN}" \
#		-H "Content-Type: image/jpeg" \
#		-H "X-Device-ID: ${CAM}" \
#		-H "X-Timestamp: $(date +"%Y-%m-%d %H:%M:%S")" \
#		--data-binary @- \
#		-o "-" \
#		"${POSTHANDLER}"
