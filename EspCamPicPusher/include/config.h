#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// Application Configuration
// ============================================================================

// NTP Configuration
const char* NTP_SERVER = "pool.ntp.org";
const char* NTP_SERVER2 = "time.nist.gov";
const long GMT_OFFSET_SEC = 3600;           // Adjust for your timezone (e.g., -18000 for EST)
const int DAYLIGHT_OFFSET_SEC = 3600;       // Adjust for daylight saving (e.g., 3600)

// Capture Schedule Configuration
// Times in 24-hour format (hour, minute)
struct CaptureTime {
    int hour;
    int minute;
};

// Define capture times (add/remove as needed)
const CaptureTime CAPTURE_TIMES[] = {
    {8, 0},    // 08:00
    {12, 0},   // 12:00
    {16, 0},   // 16:00
    {20, 0}    // 20:00
};

const int NUM_CAPTURE_TIMES = sizeof(CAPTURE_TIMES) / sizeof(CaptureTime);

// NTP Update Interval (in milliseconds)
const unsigned long NTP_UPDATE_INTERVAL = 36000000; // 10 hours

// Camera Configuration for XIAO ESP32S3 Sense
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

// Camera Settings
#define CAMERA_FRAME_SIZE FRAMESIZE_UXGA  // 1600x1200
#define CAMERA_JPEG_QUALITY 10            // 0-63, lower means higher quality

#endif // CONFIG_H
