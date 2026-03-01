#include "CameraMutex.h"

// Static member initialization
SemaphoreHandle_t CameraMutex::mutex = nullptr;

void CameraMutex::init() {
    if (mutex == nullptr) {
        mutex = xSemaphoreCreateMutex();
        if (mutex != nullptr) {
            Serial.println("Camera mutex initialized");
        } else {
            Serial.println("ERROR: Failed to create camera mutex!");
        }
    }
}

bool CameraMutex::lock(uint32_t timeoutMs) {
    if (mutex == nullptr) {
        init();
    }
    
    if (mutex == nullptr) {
        return false;
    }
    
    TickType_t timeout = (timeoutMs == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
    return xSemaphoreTake(mutex, timeout) == pdTRUE;
}

void CameraMutex::unlock() {
    if (mutex != nullptr) {
        xSemaphoreGive(mutex);
    }
}
