#!/bin/bash

CAM="CoolCam"
AUTH_TOKEN='your-secret-token-here'
POSTHANDLER='https://yourdomain.com/cams/upload.php'

echo 'Capturing and transmitting image...'

libcamera-jpeg -n --immediate=1 --autofocus-on-capture=1 --metering=average -o - | \
	curl -X POST \
		-H "X-Auth-Token: ${AUTH_TOKEN}" \
		-H "Content-Type: image/jpeg" \
		-H "X-Device-ID: ${CAM}" \
		-H "X-Timestamp: $(date +"%Y-%m-%d %H:%M:%S")" \
		--data-binary @- \
		-o "-" \
		"${POSTHANDLER}"

echo -e "\nDone."

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
