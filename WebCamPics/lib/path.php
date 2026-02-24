<?php
/**
 * Path and URL utilities
 * Auto-detects application base path for flexible installation
 * 
 * Claude Sonnet 4.5 generated, citing:
 * https://github.com/Rahamut1506/template-editor/blob/c102f119864416ecf64408fa31cfd7fc30834f9b/empty.php
 */

/**
 * Get the base path of the application
 * Auto-detects from script location
 */
function getBasePath() {
    static $basePath = null;
    
    if ($basePath === null) {
        $scriptName = $_SERVER['SCRIPT_NAME'];
        $scriptDir = dirname($scriptName);
        
        // Normalize: remove trailing slashes, handle root case
        $basePath = rtrim($scriptDir, '/');
        
        // If we're at root (dirname returns '/'), basePath should be empty
        if ($basePath === '') {
            $basePath = '';
        }
    }
    
    return $basePath;
}

/**
 * Generate URL relative to application base
 */
function baseUrl($path = '') {
    $basePath = getBasePath();
    $path = ltrim($path, '/');
    
    if ($path === '') {
        return $basePath . '/';
    }
    
    return $basePath . '/' . $path;
}

/**
 * Generate full URL including protocol and host
 */
function fullUrl($path = '') {
    $scheme = isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off' ? 'https' : 'http';
    $host = $_SERVER['HTTP_HOST'];
    $base = baseUrl($path);
    
    return $scheme . '://' . $host . $base;
}

/**
 * Get current URL
 */
function currentUrl() {
    $scheme = isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off' ? 'https' : 'http';
    $host = $_SERVER['HTTP_HOST'];
    $uri = $_SERVER['REQUEST_URI'];
    
    return $scheme . '://' . $host . $uri;
}
