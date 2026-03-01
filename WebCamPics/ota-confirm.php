<?php
/**
 * OTA Confirmation Endpoint
 * Receive OTA status update from camera after reboot
 */

require_once __DIR__ . '/lib/auth.php';
require_once __DIR__ . '/lib/ota.php';

// Enable error logging
error_log("[OTA-Confirm] Request from " . ($_SERVER['REMOTE_ADDR'] ?? 'unknown'));

// Only accept POST
if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    http_response_code(405);
    header('Content-Type: application/json');
    echo json_encode(['success' => false, 'error' => 'Method not allowed']);
    exit;
}

// Authenticate device
$deviceId = getDeviceId();

if (empty($deviceId)) {
    http_response_code(401);
    header('Content-Type: application/json');
    echo json_encode(['success' => false, 'error' => 'Authentication required - missing device ID']);
    error_log("[OTA-Confirm] Missing device ID");
    exit;
}

if (!authenticateRequest()) {
    http_response_code(401);
    header('Content-Type: application/json');
    echo json_encode(['success' => false, 'error' => 'Invalid authentication token']);
    error_log("[OTA-Confirm] Invalid token for device $deviceId");
    exit;
}

// Parse JSON body
$json = file_get_contents('php://input');
$data = json_decode($json, true);

if (!$data) {
    http_response_code(400);
    header('Content-Type: application/json');
    echo json_encode(['success' => false, 'error' => 'Invalid JSON']);
    error_log("[OTA-Confirm] Invalid JSON from device $deviceId");
    exit;
}

// Extract confirmation data
$success = $data['success'] ?? false;
$firmwareFile = $data['firmware_file'] ?? '';
$firmwareVersion = $data['firmware_version'] ?? '';
$errorMessage = $data['error'] ?? '';
$rollback = $data['rollback'] ?? false;
$message = $data['message'] ?? '';

error_log("[OTA-Confirm] Device $deviceId: " . ($success ? "SUCCESS" : "FAILED") . 
          " - Firmware: $firmwareFile, Version: $firmwareVersion");

if (!empty($errorMessage)) {
    error_log("[OTA-Confirm] Error from device $deviceId: $errorMessage");
}

// Update OTA status
if ($success) {
    // Success - update version and clear schedule
    $result = updateOtaStatus($deviceId, 'success', null, $firmwareVersion);
    
    if ($result) {
        error_log("[OTA-Confirm] Updated device $deviceId to version $firmwareVersion");
        
        http_response_code(200);
        header('Content-Type: application/json');
        echo json_encode([
            'success' => true,
            'message' => 'OTA success recorded, schedule cleared'
        ]);
    } else {
        error_log("[OTA-Confirm] Failed to update status for device $deviceId");
        
        http_response_code(500);
        header('Content-Type: application/json');
        echo json_encode([
            'success' => false,
            'error' => 'Failed to update camera configuration'
        ]);
    }
    
} elseif ($rollback) {
    // Rollback - clear schedule (firmware incompatible)
    $result = updateOtaStatus($deviceId, 'rollback', $errorMessage, null);
    
    if ($result) {
        error_log("[OTA-Confirm] Rollback recorded for device $deviceId: $errorMessage");
        
        http_response_code(200);
        header('Content-Type: application/json');
        echo json_encode([
            'success' => true,
            'message' => 'Rollback recorded, schedule cleared'
        ]);
    } else {
        error_log("[OTA-Confirm] Failed to record rollback for device $deviceId");
        
        http_response_code(500);
        header('Content-Type: application/json');
        echo json_encode([
            'success' => false,
            'error' => 'Failed to update camera configuration'
        ]);
    }
    
} else {
    // Failure - keep schedule for retry
    $result = updateOtaStatus($deviceId, 'failed', $errorMessage, null);
    
    if ($result) {
        error_log("[OTA-Confirm] Failure recorded for device $deviceId, will retry");
        
        http_response_code(200);
        header('Content-Type: application/json');
        echo json_encode([
            'success' => true,
            'message' => 'Failure recorded, OTA schedule retained for retry'
        ]);
    } else {
        error_log("[OTA-Confirm] Failed to record failure for device $deviceId");
        
        http_response_code(500);
        header('Content-Type: application/json');
        echo json_encode([
            'success' => false,
            'error' => 'Failed to update camera configuration'
        ]);
    }
}
