<?php
/**
 * OTA Firmware Management Functions
 * Handles firmware storage, checksums, and OTA scheduling
 */

require_once __DIR__ . '/storage.php';
require_once __DIR__ . '/logging.php';

/**
 * Get firmware directory path
 * @return string Full path to firmware directory
 */
function getFirmwareDir(): string {
    return dirname(__DIR__) . '/firmware';
}

/**
 * Get checksums file path
 * @return string Full path to checksums.json
 */
function getChecksumsFilePath(): string {
    return getFirmwareDir() . '/checksums.json';
}

/**
 * Load checksums data
 * @return array Checksums data
 */
function loadChecksums(): array {
    $file = getChecksumsFilePath();
    if (!file_exists($file)) {
        return [];
    }
    
    $content = file_get_contents($file);
    $data = json_decode($content, true);
    
    return is_array($data) ? $data : [];
}

/**
 * Save checksums data
 * @param array $checksums Checksums data
 * @return bool Success
 */
function saveChecksums(array $checksums): bool {
    $file = getChecksumsFilePath();
    $json = json_encode($checksums, JSON_PRETTY_PRINT);
    return file_put_contents($file, $json) !== false;
}

/**
 * Get available firmware files from firmware/ directory
 * @return array Array of firmware info with version, size, checksum
 */
function getAvailableFirmware(): array {
    $dir = getFirmwareDir();
    if (!is_dir($dir)) {
        return [];
    }
    
    $checksums = loadChecksums();
    $files = [];
    
    $items = scandir($dir);
    foreach ($items as $item) {
        if ($item === '.' || $item === '..' || $item === 'checksums.json') {
            continue;
        }
        
        $filepath = $dir . '/' . $item;
        if (!is_file($filepath) || !str_ends_with($item, '.bin')) {
            continue;
        }
        
        $info = [
            'filename' => $item,
            'size' => filesize($filepath),
            'version' => extractVersionFromFilename($item),
            'uploaded' => isset($checksums[$item]) ? $checksums[$item]['uploaded'] : date('c', filemtime($filepath)),
            'description' => isset($checksums[$item]) ? $checksums[$item]['description'] : '',
            'sha256' => isset($checksums[$item]) ? $checksums[$item]['sha256'] : ''
        ];
        
        $files[] = $info;
    }
    
    // Sort by version descending
    usort($files, function($a, $b) {
        return version_compare($b['version'], $a['version']);
    });
    
    return $files;
}

/**
 * Extract version string from firmware filename
 * @param string $filename Firmware filename (e.g., firmware_v1.2.3.bin)
 * @return string Version string (e.g., "1.2.3") or "unknown"
 */
function extractVersionFromFilename(string $filename): string {
    // Match patterns like firmware_v1.2.3.bin or firmware_1.2.3.bin
    if (preg_match('/(?:firmware[_-])?v?(\d+\.\d+\.\d+)/i', $filename, $matches)) {
        return $matches[1];
    }
    return 'unknown';
}

/**
 * Get firmware file path (sanitized)
 * @param string $filename Firmware filename
 * @return string|false Full path or false if invalid
 */
function getFirmwarePath(string $filename) {
    // Sanitize filename to prevent path traversal
    $filename = basename($filename);
    
    if (empty($filename) || !str_ends_with($filename, '.bin')) {
        return false;
    }
    
    $filepath = getFirmwareDir() . '/' . $filename;
    
    if (!file_exists($filepath) || !is_file($filepath)) {
        return false;
    }
    
    return $filepath;
}

/**
 * Calculate SHA256 checksum of firmware file
 * @param string $filepath Full path to firmware
 * @return string|false SHA256 hex string or false on error
 */
function calculateFirmwareChecksum(string $filepath) {
    if (!file_exists($filepath)) {
        return false;
    }
    
    return hash_file('sha256', $filepath);
}

/**
 * Validate firmware file exists and is readable
 * @param string $filename Firmware filename
 * @return bool True if valid
 */
function isValidFirmware(string $filename): bool {
    $filepath = getFirmwarePath($filename);
    return $filepath !== false && is_readable($filepath);
}

/**
 * Get firmware info by filename
 * @param string $filename Firmware filename
 * @return array|null Firmware info or null if not found
 */
