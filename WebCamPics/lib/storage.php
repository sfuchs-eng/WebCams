<?php
/**
 * Storage utilities for managing image files
 */

require_once __DIR__ . '/path.php';

function getImagesDir() {
    return __DIR__ . '/../images';
}

/**
 * Sanitize camera identifier (MAC address or camera name) for use in directory names
 * Allows alphanumeric characters, dots, hyphens, and underscores
 */
function sanitizeCameraIdentifier($identifier) {
    // Remove colons and replace with hyphens (for MAC addresses)
    $identifier = str_replace(':', '-', $identifier);
    // Allow only alphanumeric, dots, hyphens, underscores
    $identifier = preg_replace('/[^a-zA-Z0-9_-]/', '_', $identifier);
    return $identifier;
}

function getCameraDir($identifier) {
    $baseDir = getImagesDir();
    $safeIdentifier = sanitizeCameraIdentifier($identifier);
    return $baseDir . '/' . $safeIdentifier;
}

function ensureCameraDir($identifier) {
    $dir = getCameraDir($identifier);
    if (!is_dir($dir)) {
        mkdir($dir, 0755, true);
    }
    return $dir;
}

function saveImage($identifier, $imageData, $timestamp = null) {
    $dir = ensureCameraDir($identifier);
    
    if (!$timestamp) {
        $timestamp = date('Y-m-d_H-i-s');
    } else {
        // Convert timestamp format if needed
        $timestamp = str_replace([':', ' '], ['_', '_'], $timestamp);
    }
    
    $filename = $timestamp . '.jpg';
    $filepath = $dir . '/' . $filename;
    
    // Save raw image first
    $rawPath = $filepath . '.raw';
    if (file_put_contents($rawPath, $imageData) === false) {
        return false;
    }
    
    return $rawPath;
}

function getLatestImage($identifier) {
    $dir = getCameraDir($identifier);
    if (!is_dir($dir)) {
        return null;
    }
    
    $files = glob($dir . '/*.jpg');
    if (empty($files)) {
        return null;
    }
    
    usort($files, function($a, $b) {
        return filemtime($b) - filemtime($a);
    });
    
    return $files[0];
}

function getCameraImages($identifier, $days = 14) {
    $dir = getCameraDir($identifier);
    if (!is_dir($dir)) {
        return [];
    }
    
    $files = glob($dir . '/*.jpg');
    $cutoff = time() - ($days * 24 * 60 * 60);
    
    $images = [];
    foreach ($files as $file) {
        if (filemtime($file) >= $cutoff) {
            $images[] = [
                'path' => $file,
                'url' => baseUrl('images/' . basename(dirname($file)) . '/' . basename($file)),
                'timestamp' => filemtime($file),
                'size' => filesize($file)
            ];
        }
    }
    
    usort($images, function($a, $b) {
        return $b['timestamp'] - $a['timestamp'];
    });
    
    return $images;
}

function getAllCameras() {
    $baseDir = getImagesDir();
    if (!is_dir($baseDir)) {
        return [];
    }
    
    $cameras = [];
    $dirs = glob($baseDir . '/*', GLOB_ONLYDIR);
    
    foreach ($dirs as $dir) {
        $identifier = basename($dir);
        $latest = getLatestImage($identifier);
        if ($latest) {
            $cameras[$identifier] = [
                'mac' => $identifier,
                'latest_image' => baseUrl('images/' . basename($dir) . '/' . basename($latest)),
                'latest_timestamp' => filemtime($latest)
            ];
        }
    }
    
    return $cameras;
}

function cleanupOldImages($days = 14) {
    $baseDir = getImagesDir();
    if (!is_dir($baseDir)) {
        return;
    }
    
    $cutoff = time() - ($days * 24 * 60 * 60);
    $dirs = glob($baseDir . '/*', GLOB_ONLYDIR);
    
    $deleted = 0;
    foreach ($dirs as $dir) {
        $files = glob($dir . '/*');
        foreach ($files as $file) {
            if (filemtime($file) < $cutoff) {
                unlink($file);
                $deleted++;
            }
        }
    }
    
    return $deleted;
}
