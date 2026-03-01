<?php
/**
 * OTA Firmware Download Endpoint
 * Secure firmware download for authenticated cameras
 */

require_once __DIR__ . '/lib/auth.php';
require_once __DIR__ . '/lib/ota.php';

// Enable error logging
error_log("[OTA-Download] Request from " . ($_SERVER['REMOTE_ADDR'] ?? 'unknown'));

// Authenticate device
$deviceId = getDeviceId();

if (empty($deviceId)) {
    http_response_code(401);
    header('Content-Type: application/json');
    echo json_encode(['success' => false, 'error' => 'Authentication required - missing device ID']);
    error_log("[OTA-Download] Missing device ID");
    exit;
}

if (!authenticateRequest()) {
    http_response_code(401);
    header('Content-Type: application/json');
    echo json_encode(['success' => false, 'error' => 'Invalid authentication token']);
    error_log("[OTA-Download] Invalid token for device $deviceId");
    exit;
}

error_log("[OTA-Download] Device $deviceId authenticated");

// Get requested firmware file
$firmwareFile = $_GET['file'] ?? '';

if (empty($firmwareFile)) {
    http_response_code(400);
    header('Content-Type: application/json');
    echo json_encode(['success' => false, 'error' => 'No firmware file specified']);
    error_log("[OTA-Download] No file specified");
    exit;
}

error_log("[OTA-Download] Requested file: $firmwareFile");

// Verify OTA is scheduled for this device
$otaSchedule = getOtaSchedule($deviceId);

if (!$otaSchedule) {
    http_response_code(403);
    header('Content-Type: application/json');
    echo json_encode(['success' => false, 'error' => 'OTA not scheduled for this device']);
    error_log("[OTA-Download] OTA not scheduled for device $deviceId");
    exit;
}

// Verify requested file matches scheduled firmware
if ($otaSchedule['filename'] !== $firmwareFile) {
    http_response_code(403);
    header('Content-Type: application/json');
    echo json_encode(['success' => false, 'error' => 'Requested firmware does not match scheduled update']);
    error_log("[OTA-Download] File mismatch: requested=$firmwareFile, scheduled={$otaSchedule['filename']}");
    exit;
}

// Get firmware file path
$firmwarePath = getFirmwarePath($firmwareFile);

if (!$firmwarePath || !file_exists($firmwarePath)) {
    http_response_code(404);
    header('Content-Type: application/json');
    echo json_encode(['success' => false, 'error' => 'Firmware file not found']);
    error_log("[OTA-Download] File not found: $firmwareFile");
    exit;
}

// Log download attempt
error_log("[OTA-Download] Starting download for device $deviceId: $firmwareFile (" . filesize($firmwarePath) . " bytes)");

// Update OTA status to 'pending'
updateOtaStatus($deviceId, 'pending', null, null);

// Send firmware file
header('Content-Type: application/octet-stream');
header('Content-Length: ' . filesize($firmwarePath));
header('Content-Disposition: attachment; filename="' . $firmwareFile . '"');
header('X-Firmware-SHA256: ' . $otaSchedule['sha256']);
header('X-Firmware-Version: ' . $otaSchedule['version']);
header('Cache-Control: no-cache, no-store, must-revalidate');
header('Pragma: no-cache');
header('Expires: 0');

// Stream file in chunks to handle large files
$handle = fopen($firmwarePath, 'rb');
if ($handle) {
    while (!feof($handle)) {
        echo fread($handle, 8192);
        flush();
    }
    fclose($handle);
    error_log("[OTA-Download] Download completed for device $deviceId");
} else {
    error_log("[OTA-Download] Failed to open file for reading: $firmwareFile");
    http_response_code(500);
    exit;
}
