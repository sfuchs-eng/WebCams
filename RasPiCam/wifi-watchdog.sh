#!/bin/bash

# WiFi Connection Watchdog
# Monitors network connectivity and restarts WiFi interface if connection is lost

# Configuration
LOG_FILE="/var/log/wifi-watchdog.log"
PING_HOST="8.8.8.8"
CHECK_INTERVAL=30
FAIL_THRESHOLD=3

fail_count=0

# Ensure log file exists and is writable
if [ ! -f "$LOG_FILE" ]; then
    sudo touch "$LOG_FILE"
    sudo chmod 666 "$LOG_FILE"
fi

# Function to log messages
log_message() {
    # uncomment if you want logging. But ensure logrotation is set up to prevent log file from growing indefinitely
    #echo "$(date '+%Y-%m-%d %H:%M:%S') - $1" | tee -a "$LOG_FILE"
}

# Function to restart WiFi interface
restart_wifi() {
    log_message "Restarting WiFi interface..."
    
    # Try to bring down the interface
    sudo ip link set wlan0 down
    sleep 2
    
    # Bring it back up
    sudo ip link set wlan0 up
    sleep 5
    
    # Restart network service
    if systemctl is-active --quiet dhcpcd; then
        sudo systemctl restart dhcpcd
    elif systemctl is-active --quiet NetworkManager; then
        sudo systemctl restart NetworkManager
    else
        log_message "Warning: No known network service found (dhcpcd/NetworkManager)"
    fi
    
    sleep 10
}

# Main watchdog loop
log_message "WiFi watchdog started (checking every ${CHECK_INTERVAL}s, threshold: ${FAIL_THRESHOLD})"

while true; do
    if ping -c 1 -W 2 $PING_HOST > /dev/null 2>&1; then
        # Connection successful
        if [ $fail_count -gt 0 ]; then
            log_message "Connection restored after $fail_count failed attempts"
        fi
        fail_count=0
    else
        # Connection failed
        ((fail_count++))
        log_message "Ping to $PING_HOST failed ($fail_count/$FAIL_THRESHOLD)"
        
        if [ $fail_count -ge $FAIL_THRESHOLD ]; then
            log_message "Threshold reached, attempting WiFi restart..."
            restart_wifi
            fail_count=0
        fi
    fi
    
    sleep $CHECK_INTERVAL
done
