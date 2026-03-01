#ifndef SCHEDULE_MANAGER_H
#define SCHEDULE_MANAGER_H

#include <Arduino.h>
#include <time.h>

struct ScheduleTime {
    int hour;
    int minute;
};

class ScheduleManager {
public:
    ScheduleManager();
    
    /**
     * Calculate the next wake time based on current time and schedule
     * @param currentTime Current time as tm struct
     * @param schedule Array of scheduled capture times
     * @param numTimes Number of times in schedule
     * @param sleepMarginSec Wake up N seconds before capture
     * @return Wake time as time_t epoch timestamp, or 0 if error
     */
    time_t getNextWakeTime(struct tm* currentTime, ScheduleTime* schedule, int numTimes, int sleepMarginSec);
    
    /**
     * Calculate seconds until next wake time
     * @param currentTime Current time as tm struct
     * @param schedule Array of scheduled capture times
     * @param numTimes Number of times in schedule
     * @param sleepMarginSec Wake up N seconds before capture
     * @return Seconds until wake, or -1 if error
     */
    long getSecondsUntilWake(struct tm* currentTime, ScheduleTime* schedule, int numTimes, int sleepMarginSec);
    
    /**
     * Get the next scheduled capture time (actual capture, not wake time)
     * @param currentTime Current time as tm struct
     * @param schedule Array of scheduled capture times
     * @param numTimes Number of times in schedule
     * @return Next capture time as time_t epoch timestamp, or 0 if error
     */
    time_t getNextCaptureTime(struct tm* currentTime, ScheduleTime* schedule, int numTimes);
    
    /**
     * Check if it's time to capture (within 1 minute of scheduled time)
     * @param currentTime Current time as tm struct
     * @param schedule Array of scheduled capture times
     * @param numTimes Number of times in schedule
     * @return True if current time matches any scheduled time
     */
    bool isTimeToCapture(struct tm* currentTime, ScheduleTime* schedule, int numTimes);
    
    /**
     * Format time as string for display
     * @param timeinfo Time struct
     * @return Formatted time string (YYYY-MM-DD HH:MM:SS)
     */
    static String formatTime(struct tm* timeinfo);
    
    /**
     * Get current time as tm struct
     * @param timeinfo Pointer to tm struct to fill
     * @return True if successful
     */
    static bool getCurrentTime(struct tm* timeinfo);

private:
    /**
     * Find next scheduled time after current time
     * Returns index of next time, or -1 if all times have passed today
     */
    int findNextScheduledTime(struct tm* currentTime, ScheduleTime* schedule, int numTimes);
    
    /**
     * Convert time components to epoch timestamp
     */
    time_t makeTime(int year, int month, int day, int hour, int minute, int second);
    
    /**
     * Compare two times (hour:minute only)
     * Returns: -1 if time1 < time2, 0 if equal, 1 if time1 > time2
     */
    int compareTimes(int hour1, int min1, int hour2, int min2);
};

#endif // SCHEDULE_MANAGER_H
