<?php
/**
 * Remote Logging API Endpoint
 * Allows cameras to send log messages remotely for centralized troubleshooting
 */

header('Content-Type: application/json');

require_once __DIR__ . '/lib/auth.php';
require_once __DIR__ . '/lib/logging.php';

// Only accept POST requests
if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    http_response_code(405);
    echo json_encode(['success' => false, 'error' => 'Method not allowed']);
    exit;
}

// Authenticate request
if (!authenticateRequest()) {
    http_response_code(401);
    echo json_encode(['success' => false, 'error' => 'Unauthorized']);
    exit;
}

// Get device ID
$deviceId = getDeviceId();
if (!$deviceId) {
    http_response_code(400);
    echo json_encode(['success' => false, 'error' => 'Missing X-Device-ID header']);
    exit;
}

// Parse JSON body
$json = file_get_contents('php://input');
if (empty($json)) {
    http_response_code(400);
    echo json_encode(['success' => false, 'error' => 'Empty request body']);
    exit;
}

$data = json_decode($json, true);
if (!$data) {
    http_response_code(400);
    echo json_encode(['success' => false, 'error' => 'Invalid JSON']);
    exit;
}

// Support both single log entry and batch of entries
$entries = [];
if (isset($data['entries']) && is_array($data['entries'])) {
    // Batch mode
    $entries = $data['entries'];
} else {
    // Single entry mode
    $entries = [$data];
}

// Validate and write each entry
$successCount = 0;
$failureCount = 0;
$errors = [];

foreach ($entries as $index => $entry) {
    // Validate required fields
    $level = strtoupper($entry['level'] ?? 'INFO');
    $component = $entry['component'] ?? 'Unknown';
    $message = $entry['message'] ?? '';
    $context = $entry['context'] ?? [];
    
    // Validate log level
    $validLevels = ['DEBUG', 'INFO', 'WARN', 'ERROR'];
    if (!in_array($level, $validLevels)) {
        $level = 'INFO';
    }
    
    // Validate message not empty
    if (empty($message)) {
        $failureCount++;
        $errors[] = "Entry $index: Empty message";
        continue;
    }
    
    // Write to camera log
    if (writeCameraLog($deviceId, $level, $component, $message, $context)) {
        $successCount++;
    } else {
        $failureCount++;
        $errors[] = "Entry $index: Write failed";
    }
}

// Build response
$response = [
    'success' => $successCount > 0,
    'device_id' => $deviceId,
    'entries_received' => count($entries),
    'entries_written' => $successCount
];

if ($failureCount > 0) {
    $response['failures'] = $failureCount;
    $response['errors'] = $errors;
}

// Return success response
http_response_code(200);
echo json_encode($response);
