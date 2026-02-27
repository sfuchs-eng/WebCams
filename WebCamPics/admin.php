<?php
/**
 * Admin Interface
 * Configure camera settings
 */

require_once __DIR__ . '/lib/auth.php';
require_once __DIR__ . '/lib/storage.php';
require_once __DIR__ . '/lib/path.php';

$config = loadConfig();
$message = '';
$messageType = '';

// Handle form submission
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    if (isset($_POST['action'])) {
        switch ($_POST['action']) {
            case 'save_camera':
                $identifier = $_POST['mac'];
                $cameras = loadCamerasConfig();
                
                // Find or create camera entry
                $key = null;
                $identifierNormalized = strtoupper(str_replace(['-', ':', ' '], '', $identifier));
                
                foreach ($cameras as $k => $cam) {
                    if ($k === '_example_') continue;
                    
                    // Check both device_id and mac fields
                    $cameraIdField = isset($cam['device_id']) ? $cam['device_id'] : (isset($cam['mac']) ? $cam['mac'] : '');
                    if (empty($cameraIdField)) continue;
                    
                    $cameraMacNormalized = strtoupper(str_replace(['-', ':', ' '], '', $cameraIdField));
                    if ($cameraMacNormalized === $identifierNormalized) {
                        $key = $k;
                        break;
                    }
                }
                
                if (!$key) {
                    $key = 'cam_' . strtolower(str_replace([':', '-', ' '], '', $identifier));
                }
                
                // Determine if identifier is a MAC address
                $isMac = preg_match('/^[0-9A-Fa-f]{2}[:-]?[0-9A-Fa-f]{2}[:-]?[0-9A-Fa-f]{2}[:-]?[0-9A-Fa-f]{2}[:-]?[0-9A-Fa-f]{2}[:-]?[0-9A-Fa-f]{2}$/', $identifier);
                
                // Always save device_id, and mac field for backward compatibility if it's a MAC
                $cameras[$key] = [
                    'device_id' => $identifier,
                    'location' => $_POST['location'],
                    'title' => $_POST['title'],
                    'status' => $_POST['status'] ?? 'hidden',  // 3-state: disabled, hidden, enabled
                    'rotate' => intval($_POST['rotate']),
                    'add_timestamp' => isset($_POST['add_timestamp']),
                    'add_title' => isset($_POST['add_title']),
                    'font_size' => intval($_POST['font_size']),
                    'font_color' => $_POST['font_color'],
                    'font_outline' => isset($_POST['font_outline'])
                ];
                
                // Add mac field for backward compatibility if it's a MAC address
                if ($isMac) {
                    $cameras[$key]['mac'] = $identifier;
                }
                
                if (saveCamerasConfig($cameras)) {
                    $message = 'Camera configuration saved successfully!';
                    $messageType = 'success';
                } else {
                    $message = 'Failed to save camera configuration.';
                    $messageType = 'error';
                }
                break;
                
            case 'delete_camera':
                $identifier = $_POST['mac'];
                $cameras = loadCamerasConfig();
                
                $identifierNormalized = strtoupper(str_replace(['-', ':', ' '], '', $identifier));
                
                foreach ($cameras as $k => $cam) {
                    if ($k === '_example_') continue;
                    
                    // Check both device_id and mac fields
                    $cameraIdField = isset($cam['device_id']) ? $cam['device_id'] : (isset($cam['mac']) ? $cam['mac'] : '');
                    if (empty($cameraIdField)) continue;
                    
                    $cameraMacNormalized = strtoupper(str_replace(['-', ':', ' '], '', $cameraIdField));
                    if ($cameraMacNormalized === $identifierNormalized) {
                        unset($cameras[$k]);
                        break;
                    }
                }
                
                if (saveCamerasConfig($cameras)) {
                    $message = 'Camera deleted successfully!';
                    $messageType = 'success';
                } else {
                    $message = 'Failed to delete camera.';
                    $messageType = 'error';
                }
                break;
        }
    }
}

// Get all cameras (from filesystem)
$allCameras = getAllCameras();
$camerasConfig = loadCamerasConfig();

