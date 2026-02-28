<?php
/**
 * Admin Interface
 * Configure camera settings
 */

require_once __DIR__ . '/lib/auth.php';
require_once __DIR__ . '/lib/storage.php';
require_once __DIR__ . '/lib/path.php';
require_once __DIR__ . '/lib/fab-menu.php';

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
    <main class="container">
        <div class="page-title">
            <h1>⚙️ Camera Administration</h1>
        </div>
        
        <?php if ($message): ?>
            <div class="message <?php echo $messageType; ?>">
                <?php echo htmlspecialchars($message); ?>
            </div>
        <?php endif; ?>
        
        <section class="admin-section">
            <h2>Configuration Info</h2>
            <div class="info-box">
                <p><strong>Upload URL:</strong> <?php echo htmlspecialchars(getUploadUrl()); ?></p>
                <p><strong>Auth Token:</strong> <code><?php echo htmlspecialchars($config['auth_tokens'][0] ?? 'not_configured'); ?></code></p>
                <p><strong>Image Retention:</strong> <?php echo $config['image_retention_days']; ?> days</p>
            </div>
        </section>
        
        <section class="admin-section">
            <h2>Cameras</h2>
            
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
                                <button type="button" class="btn btn-small" onclick="openEditModal('<?php echo htmlspecialchars($mac, ENT_QUOTES); ?>')">Edit</button>
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
        
        <!-- Edit Camera Modal -->
        <div id="editModal" class="modal">
            <div class="modal-content">
                <div class="modal-header">
                    <h2 id="modalTitle">Edit Camera</h2>
                    <button type="button" class="modal-close" onclick="closeEditModal()">&times;</button>
                </div>
                
                <form method="POST" id="editCameraForm">
                    <input type="hidden" name="action" value="save_camera">
                    <input type="hidden" name="mac" id="edit_mac">
                    
                    <div class="form-group">
                        <label for="edit_title">Camera Title:</label>
                        <input type="text" id="edit_title" name="title" required>
                    </div>
                    
                    <div class="form-group">
                        <label for="edit_location">Location:</label>
                        <select id="edit_location" name="location" required>
                            <?php foreach ($config['locations'] as $locId => $loc): ?>
                                <option value="<?php echo htmlspecialchars($locId); ?>">
                                    <?php echo htmlspecialchars($loc['title']); ?>
                                </option>
                            <?php endforeach; ?>
                        </select>
                    </div>
                    
                    <div class="form-group">
                        <label for="edit_status">Camera Status:</label>
                        <select id="edit_status" name="status" required>
                            <option value="disabled">⊗ Disabled - Images discarded (not stored)</option>
                            <option value="hidden">◐ Hidden - Images stored but not shown to visitors</option>
                            <option value="enabled">✓ Enabled - Full functionality (stored and visible)</option>
                        </select>
                    </div>
                    
                    <div class="form-group">
                        <label for="edit_rotate">Rotation (degrees):</label>
                        <select id="edit_rotate" name="rotate">
                            <option value="0">0°</option>
                            <option value="90">90°</option>
                            <option value="180">180°</option>
                            <option value="270">270°</option>
                        </select>
                    </div>
                    
                    <div class="form-group">
                        <label>
                            <input type="checkbox" name="add_title" id="edit_add_title">
                            Add Title Overlay
                        </label>
                    </div>
                    
                    <div class="form-group">
                        <label>
                            <input type="checkbox" name="add_timestamp" id="edit_add_timestamp">
                            Add Timestamp Overlay
                        </label>
                    </div>
                    
                    <div class="form-group">
                        <label for="edit_font_size">Font Size:</label>
                        <input type="number" id="edit_font_size" name="font_size" min="10" max="30">
                    </div>
                    
                    <div class="form-group">
                        <label for="edit_font_color">Font Color:</label>
                        <input type="color" id="edit_font_color" name="font_color">
                    </div>
                    
                    <div class="form-group">
                        <label>
                            <input type="checkbox" name="font_outline" id="edit_font_outline">
                            Add Text Outline (for better visibility)
                        </label>
                    </div>
                    
                    <div class="form-actions">
                        <button type="submit" class="btn btn-primary">Save Changes</button>
                        <button type="button" class="btn" onclick="closeEditModal()">Cancel</button>
                    </div>
                </form>
            </div>
        </div>
    </main>
    
    <?php renderFabMenu(); ?>
    
    <script>
        // Camera data for modal
        const cameraData = <?php echo json_encode($cameras); ?>;
        
        function openEditModal(mac) {
            const camera = cameraData[mac];
            if (!camera) return;
            
            // Populate form fields
            document.getElementById('modalTitle').textContent = 'Edit Camera: ' + camera.title;
            document.getElementById('edit_mac').value = camera.device_id || camera.mac;
            document.getElementById('edit_title').value = camera.title;
            document.getElementById('edit_location').value = camera.location;
            document.getElementById('edit_status').value = camera.status || 'enabled';
            document.getElementById('edit_rotate').value = camera.rotate;
            document.getElementById('edit_add_title').checked = camera.add_title;
            document.getElementById('edit_add_timestamp').checked = camera.add_timestamp;
            document.getElementById('edit_font_size').value = camera.font_size;
            document.getElementById('edit_font_color').value = camera.font_color;
            document.getElementById('edit_font_outline').checked = camera.font_outline;
            
            // Show modal
            document.getElementById('editModal').classList.add('active');
            document.body.style.overflow = 'hidden';
        }
        
        function closeEditModal() {
            document.getElementById('editModal').classList.remove('active');
            document.body.style.overflow = '';
        }
        
        // Close modal on ESC key
        document.addEventListener('keydown', function(e) {
            if (e.key === 'Escape') {
                closeEditModal();
            }
        });
        
        // Close modal on background click
        document.getElementById('editModal').addEventListener('click', function(e) {
            if (e.target === this) {
                closeEditModal();
            }
        });
    </script>
</body>
</html>
