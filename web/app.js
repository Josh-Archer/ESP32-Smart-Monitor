// ESP32 Control Panel JavaScript with PWA Support
class ESP32ControlPanel {
    constructor() {
        this.deviceData = {};
        this.autoRefreshInterval = null;
        this.consoleVisible = false;
        this.autoScroll = true;
        this.telnetSocket = null;
        this.connectionCheckInterval = null;
        this.deferredPrompt = null; // For PWA installation
        // Base URL: always use device mDNS directly
        this.base = 'http://poop-monitor.local';
        this.init();
    }

    api(path) { return `${this.base}${path}`; }

    async init() {
        // Check for cache bust parameter
        const urlParams = new URLSearchParams(window.location.search);
        if (urlParams.has('cb') || urlParams.has('bust')) {
            await this.bustCacheQuiet();
        }
        
        await this.initPWA();
        await this.loadDeviceStatus();
        this.startAutoRefresh();
        this.startConnectionCheck();
        this.setupEventListeners();
    }

    // PWA Initialization
    async initPWA() {
        // Register service worker
        if ('serviceWorker' in navigator) {
            try {
                const registration = await navigator.serviceWorker.register('/sw.js');
                console.log('[PWA] Service Worker registered:', registration);
                
                // Check for updates
                registration.addEventListener('updatefound', () => {
                    const newWorker = registration.installing;
                    newWorker.addEventListener('statechange', () => {
                        if (newWorker.state === 'installed' && navigator.serviceWorker.controller) {
                            this.showToast('App update available! Refresh to update.', 'info');
                        }
                    });
                });
            } catch (error) {
                console.error('[PWA] Service Worker registration failed:', error);
            }
        }

        // Listen for PWA install prompt
        window.addEventListener('beforeinstallprompt', (e) => {
            console.log('[PWA] Install prompt available');
            e.preventDefault();
            this.deferredPrompt = e;
            this.showInstallButton();
        });

        // Listen for successful app installation
        window.addEventListener('appinstalled', () => {
            console.log('[PWA] App installed successfully');
            this.showToast('ESP32 Control Panel installed as app!', 'success');
            this.hideInstallButton();
        });

        // Check if already installed
        if (window.matchMedia('(display-mode: standalone)').matches) {
            console.log('[PWA] Running as installed app');
            this.hideInstallButton();
        } else {
            // Show cache bust button for non-PWA usage
            this.showCacheBustButton();
        }
    }

    showInstallButton() {
        const navbar = document.querySelector('.navbar .container');
        if (!document.getElementById('install-btn') && navbar) {
            const installBtn = document.createElement('button');
            installBtn.id = 'install-btn';
            installBtn.className = 'btn btn-outline-light btn-sm me-2';
            installBtn.innerHTML = '<i class="bi bi-download"></i> Install App';
            installBtn.onclick = () => this.installPWA();
            
            const navbarNav = navbar.querySelector('.navbar-nav');
            navbarNav.parentNode.insertBefore(installBtn, navbarNav);
        }
        
        // Always show cache bust button
        this.showCacheBustButton();
    }

    showCacheBustButton() {
        const navbar = document.querySelector('.navbar .container');
        if (!document.getElementById('cache-bust-btn') && navbar) {
            const cacheBustBtn = document.createElement('button');
            cacheBustBtn.id = 'cache-bust-btn';
            cacheBustBtn.className = 'btn btn-outline-warning btn-sm me-2';
            cacheBustBtn.innerHTML = '<i class="bi bi-arrow-clockwise"></i> Refresh Cache';
            cacheBustBtn.onclick = () => this.bustCache();
            
            const navbarNav = navbar.querySelector('.navbar-nav');
            navbarNav.parentNode.insertBefore(cacheBustBtn, navbarNav);
        }
    }

    async bustCacheQuiet() {
        try {
            // Clear localStorage
            localStorage.clear();
            
            // Clear service worker cache if available
            if ('caches' in window) {
                const cacheNames = await caches.keys();
                await Promise.all(
                    cacheNames.map(cacheName => caches.delete(cacheName))
                );
            }
            
            // Unregister service worker to force fresh registration
            if ('serviceWorker' in navigator) {
                const registrations = await navigator.serviceWorker.getRegistrations();
                await Promise.all(
                    registrations.map(registration => registration.unregister())
                );
            }
            
            console.log('[CACHE] Cache cleared via URL parameter');
            
        } catch (error) {
            console.error('Error busting cache:', error);
        }
    }

