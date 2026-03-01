<?php
/**
 * Admin Interface
 * Configure camera settings
 */

require_once __DIR__ . '/lib/auth.php';
require_once __DIR__ . '/lib/storage.php';
require_once __DIR__ . '/lib/path.php';
require_once __DIR__ . '/lib/fab-menu.php';
require_once __DIR__ . '/lib/ota.php';

$config = loadConfig();
$message = '';
$messageType = '';

// Handle success messages from redirects
if (isset($_GET['msg'])) {
    switch ($_GET['msg']) {
        case 'saved':
            $message = 'Camera configuration saved successfully!';
            $messageType = 'success';
            break;
        case 'deleted':
            $message = 'Camera deleted successfully!';
            $messageType = 'success';
            break;
        case 'purged':
            $message = 'Camera images purged successfully!';
            $messageType = 'success';
            break;
        case 'ota_scheduled':
            $message = 'OTA update scheduled successfully!';
            $messageType = 'success';
            break;
        case 'ota_cleared':
            $message = 'OTA schedule cleared!';
            $messageType = 'success';
            break;
        case 'ota_error':
            $message = 'Failed to schedule OTA update.';
            $messageType = 'error';
            break;
        case 'ota_retry_reset':
            $message = 'OTA retry counter reset successfully!';
            $messageType = 'success';
            break;
    }
}

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
                
                $saveSuccess = saveCamerasConfig($cameras);
                
                // Handle OTA firmware scheduling
                if ($saveSuccess && isset($_POST['ota_firmware'])) {
                    $otaFirmware = trim($_POST['ota_firmware']);
                    if (empty($otaFirmware)) {
                        // Clear OTA schedule
                        clearOtaSchedule($identifier);
                    } else {
                        // Schedule OTA update
                        scheduleOtaUpdate($identifier, $otaFirmware);
                    }
                }
                
                if ($saveSuccess) {
                    header('Location: ' . baseUrl('admin.php?msg=saved'));
                    exit;
                } else {
                    $message = 'Failed to save camera configuration.';
                    $messageType = 'error';
                }
                break;
                
            case 'delete_camera':
                $identifier = $_POST['mac'];
                $cameras = loadCamerasConfig();
                
                $identifierNormalized = strtoupper(str_replace(['-', ':', ' '], '', $identifier));
                $found = false;
                $keyToDelete = null;
                
                foreach ($cameras as $k => $cam) {
                    if ($k === '_example_') continue;
                    
                    // Check both device_id and mac fields
                    $cameraIdField = isset($cam['device_id']) ? $cam['device_id'] : (isset($cam['mac']) ? $cam['mac'] : '');
                    if (empty($cameraIdField)) continue;
                    
                    $cameraMacNormalized = strtoupper(str_replace(['-', ':', ' '], '', $cameraIdField));
                    if ($cameraMacNormalized === $identifierNormalized) {
                        $keyToDelete = $k;
                        $found = true;
                        break;
                    }
                }
                
                if ($found && $keyToDelete !== null) {
                    // Remove from config
                    unset($cameras[$keyToDelete]);
                    
                    // Save updated config
                    if (saveCamerasConfig($cameras)) {
                        // Delete all images and directory for this camera
                        deleteCameraImages($identifier);
                        
                        header('Location: ' . baseUrl('admin.php?msg=deleted'));
                        exit;
                    } else {
                        $message = 'Failed to save camera configuration after deletion.';
                        $messageType = 'error';
                    }
                } else {
                    $message = 'Camera not found in configuration.';
                    $messageType = 'error';
                }
                break;
                
            case 'purge_images':
                $identifier = $_POST['mac'];
                
                // Delete all images for this camera
                $success = deleteCameraImages($identifier);
                
                // Handle AJAX request
                if (isset($_SERVER['HTTP_X_REQUESTED_WITH']) && 
                    strtolower($_SERVER['HTTP_X_REQUESTED_WITH']) === 'xmlhttprequest') {
                    header('Content-Type: application/json');
                    if ($success) {
                        echo json_encode(['success' => true, 'message' => 'Images purged successfully']);
                    } else {
                        echo json_encode(['success' => false, 'message' => 'Failed to purge images']);
                    }
                    exit;
                }
                
                // Fallback to redirect for non-AJAX requests
                header('Location: ' . baseUrl('admin.php?msg=purged'));
                exit;
                break;
                
            case 'schedule_ota':
                $identifier = $_POST['mac'];
                $firmwareFile = $_POST['firmware_file'] ?? '';
                
                if (empty($firmwareFile)) {
                    $success = clearOtaSchedule($identifier);
                    $msg = $success ? 'ota_cleared' : 'ota_error';
                } else {
                    $success = scheduleOtaUpdate($identifier, $firmwareFile);
                    $msg = $success ? 'ota_scheduled' : 'ota_error';
                }
                
                // Handle AJAX request
                if (isset($_SERVER['HTTP_X_REQUESTED_WITH']) && 
                    strtolower($_SERVER['HTTP_X_REQUESTED_WITH']) === 'xmlhttprequest') {
                    header('Content-Type: application/json');
                    echo json_encode(['success' => $success]);
                    exit;
                }
                
                header('Location: ' . baseUrl('admin.php?msg=' . $msg));
                exit;
                break;
                
            case 'reset_ota_retry':
                $identifier = $_POST['mac'];
                $success = resetOtaRetryCount($identifier);
                
                // Handle AJAX request
                if (isset($_SERVER['HTTP_X_REQUESTED_WITH']) && 
                    strtolower($_SERVER['HTTP_X_REQUESTED_WITH']) === 'xmlhttprequest') {
                    header('Content-Type: application/json');
                    echo json_encode(['success' => $success]);
                    exit;
                }
                
                $msg = $success ? 'ota_retry_reset' : 'ota_error';
                header('Location: ' . baseUrl('admin.php?msg=' . $msg));
                exit;
                break;
        }
    }
}

