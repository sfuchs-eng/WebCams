<?php
/**
 * Main Viewer Interface
 * Displays latest images from all cameras grouped by location
 */

require_once __DIR__ . '/lib/auth.php';
require_once __DIR__ . '/lib/storage.php';
require_once __DIR__ . '/lib/path.php';

$config = loadConfig();
$cameras = getAllCameras();
$camerasConfig = loadCamerasConfig();

// Group cameras by location (only show enabled cameras)
$locations = [];
foreach ($cameras as $mac => $cameraData) {
    $camConfig = getCameraConfig($mac);
    
    // Only show cameras with status 'enabled' in public view
    $status = $camConfig['status'] ?? 'enabled';
    if ($status !== 'enabled') {
        continue;
    }
    
    $location = $camConfig['location'];
    
    if (!isset($locations[$location])) {
        $locations[$location] = [
            'cameras' => [],
            'title' => $config['locations'][$location]['title'] ?? ucfirst($location),
            'description' => $config['locations'][$location]['description'] ?? ''
        ];
    }
    
    $locations[$location]['cameras'][] = array_merge($cameraData, $camConfig);
}

// Sort locations
ksort($locations);
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WebCam Viewer</title>
    <link rel="stylesheet" href="<?php echo baseUrl('assets/style.css'); ?>">
</head>
<body>
    <header>
        <h1>📷 WebCam Viewer</h1>
        <nav>
            <a href="<?php echo baseUrl('index.php'); ?>">Home</a>
            <a href="<?php echo baseUrl('admin.php'); ?>">Settings</a>
        </nav>
    </header>
    
    <main class="container">
        <?php if (empty($locations)): ?>
            <div class="empty-state">
                <h2>No cameras found</h2>
                <p>Waiting for cameras to send their first image...</p>
            </div>
        <?php else: ?>
            <?php foreach ($locations as $locationId => $location): ?>
                <section class="location-section">
                    <div class="location-header">
                        <h2><?php echo htmlspecialchars($location['title']); ?></h2>
                        <?php if ($location['description']): ?>
                            <p class="location-description"><?php echo htmlspecialchars($location['description']); ?></p>
                        <?php endif; ?>
                        <a href="<?php echo baseUrl('location.php?location=' . urlencode($locationId)); ?>" class="view-history">View History →</a>
                    </div>
                    
                    <div class="camera-grid">
                        <?php foreach ($location['cameras'] as $camera): ?>
                            <div class="camera-card">
                                <div class="camera-title">
                                    <?php echo htmlspecialchars($camera['title']); ?>
                                </div>
                                <a href="<?php echo baseUrl('location.php?location=' . urlencode($locationId) . '&camera=' . urlencode($camera['mac'])); ?>">
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
                </section>
            <?php endforeach; ?>
        <?php endif; ?>
    </main>
    
    <footer>
        <p>Last updated: <?php echo date('Y-m-d H:i:s'); ?></p>
        <p>
            <a href="javascript:location.reload()">Refresh</a> | 
            <a href="<?php echo baseUrl('admin.php'); ?>">Admin</a>
        </p>
    </footer>
    
    <script>
        // Auto-refresh every 5 minutes
        setTimeout(function() {
            location.reload();
        }, 300000);
    </script>
</body>
</html>
