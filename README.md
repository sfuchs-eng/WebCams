# WebCams

The WebCams project aims to use a simple web-hosted PHP application to receive and display images captured by multiple camera devices based on either ESP32 (e.g. XIAO ESP32S3 Sense) or a Raspberry Pi (e.g. Raspberry Pi Zero W with a camera module).

The project consists of three main components:

- **WebCamPics**: A PHP application that receives the images from an arbitrary number of EspCamPicPusher devices and displays them in a web interface. It also provides an API endpoint for receiving image uploads. For details see the [WebCamPics README](WebCamPics/README.md).
- **EspCamPicPusher**: An ESP32-S3-based application that captures images from a connected camera and pushes them to a server via HTTPS POST requests. It's based on the Seeeduino XIAO ESP32S3 Sense board, which includes a built-in OV2640 camera.
- **RasPiCamPicPusher**: A bash script that runs on a Raspberry Pi with a camera module. It captures images at scheduled intervals and pushes them to the same server endpoint as the ESP32 devices. Hook it up to cron for regular execution.

## Notice

This project is licensed under the MIT License. See the [LICENSE.md](LICENSE.md) file for details.

It's a quick hack dominantly using Claude Sonnet 4.5 for code generation and refactoring. The project is not intended for professional production use but rather as a fun side private project.
