#ifndef CAMERACAPTURE_H
#define CAMERACAPTURE_H

#include <Arduino.h>
#include "esp_camera.h"

/**
 * CameraCapture - Camera warm-up and capture utilities
 * 
 * Provides camera sensor warm-up functionality to ensure proper auto white balance,
 * auto exposure, and auto gain control before capturing images. This is especially
 * important after waking from deep sleep when light conditions may have changed.
 * 
 * THREAD SAFETY: All methods assume CameraMutex is already locked by the caller.
 * 
 * Usage Pattern:
 *   CameraMutex::lock(5000);
 *   CameraCapture::warmUpSensor();
 *   camera_fb_t* fb = CameraCapture::captureFrame();
 *   // ... use frame ...
 *   CameraCapture::releaseFrame(fb);
 *   CameraMutex::unlock();
 */
class CameraCapture {
public:
    /**
     * Warm up camera sensor by capturing and discarding dummy frames.
     * This allows AWB/AEC/AGC to adapt to current light conditions.
     * 
     * Default timing: 3 frames with 200ms delays + 500ms settling = ~900ms total
     * 
     * IMPORTANT: Must be called with CameraMutex already locked!
     * 
     * @param numFrames Number of dummy frames to capture (default: 3)
     * @param frameDelay Delay between frames in ms (default: 200)
     * @param settlingDelay Final settling delay in ms (default: 500)
     */
    static void warmUpSensor(int numFrames = 3, int frameDelay = 200, int settlingDelay = 500);
    
    /**
     * Capture a single frame with optional warm-up.
     * 
     * IMPORTANT: Must be called with CameraMutex already locked!
     * 
     * @param withWarmup If true, performs sensor warm-up before capture
     * @return Frame buffer pointer, or nullptr on failure
     */
    static camera_fb_t* captureFrame(bool withWarmup = true);
    
    /**
     * Release a previously captured frame buffer.
     * 
     * @param fb Frame buffer to release
     */
    static void releaseFrame(camera_fb_t* fb);
    
    /**
     * Capture a frame with warm-up, acquiring and releasing mutex automatically.
     * Convenience method for simple capture scenarios.
     * 
     * @param timeoutMs Mutex acquisition timeout in milliseconds
     * @return Frame buffer pointer, or nullptr on failure (must call releaseFrame when done)
     */
    static camera_fb_t* captureWithMutex(int timeoutMs = 5000);
    
private:
    // Static utility class - no instances
    CameraCapture() = delete;
    ~CameraCapture() = delete;
    CameraCapture(const CameraCapture&) = delete;
    CameraCapture& operator=(const CameraCapture&) = delete;
};

#endif // CAMERACAPTURE_H
