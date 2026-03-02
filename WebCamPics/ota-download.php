<?php
/**
 * OTA Firmware Download Endpoint
 * Secure firmware download for authenticated cameras
 */

require_once __DIR__ . '/lib/auth.php';
require_once __DIR__ . '/lib/ota.php';
require_once __DIR__ . '/lib/logging.php';

// Log download attempt with full request details
logOta("Download request received", [
    'remote_addr' => $_SERVER['REMOTE_ADDR'] ?? 'unknown',
    'request_uri' => $_SERVER['REQUEST_URI'] ?? 'unknown',
    'user_agent' => $_SERVER['HTTP_USER_AGENT'] ?? 'unknown',
    'file_param' => $_GET['file'] ?? 'missing'
]);

// Authenticate device
$deviceId = getDeviceId();

if (empty($deviceId)) {
    http_response_code(401);
    header('Content-Type: application/json');
    logOta("Download failed: missing device ID", [
        'remote_addr' => $_SERVER['REMOTE_ADDR'] ?? 'unknown'
    ], LOG_LEVEL_ERROR);
    echo json_encode(['success' => false, 'error' => 'Authentication required - missing device ID']);
    exit;
}

if (!authenticateRequest()) {
    http_response_code(401);
    header('Content-Type: application/json');
    logOta("Download failed: invalid token for device $deviceId", [
        'device_id' => $deviceId,
        'remote_addr' => $_SERVER['REMOTE_ADDR'] ?? 'unknown'
    ], LOG_LEVEL_ERROR);
    echo json_encode(['success' => false, 'error' => 'Invalid authentication token']);
    exit;
}

logOta("Device $deviceId authenticated for download", [
    'device_id' => $deviceId
]);

// Get requested firmware file
$firmwareFile = $_GET['file'] ?? '';

if (empty($firmwareFile)) {
    http_response_code(400);
    header('Content-Type: application/json');
    logOta("Download failed: no firmware file specified", [
        'device_id' => $deviceId
    ], LOG_LEVEL_ERROR);
    echo json_encode(['success' => false, 'error' => 'No firmware file specified']);
    exit;
}

logOta("Firmware file requested: $firmwareFile", [
    'device_id' => $deviceId,
    'firmware_file' => $firmwareFile
]);

// Verify OTA is scheduled for this device
$otaSchedule = getOtaSchedule($deviceId);

if (!$otaSchedule) {
    http_response_code(403);
    header('Content-Type: application/json');
    logOta("Download failed: OTA not scheduled for device $deviceId", [
        'device_id' => $deviceId,
        'requested_file' => $firmwareFile
    ], LOG_LEVEL_ERROR);
    echo json_encode(['success' => false, 'error' => 'OTA not scheduled for this device']);
    exit;
}

// Verify requested file matches scheduled firmware
if ($otaSchedule['filename'] !== $firmwareFile) {
    http_response_code(403);
    header('Content-Type: application/json');
    logOta("Download failed: file mismatch for device $deviceId", [
        'device_id' => $deviceId,
        'requested_file' => $firmwareFile,
        'scheduled_file' => $otaSchedule['filename']
    ], LOG_LEVEL_ERROR);
    echo json_encode(['success' => false, 'error' => 'Requested firmware does not match scheduled update']);
    exit;
}

// Get firmware file path
$firmwarePath = getFirmwarePath($firmwareFile);

if (!$firmwarePath || !file_exists($firmwarePath)) {
    http_response_code(404);
    header('Content-Type: application/json');
    logOta("Download failed: firmware file not found", [
        'device_id' => $deviceId,
        'firmware_file' => $firmwareFile
    ], LOG_LEVEL_ERROR);
    echo json_encode(['success' => false, 'error' => 'Firmware file not found']);
    exit;
}

// Log download attempt
logOta("Starting firmware download for device $deviceId", [
    'device_id' => $deviceId,
    'firmware_file' => $firmwareFile,
    'size' => filesize($firmwarePath),
    'version' => $otaSchedule['version'],
    'sha256' => $otaSchedule['sha256']
]);

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
    $bytesSent = 0;
    while (!feof($handle)) {
        $chunk = fread($handle, 8192);
        echo $chunk;
        $bytesSent += strlen($chunk);
        flush();
    }
    fclose($handle);
    
    logOta("Download completed for device $deviceId", [
        'device_id' => $deviceId,
        'firmware_file' => $firmwareFile,
        'bytes_sent' => $bytesSent
    ]);
} else {
    logOta("Download failed: could not open file for reading", [
        'device_id' => $deviceId,
        'firmware_file' => $firmwareFile
    ], LOG_LEVEL_ERROR);
    http_response_code(500);
    exit;
}
