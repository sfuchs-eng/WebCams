#!/bin/bash

# WiFi Stability Setup Script for Raspberry Pi
# Disables power management and optimizes WiFi settings

echo "=== Setting up WiFi stability improvements ==="

# Check if running as root
if [ "$EUID" -eq 0 ]; then
    echo "Please run without sudo (script will prompt for sudo when needed)"
    exit 1
fi

# Disable WiFi power management (reduces dropouts)
echo ""
echo "1. Disabling WiFi power management..."
sudo iw dev wlan0 set power_save off

# Make permanent via rc.local
if [ -f /etc/rc.local ]; then
    if ! grep -q "iw dev wlan0 set power_save off" /etc/rc.local; then
        echo "   Adding to /etc/rc.local for persistence..."
        sudo sed -i '/^exit 0/i \
# Disable WiFi power management for stability\
iw dev wlan0 set power_save off\
' /etc/rc.local
    else
        echo "   Already configured in /etc/rc.local"
    fi
else
    echo "   Warning: /etc/rc.local not found, creating it..."
    echo '#!/bin/sh -e
#
# rc.local
#
# This script is executed at the end of each multiuser runlevel.

# Disable WiFi power management for stability
iw dev wlan0 set power_save off

exit 0' | sudo tee /etc/rc.local > /dev/null
    sudo chmod +x /etc/rc.local
fi

# Optimize WiFi regulatory domain
echo ""
echo "2. Checking WiFi regulatory domain..."
CURRENT_COUNTRY=$(sudo iw reg get | grep country | cut -d' ' -f2 | tr -d ':')
if [ -n "$CURRENT_COUNTRY" ]; then
    echo "   Current country code: $CURRENT_COUNTRY"
    echo "   If this is incorrect, run: sudo raspi-config nonint do_wifi_country XX (replace XX with your country code)"
else
    echo "   Warning: No country code set. This may cause WiFi issues."
    echo "   Set your country code with: sudo raspi-config nonint do_wifi_country XX"
fi

# Disable IPv6 if not needed (can cause connection issues)
echo ""
echo "3. Checking IPv6 configuration..."
if grep -q "net.ipv6.conf.all.disable_ipv6" /etc/sysctl.conf 2>/dev/null; then
    echo "   IPv6 already disabled"
else
    read -p "   Disable IPv6 to reduce connection issues? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "   Disabling IPv6..."
        echo "# Disable IPv6 for WiFi stability" | sudo tee -a /etc/sysctl.conf > /dev/null
        echo "net.ipv6.conf.all.disable_ipv6 = 1" | sudo tee -a /etc/sysctl.conf > /dev/null
        echo "net.ipv6.conf.default.disable_ipv6 = 1" | sudo tee -a /etc/sysctl.conf > /dev/null
        echo "net.ipv6.conf.lo.disable_ipv6 = 1" | sudo tee -a /etc/sysctl.conf > /dev/null
        sudo sysctl -p > /dev/null 2>&1
        echo "   IPv6 disabled"
    fi
fi

# Check current power save status
echo ""
echo "4. Current WiFi power management status:"
sudo iw dev wlan0 get power_save

# Force 2.4 GHz band (better range and stability)
echo ""
echo "5. Checking WiFi band configuration..."
if command -v nmcli &> /dev/null; then
    # NetworkManager is available
    ACTIVE_CONNECTION=$(nmcli -t -f NAME connection show --active | grep -v 'lo' | head -n 1)
    if [ -n "$ACTIVE_CONNECTION" ]; then
        CURRENT_BAND=$(nmcli -t -f 802-11-wireless.band connection show "$ACTIVE_CONNECTION" | cut -d':' -f2)
        echo "   Active connection: $ACTIVE_CONNECTION"
        if [ "$CURRENT_BAND" = "bg" ]; then
            echo "   Already configured for 2.4 GHz band"
        else
            echo "   Current band setting: ${CURRENT_BAND:-auto}"
            read -p "   Force 2.4 GHz band for better range/stability? (y/n) " -n 1 -r
            echo
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                echo "   Configuring 2.4 GHz band (bg)..."
                sudo nmcli connection modify "$ACTIVE_CONNECTION" 802-11-wireless.band bg
                echo "   Band set to 2.4 GHz (bg)"
                echo "   Note: Connection will use new setting on next reconnect"
            fi
        fi
    else
        echo "   Warning: No active WiFi connection found"
    fi
else
    echo "   NetworkManager (nmcli) not found - using alternative method"
    echo "   Note: To force 2.4 GHz, configure your router to separate bands"
    echo "   or use wpa_supplicant configuration with freq_list parameter"
fi

# Additional recommendations
echo ""
echo "=== Additional Recommendations ==="
echo "- Use a quality power supply (5V 3A minimum for Pi 4/5)"
echo "- Add heat sinks if using Pi 4/5"
echo "- Consider USB WiFi dongle if onboard WiFi is problematic"
echo "- Place Pi closer to router or use WiFi repeater"
echo "- Use 2.4GHz WiFi band (better range than 5GHz)"
echo "- Enfore 2.4GHz band via the following NetworkManager command:"
echo "  sudo nmcli connection modify <YourSSID> 802-11-wireless.band bg"
echo ""
echo "=== Setup Complete ==="
echo "Reboot recommended for all changes to take effect: sudo reboot"

# Show current band setting
# nmcli -f 802-11-wireless.band connection show "YourSSID"

# Set to 2.4 GHz only
# sudo nmcli connection modify "YourSSID" 802-11-wireless.band bg

# Set to 5 GHz only  
# sudo nmcli connection modify "YourSSID" 802-11-wireless.band a

# Reset to auto
# sudo nmcli connection modify "YourSSID" 802-11-wireless.band ""