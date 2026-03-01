#ifndef CAMERA_MUTEX_H
#define CAMERA_MUTEX_H

#include <Arduino.h>

/**
 * Simple mutex wrapper for camera access synchronization
 * Prevents concurrent camera captures from different cores/tasks
 */
class CameraMutex {
public:
    static void init();
    static bool lock(uint32_t timeoutMs = 5000);
    static void unlock();
    
private:
    static SemaphoreHandle_t mutex;
};

#endif // CAMERA_MUTEX_H