    async bustCache() {
        const confirmResult = await this.showConfirm('Clear Cache', 'This will clear all cached data and reload the app. Continue?');
        if (!confirmResult) return;

        try {
            await this.bustCacheQuiet();
            this.showToast('Cache cleared successfully. Reloading...', 'success');
            
            // Force hard reload with cache bust
            setTimeout(() => {
                window.location.href = window.location.href + '?cb=' + Date.now();
            }, 1000);
            
        } catch (error) {
            console.error('Error busting cache:', error);
            this.showToast('Error clearing cache: ' + error.message, 'error');
        }
    }

    hideInstallButton() {
        const installBtn = document.getElementById('install-btn');
        if (installBtn) {
            installBtn.remove();
        }
    }

    async installPWA() {
        if (!this.deferredPrompt) return;

        this.deferredPrompt.prompt();
        const { outcome } = await this.deferredPrompt.userChoice;
        
        if (outcome === 'accepted') {
            console.log('[PWA] User accepted the install prompt');
        } else {
            console.log('[PWA] User dismissed the install prompt');
        }
        
        this.deferredPrompt = null;
        this.hideInstallButton();
    }

    // Device Status Management
    async loadDeviceStatus() {
        try {
            const response = await fetch(this.api('/status'));
            if (!response.ok) throw new Error('Failed to fetch status');
            
            this.deviceData = await response.json();
            
            // Cache status data for offline use
            this.cacheDeviceStatus();
            
            this.updateInterface();
            this.updateConnectionStatus(true);
            
            // Store last connected time
            localStorage.setItem('esp32-last-connected', Date.now().toString());
            
        } catch (error) {
            console.error('Error loading device status:', error);
            
            // Try to load cached data
            const cachedData = this.loadCachedStatus();
            if (cachedData) {
                this.deviceData = cachedData;
                this.updateInterface();
                this.showToast('Showing cached data - device offline', 'warning');
            } else {
                this.showToast('Failed to load device status', 'error');
            }
            
            this.updateConnectionStatus(false);
        }
    }

    cacheDeviceStatus() {
        const cacheData = {
            ...this.deviceData,
            timestamp: Date.now()
        };
        localStorage.setItem('esp32-device-status', JSON.stringify(cacheData));
    }

    loadCachedStatus() {
        try {
            const cached = localStorage.getItem('esp32-device-status');
            if (cached) {
                const data = JSON.parse(cached);
                // Only use cache if less than 1 hour old
                if (Date.now() - data.timestamp < 3600000) {
                    return data;
                }
            }
        } catch (error) {
            console.error('Error loading cached status:', error);
        }
        return null;
    }

    updateInterface() {
        // Update page title and device name
        document.getElementById('page-title').textContent = this.deviceData.device + ' Control Panel';
        document.getElementById('device-name').textContent = this.deviceData.device;
        
        // Update status cards
        document.getElementById('version').textContent = this.deviceData.version;
        document.getElementById('ip').textContent = this.deviceData.ip;
        document.getElementById('uptime').textContent = this.deviceData.current_uptime_formatted || this.formatUptime(this.deviceData.uptime);
        
        // WiFi signal strength
        this.updateWiFiSignal();
        
        // Update alert status and controls
        this.updateAlertStatus();
        this.updateAlertControls();
    }

    updateWiFiSignal() {
        const rssi = this.deviceData.wifi_rssi;
        const signalElement = document.getElementById('wifi-signal');
        const iconElement = document.getElementById('wifi-icon');
        
        let signalText = 'Unknown';
        let iconClass = 'bi bi-wifi';
        let colorClass = '';
        
        if (rssi > -50) {
            signalText = `Excellent (${rssi} dBm)`;
            iconClass = 'bi bi-wifi';
            colorClass = 'wifi-excellent';
        } else if (rssi > -60) {
            signalText = `Good (${rssi} dBm)`;
            iconClass = 'bi bi-wifi';
            colorClass = 'wifi-good';
        } else if (rssi > -70) {
            signalText = `Fair (${rssi} dBm)`;
            iconClass = 'bi bi-wifi-1';
            colorClass = 'wifi-fair';
        } else {
            signalText = `Poor (${rssi} dBm)`;
            iconClass = 'bi bi-wifi-off';
            colorClass = 'wifi-poor';
        }
        
        signalElement.textContent = signalText;
        iconElement.className = `${iconClass} ${colorClass} fs-2`;
    }

