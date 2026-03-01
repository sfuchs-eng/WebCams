<?php
/**
 * OTA Firmware Upload Endpoint
 * Admin interface for uploading firmware binaries
 */

require_once __DIR__ . '/lib/ota.php';
require_once __DIR__ . '/lib/path.php';

// Authentication is handled by .htaccess (same realm as admin.php)
// No need for duplicate auth code here

// Handle firmware upload
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    header('Content-Type: application/json');
    
    // Check if file was uploaded
    if (!isset($_FILES['firmware'])) {
        http_response_code(400);
        echo json_encode(['success' => false, 'error' => 'No firmware file provided']);
        exit;
    }
    
    $description = $_POST['description'] ?? null;
    
    // Save firmware
    $result = saveUploadedFirmware($_FILES['firmware'], $description);
    
    if ($result['success']) {
        http_response_code(200);
        echo json_encode([
            'success' => true,
            'filename' => $result['filename'],
            'size' => $result['size'],
            'sha256' => $result['sha256'],
            'version' => $result['version'],
            'message' => 'Firmware uploaded successfully'
        ]);
    } else {
        http_response_code(400);
        echo json_encode([
            'success' => false,
            'error' => $result['error']
        ]);
    }
    exit;
}

// GET request - return upload form or available firmware list
if ($_SERVER['REQUEST_METHOD'] === 'GET') {
    // If Accept header is JSON, return firmware list
    $accept = $_SERVER['HTTP_ACCEPT'] ?? '';
    if (strpos($accept, 'application/json') !== false) {
        header('Content-Type: application/json');
        $firmware = getAvailableFirmware();
        echo json_encode(['success' => true, 'firmware' => $firmware]);
        exit;
    }
    
    // Otherwise show HTML form
    ?>
    <!DOCTYPE html>
    <html>
    <head>
        <title>OTA Firmware Upload</title>
        <style>
            body { font-family: Arial, sans-serif; max-width: 800px; margin: 50px auto; padding: 20px; }
            .upload-form { background: #f5f5f5; padding: 20px; border-radius: 5px; }
            .form-group { margin-bottom: 15px; }
            label { display: block; margin-bottom: 5px; font-weight: bold; }
            input[type="file"], input[type="text"] { width: 100%; padding: 8px; }
            button { background: #007bff; color: white; padding: 10px 20px; border: none; border-radius: 3px; cursor: pointer; }
            button:hover { background: #0056b3; }
            .message { padding: 10px; margin: 10px 0; border-radius: 3px; }
            .success { background: #d4edda; color: #155724; }
            .error { background: #f8d7da; color: #721c24; }
            table { width: 100%; border-collapse: collapse; margin-top: 20px; }
            th, td { padding: 10px; text-align: left; border-bottom: 1px solid #ddd; }
            th { background: #007bff; color: white; }
        </style>
    </head>
    <body>
        <h1>OTA Firmware Upload</h1>
        
        <div class="upload-form">
            <h2>Upload New Firmware</h2>
            <form method="post" enctype="multipart/form-data" id="uploadForm">
                <div class="form-group">
                    <label for="firmware">Firmware File (.bin):</label>
                    <input type="file" name="firmware" id="firmware" accept=".bin" required>
                </div>
                <div class="form-group">
                    <label for="description">Description (optional):</label>
                    <input type="text" name="description" id="description" placeholder="e.g., Added OTA support">
                </div>
                <button type="submit">Upload Firmware</button>
            </form>
            <div id="message"></div>
        </div>
        
        <h2>Available Firmware</h2>
        <table id="firmwareTable">
            <thead>
                <tr>
                    <th>Filename</th>
                    <th>Version</th>
                    <th>Size</th>
                    <th>Uploaded</th>
                    <th>SHA256</th>
                </tr>
            </thead>
            <tbody>
                <?php
                $firmware = getAvailableFirmware();
                if (empty($firmware)) {
                    echo '<tr><td colspan="5">No firmware files available</td></tr>';
                } else {
                    foreach ($firmware as $fw) {
                        $size = number_format($fw['size'] / 1024 / 1024, 2) . ' MB';
                        $uploaded = date('Y-m-d H:i', strtotime($fw['uploaded']));
                        $sha256Short = substr($fw['sha256'], 0, 16) . '...';
                        echo "<tr>";
                        echo "<td>{$fw['filename']}</td>";
                        echo "<td>{$fw['version']}</td>";
                        echo "<td>{$size}</td>";
                        echo "<td>{$uploaded}</td>";
                        echo "<td title='{$fw['sha256']}'>{$sha256Short}</td>";
                        echo "</tr>";
                    }
                }
                ?>
            </tbody>
        </table>
        
        <p><a href="<?= baseUrl('admin.php') ?>">← Back to Admin</a></p>
        
        <script>
            document.getElementById('uploadForm').addEventListener('submit', async function(e) {
                e.preventDefault();
                
                const formData = new FormData(this);
                const messageDiv = document.getElementById('message');
                
                messageDiv.innerHTML = '<div class="message">Uploading...</div>';
                
                try {
                    const response = await fetch('<?= baseUrl('ota-upload.php') ?>', {
                        method: 'POST',
                        body: formData
                    });
                    
                    const result = await response.json();
                    
                    if (result.success) {
                        messageDiv.innerHTML = '<div class="message success">' + result.message + '<br>' +
                            'Version: ' + result.version + '<br>' +
                            'Size: ' + (result.size / 1024 / 1024).toFixed(2) + ' MB<br>' +
                            'SHA256: ' + result.sha256.substring(0, 16) + '...</div>';
                        
                        // Reload page after 2 seconds to show new firmware
                        setTimeout(() => { location.reload(); }, 2000);
                    } else {
                        messageDiv.innerHTML = '<div class="message error">Error: ' + result.error + '</div>';
                    }
                } catch (error) {
                    messageDiv.innerHTML = '<div class="message error">Upload failed: ' + error.message + '</div>';
                }
            });
        </script>
    </body>
    </html>
    <?php
    exit;
}
