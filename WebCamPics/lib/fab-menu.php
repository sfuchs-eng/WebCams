<?php
/**
 * Floating Action Button Menu Component
 * Renders the FAB menu with navigation links
 */

function renderFabMenu() {
    ?>
    <!-- Floating Action Button Menu -->
    <div class="fab-container">
        <div class="fab-menu" id="fabMenu">
            <a href="<?php echo baseUrl('index.php'); ?>" class="fab-menu-item">
                <span class="fab-icon">🏠</span>
                <span class="fab-label">Home</span>
            </a>
            <a href="<?php echo baseUrl('admin.php'); ?>" class="fab-menu-item">
                <span class="fab-icon">⚙️</span>
                <span class="fab-label">Settings</span>
            </a>
        </div>
        <button class="fab-button" id="fabButton" aria-label="Menu">
            <span class="fab-icon-hamburger">☰</span>
        </button>
    </div>
    
    <script src="<?php echo baseUrl('assets/fab-menu.js'); ?>"></script>
    <?php
}