function getFirmwareInfo(string $filename): ?array {
    $filepath = getFirmwarePath($filename);
    if (!$filepath) {
        return null;
    }
    
    $checksums = loadChecksums();
    
    $info = [
        'filename' => $filename,
        'size' => filesize($filepath),
        'version' => extractVersionFromFilename($filename),
        'sha256' => isset($checksums[$filename]) ? $checksums[$filename]['sha256'] : calculateFirmwareChecksum($filepath)
    ];
    
    // Update checksum if not stored
    if (!isset($checksums[$filename]['sha256']) && $info['sha256']) {
        $checksums[$filename] = [
            'sha256' => $info['sha256'],
            'size' => $info['size'],
            'uploaded' => date('c'),
            'description' => $checksums[$filename]['description'] ?? ''
        ];
        saveChecksums($checksums);
    }
    
    return $info;
}

/**
 * Save uploaded firmware file
 * @param array $file $_FILES array element
 * @param string|null $description Optional description
 * @return array Result with success, filename, error
 */
function saveUploadedFirmware(array $file, ?string $description = null): array {
    // Validate file upload
    if (!isset($file['tmp_name']) || $file['error'] !== UPLOAD_ERR_OK) {
        return ['success' => false, 'error' => 'File upload error'];
    }
    
    $filename = basename($file['name']);
    
    // Validate extension
    if (!str_ends_with(strtolower($filename), '.bin')) {
        return ['success' => false, 'error' => 'Invalid file type. Must be .bin'];
    }
    
    // Validate size (100KB - 3MB)
    $size = filesize($file['tmp_name']);
    if ($size < 100 * 1024 || $size > 3 * 1024 * 1024) {
        return ['success' => false, 'error' => 'Invalid file size. Must be between 100KB and 3MB'];
    }
    
    // Calculate checksum before moving
    $sha256 = hash_file('sha256', $file['tmp_name']);
    if (!$sha256) {
        return ['success' => false, 'error' => 'Failed to calculate checksum'];
    }
    
    // Move to firmware directory
    $destPath = getFirmwareDir() . '/' . $filename;
    if (!move_uploaded_file($file['tmp_name'], $destPath)) {
        return ['success' => false, 'error' => 'Failed to save firmware file'];
    }
    
    chmod($destPath, 0664);
    
    // Update checksums file
    $checksums = loadChecksums();
    $checksums[$filename] = [
        'sha256' => $sha256,
        'size' => $size,
        'uploaded' => date('c'),
        'description' => $description ?? ''
    ];
    
    if (!saveChecksums($checksums)) {
        return ['success' => false, 'error' => 'Failed to update checksums file'];
    }
    
    return [
        'success' => true,
        'filename' => $filename,
        'size' => $size,
        'sha256' => $sha256,
        'version' => extractVersionFromFilename($filename)
    ];
}

/**
 * Get OTA schedule for specific camera
 * @param string $deviceId Camera device ID
 * @return array|null OTA info or null if none scheduled
 */
function getOtaSchedule(string $deviceId): ?array {
    $cameras = loadCamerasConfig();
    
    // Normalize device ID for comparison
    $identifierNormalized = strtoupper(str_replace(['-', ':', ' '], '', $deviceId));
    
    foreach ($cameras as $key => $camera) {
        if ($key === '_example_') continue;
        
        // Check device_id or mac field
        $cameraId = $camera['device_id'] ?? $camera['mac'] ?? '';
        if (empty($cameraId)) continue;
        
        $cameraIdNormalized = strtoupper(str_replace(['-', ':', ' '], '', $cameraId));
        
        if ($cameraIdNormalized === $identifierNormalized) {
            // Check if OTA scheduled
            if (!empty($camera['ota_scheduled'])) {
                // Check retry limit (max 2 attempts)
                $retryCount = $camera['ota_retry_count'] ?? 0;
                if ($retryCount >= 2) {
                    // Retry limit reached, don't offer OTA anymore
                    logOta("Retry limit reached for device $deviceId", [
                        'scheduled_firmware' => $camera['ota_scheduled'],
                        'retry_count' => $retryCount
                    ], LOG_LEVEL_WARN);
                    return null;
                }
                
                $firmwareFile = $camera['ota_scheduled'];
                $firmwareInfo = getFirmwareInfo($firmwareFile);
                
                if ($firmwareInfo) {
                    return $firmwareInfo;
                }
            }
            return null;
        }
    }
    
    return null;
}

/**
 * Schedule OTA update for camera
 * @param string $deviceId Camera device ID
 * @param string $firmwareFile Firmware filename
 * @return bool Success
 */
