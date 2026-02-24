<?php
/**
 * Authentication utilities
 */

function loadConfig() {
    $configFile = __DIR__ . '/../config/config.json';
    if (!file_exists($configFile)) {
        return null;
    }
    $content = file_get_contents($configFile);
    return json_decode($content, true);
}

function loadCamerasConfig() {
    $configFile = __DIR__ . '/../config/cameras.json';
    if (!file_exists($configFile)) {
        return [];
    }
    $content = file_get_contents($configFile);
    return json_decode($content, true);
}

function saveCamerasConfig($cameras) {
    $configFile = __DIR__ . '/../config/cameras.json';
    return file_put_contents($configFile, json_encode($cameras, JSON_PRETTY_PRINT));
}

function cameraConfigExists($mac) {
    $cameras = loadCamerasConfig();
    $macNormalized = strtoupper(str_replace(['-', ':'], '', $mac));
    
    foreach ($cameras as $key => $camera) {
        if ($key === '_example_') continue;
        $cameraMacNormalized = strtoupper(str_replace(['-', ':'], '', $camera['mac']));
        if ($cameraMacNormalized === $macNormalized) {
            return true;
        }
    }
    
    return false;
}

function createDefaultCameraConfig($mac) {
    $cameras = loadCamerasConfig();
    
    // Generate a unique key for the camera
    $key = 'cam_' . strtolower(str_replace([':', '-'], '', $mac));
    
    // Create default config with camera HIDDEN (processed but not shown)
    $cameras[$key] = [
        'mac' => $mac,
        'location' => 'unknown',
        'title' => 'Camera ' . substr($mac, -8),
        'status' => 'hidden',  // 3-state: 'disabled', 'hidden', 'enabled'
        'rotate' => 0,
        'add_timestamp' => true,
        'add_title' => true,
        'font_size' => 16,
        'font_color' => '#FFFFFF',
        'font_outline' => true
    ];
    
    // Save config
    saveCamerasConfig($cameras);
    
    return $cameras[$key];
}

function getCameraConfig($mac) {
    $cameras = loadCamerasConfig();
    $macNormalized = strtoupper(str_replace(['-', ':'], '', $mac));
    
    foreach ($cameras as $key => $camera) {
        if ($key === '_example_') continue;
        $cameraMacNormalized = strtoupper(str_replace(['-', ':'], '', $camera['mac']));
        if ($cameraMacNormalized === $macNormalized) {
            return $camera;
        }
    }
    
    // Return default config if not found
    return [
        'mac' => $mac,
        'location' => 'unknown',
        'title' => 'Camera ' . substr($mac, -8),
        'status' => 'enabled',
        'rotate' => 0,
        'add_timestamp' => true,
        'add_title' => true,
        'font_size' => 16,
        'font_color' => '#FFFFFF',
        'font_outline' => true
    ];
}

function authenticateRequest() {
    $config = loadConfig();
    if (!$config) {
        return false;
    }
    
    $headers = getallheaders();
    $authHeader = isset($headers['Authorization']) ? $headers['Authorization'] : '';
    
    // Check Bearer token
    if (preg_match('/Bearer\s+(.+)/', $authHeader, $matches)) {
        $token = $matches[1];
        return $token === $config['auth_token'];
    }
    
    return false;
}

function getDeviceId() {
    $headers = getallheaders();
    return isset($headers['X-Device-ID']) ? $headers['X-Device-ID'] : null;
}

function getTimestamp() {
    $headers = getallheaders();
    return isset($headers['X-Timestamp']) ? $headers['X-Timestamp'] : null;
}

/**
 * Get the upload endpoint URL
 */
function getUploadUrl() {
    require_once __DIR__ . '/path.php';
    return fullUrl('upload.php');
}