    updateAlertStatus() {
        const alertStatusDiv = document.getElementById('alert-status');
        const alertsPaused = this.deviceData.alerts_paused === 'true';
        
        if (alertsPaused) {
            const timeRemaining = parseInt(this.deviceData.alerts_paused_time_remaining_seconds) || 0;
            
            if (timeRemaining > 0) {
                const minutes = Math.floor(timeRemaining / 60);
                const seconds = timeRemaining % 60;
                alertStatusDiv.innerHTML = `
                    <div class="alert alert-warning mb-0 alert-paused">
                        <i class="bi bi-bell-slash"></i>
                        <strong>Alerts are PAUSED</strong><br>
                        Time remaining: ${minutes} minutes, ${seconds} seconds
                    </div>
                `;
            } else {
                alertStatusDiv.innerHTML = `
                    <div class="alert alert-warning mb-0 alert-paused">
                        <i class="bi bi-bell-slash"></i>
                        <strong>Alerts are PAUSED</strong><br>
                        Paused indefinitely
                    </div>
                `;
            }
        } else {
            alertStatusDiv.innerHTML = `
                <div class="alert alert-success mb-0 alert-enabled">
                    <i class="bi bi-bell"></i>
                    <strong>Alerts are ENABLED</strong>
                </div>
            `;
        }
    }

    updateAlertControls() {
        const controlsDiv = document.getElementById('alert-controls');
        const alertsPaused = this.deviceData.alerts_paused === 'true';
        
        if (alertsPaused) {
            controlsDiv.innerHTML = `
                <button class="btn btn-success" onclick="controlPanel.resumeAlerts()">
                    <i class="bi bi-play-fill"></i> Resume Alerts
                </button>
            `;
        } else {
            controlsDiv.innerHTML = `
                <button class="btn btn-warning me-2 mb-2" onclick="controlPanel.pauseAlerts(30)">
                    <i class="bi bi-pause"></i> Pause 30min
                </button>
                <button class="btn btn-warning me-2 mb-2" onclick="controlPanel.pauseAlerts(60)">
                    <i class="bi bi-pause"></i> Pause 1hr
                </button>
                <button class="btn btn-warning me-2 mb-2" onclick="controlPanel.pauseAlerts(180)">
                    <i class="bi bi-pause"></i> Pause 3hr
                </button>
                <button class="btn btn-secondary mb-2" onclick="controlPanel.pauseAlerts('indefinite')">
                    <i class="bi bi-stop"></i> Pause Until Resume
                </button>
            `;
        }
    }

    updateConnectionStatus(isOnline) {
        const statusElement = document.getElementById('connection-status');
        if (isOnline) {
            statusElement.innerHTML = '<i class="bi bi-circle-fill text-success"></i> Connected';
            statusElement.className = 'navbar-text connection-status online';
        } else {
            statusElement.innerHTML = '<i class="bi bi-circle-fill text-danger"></i> Disconnected';
            statusElement.className = 'navbar-text connection-status offline';
        }
    }

    // Device Control Functions
    async rebootDevice() {
        const result = await this.showConfirm('Reboot Device', 'Are you sure you want to reboot the device?');
        if (!result) return;
        
        try {
            const response = await fetch(this.api('/reboot'));
            if (response.ok) {
                this.showToast('Device is rebooting... Page will refresh in 15 seconds.', 'success');
                setTimeout(() => {
                    window.location.reload();
                }, 15000);
            } else {
                throw new Error('Reboot request failed');
            }
        } catch (error) {
            this.showToast('Failed to reboot device: ' + error.message, 'error');
        }
    }

