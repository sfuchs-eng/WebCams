<?php
/**
 * Generate a simple test JPEG image
 */

// Create a 640x480 image with a gradient and text
$width = 640;
$height = 480;
$image = imagecreatetruecolor($width, $height);

// Create gradient background
for ($y = 0; $y < $height; $y++) {
    $r = (int)(255 * ($y / $height));
    $g = 100;
    $b = (int)(200 * (1 - $y / $height));
    $color = imagecolorallocate($image, $r, $g, $b);
    imagefilledrectangle($image, 0, $y, $width, $y, $color);
}

// Add text
$white = imagecolorallocate($image, 255, 255, 255);
$black = imagecolorallocate($image, 0, 0, 0);

$text = "TEST IMAGE";
$timestamp = date('Y-m-d H:i:s');

// Add black shadow
imagestring($image, 5, 251, 201, $text, $black);
imagestring($image, 3, 201, 251, $timestamp, $black);

// Add white text
imagestring($image, 5, 250, 200, $text, $white);
imagestring($image, 3, 200, 250, $timestamp, $white);

// Output to file
$outputFile = __DIR__ . '/test_image.jpg';
imagejpeg($image, $outputFile, 85);
// GdImage objects auto-cleanup in PHP 8.0+ (imagedestroy deprecated in PHP 8.5)

echo "Test image created: $outputFile\n";
echo "Size: " . filesize($outputFile) . " bytes\n";