// Merge with config
$cameras = [];
foreach ($allCameras as $mac => $cameraData) {
    $cameras[$mac] = getCameraConfig($mac);
}

$editMac = $_GET['edit'] ?? null;
$editCamera = null;
if ($editMac && isset($cameras[$editMac])) {
    $editCamera = $cameras[$editMac];
}
?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Admin - WebCam Viewer</title>
    <link rel="stylesheet" href="<?php echo baseUrl('assets/style.css'); ?>">
</head>
<body>
    <header>
        <h1>⚙️ Camera Administration</h1>
        <nav>
            <a href="<?php echo baseUrl('index.php'); ?>">← Back to Viewer</a>
        </nav>
    </header>
    
    <main class="container">
        <?php if ($message): ?>
            <div class="message <?php echo $messageType; ?>">
                <?php echo htmlspecialchars($message); ?>
            </div>
        <?php endif; ?>
        
        <section class="admin-section">
            <h2>Configuration Info</h2>
            <div class="info-box">
                <p><strong>Upload URL:</strong> <?php echo htmlspecialchars(getUploadUrl()); ?></p>
                <p><strong>Auth Token:</strong> <code><?php echo htmlspecialchars($config['auth_token']); ?></code></p>
                <p><strong>Image Retention:</strong> <?php echo $config['image_retention_days']; ?> days</p>
            </div>
        </section>
        
        <section class="admin-section">
            <h2>Detected Cameras</h2>
            
            <?php if (empty($cameras)): ?>
                <p>No cameras detected yet. Cameras will appear here after sending their first image.</p>
            <?php else: ?>
                <div class="camera-list">
                    <?php foreach ($cameras as $mac => $camera): ?>
                        <?php 
                            $status = $camera['status'] ?? 'enabled';
                            $statusLabels = [
                                'disabled' => '⊗ Disabled',
                                'hidden' => '◐ Hidden',
                                'enabled' => '✓ Enabled'
                            ];
                            $statusClasses = [
                                'disabled' => 'status-disabled',
                                'hidden' => 'status-hidden',
                                'enabled' => 'status-enabled'
                            ];
                        ?>
                        <div class="camera-item">
                            <div class="camera-item-header">
                                <h3><?php echo htmlspecialchars($camera['title']); ?></h3>
                                <span class="<?php echo $statusClasses[$status]; ?>">
                                    <?php echo $statusLabels[$status]; ?>
                                </span>
                            </div>
                            <div class="camera-item-details">
                                <p><strong>Identifier:</strong> <?php echo htmlspecialchars(isset($camera['device_id']) ? $camera['device_id'] : $camera['mac']); ?></p>
                                <p><strong>Location:</strong> <?php echo htmlspecialchars($camera['location']); ?></p>
                                <p><strong>Rotation:</strong> <?php echo $camera['rotate']; ?>°</p>
                                <p><strong>Text Overlay:</strong> 
                                    <?php echo $camera['add_title'] ? '✓ Title' : '✗ Title'; ?>, 
                                    <?php echo $camera['add_timestamp'] ? '✓ Timestamp' : '✗ Timestamp'; ?>
                                </p>
                            </div>
                            <div class="camera-item-actions">
                                <a href="<?php echo baseUrl('admin.php?edit=' . urlencode($mac)); ?>" class="btn btn-small">Edit</a>
                                <form method="POST" style="display: inline;" onsubmit="return confirm('Delete this camera configuration?');">
                                    <input type="hidden" name="action" value="delete_camera">
                                    <input type="hidden" name="mac" value="<?php echo htmlspecialchars(isset($camera['device_id']) ? $camera['device_id'] : $camera['mac']); ?>">
                                    <button type="submit" class="btn btn-small btn-danger">Delete</button>
                                </form>
                            </div>
                        </div>
                    <?php endforeach; ?>
                </div>
            <?php endif; ?>
        </section>
        
        <?php if ($editCamera): ?>
        <section class="admin-section edit-form">
            <h2>Edit Camera: <?php echo htmlspecialchars($editCamera['title']); ?></h2>
            
            <form method="POST">
                <input type="hidden" name="action" value="save_camera">
                <input type="hidden" name="mac" value="<?php echo htmlspecialchars(isset($editCamera['device_id']) ? $editCamera['device_id'] : $editCamera['mac']); ?>">
                
                <div class="form-group">
                    <label for="title">Camera Title:</label>
                    <input type="text" id="title" name="title" value="<?php echo htmlspecialchars($editCamera['title']); ?>" required>
                </div>
                
                <div class="form-group">
                    <label for="location">Location:</label>
                    <select id="location" name="location" required>
                        <?php foreach ($config['locations'] as $locId => $loc): ?>
                            <option value="<?php echo htmlspecialchars($locId); ?>" 
                                    <?php echo $locId === $editCamera['location'] ? 'selected' : ''; ?>>
                                <?php echo htmlspecialchars($loc['title']); ?>
                            </option>
                        <?php endforeach; ?>
                    </select>
                </div>
                
                <div class="form-group">
                    <label for="status">Camera Status:</label>
                    <select id="status" name="status" required>
                        <?php 
                            $currentStatus = $editCamera['status'] ?? 'enabled';
                        ?>
                        <option value="disabled" <?php echo $currentStatus === 'disabled' ? 'selected' : ''; ?>>
                            ⊗ Disabled - Images discarded (not stored)
                        </option>
                        <option value="hidden" <?php echo $currentStatus === 'hidden' ? 'selected' : ''; ?>>
                            ◐ Hidden - Images stored but not shown to visitors
                        </option>
                        <option value="enabled" <?php echo $currentStatus === 'enabled' ? 'selected' : ''; ?>>
                            ✓ Enabled - Full functionality (stored and visible)
                        </option>
                    </select>
                </div>
                
                <div class="form-group">
                    <label for="rotate">Rotation (degrees):</label>
                    <select id="rotate" name="rotate">
                        <option value="0" <?php echo $editCamera['rotate'] == 0 ? 'selected' : ''; ?>>0°</option>
                        <option value="90" <?php echo $editCamera['rotate'] == 90 ? 'selected' : ''; ?>>90°</option>
                        <option value="180" <?php echo $editCamera['rotate'] == 180 ? 'selected' : ''; ?>>180°</option>
                        <option value="270" <?php echo $editCamera['rotate'] == 270 ? 'selected' : ''; ?>>270°</option>
                    </select>
                </div>
                
                <div class="form-group">
                    <label>
                        <input type="checkbox" name="add_title" <?php echo $editCamera['add_title'] ? 'checked' : ''; ?>>
                        Add Title Overlay
                    </label>
                </div>
                
                <div class="form-group">
                    <label>
                        <input type="checkbox" name="add_timestamp" <?php echo $editCamera['add_timestamp'] ? 'checked' : ''; ?>>
                        Add Timestamp Overlay
                    </label>
                </div>
                
                <div class="form-group">
                    <label for="font_size">Font Size:</label>
                    <input type="number" id="font_size" name="font_size" 
                           value="<?php echo $editCamera['font_size']; ?>" min="10" max="30">
                </div>
                
                <div class="form-group">
                    <label for="font_color">Font Color:</label>
                    <input type="color" id="font_color" name="font_color" 
                           value="<?php echo $editCamera['font_color']; ?>">
                </div>
                
                <div class="form-group">
                    <label>
                        <input type="checkbox" name="font_outline" <?php echo $editCamera['font_outline'] ? 'checked' : ''; ?>>
                        Add Text Outline (for better visibility)
                    </label>
                </div>
                
                <div class="form-actions">
                    <button type="submit" class="btn btn-primary">Save Changes</button>
                    <a href="<?php echo baseUrl('admin.php'); ?>" class="btn">Cancel</a>
                </div>
            </form>
        </section>
        <?php endif; ?>
    </main>
    
    <footer>
        <p><a href="<?php echo baseUrl('index.php'); ?>">← Back to Viewer</a></p>
    </footer>
</body>
</html>