// Get all cameras from config (not just those with images)
$camerasConfig = loadCamerasConfig();
$cameras = [];

foreach ($camerasConfig as $key => $cameraData) {
    if ($key === '_example_') continue;
    
    // Get the identifier (device_id or mac)
    $identifier = isset($cameraData['device_id']) ? $cameraData['device_id'] : $cameraData['mac'];
    
    // Get full config and add image count
    $camera = getCameraConfig($identifier);
    $camera['image_count'] = countCameraImages($identifier);
    
    $cameras[$identifier] = $camera;
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
        <div class="page-title" style="display: flex; justify-content: space-between; align-items: center;">
            <h1>⚙️ Camera Administration</h1>
            <div style="display: flex; gap: 15px; align-items: center;">
                <a href="<?php echo baseUrl('ota-upload.php'); ?>" class="btn" style="background: #007bff; color: white; padding: 10px 20px; text-decoration: none; border-radius: 5px; font-size: 0.9em;" title="Manage Firmware">🔄 Firmware</a>
                <a href="<?php echo baseUrl('index.php'); ?>" class="home-button" style="font-size: 2em; text-decoration: none;" title="Back to Home">&#x1F3E0;</a>
            </div>
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
                                <p><strong>No images:</strong> <span id="img-count-<?php echo htmlspecialchars($mac); ?>"><?php echo $camera['image_count'] ?? 0; ?></span></p>
                                <p><strong>Firmware:</strong> <?php echo htmlspecialchars($camera['firmware_version'] ?? 'Unknown'); ?></p>
                                <?php if (!empty($camera['ota_scheduled'])): ?>
                                    <p><strong>OTA Update:</strong> 
                                        <span style="color: #007bff;">⏳ Pending: <?php echo htmlspecialchars($camera['ota_scheduled']); ?></span>
                                        <?php if (!empty($camera['ota_retry_count'])): ?>
                                            <span style="color: #ff9800;"> (Attempt <?php echo ($camera['ota_retry_count'] + 1); ?>/2)</span>
                                        <?php endif; ?>
                                    </p>
                                <?php elseif (!empty($camera['ota_last_status']) && $camera['ota_last_status'] === 'failed' && !empty($camera['ota_retry_count']) && $camera['ota_retry_count'] >= 2): ?>
                                    <p><strong>OTA Status:</strong> <span style="color: #dc3545;">🚫 Failed (<?php echo $camera['ota_retry_count']; ?>/2 retries)</span></p>
                                <?php elseif (!empty($camera['ota_last_status']) && $camera['ota_last_status'] === 'success'): ?>
                                    <p><strong>Last OTA:</strong> <span style="color: #28a745;">✅ Success</span></p>
                                <?php elseif (!empty($camera['ota_last_status']) && $camera['ota_last_status'] === 'rollback'): ?>
                                    <p><strong>OTA Status:</strong> <span style="color: #ff9800;">⚠️ Rolled back</span></p>
                                <?php endif; ?>
                                <p><strong></strong>Rotation:</strong> <?php echo $camera['rotate']; ?>°</p>
                                <p><strong>Text Overlay:</strong> 
                                    <?php echo $camera['add_title'] ? '✓ Title' : '✗ Title'; ?>, 
                                    <?php echo $camera['add_timestamp'] ? '✓ Timestamp' : '✗ Timestamp'; ?>
                                </p>
                            </div>
                            <div class="camera-item-actions">
                                <button type="button" class="btn btn-small" onclick="openEditModal('<?php echo htmlspecialchars($mac, ENT_QUOTES); ?>')">Edit</button>
                                <button type="button" class="btn btn-small btn-warning" onclick="purgeImages('<?php echo htmlspecialchars($mac, ENT_QUOTES); ?>', '<?php echo htmlspecialchars(isset($camera['device_id']) ? $camera['device_id'] : $camera['mac'], ENT_QUOTES); ?>')">Purge img</button>
                                <form method="POST" style="display: inline;" onsubmit="return confirm('Delete this camera configuration and all images?');">
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
                    
                    <fieldset class="ota-management" style="margin-top: 20px; padding: 15px; border: 2px solid #007bff; border-radius: 5px;">
                        <legend style="font-weight: bold; color: #007bff;">🔄 OTA Firmware Update</legend>
                        
                        <div class="form-group">
                            <label>Current Firmware:</label>
                            <p id="edit_firmware_version" style="margin: 5px 0; font-family: monospace;">Unknown</p>
                        </div>
                        
                        <div class="form-group">
                            <label>Update Status:</label>
                            <p id="edit_ota_status" style="margin: 5px 0;">No updates scheduled</p>
                        </div>
                        
                        <div class="form-group">
                            <label for="edit_ota_firmware">Schedule Update:</label>
                     <select id="edit_ota_firmware" name="ota_firmware">
                                <option value="">-- No update scheduled --</option>
                                <?php 
                                $availableFirmware = getAvailableFirmware();
                                foreach ($availableFirmware as $fw): 
                                    $size = number_format($fw['size'] / 1024 / 1024, 2);
                                ?>
                                    <option value="<?php echo htmlspecialchars($fw['filename']); ?>">
                                        v<?php echo htmlspecialchars($fw['version']); ?> (<?php echo $size; ?> MB)
                                    </option>
                                <?php endforeach; ?>
                            </select>
                            <small style="display: block; margin-top: 5px; color: #666;">Camera will download and install on next image upload</small>
                        </div>
                        
                        <button type="button" class="btn" onclick="clearOtaSchedule()" style="background: #dc3545; color: white;">Clear Scheduled Update</button>
                    </fieldset>
                    
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
            
            // Populate OTA info
            document.getElementById('edit_firmware_version').textContent = camera.firmware_version || 'Unknown';
            document.getElementById('edit_ota_firmware').value = camera.ota_scheduled || '';
            
            // Update OTA status display
            let statusHtml = 'No updates scheduled';
            const retryCount = camera.ota_retry_count || 0;
            
            if (camera.ota_scheduled) {
                statusHtml = '⏳ Pending: ' + camera.ota_scheduled;
                if (retryCount > 0) {
                    statusHtml += ' <span style="color: #ff9800;">(Attempt ' + (retryCount + 1) + '/2)</span>';
                }
                if (camera.ota_last_status === 'failed' && camera.ota_last_error) {
                    statusHtml += '<br><span style="color: #dc3545;">❌ Last attempt failed: ' + camera.ota_last_error + '</span>';
                }
            } else if (retryCount >= 2 && camera.ota_last_status === 'failed') {
                statusHtml = '<span style="color: #dc3545;">🚫 Max retries reached (' + retryCount + '/2)</span>';
                if (camera.ota_last_error) {
                    statusHtml += '<br>' + camera.ota_last_error;
                }
                statusHtml += '<br><button type="button" class="btn" onclick="resetOtaRetryCount()" style="margin-top: 10px; background: #ff9800; color: white; font-size: 0.85em;">Reset Retry Counter</button>';
            } else if (camera.ota_last_status === 'success' && camera.ota_last_attempt) {
                const attemptDate = new Date(camera.ota_last_attempt);
                statusHtml = '✅ Last update successful (' + attemptDate.toLocaleString() + ')';
            } else if (camera.ota_last_status === 'rollback') {
                statusHtml = '⚠️ Last update rolled back';
                if (camera.ota_last_error) {
                    statusHtml += ': ' + camera.ota_last_error;
                }
            }
            document.getElementById('edit_ota_status').innerHTML = statusHtml;
            
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
        
        // Purge images via AJAX
        function purgeImages(displayMac, identifier) {
            if (!confirm('Purge all stored images for this camera? The camera configuration will remain.')) {
                return;
            }
            
            const formData = new FormData();
            formData.append('action', 'purge_images');
            formData.append('mac', identifier);
            
            fetch(window.location.href, {
                method: 'POST',
                headers: {
                    'X-Requested-With': 'XMLHttpRequest'
                },
                body: formData
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    // Update image count display
                    const countElement = document.getElementById('img-count-' + displayMac);
                    if (countElement) {
                        countElement.textContent = '0';
                    }
                } else {
                    alert('Failed to purge images: ' + (data.message || 'Unknown error'));
                }
            })
            .catch(error => {
                console.error('Error:', error);
                alert('Failed to purge images: ' + error);
            });
        }
        
        // Clear OTA schedule
        function clearOtaSchedule() {
            const mac = document.getElementById('edit_mac').value;
            
            if (!confirm('Clear the scheduled OTA update for this camera?')) {
                return;
            }
            
            const formData = new FormData();
            formData.append('action', 'schedule_ota');
            formData.append('mac', mac);
            formData.append('firmware_file', '');  // Empty = clear schedule
            
            fetch(window.location.href, {
                method: 'POST',
                headers: {
                    'X-Requested-With': 'XMLHttpRequest'
                },
                body: formData
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    alert('OTA schedule cleared successfully!');
                    location.reload();
                } else {
                    alert('Failed to clear OTA schedule');
                }
            })
            .catch(error => {
                console.error('Error:', error);
                alert('Failed to clear OTA schedule: ' + error);
            });
        }
        
        // Reset OTA retry counter
        function resetOtaRetryCount() {
            const mac = document.getElementById('edit_mac').value;
            
            if (!confirm('Reset the OTA retry counter for this camera? This will allow reattempting the failed firmware update.')) {
                return;
            }
            
            const formData = new FormData();
            formData.append('action', 'reset_ota_retry');
            formData.append('mac', mac);
            
            fetch(window.location.href, {
                method: 'POST',
                headers: {
                    'X-Requested-With': 'XMLHttpRequest'
                },
                body: formData
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    alert('Retry counter reset successfully! You can now schedule a new OTA update.');
                    location.reload();
                } else {
                    alert('Failed to reset retry counter');
                }
            })
            .catch(error => {
                console.error('Error:', error);
                alert('Failed to reset retry counter: ' + error);
            });
        }
        
        // Note: OTA scheduling is now handled server-side as part of the save_camera action
    </script>
</body>
</html>
