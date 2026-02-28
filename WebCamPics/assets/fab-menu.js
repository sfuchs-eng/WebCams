/**
 * Floating Action Button Menu
 * Handles FAB menu toggle and click-outside-to-close functionality
 */

(function() {
    'use strict';
    
    // Initialize FAB menu when DOM is ready
    function initFabMenu() {
        const fabButton = document.getElementById('fabButton');
        const fabMenu = document.getElementById('fabMenu');
        
        if (!fabButton || !fabMenu) {
            console.warn('FAB menu elements not found');
            return;
        }
        
        // Toggle menu on button click
        fabButton.addEventListener('click', function(e) {
            e.stopPropagation();
            fabMenu.classList.toggle('open');
            fabButton.classList.toggle('open');
        });
        
        // Close menu when clicking outside
        document.addEventListener('click', function(e) {
            if (!e.target.closest('.fab-container')) {
                fabMenu.classList.remove('open');
                fabButton.classList.remove('open');
            }
        });
        
        // Close menu when pressing Escape key
        document.addEventListener('keydown', function(e) {
            if (e.key === 'Escape') {
                fabMenu.classList.remove('open');
                fabButton.classList.remove('open');
            }
        });
    }
    
    // Initialize when DOM is ready
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initFabMenu);
    } else {
        initFabMenu();
    }
})();
