#!/bin/bash

# WiFi Stability Tools Installation Script for RasPiCam
# This script installs WiFi monitoring and stability improvements
#
# Recommended alternative:
# install NetworkManager and use its built-in WiFi management features

set -e

echo "=== Installing WiFi Stability Tools for RasPiCam ==="
echo ""

# Check if running as root
if [ "$EUID" -eq 0 ]; then
    echo "Error: Please run without sudo (script will prompt for sudo when needed)"
    exit 1
fi

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Make scripts executable
echo "1. Making scripts executable..."
chmod +x capture-upload-image.sh
chmod +x setup-wifi-stability.sh
chmod +x wifi-watchdog.sh
echo "   Done"

# Apply WiFi optimizations
echo ""
echo "2. Applying WiFi stability optimizations..."
./setup-wifi-stability.sh

# Install systemd service for WiFi watchdog
echo ""
echo "3. Installing WiFi watchdog systemd service..."

# Update the service file with the correct path
WIFI_WATCHDOG_PATH="$SCRIPT_DIR/wifi-watchdog.sh"
sudo sed "s|/home/pi/RasPiCam/wifi-watchdog.sh|$WIFI_WATCHDOG_PATH|g" \
    systemd/wifi-watchdog.service > /tmp/wifi-watchdog.service

sudo mv /tmp/wifi-watchdog.service /etc/systemd/system/wifi-watchdog.service
sudo chmod 644 /etc/systemd/system/wifi-watchdog.service

# Reload systemd
sudo systemctl daemon-reload

# Enable and start the watchdog service
echo ""
read -p "Enable and start WiFi watchdog service now? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    sudo systemctl enable wifi-watchdog.service
    sudo systemctl start wifi-watchdog.service
    echo "   WiFi watchdog service started"
else
    echo "   Skipped. You can enable it later with:"
    echo "   sudo systemctl enable wifi-watchdog.service"
    echo "   sudo systemctl start wifi-watchdog.service"
fi

# Display status commands
echo ""
echo "=== Installation Complete ==="
echo ""
echo "Useful commands:"
echo "  Check WiFi watchdog status:"
echo "    systemctl status wifi-watchdog.service"
echo ""
echo "  View WiFi watchdog logs:"
echo "    sudo journalctl -u wifi-watchdog -f"
echo "    sudo tail -f /var/log/wifi-watchdog.log"
echo ""
echo "  Test manual capture:"
echo "    ./capture-upload-image.sh"
echo ""
echo "  Configure cron job for scheduled captures:"
echo "    crontab -e"
echo "    # Add line (example: every 5 minutes):"
echo "    */5 * * * * $SCRIPT_DIR/capture-upload-image.sh >> /var/log/webcam-upload.log 2>&1"
echo ""
echo "Reboot recommended for all changes to take effect: sudo reboot"
