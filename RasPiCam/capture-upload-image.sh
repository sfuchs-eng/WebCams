#!/bin/bash

# Configuration, either put here or provide from environment variables
CAM="${CAM_NAME:-CoolCam}"
AUTH_TOKEN="${CAM_AUTH_TOKEN:-your-secret-token-here}"
POSTHANDLER="${CAM_POSTHANDLER:-https://yourdomain.com/cams/upload.php}"

# Retry settings
MAX_RETRIES=3
RETRY_DELAY=5
CAPTURE_TIMEOUT=10
CONNECTION_TIMEOUT=10
MAX_TIME=30

# Log configuration
LOG_DIR="/var/log/webcam"
LOG_FILE="${LOG_DIR}/capture.log"

# Ensure log directory exists
mkdir -p "${LOG_DIR}"

# Log rotation function - rotates daily, keeps only previous day
rotate_logs() {
	if [ ! -d "${LOG_DIR}" ]; then
		mkdir -p "${LOG_DIR}"
	fi
    if [ -f "${LOG_FILE}" ]; then
        # Get the modification date of the log file
        local log_date=$(date -r "${LOG_FILE}" +%Y-%m-%d)
        local today=$(date +%Y-%m-%d)
        
        # If the log file is from a different day, rotate it
        if [ "${log_date}" != "${today}" ]; then
            mv "${LOG_FILE}" "${LOG_FILE}.${log_date}"
        fi
    fi
    
    # Remove logs older than 30 days (keep current day and previous 29 days)
    find "${LOG_DIR}" -name "capture.log.*" -type f -mtime +30 -delete 2>/dev/null
}

# Perform log rotation
rotate_logs

# Redirect all output to log file and stdout
exec > >(tee -a "${LOG_FILE}") 2>&1

# Function to check internet connectivity
check_connectivity() {
    ping -c 1 -W 2 8.8.8.8 > /dev/null 2>&1
    return $?
}

# Function to capture and upload with retries
capture_and_upload() {
    local attempt=1
    
    echo "$(date '+%Y-%m-%d %H:%M:%S') - Starting capture and upload process"
    echo "Camera: ${CAM}"
    echo "Post Handler: ${POSTHANDLER}"

    while [ $attempt -le $MAX_RETRIES ]; do
        echo "$(date '+%Y-%m-%d %H:%M:%S') - Attempt $attempt/$MAX_RETRIES"
        
        # Check connectivity first
        if ! check_connectivity; then
            echo "$(date '+%Y-%m-%d %H:%M:%S') - No network connectivity, waiting..."
            sleep $RETRY_DELAY
            ((attempt++))
            continue
        fi
        
        # Capture and upload (removed --immediate=1 to allow autofocus and metering to work properly, but added --timeout to prevent hanging)
        if libcamera-jpeg -n --autofocus-on-capture=1 --metering=average \
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
        
        echo "$(date '+%Y-%m-%d %H:%M:%S') - Upload failed, waiting before retry..."
        sleep $RETRY_DELAY
        ((attempt++))
    done
    
    echo "$(date '+%Y-%m-%d %H:%M:%S') - All $MAX_RETRIES attempts failed" >&2
    return 1
}

# Main execution
echo "$(date '+%Y-%m-%d %H:%M:%S') - Capturing and transmitting image..."
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