function scheduleOtaUpdate(string $deviceId, string $firmwareFile): bool {
    // Validate firmware exists
    if (!isValidFirmware($firmwareFile)) {
        return false;
    }
    
    $cameras = loadCamerasConfig();
    $identifierNormalized = strtoupper(str_replace(['-', ':', ' '], '', $deviceId));
    
    foreach ($cameras as $key => $camera) {
        if ($key === '_example_') continue;
        
        $cameraId = $camera['device_id'] ?? $camera['mac'] ?? '';
        if (empty($cameraId)) continue;
        
        $cameraIdNormalized = strtoupper(str_replace(['-', ':', ' '], '', $cameraId));
        
        if ($cameraIdNormalized === $identifierNormalized) {
            $cameras[$key]['ota_scheduled'] = $firmwareFile;
            $cameras[$key]['ota_retry_count'] = 0; // Reset retry counter
            $cameras[$key]['ota_last_status'] = null;
            $cameras[$key]['ota_last_error'] = null;
            
            $result = saveCamerasConfig($cameras);
            if ($result) {
                $firmwareInfo = getFirmwareInfo($firmwareFile);
                logOta("OTA scheduled for device $deviceId", [
                    'firmware_file' => $firmwareFile,
                    'version' => $firmwareInfo['version'] ?? 'unknown',
                    'size' => $firmwareInfo['size'] ?? 0
                ]);
            }
            return $result;
        }
    }
    
    return false;
}

/**
 * Clear OTA schedule for camera
 * @param string $deviceId Camera device ID  
 * @return bool Success
 */
function clearOtaSchedule(string $deviceId): bool {
    $cameras = loadCamerasConfig();
    $identifierNormalized = strtoupper(str_replace(['-', ':', ' '], '', $deviceId));
    
    foreach ($cameras as $key => $camera) {
        if ($key === '_example_') continue;
        
        $cameraId = $camera['device_id'] ?? $camera['mac'] ?? '';
        if (empty($cameraId)) continue;
        
        $cameraIdNormalized = strtoupper(str_replace(['-', ':', ' '], '', $cameraId));
        
        if ($cameraIdNormalized === $identifierNormalized) {
            $cameras[$key]['ota_scheduled'] = null;
            
            $result = saveCamerasConfig($cameras);
            if ($result) {
                logOta("OTA schedule cleared for device $deviceId");
            }
            return $result;
        }
    }
    
    return false;
}

/**
 * Update OTA status after attempt
 * @param string $deviceId Camera device ID
 * @param string $status "success", "failed", "pending"
 * @param string|null $error Error message if failed
 * @param string|null $version New firmware version if success
 * @return bool Success
 */
function updateOtaStatus(string $deviceId, string $status, ?string $error = null, ?string $version = null): bool {
    $cameras = loadCamerasConfig();
    $identifierNormalized = strtoupper(str_replace(['-', ':', ' '], '', $deviceId));
    
    foreach ($cameras as $key => $camera) {
        if ($key === '_example_') continue;
        
        $cameraId = $camera['device_id'] ?? $camera['mac'] ?? '';
        if (empty($cameraId)) continue;
        
        $cameraIdNormalized = strtoupper(str_replace(['-', ':', ' '], '', $cameraId));
        
        if ($cameraIdNormalized === $identifierNormalized) {
            $cameras[$key]['ota_last_attempt'] = date('c');
            $cameras[$key]['ota_last_status'] = $status;
            $cameras[$key]['ota_last_error'] = $error;
            
            // Initialize retry count if not set
            if (!isset($cameras[$key]['ota_retry_count'])) {
                $cameras[$key]['ota_retry_count'] = 0;
            }
            
            // Update firmware version on success and clear schedule
            if ($status === 'success' && $version) {
                $cameras[$key]['firmware_version'] = $version;
                $cameras[$key]['ota_scheduled'] = null;
                $cameras[$key]['ota_retry_count'] = 0; // Reset counter on success
                
                $result = saveCamerasConfig($cameras);
                if ($result) {
                    logOta("OTA successful for device $deviceId", [
                        'new_version' => $version
                    ]);
                }
                return $result;
            }
            
            // Increment retry count on failure
            elseif ($status === 'failed') {
                $cameras[$key]['ota_retry_count']++;
                $newRetryCount = $cameras[$key]['ota_retry_count'];
                
                // If retry limit reached (>= 2), clear schedule
                if ($newRetryCount >= 2) {
                    $cameras[$key]['ota_scheduled'] = null;
                    $cameras[$key]['ota_last_error'] = ($error ?? '') . ' (Max retries reached - manual intervention required)';
                    
                    $result = saveCamerasConfig($cameras);
                    if ($result) {
                        logOta("OTA failed for device $deviceId - retry limit reached", [
                            'retry_count' => $newRetryCount,
                            'error' => $error
                        ], LOG_LEVEL_ERROR);
                    }
                    return $result;
                } else {
                    $result = saveCamerasConfig($cameras);
                    if ($result) {
                        logOta("OTA failed for device $deviceId - will retry", [
                            'retry_count' => $newRetryCount,
                            'error' => $error
                        ], LOG_LEVEL_WARN);
                    }
                    return $result;
                }
            }
            
            // Clear schedule on rollback and reset counter
            elseif ($status === 'rollback') {
                $cameras[$key]['ota_scheduled'] = null;
                $cameras[$key]['ota_retry_count'] = 0;
                
                $result = saveCamerasConfig($cameras);
                if ($result) {
                    logOta("OTA rollback for device $deviceId", [
                        'error' => $error
                    ], LOG_LEVEL_ERROR);
                }
                return $result;
            }
            
            // Pending status
            else {
                return saveCamerasConfig($cameras);
            }
        }
    }
    
    return false;
}