    // Alert Control Functions
    async pauseAlerts(duration) {
        const message = duration === 'indefinite' ? 
            'Pause alerts indefinitely?' : 
            `Pause alerts for ${duration} minutes?`;
            
        const result = await this.showConfirm('Pause Alerts', message);
        if (!result) return;
        
        try {
            const response = await fetch(this.api(`/alerts/pause/${duration}`));
            if (response.ok) {
                this.showToast('Alerts paused successfully', 'success');
                setTimeout(() => this.loadDeviceStatus(), 500);
            } else {
                throw new Error('Pause request failed');
            }
        } catch (error) {
            this.showToast('Failed to pause alerts: ' + error.message, 'error');
        }
    }

    async resumeAlerts() {
        const result = await this.showConfirm('Resume Alerts', 'Resume alerts?');
        if (!result) return;
        
        try {
            const response = await fetch(this.api('/alerts/resume'));
            if (response.ok) {
                this.showToast('Alerts resumed successfully', 'success');
                setTimeout(() => this.loadDeviceStatus(), 500);
            } else {
                throw new Error('Resume request failed');
            }
        } catch (error) {
            this.showToast('Failed to resume alerts: ' + error.message, 'error');
        }
    }

    // Console/Telnet Functions
    toggleTelnetLog() {
        this.consoleVisible = !this.consoleVisible;
        const container = document.getElementById('console-container');
        
        if (this.consoleVisible) {
            container.style.display = 'block';
            this.startTelnetStream();
        } else {
            container.style.display = 'none';
            this.stopTelnetStream();
        }
    }

    async startTelnetStream() {
        try {
            const response = await fetch(this.api('/telnet/start'));
            if (response.ok) {
                this.pollTelnetOutput();
                this.showToast('Console stream started', 'success');
            }
        } catch (error) {
            this.showToast('Failed to start console stream', 'error');
        }
    }

    async stopTelnetStream() {
        try {
            await fetch(this.api('/telnet/stop'));
        } catch (error) {
            console.error('Error stopping telnet stream:', error);
        }
    }

    async pollTelnetOutput() {
        if (!this.consoleVisible) return;
        
        try {
            const response = await fetch(this.api('/telnet/output'));
            if (response.ok) {
                const data = await response.json();
                if (data.output) {
                    this.appendToConsole(data.output);
                }
            }
        } catch (error) {
            console.error('Error polling telnet output:', error);
        }
        
        // Continue polling if console is still visible
        if (this.consoleVisible) {
            setTimeout(() => this.pollTelnetOutput(), 1000);
        }
    }

    appendToConsole(text) {
        const consoleElement = document.getElementById('console-log');
        const timestamp = new Date().toLocaleTimeString();
        
        // Parse log level from text
        let logClass = '';
        if (text.toLowerCase().includes('error')) logClass = 'log-error';
        else if (text.toLowerCase().includes('warning')) logClass = 'log-warning';
        else if (text.toLowerCase().includes('info')) logClass = 'log-info';
        else if (text.toLowerCase().includes('success')) logClass = 'log-success';
        
        const logEntry = document.createElement('div');
        logEntry.className = `log-entry ${logClass}`;
        logEntry.innerHTML = `<span class="log-timestamp">[${timestamp}]</span> ${this.escapeHtml(text)}`;
        
        consoleElement.appendChild(logEntry);
        
        // Auto-scroll if enabled
        if (this.autoScroll) {
            consoleElement.scrollTop = consoleElement.scrollHeight;
        }
        
        // Limit console lines to prevent memory issues
        const maxLines = 1000;
        while (consoleElement.children.length > maxLines) {
            consoleElement.removeChild(consoleElement.firstChild);
        }
    }

    clearConsole() {
        document.getElementById('console-log').innerHTML = '';
    }

    toggleAutoScroll() {
        this.autoScroll = !this.autoScroll;
        const button = document.getElementById('autoscroll-text');
        button.textContent = `Auto-scroll: ${this.autoScroll ? 'ON' : 'OFF'}`;
        
        if (this.autoScroll) {
            const consoleElement = document.getElementById('console-log');
            consoleElement.scrollTop = consoleElement.scrollHeight;
        }
    }

    // Utility Functions
    formatUptime(uptimeMs) {
        const seconds = Math.floor(uptimeMs / 1000);
        const hours = Math.floor(seconds / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        const secs = seconds % 60;
        return `${hours}h ${minutes}m ${secs}s`;
    }

    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }

