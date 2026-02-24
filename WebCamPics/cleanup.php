<?php
/**
 * Cleanup Script
 * Removes images older than retention period
 * 
 * Usage: php cleanup.php
 * Or via cron: 0 3 * * * /usr/bin/php /path/to/cleanup.php
 */

require_once __DIR__ . '/lib/auth.php';
require_once __DIR__ . '/lib/storage.php';

echo "=== WebCam Image Cleanup ===\n";
echo "Started: " . date('Y-m-d H:i:s') . "\n\n";

$config = loadConfig();
$retentionDays = $config['image_retention_days'] ?? 14;

echo "Retention period: {$retentionDays} days\n";
echo "Cleaning up images older than " . date('Y-m-d H:i:s', time() - ($retentionDays * 24 * 60 * 60)) . "...\n\n";

$deleted = cleanupOldImages($retentionDays);

echo "Cleanup complete!\n";
echo "Files deleted: {$deleted}\n";
echo "Finished: " . date('Y-m-d H:i:s') . "\n";

// Log the cleanup
$logEntry = sprintf(
    "[%s] Cleanup: %d files deleted (retention: %d days)\n",
    date('Y-m-d H:i:s'),
    $deleted,
    $retentionDays
);
file_put_contents(__DIR__ . '/logs/cleanup.log', $logEntry, FILE_APPEND);
