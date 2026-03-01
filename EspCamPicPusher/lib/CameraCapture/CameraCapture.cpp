#include "CameraCapture.h"
#include "CameraMutex.h"

void CameraCapture::warmUpSensor(int numFrames, int frameDelay, int settlingDelay) {
    Serial.println("Warming up camera sensor...");
    
    // Capture and discard dummy frames to let AWB/AEC/AGC adapt
    for (int i = 0; i < numFrames; i++) {
        camera_fb_t* dummy = esp_camera_fb_get();
        if (dummy) {
            Serial.printf("  Dummy frame %d discarded (%d bytes)\n", i + 1, dummy->len);
            esp_camera_fb_return(dummy);
        } else {
            Serial.printf("  Warning: Dummy frame %d capture failed\n", i + 1);
        }
        
        // Delay between frames (skip after last frame)
        if (i < numFrames - 1) {
            delay(frameDelay);
        }
    }
    
    // Additional settling time for auto-parameters to fully converge
    if (settlingDelay > 0) {
        delay(settlingDelay);
    }
    
    Serial.println("Sensor adaptation complete");
}

camera_fb_t* CameraCapture::captureFrame(bool withWarmup) {
    if (withWarmup) {
        warmUpSensor();
    }
    
    // Capture actual image (with proper AWB/AEC/AGC if warmed up)
    camera_fb_t* fb = esp_camera_fb_get();
    
    if (!fb) {
        Serial.println("ERROR: Camera capture failed");
        return nullptr;
    }
    
    Serial.printf("Image captured: %d bytes\n", fb->len);
    return fb;
}

void CameraCapture::releaseFrame(camera_fb_t* fb) {
    if (fb) {
        esp_camera_fb_return(fb);
    }
}

camera_fb_t* CameraCapture::captureWithMutex(int timeoutMs) {
    if (!CameraMutex::lock(timeoutMs)) {
        Serial.printf("ERROR: Failed to acquire camera mutex (timeout after %d ms)\n", timeoutMs);
        return nullptr;
    }
    
    camera_fb_t* fb = captureFrame(true);
    
    // Note: Mutex is NOT unlocked here - caller must call releaseFrame() 
    // and then CameraMutex::unlock() to maintain thread safety
    // This allows caller to process the frame while holding the lock
    
    if (!fb) {
        CameraMutex::unlock();
    }
    
    return fb;
}