    showToast(message, type = 'info') {
        const toastContainer = document.getElementById('toast-container');
        const toastId = 'toast-' + Date.now();
        
        const toastHtml = `
            <div id="${toastId}" class="toast toast-${type}" role="alert" aria-live="assertive" aria-atomic="true">
                <div class="toast-header">
                    <i class="bi bi-${this.getToastIcon(type)} me-2"></i>
                    <strong class="me-auto">ESP32 Control Panel</strong>
                    <button type="button" class="btn-close" data-bs-dismiss="toast" aria-label="Close"></button>
                </div>
                <div class="toast-body">
                    ${this.escapeHtml(message)}
                </div>
            </div>
        `;
        
        toastContainer.insertAdjacentHTML('beforeend', toastHtml);
        
        const toastElement = document.getElementById(toastId);
        const toast = new bootstrap.Toast(toastElement, { autohide: true, delay: 5000 });
        toast.show();
        
        // Clean up after toast is hidden
        toastElement.addEventListener('hidden.bs.toast', () => {
            toastElement.remove();
        });
    }

    getToastIcon(type) {
        switch (type) {
            case 'success': return 'check-circle';
            case 'error': return 'exclamation-triangle';
            case 'warning': return 'exclamation-triangle';
            default: return 'info-circle';
        }
    }

    async showConfirm(title, message) {
        return new Promise((resolve) => {
            const modalId = 'confirm-modal-' + Date.now();
            const modalHtml = `
                <div class="modal fade" id="${modalId}" tabindex="-1" aria-hidden="true">
                    <div class="modal-dialog">
                        <div class="modal-content">
                            <div class="modal-header">
                                <h5 class="modal-title">${this.escapeHtml(title)}</h5>
                                <button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="Close"></button>
                            </div>
                            <div class="modal-body">
                                ${this.escapeHtml(message)}
                            </div>
                            <div class="modal-footer">
                                <button type="button" class="btn btn-secondary" data-bs-dismiss="modal">Cancel</button>
                                <button type="button" class="btn btn-primary confirm-btn">Confirm</button>
                            </div>
                        </div>
                    </div>
                </div>
            `;
            
            document.body.insertAdjacentHTML('beforeend', modalHtml);
            const modalElement = document.getElementById(modalId);
            const modal = new bootstrap.Modal(modalElement);
            
            modalElement.querySelector('.confirm-btn').addEventListener('click', () => {
                modal.hide();
                resolve(true);
            });
            
            modalElement.addEventListener('hidden.bs.modal', () => {
                modalElement.remove();
                resolve(false);
            });
            
            modal.show();
        });
    }

    startAutoRefresh() {
        this.autoRefreshInterval = setInterval(() => {
            this.loadDeviceStatus();
        }, 30000);
    }

    startConnectionCheck() {
        this.connectionCheckInterval = setInterval(async () => {
            try {
                const response = await fetch(this.api('/status'), { 
                    method: 'HEAD',
                    cache: 'no-cache'
                });
                this.updateConnectionStatus(response.ok);
            } catch (error) {
                this.updateConnectionStatus(false);
            }
        }, 10000);
    }

    setupEventListeners() {
        // Add keyboard shortcuts
        document.addEventListener('keydown', (e) => {
            if (e.ctrlKey || e.metaKey) {
                switch (e.key) {
                    case 'r':
                        e.preventDefault();
                        this.loadDeviceStatus();
                        break;
                    case '`':
                        e.preventDefault();
                        this.toggleTelnetLog();
                        break;
                }
            }
        });
    }
}

// Global functions for onclick handlers
let controlPanel;

function rebootDevice() {
    controlPanel.rebootDevice();
}

function pauseAlerts(duration) {
    controlPanel.pauseAlerts(duration);
}

function resumeAlerts() {
    controlPanel.resumeAlerts();
}

function toggleTelnetLog() {
    controlPanel.toggleTelnetLog();
}

function clearConsole() {
    controlPanel.clearConsole();
}

function toggleAutoScroll() {
    controlPanel.toggleAutoScroll();
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    controlPanel = new ESP32ControlPanel();
});
