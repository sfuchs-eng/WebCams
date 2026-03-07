# WebCams

The WebCams project aims to use a simple web-hosted PHP application to receive and display images captured by multiple camera devices based on either ESP32 (e.g. XIAO ESP32S3 Sense) or a Raspberry Pi (e.g. Raspberry Pi Zero W with a camera module).

New firmware updates can be OTA pushed to the ESP32 devices from the same web application, allowing for easy maintenance and feature updates without physical access to the devices.

The project consists of three main components:

- **WebCamPics**: The website part. A PHP application that receives the images from an arbitrary number of camera devices accessing the website's API. The application then displays the images in a web interface. For details see the [WebCamPics README](WebCamPics/README.md).
- **EspCamPicPusher**: An ESP32-S3-based application that captures images from a connected camera and pushes them to a server via HTTPS POST requests. It's based on the Seeeduino XIAO ESP32S3 Sense board, which includes a built-in OV2640 camera. Adjustment to other ESP32-based boards with camera modules should be straightforward. For details see the [EspCamPicPusher README](EspCamPicPusher/README.md).
- **RasPiCamPicPusher**: A bash script that runs on a Raspberry Pi with a camera module. It captures images at scheduled intervals and pushes them to the same server endpoint as the ESP32 devices. Hook it up to cron for regular execution. The same script can be adapted to other Linux-based devices with camera capabilities. Or it might be adapted to capture images from network cameras not capable of accessing the image pushing API and thus acting as a proxy. For details see the [RasPiCamPicPusher README](RasPiCamPicPusher/README.md).

## Notice

This project is licensed under the MIT License. See the [LICENSE.md](LICENSE.md) file for details.

It's a quick hack dominantly using Claude Sonnet for code generation and refactoring. The project is not intended for professional production use but rather as a fun side private project.
