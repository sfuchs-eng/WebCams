# WebCams

The WebCams project consists of two main components:

- **EspCamPicPusher**: An ESP32-S3-based application that captures images from a connected camera and pushes them to a server via HTTPS POST requests. It's based on the Seeeduino XIAO ESP32S3 Sense board, which includes a built-in OV2640 camera.
- **WebCamPics**: A PHP application that receives the images from an arbitrary number of EspCamPicPusher devices and displays them in a web interface. It also provides an API endpoint for receiving image uploads.

## ATTENTION

**vibe-coded draft pending review and testing...**
