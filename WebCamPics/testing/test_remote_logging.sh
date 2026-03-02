#!/bin/bash
# Test script for remote logging API

# Configuration - UPDATE THESE
SERVER_URL="https://your-server.com/webcams"
AUTH_TOKEN="your-auth-token-here"
DEVICE_ID="AA:BB:CC:DD:EE:FF"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "======================================"
echo "  Remote Logging API Test"
echo "======================================"
echo ""

# Test 1: Single log entry
echo -e "${YELLOW}Test 1: Single log entry${NC}"
RESPONSE=$(curl -s -w "\n%{http_code}" -X POST \
  -H "Content-Type: application/json" \
  -H "X-Auth-Token: ${AUTH_TOKEN}" \
  -H "X-Device-ID: ${DEVICE_ID}" \
  -d '{
    "level": "INFO",
    "component": "Test",
    "message": "Single log entry test",
    "context": {"test_id": 1, "timestamp": "'$(date -Iseconds)'"}
  }' \
  "${SERVER_URL}/log.php")

HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
BODY=$(echo "$RESPONSE" | head -n-1)

if [ "$HTTP_CODE" -eq 200 ]; then
  echo -e "${GREEN}✓ Success (HTTP $HTTP_CODE)${NC}"
  echo "Response: $BODY"
else
  echo -e "${RED}✗ Failed (HTTP $HTTP_CODE)${NC}"
  echo "Response: $BODY"
fi
echo ""

# Test 2: Batch log entries
echo -e "${YELLOW}Test 2: Batch log entries${NC}"
RESPONSE=$(curl -s -w "\n%{http_code}" -X POST \
  -H "Content-Type: application/json" \
  -H "X-Auth-Token: ${AUTH_TOKEN}" \
  -H "X-Device-ID: ${DEVICE_ID}" \
  -d '{
    "entries": [
      {
        "level": "DEBUG",
        "component": "Test",
        "message": "Debug message",
        "context": {"entry": 1}
      },
      {
        "level": "INFO",
        "component": "Test",
        "message": "Info message",
        "context": {"entry": 2}
      },
      {
        "level": "WARN",
        "component": "Test",
        "message": "Warning message",
        "context": {"entry": 3}
      },
      {
        "level": "ERROR",
        "component": "Test",
        "message": "Error message",
        "context": {"entry": 4, "critical": true}
      }
    ]
  }' \
  "${SERVER_URL}/log.php")

HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
BODY=$(echo "$RESPONSE" | head -n-1)

if [ "$HTTP_CODE" -eq 200 ]; then
  echo -e "${GREEN}✓ Success (HTTP $HTTP_CODE)${NC}"
  echo "Response: $BODY"
else
  echo -e "${RED}✗ Failed (HTTP $HTTP_CODE)${NC}"
  echo "Response: $BODY"
fi
echo ""

# Test 3: OTA simulation log
echo -e "${YELLOW}Test 3: OTA simulation log${NC}"
RESPONSE=$(curl -s -w "\n%{http_code}" -X POST \
  -H "Content-Type: application/json" \
  -H "X-Auth-Token: ${AUTH_TOKEN}" \
  -H "X-Device-ID: ${DEVICE_ID}" \
  -d '{
    "level": "INFO",
    "component": "OTA",
    "message": "Update available, processing",
    "context": {
      "mode": "CAPTURE",
      "firmware_file": "firmware_v1.1.1.bin",
      "version": "1.1.1",
      "size": 1234567
    }
  }' \
  "${SERVER_URL}/log.php")

HTTP_CODE=$(echo "$RESPONSE" | tail -n1)
BODY=$(echo "$RESPONSE" | head -n-1)

if [ "$HTTP_CODE" -eq 200 ]; then
  echo -e "${GREEN}✓ Success (HTTP $HTTP_CODE)${NC}"
  echo "Response: $BODY"
else
  echo -e "${RED}✗ Failed (HTTP $HTTP_CODE)${NC}"
  echo "Response: $BODY"
fi
echo ""

# Show log file
DATE=$(date +%Y-%m-%d)
SAFE_DEVICE_ID=$(echo "$DEVICE_ID" | sed 's/:/_/g')
LOG_FILE="../logs/camera_${SAFE_DEVICE_ID}_${DATE}.log"

echo -e "${YELLOW}Checking log file: $LOG_FILE${NC}"
if [ -f "$LOG_FILE" ]; then
  echo -e "${GREEN}✓ Log file exists${NC}"
  echo "Recent entries:"
  tail -n 10 "$LOG_FILE"
else
  echo -e "${RED}✗ Log file not found${NC}"
fi

echo ""
echo "======================================"
echo "  Test Complete"
echo "======================================"
