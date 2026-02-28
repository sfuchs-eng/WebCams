<?php
/**
 * Authentication utilities
 */

// Polyfill for getallheaders() on nginx/php-fpm
if (!function_exists('getallheaders')) {
    function getallheaders() {
        $headers = [];
        foreach ($_SERVER as $name => $value) {
            if (substr($name, 0, 5) == 'HTTP_') {
                $headerName = str_replace(' ', '-', ucwords(strtolower(str_replace('_', ' ', substr($name, 5)))));
                $headers[$headerName] = $value;
            }
        }
        return $headers;
    }
}

/**
 * Get header value case-insensitively
 */
function getHeaderCaseInsensitive($headerName) {
    $headers = getallheaders();
    $headerNameLower = strtolower($headerName);
    
    foreach ($headers as $name => $value) {
        if (strtolower($name) === $headerNameLower) {
            return $value;
        }
    }
    
    return null;
}

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

function cameraConfigExists($identifier) {
    $cameras = loadCamerasConfig();
    $identifierNormalized = strtoupper(str_replace(['-', ':', ' '], '', $identifier));
    
    foreach ($cameras as $key => $camera) {
        if ($key === '_example_') continue;
        
        // Check if camera identifier matches
        $cameraIdField = isset($camera['device_id']) ? $camera['device_id'] : $camera['mac'];
        $cameraIdNormalized = strtoupper(str_replace(['-', ':', ' '], '', $cameraIdField));
        
        if ($cameraIdNormalized === $identifierNormalized) {
            return true;
        }
    }
    
    return false;
}

function createDefaultCameraConfig($identifier) {
    require_once __DIR__ . '/storage.php';
    $cameras = loadCamerasConfig();
    
    // Generate a unique key for the camera
    $sanitized = sanitizeCameraIdentifier($identifier);
    
    // Fallback if sanitization fails
    if ($sanitized === false) {
        $sanitized = 'cam_' . substr(md5($identifier), 0, 8);
    }
    
    $key = 'cam_' . strtolower(str_replace([':', '-', ' '], '', $sanitized));
    
    // Determine if identifier looks like a MAC address
    $isMac = preg_match('/^[0-9A-Fa-f]{2}[:-]?[0-9A-Fa-f]{2}[:-]?[0-9A-Fa-f]{2}[:-]?[0-9A-Fa-f]{2}[:-]?[0-9A-Fa-f]{2}[:-]?[0-9A-Fa-f]{2}$/', $identifier);
    
    // Create default config with camera HIDDEN (processed but not shown)
    $cameras[$key] = [
        'device_id' => $identifier,
        'location' => 'unknown',
        'title' => $isMac ? 'Camera ' . substr($identifier, -8) : $identifier,
        'status' => 'hidden',  // 3-state: 'disabled', 'hidden', 'enabled'
        'rotate' => 0,
        'add_timestamp' => true,
        'add_title' => true,
        'font_size' => 16,
        'font_color' => '#FFFFFF',
        'font_outline' => true
    ];
    
    // Keep 'mac' field for backward compatibility if it's a MAC address
    if ($isMac) {
        $cameras[$key]['mac'] = $identifier;
    }
    
    // Save config
    saveCamerasConfig($cameras);
    
    return $cameras[$key];
}

function getCameraConfig($identifier) {
    $cameras = loadCamerasConfig();
    $identifierNormalized = strtoupper(str_replace(['-', ':', ' '], '', $identifier));
    
    foreach ($cameras as $key => $camera) {
        if ($key === '_example_') continue;
        
        // Check if camera identifier matches
        $cameraIdField = isset($camera['device_id']) ? $camera['device_id'] : $camera['mac'];
        $cameraIdNormalized = strtoupper(str_replace(['-', ':', ' '], '', $cameraIdField));
        
        if ($cameraIdNormalized === $identifierNormalized) {
            return $camera;
        }
    }
    
    // Return default config if not found
    $isMac = preg_match('/^[0-9A-Fa-f]{2}[:-]?[0-9A-Fa-f]{2}[:-]?[0-9A-Fa-f]{2}[:-]?[0-9A-Fa-f]{2}[:-]?[0-9A-Fa-f]{2}[:-]?[0-9A-Fa-f]{2}$/', $identifier);
    
    return [
        'device_id' => $identifier,
        'mac' => $isMac ? $identifier : '',
        'location' => 'unknown',
        'title' => $isMac ? 'Camera ' . substr($identifier, -8) : $identifier,
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
    if (!$config || !isset($config['auth_tokens'])) {
        return false;
    }
    
    // Try multiple sources for auth token (fallback order)
    $authHeader = '';
    
    // 1. Try custom X-Auth-Token header (works with Apache/PHP-FPM)
    $xAuthToken = getHeaderCaseInsensitive('X-Auth-Token');
    if ($xAuthToken) {
        $authHeader = 'Bearer ' . $xAuthToken;
    }
    // 2. Try standard Authorization header
    else {
        $authorization = getHeaderCaseInsensitive('Authorization');
        if ($authorization) {
            $authHeader = $authorization;
        } 
        // 3. Try $_SERVER variants
        elseif (isset($_SERVER['HTTP_AUTHORIZATION'])) {
            $authHeader = $_SERVER['HTTP_AUTHORIZATION'];
        } 
        elseif (isset($_SERVER['REDIRECT_HTTP_AUTHORIZATION'])) {
            $authHeader = $_SERVER['REDIRECT_HTTP_AUTHORIZATION'];
        }
    }
    
    // Check Bearer token
    if (preg_match('/Bearer\s+(.+)/', $authHeader, $matches)) {
        $token = $matches[1];
        return in_array($token, $config['auth_tokens']);
    }
    
    return false;
}

/**
 * Authenticate legacy POST request
 * Returns camera identifier if authenticated, false otherwise
 */
function authenticateLegacyRequest() {
    $config = loadConfig();
    if (!$config || !isset($config['auth_tokens'])) {
        return false;
    }
    
    if (!isset($_POST['auth']) || !isset($_POST['cam'])) {
        return false;
    }
    
    $token = $_POST['auth'];
    $cameraId = $_POST['cam'];
    
    // Check if token is valid
    if (!in_array($token, $config['auth_tokens'])) {
        return false;
    }
    
    // Return the camera identifier
    return $cameraId;
}

function getDeviceId() {
    return getHeaderCaseInsensitive('X-Device-ID');
}

function getTimestamp() {
    return getHeaderCaseInsensitive('X-Timestamp');
}

/**
 * Get the upload endpoint URL
 */
function getUploadUrl() {
    require_once __DIR__ . '/path.php';
    return fullUrl('upload.php');
}
