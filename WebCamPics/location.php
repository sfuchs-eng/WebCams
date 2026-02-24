<?php
/**
 * Location Detail View
 * Shows image history for cameras in a specific location
 */

require_once __DIR__ . '/lib/auth.php';
require_once __DIR__ . '/lib/storage.php';
require_once __DIR__ . '/lib/path.php';

$config = loadConfig();
$locationId = $_GET['location'] ?? null;
$selectedCamera = $_GET['camera'] ?? null;
$days = isset($_GET['days']) ? intval($_GET['days']) : 14;

if (!$locationId) {
    header('Location: ' . baseUrl('index.php'));
    exit;
}

// Get all cameras
$allCameras = getAllCameras();
$camerasConfig = loadCamerasConfig();

// Filter cameras by location (only show enabled cameras)
$locationCameras = [];
foreach ($allCameras as $mac => $cameraData) {
    $camConfig = getCameraConfig($mac);
    $status = $camConfig['status'] ?? 'enabled';
    // Only show cameras with status 'enabled' in public view
    if ($camConfig['location'] === $locationId && $status === 'enabled') {
        $locationCameras[$mac] = array_merge($cameraData, $camConfig);
    }
}

if (empty($locationCameras) && !$selectedCamera) {
    header('Location: ' . baseUrl('index.php'));
    exit;
}

$locationTitle = $config['locations'][$locationId]['title'] ?? ucfirst($locationId);

// Get images for selected camera or first camera
if ($selectedCamera && isset($locationCameras[$selectedCamera])) {
    $displayCamera = $selectedCamera;
} else {
    $displayCamera = array_key_first($locationCameras);
}

$images = [];
if ($displayCamera) {
    $images = getCameraImages($displayCamera, $days);
}

$cameraInfo = $displayCamera ? getCameraConfig($displayCamera) : null;
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title><?php echo htmlspecialchars($locationTitle); ?> - WebCam Viewer</title>
    <link rel="stylesheet" href="<?php echo baseUrl('assets/style.css'); ?>">
</head>
<body>
    <header>
        <h1>📷 <?php echo htmlspecialchars($locationTitle); ?></h1>
        <nav>
            <a href="<?php echo baseUrl('index.php'); ?>">← Back to Overview</a>
            <a href="<?php echo baseUrl('admin.php'); ?>">Settings</a>
        </nav>
    </header>
    
    <main class="container">
        <?php if (!empty($locationCameras)): ?>
            <div class="camera-selector">
                <label>Select Camera:</label>
                <div class="camera-tabs">
                    <?php foreach ($locationCameras as $mac => $camera): ?>
                        <a href="<?php echo baseUrl('location.php?location=' . urlencode($locationId) . '&camera=' . urlencode($mac) . '&days=' . $days); ?>" 
                           class="camera-tab <?php echo $mac === $displayCamera ? 'active' : ''; ?>">
                            <?php echo htmlspecialchars($camera['title']); ?>
                        </a>
                    <?php endforeach; ?>
                </div>
            </div>
            
            <div class="time-range-selector">
                <label>Show last:</label>
                <a href="<?php echo baseUrl('location.php?location=' . urlencode($locationId) . '&camera=' . urlencode($displayCamera) . '&days=1'); ?>" 
                   class="time-range <?php echo $days === 1 ? 'active' : ''; ?>">24 hours</a>
                <a href="<?php echo baseUrl('location.php?location=' . urlencode($locationId) . '&camera=' . urlencode($displayCamera) . '&days=3'); ?>" 
                   class="time-range <?php echo $days === 3 ? 'active' : ''; ?>">3 days</a>
                <a href="<?php echo baseUrl('location.php?location=' . urlencode($locationId) . '&camera=' . urlencode($displayCamera) . '&days=7'); ?>" 
                   class="time-range <?php echo $days === 7 ? 'active' : ''; ?>">7 days</a>
                <a href="<?php echo baseUrl('location.php?location=' . urlencode($locationId) . '&camera=' . urlencode($displayCamera) . '&days=14'); ?>" 
                   class="time-range <?php echo $days === 14 ? 'active' : ''; ?>">14 days</a>
            </div>
        <?php endif; ?>
        
        <?php if ($cameraInfo): ?>
            <div class="camera-details">
                <h2><?php echo htmlspecialchars($cameraInfo['title']); ?></h2>
                <p class="camera-mac">MAC: <?php echo htmlspecialchars($cameraInfo['mac']); ?></p>
            </div>
        <?php endif; ?>
        
        <?php if (empty($images)): ?>
            <div class="empty-state">
                <h2>No images found</h2>
                <p>No images available for the selected time range.</p>
            </div>
        <?php else: ?>
            <div class="image-count">
                <?php echo count($images); ?> image(s) found
            </div>
            
            <div class="image-gallery">
                <?php foreach ($images as $image): ?>
                    <div class="gallery-item">
                        <a href="<?php echo htmlspecialchars($image['url']); ?>" target="_blank">
                            <img src="<?php echo htmlspecialchars($image['url']); ?>" 
                                 alt="Image from <?php echo date('Y-m-d H:i:s', $image['timestamp']); ?>"
                                 loading="lazy">
                        </a>
                        <div class="gallery-info">
                            <span class="timestamp">
                                <?php echo date('M d, H:i:s', $image['timestamp']); ?>
                            </span>
                            <span class="size">
                                <?php echo number_format($image['size'] / 1024, 1); ?> KB
                            </span>
                        </div>
                    </div>
                <?php endforeach; ?>
            </div>
        <?php endif; ?>
    </main>
    
    <footer>
        <p><a href="<?php echo baseUrl('index.php'); ?>">← Back to Overview</a></p>
    </footer>
</body>
</html>
