<?php
/**
 * Image Upload Endpoint
 * Receives images from ESP32 cameras via HTTP POST
 */

header('Content-Type: application/json');

require_once __DIR__ . '/lib/auth.php';
require_once __DIR__ . '/lib/storage.php';
require_once __DIR__ . '/lib/image.php';

// Only accept POST requests
if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    http_response_code(405);
    echo json_encode(['error' => 'Method not allowed']);
    exit;
}

// Check for legacy POST interface (multipart/form-data with auth, cam, pic parameters)
if (isset($_POST['auth']) && isset($_POST['cam']) && isset($_FILES['pic'])) {
    // Legacy authentication
    $deviceId = authenticateLegacyRequest();
    if ($deviceId === false) {
        http_response_code(401);
        echo json_encode(['error' => 'Unauthorized']);
        exit;
    }
    
    // Validate file upload
    $pic = $_FILES['pic'];
    if ($pic['error'] !== UPLOAD_ERR_OK) {
        http_response_code(400);
        echo json_encode(['error' => 'File upload failed: ' . $pic['error']]);
        exit;
    }
    
    // Read uploaded file
    $imageData = file_get_contents($pic['tmp_name']);
    if ($imageData === false || empty($imageData)) {
        http_response_code(400);
        echo json_encode(['error' => 'Failed to read uploaded file']);
        exit;
    }
    
    // Validate image size
    $imageSize = strlen($imageData);
    $config = loadConfig();
    $maxSize = ($config['upload_max_size_mb'] ?? 5) * 1024 * 1024;
    
    if ($imageSize > $maxSize) {
        http_response_code(413);
        echo json_encode(['error' => 'Image too large']);
        exit;
    }
    
    // Check if it's a valid JPEG
    $finfo = new finfo(FILEINFO_MIME_TYPE);
    $mimeType = $finfo->buffer($imageData);
    if ($mimeType !== 'image/jpeg') {
        http_response_code(400);
        echo json_encode(['error' => 'Invalid image format. Only JPEG is accepted.']);
        exit;
    }
    
    // Auto-create config entry for new cameras (hidden by default)
    if (!cameraConfigExists($deviceId)) {
        createDefaultCameraConfig($deviceId);
    }
    
    // Get timestamp from header (if provided) or use current time
    $timestamp = getTimestamp();
    
    // Save raw image
    $rawPath = saveImage($deviceId, $imageData, $timestamp);
    if (!$rawPath) {
        http_response_code(500);
        echo json_encode(['error' => 'Failed to save image']);
        exit;
    }
    
    // Process image (rotate, add text, etc.)
    $processedPath = processImage($rawPath, $deviceId);
    if (!$processedPath) {
        http_response_code(500);
        echo json_encode(['error' => 'Failed to process image']);
        exit;
    }
    
    // Log the upload
    $logEntry = sprintf(
        "[%s] Legacy upload from %s (%d bytes) - Saved as %s\n",
        date('Y-m-d H:i:s'),
        $deviceId,
        $imageSize,
        basename($processedPath)
    );
    file_put_contents(__DIR__ . '/logs/upload.log', $logEntry, FILE_APPEND);
    
    // Return success response (JSON format)
    http_response_code(200);
    echo json_encode([
        'success' => true,
        'device_id' => $deviceId,
        'timestamp' => $timestamp ?? date('Y-m-d H:i:s'),
        'size' => $imageSize,
        'filename' => basename($processedPath)
    ]);
    exit;
}

// Authenticate request (current Bearer token method)
if (!authenticateRequest()) {
    http_response_code(401);
    echo json_encode(['error' => 'Unauthorized']);
    exit;
}

// Get device ID (MAC address)
$deviceId = getDeviceId();
if (!$deviceId) {
    http_response_code(400);
    echo json_encode(['error' => 'Missing X-Device-ID header']);
    exit;
}

// Auto-create config entry for new cameras (disabled by default)
if (!cameraConfigExists($deviceId)) {
    createDefaultCameraConfig($deviceId);
}

// Get timestamp from header
$timestamp = getTimestamp();

// Get image data from POST body
$imageData = file_get_contents('php://input');
if (empty($imageData)) {
    http_response_code(400);
    echo json_encode(['error' => 'No image data received']);
    exit;
}

// Validate image size
$imageSize = strlen($imageData);
$config = loadConfig();
$maxSize = ($config['upload_max_size_mb'] ?? 5) * 1024 * 1024;

if ($imageSize > $maxSize) {
    http_response_code(413);
    echo json_encode(['error' => 'Image too large']);
    exit;
}

// Check if it's a valid JPEG
$finfo = new finfo(FILEINFO_MIME_TYPE);
$mimeType = $finfo->buffer($imageData);
if ($mimeType !== 'image/jpeg') {
    http_response_code(400);
    echo json_encode(['error' => 'Invalid image format. Only JPEG is accepted.']);
    exit;
}

// Save raw image
$rawPath = saveImage($deviceId, $imageData, $timestamp);
if (!$rawPath) {
    http_response_code(500);
    echo json_encode(['error' => 'Failed to save image']);
    exit;
}

// Process image (rotate, add text, etc.)
$processedPath = processImage($rawPath, $deviceId);
if (!$processedPath) {
    http_response_code(500);
    echo json_encode(['error' => 'Failed to process image']);
    exit;
}

// Log the upload
$logEntry = sprintf(
    "[%s] Image received from %s (%d bytes) - Saved as %s\n",
    date('Y-m-d H:i:s'),
    $deviceId,
    $imageSize,
    basename($processedPath)
);
file_put_contents(__DIR__ . '/logs/upload.log', $logEntry, FILE_APPEND);

// Return success response
http_response_code(200);
echo json_encode([
    'success' => true,
    'device_id' => $deviceId,
    'timestamp' => $timestamp ?? date('Y-m-d H:i:s'),
    'size' => $imageSize,
    'filename' => basename($processedPath)
]);