/**
 * Update camera firmware version (reported by camera)
 * @param string $deviceId Camera device ID
 * @param string $version Firmware version
 * @return bool Success
 */
function updateCameraFirmwareVersion(string $deviceId, string $version): bool {
    $cameras = loadCamerasConfig();
    $identifierNormalized = strtoupper(str_replace(['-', ':', ' '], '', $deviceId));
    
    foreach ($cameras as $key => $camera) {
        if ($key === '_example_') continue;
        
        $cameraId = $camera['device_id'] ?? $camera['mac'] ?? '';
        if (empty($cameraId)) continue;
        
        $cameraIdNormalized = strtoupper(str_replace(['-', ':', ' '], '', $cameraId));
        
        if ($cameraIdNormalized === $identifierNormalized) {
            $cameras[$key]['firmware_version'] = $version;
            return saveCamerasConfig($cameras);
        }
    }
    
    return false;
}

/**
 * Reset OTA retry counter for a camera (admin function)
 * Allows retrying failed OTA after investigating/fixing issues
 * @param string $deviceId Camera device ID
 * @return bool Success
 */
function resetOtaRetryCount(string $deviceId): bool {
    $cameras = loadCamerasConfig();
    $identifierNormalized = strtoupper(str_replace(['-', ':', ' '], '', $deviceId));
    
    foreach ($cameras as $key => $camera) {
        if ($key === '_example_') continue;
        
        $cameraId = $camera['device_id'] ?? $camera['mac'] ?? '';
        if (empty($cameraId)) continue;
        
        $cameraIdNormalized = strtoupper(str_replace(['-', ':', ' '], '', $cameraId));
        
        if ($cameraIdNormalized === $identifierNormalized) {
            $cameras[$key]['ota_retry_count'] = 0;
            $cameras[$key]['ota_last_error'] = null;
            return saveCamerasConfig($cameras);
        }
    }
    
    return false;
}

/**
 * Count cameras with specific firmware scheduled
 * @param string $firmwareFile Firmware filename
 * @return int Number of cameras
 */
function countCamerasWithFirmware(string $firmwareFile): int {
    $cameras = loadCamerasConfig();
    $count = 0;
    
    foreach ($cameras as $key => $camera) {
        if ($key === '_example_') continue;
        
        if (isset($camera['ota_scheduled']) && $camera['ota_scheduled'] === $firmwareFile) {
            $count++;
        }
    }
    
    return $count;
}

/**
 * Delete firmware file
 * @param string $filename Firmware filename
 * @return bool Success
 */
function deleteFirmware(string $filename): bool {
    // Check if any cameras are using this firmware
    if (countCamerasWithFirmware($filename) > 0) {
        return false; // Don't delete if scheduled
    }
    
    $filepath = getFirmwarePath($filename);
    if (!$filepath) {
        return false;
    }
    
    // Delete file
    if (!unlink($filepath)) {
        return false;
    }
    
    // Remove from checksums
    $checksums = loadChecksums();
    if (isset($checksums[$filename])) {
        unset($checksums[$filename]);
        saveChecksums($checksums);
    }
    
    return true;
}
