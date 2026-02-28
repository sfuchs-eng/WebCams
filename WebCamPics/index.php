<?php
/**
 * Main Viewer Interface
 * Displays latest images from all cameras grouped by location
 */

require_once __DIR__ . '/lib/auth.php';
require_once __DIR__ . '/lib/storage.php';
require_once __DIR__ . '/lib/path.php';
require_once __DIR__ . '/lib/fab-menu.php';

$config = loadConfig();
$cameras = getAllCameras();
$camerasConfig = loadCamerasConfig();

// Build flat array of enabled cameras
$enabledCameras = [];
foreach ($cameras as $mac => $cameraData) {
    $camConfig = getCameraConfig($mac);
    
    // Only show cameras with status 'enabled' in public view
    $status = $camConfig['status'] ?? 'enabled';
    if ($status !== 'enabled') {
        continue;
    }
    
    $enabledCameras[] = array_merge($cameraData, $camConfig);
}

// Sort cameras by title
usort($enabledCameras, function($a, $b) {
    return strcasecmp($a['title'], $b['title']);
});
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WebCams</title>
    <link rel="stylesheet" href="<?php echo baseUrl('assets/style.css'); ?>">
</head>
<body>
    <main class="container">
        <?php if (empty($enabledCameras)): ?>
            <div class="empty-state">
                <h2>No cameras found</h2>
                <p>Waiting for cameras to send their first image...</p>
            </div>
        <?php else: ?>
            <div class="camera-grid">
                <?php foreach ($enabledCameras as $camera): ?>
                    <div class="camera-card">
                        <div class="camera-title">
                            <?php echo htmlspecialchars($camera['title']); ?>
                        </div>
                        <a href="<?php echo baseUrl('location.php?location=' . urlencode($camera['location']) . '&camera=' . urlencode($camera['mac'])); ?>">
                            <img src="<?php echo htmlspecialchars($camera['latest_image']); ?>" 
                                 alt="<?php echo htmlspecialchars($camera['title']); ?>"
                                 loading="lazy">
                        </a>
                        <div class="camera-info">
                            <span class="timestamp">
                                <?php echo date('M d, Y H:i:s', $camera['latest_timestamp']); ?>
                            </span>
                            <span class="mac">
                                <?php echo htmlspecialchars(substr($camera['mac'], -8)); ?>
                            </span>
                        </div>
                    </div>
                <?php endforeach; ?>
            </div>
        <?php endif; ?>
    </main>
    
    <?php renderFabMenu(); ?>
    
    <script>
        // Auto-refresh every 5 minutes
        setTimeout(function() {
            location.reload();
        }, 300000);
    </script>
</body>
</html>
