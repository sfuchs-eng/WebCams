#!/bin/bash
# WebCamPics Setup Script
# Run this script after copying the application to your web server

echo "==================================="
echo "  WebCamPics Setup Script"
echo "==================================="
echo ""

# Get the directory where the script is located
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$DIR"

echo "Working directory: $DIR"
echo ""

# Check PHP installation
echo "Checking PHP installation..."
if ! command -v php &> /dev/null; then
    echo "ERROR: PHP is not installed or not in PATH"
    exit 1
fi

PHP_VERSION=$(php -v | head -n 1 | cut -d ' ' -f 2 | cut -d '.' -f 1,2)
echo "PHP version: $PHP_VERSION ✓"

# Check GD extension
echo "Checking GD extension..."
if php -m | grep -q "gd"; then
    echo "GD extension: Installed ✓"
else
    echo "WARNING: GD extension not found. Image processing will not work!"
    echo "Install it with: sudo apt-get install php-gd (Ubuntu/Debian)"
fi

echo ""
echo "Setting up directories..."

# Create necessary directories
mkdir -p images
mkdir -p logs
mkdir -p config

echo "Directories created ✓"

echo ""
echo "Setting permissions..."

# Set permissions
chmod 755 .
chmod 755 images
chmod 755 logs
chmod 755 config
chmod 755 lib
chmod 755 assets

# Make sure PHP files are readable
chmod 644 *.php
chmod 644 lib/*.php
chmod 644 config/*.json
chmod 644 assets/*.css

# Secure .htpasswd file (only web server can read)
if [ -f "config/.htpasswd" ]; then
    chmod 600 config/.htpasswd
    echo ".htpasswd permissions secured ✓"
fi

# Make upload endpoint accessible
chmod 644 upload.php

# Make cleanup script executable
chmod 755 cleanup.php

echo "Permissions set ✓"

echo ""
echo "Checking configuration..."

if [ -f "config/config.json" ]; then
    # Check if default token is still in use
    if grep -q "your_secret_token_here" config/config.json; then
        echo ""
        echo "⚠️  WARNING: Default authentication token detected!"
        echo ""
        echo "Please edit config/config.json and change 'auth_token'"
        echo "to a secure random string before using in production."
        echo ""
        echo "Generate a random token with:"
        echo "  openssl rand -hex 32"
        echo ""
    else
        echo "Authentication token: Configured ✓"
    fi
else
    echo "ERROR: config/config.json not found!"
    exit 1
fi

echo ""
echo "Configuring .htaccess paths..."

# Update AuthUserFile path in .htaccess with absolute path
HTACCESS_FILE=".htaccess"
PASSWD_PATH="$DIR/config/.htpasswd"

if [ -f "$HTACCESS_FILE" ]; then
    # Check if .htaccess has the AuthUserFile directive
    if grep -q "AuthUserFile" "$HTACCESS_FILE"; then
        # Update the path to the absolute path
        if [[ "$OSTYPE" == "darwin"* ]]; then
            # macOS
            sed -i '' "s|AuthUserFile .*\.htpasswd|AuthUserFile $PASSWD_PATH|g" "$HTACCESS_FILE"
        else
            # Linux
            sed -i "s|AuthUserFile .*\.htpasswd|AuthUserFile $PASSWD_PATH|g" "$HTACCESS_FILE"
        fi
        echo "Updated .htaccess with absolute path: $PASSWD_PATH ✓"
    fi
fi

echo ""
echo "==================================="
echo "  Setup Complete!"
echo "==================================="
echo ""
echo "Next steps:"
echo "1. Edit config/config.json and set a secure auth_token"
echo "2. Create HTTP Basic Auth user for admin panel:"
echo "   htpasswd -c config/.htpasswd adminuser"
echo "   (You'll be prompted for a password)"
echo "2. Configure your ESP32-CAM devices with the upload URL and token"
echo "3. Access the web interface at: http://your-domain/webcams/"
echo ""
echo "Upload endpoint: $DIR/upload.php"
echo ""
echo "Optional: Set up a cron job for automatic cleanup:"
echo "  0 3 * * * /usr/bin/php $DIR/cleanup.php"
echo ""
