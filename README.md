# WebCams

The WebCams project aims to use a simple web-hosted PHP application to receive and display images captured by multiple camera devices based on either ESP32 (e.g. XIAO ESP32S3 Sense) or a Raspberry Pi (e.g. Raspberry Pi Zero W with a camera module).

The project consists of three main components:

- **WebCamPics**: A PHP application that receives the images from an arbitrary number of EspCamPicPusher devices and displays them in a web interface. It also provides an API endpoint for receiving image uploads. For details see the [WebCamPics README](WebCamPics/README.md).
- **EspCamPicPusher**: An ESP32-S3-based application that captures images from a connected camera and pushes them to a server via HTTPS POST requests. It's based on the Seeeduino XIAO ESP32S3 Sense board, which includes a built-in OV2640 camera.
- **RasPiCamPicPusher**: A bash script that runs on a Raspberry Pi with a camera module. It captures images at scheduled intervals and pushes them to the same server endpoint as the ESP32 devices. Hook it up to cron for regular execution.

## Disclaimer

This project is intended for educational and personal use only. It's provided as-is without support and may contain bugs, incomplete features, or security vulnerabilities. It should not be used for any commercial or illegal activities. Always ensure that you have permission to capture and display images from the cameras you are using, and respect privacy laws and regulations in your area. The author is not responsible for any misuse of this project.
