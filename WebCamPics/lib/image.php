<?php
/**
 * Image manipulation utilities
 */

require_once __DIR__ . '/auth.php';

function processImage($rawImagePath, $mac) {
    $cameraConfig = getCameraConfig($mac);
    
    // Handle 3-state status: disabled, hidden, enabled
    $status = $cameraConfig['status'] ?? 'enabled';
    
    if ($status === 'disabled') {
        // Disabled: discard image completely
        unlink($rawImagePath);
        return false;
    }
    
    // For 'hidden' and 'enabled': process and store the image
    
    // Load image
    $image = imagecreatefromjpeg($rawImagePath);
    if (!$image) {
        unlink($rawImagePath);
        return false;
    }
    
    // Get original dimensions
    $width = imagesx($image);
    $height = imagesy($image);
    
    // Apply rotation if needed
    if ($cameraConfig['rotate'] != 0) {
        $image = imagerotate($image, -$cameraConfig['rotate'], 0);
        
        // Update dimensions after rotation
        if ($cameraConfig['rotate'] == 90 || $cameraConfig['rotate'] == 270) {
            $temp = $width;
            $width = $height;
            $height = $temp;
        }
    }
    
    // Add text overlays
    if ($cameraConfig['add_title'] || $cameraConfig['add_timestamp']) {
        $fontSize = $cameraConfig['font_size'];
        $fontColor = hexToRgb($cameraConfig['font_color']);
        
        // Setup font
        $font = null; // Use built-in font for now
        
        $yPosition = 20 + $fontSize;
        
        // Add title
        if ($cameraConfig['add_title']) {
            $title = $cameraConfig['title'];
            addTextWithOutline($image, $fontSize, $yPosition, $title, $fontColor, $cameraConfig['font_outline']);
            $yPosition += $fontSize + 10;
        }
        
        // Add timestamp
        if ($cameraConfig['add_timestamp']) {
            $timestamp = basename($rawImagePath, '.jpg.raw');
            $timestamp = str_replace(['_'], [' ', ':'], $timestamp);
            // Format: YYYY-MM-DD HH:MM:SS
            $displayTime = preg_replace('/(\d{4})-(\d{2})-(\d{2}) (\d{2}) (\d{2}) (\d{2})/', '$1-$2-$3 $4:$5:$6', $timestamp);
            addTextWithOutline($image, $fontSize - 2, $yPosition, $displayTime, $fontColor, $cameraConfig['font_outline']);
        }
    }
    
    // Save processed image
    $outputPath = str_replace('.jpg.raw', '.jpg', $rawImagePath);
    $success = imagejpeg($image, $outputPath, 90);
    
    // GdImage objects auto-cleanup in PHP 8.0+ (imagedestroy deprecated in PHP 8.5)
    
    // Remove raw file
    unlink($rawImagePath);
    
    return $success ? $outputPath : false;
}

function addTextWithOutline($image, $fontSize, $y, $text, $color, $outline = true) {
    $x = 10;
    
    // Allocate colors
    $textColor = imagecolorallocate($image, $color['r'], $color['g'], $color['b']);
    
    // Use GD built-in fonts for simplicity
    $fontIndex = 5; // Large built-in font
    
    if ($outline) {
        $outlineColor = imagecolorallocate($image, 0, 0, 0);
        // Draw outline
        for ($ox = -1; $ox <= 1; $ox++) {
            for ($oy = -1; $oy <= 1; $oy++) {
                imagestring($image, $fontIndex, $x + $ox, $y + $oy, $text, $outlineColor);
            }
        }
    }
    
    // Draw main text
    imagestring($image, $fontIndex, $x, $y, $text, $textColor);
}

function hexToRgb($hex) {
    $hex = ltrim($hex, '#');
    
    if (strlen($hex) == 3) {
        $r = hexdec(str_repeat(substr($hex, 0, 1), 2));
        $g = hexdec(str_repeat(substr($hex, 1, 1), 2));
        $b = hexdec(str_repeat(substr($hex, 2, 1), 2));
    } else {
        $r = hexdec(substr($hex, 0, 2));
        $g = hexdec(substr($hex, 2, 2));
        $b = hexdec(substr($hex, 4, 2));
    }
    
    return ['r' => $r, 'g' => $g, 'b' => $b];
}

function createThumbnail($imagePath, $maxWidth = 400, $maxHeight = 300) {
    $thumbPath = str_replace('.jpg', '_thumb.jpg', $imagePath);
    
    if (file_exists($thumbPath)) {
        return $thumbPath;
    }
    
    $source = imagecreatefromjpeg($imagePath);
    if (!$source) {
        return false;
    }
    
    $width = imagesx($source);
    $height = imagesy($source);
    
    // Calculate new dimensions
    $ratio = min($maxWidth / $width, $maxHeight / $height);
    $newWidth = intval($width * $ratio);
    $newHeight = intval($height * $ratio);
    
    // Create thumbnail
    $thumb = imagecreatetruecolor($newWidth, $newHeight);
    imagecopyresampled($thumb, $source, 0, 0, 0, 0, $newWidth, $newHeight, $width, $height);
    
    imagejpeg($thumb, $thumbPath, 80);
    
    // GdImage objects auto-cleanup in PHP 8.0+ (imagedestroy deprecated in PHP 8.5)
    
    return $thumbPath;
}
