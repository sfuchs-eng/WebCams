<?php
/**
 * Test script for sanitizeCameraIdentifier() function
 */

require_once __DIR__ . '/lib/storage.php';

echo "==================================="  . PHP_EOL;
echo "  Camera Identifier Sanitization Test" . PHP_EOL;
echo "==================================="  . PHP_EOL . PHP_EOL;

// Test cases
$testCases = [
    // [input, description]
    ['AA:BB:CC:DD:EE:FF', 'MAC address (standard)'],
    ['test_cam', 'Short name'],
    ['ab', 'Too short (2 chars)'],
    ['x', 'Too short (1 char)'],
    ['', 'Empty string'],
    ['a very long camera name that exceeds the maximum allowed length for directory names and should be truncated', 'Very long name (>64 chars)'],
    ['Special!@#$%Chars', 'Special characters'],
    ['camera-01', 'Hyphenated name'],
    ['camera.location.01', 'Dotted name'],
    ['123', 'Numeric only (min length)'],
    ['front_door_camera', 'Normal name'],
    ['αβγδε', 'Unicode characters'],
    ['../../blah', 'Path traversal attempt'],
];

echo "Testing sanitizeCameraIdentifier() function:\n";
echo str_repeat('-', 80) . PHP_EOL . PHP_EOL;

foreach ($testCases as $test) {
    list($input, $description) = $test;
    $result = sanitizeCameraIdentifier($input);
    
    echo "Input:       " . var_export($input, true) . PHP_EOL;
    echo "Description: $description" . PHP_EOL;
    echo "Result:      " . var_export($result, true) . PHP_EOL;
    
    if ($result !== false) {
        echo "Length:      " . strlen($result) . " chars" . PHP_EOL;
        echo "Valid:       " . (strlen($result) >= 3 && strlen($result) <= 64 ? 'YES' : 'NO') . PHP_EOL;
    } else {
        echo "Valid:       NO (returned false)" . PHP_EOL;
    }
    
    echo str_repeat('-', 80) . PHP_EOL . PHP_EOL;
}

echo "Test completed!" . PHP_EOL;
