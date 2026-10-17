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
        if (typeof window !== 'undefined') {
            this.init();
        }
    }

    api(path) { return `${this.base}${path}`; }

    async init() {
        // Check for cache bust parameter
        if (typeof window !== 'undefined' && window.location) {
            const urlParams = new URLSearchParams(window.location.search);
            if (urlParams.has('cb') || urlParams.has('bust')) {
                await this.bustCacheQuiet();
            }
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
        
        // WiFi signal strength and RSSI bar
        this.updateWiFiSignal();
        this.updateRssiBar();

        // System resource usage bars (heap and flash)
        this.updateHeapBar();
        this.updateFlashBar();
        
        // Update alert status and controls
        this.updateAlertStatus();
        this.updateAlertControls();
    }

    // RSSI 0 means no signal / unknown (WiFi.RSSI() when disconnected), not 0 dBm.
    isRssiUnusable() {
        const connected = this.deviceData.wifi_connected;
        if (connected === false || connected === 'false') return true;
        const rssi = this.deviceData.wifi_rssi;
        return rssi === 0 || rssi === '0';
    }

    finiteNumber(value) {
        if (typeof value === 'number') {
            return Number.isFinite(value) ? value : null;
        }
        if (typeof value === 'string' && value.trim() !== '') {
            const n = Number(value);
            return Number.isFinite(n) ? n : null;
        }
        return null;
    }

    scaleKilobytes(value) {
        const n = this.finiteNumber(value);
        return n === null ? null : n * 1024;
    }

    clampPercent(value) {
        const n = Number(value);
        if (!Number.isFinite(n)) return 0;
        return Math.min(100, Math.max(0, Math.round(n)));
    }

    paintBar(bar, pct, barClass, visible) {
        const safe = this.clampPercent(pct);
        if (!bar) return safe;
        bar.style.width = `${safe}%`;
        bar.setAttribute('aria-valuenow', String(safe));
        bar.className = `progress-bar ${barClass}`;
        bar.style.display = visible ? '' : 'none';
        const track = bar.parentElement;
        if (track && typeof track.className === 'string' && track.className.split(/\s+/).includes('progress')) {
            track.style.display = visible ? '' : 'none';
        }
        return safe;
    }

    updateWiFiSignal() {
        const rssi = this.deviceData.wifi_rssi;
        const signalElement = document.getElementById('wifi-signal');
        const iconElement = document.getElementById('wifi-icon');

        let signalText = 'No signal';
        let iconClass = 'bi bi-wifi-off';
        let colorClass = 'wifi-poor';

        if (!this.isRssiUnusable() && typeof rssi === 'number' && Number.isFinite(rssi)) {
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
        }

        if (signalElement) signalElement.textContent = signalText;
        if (iconElement) iconElement.className = `${iconClass} ${colorClass} fs-2`;
        this.updateRssiBar();
    }

    updateRssiBar() {
        const rssi = this.deviceData.wifi_rssi;
        let pct = 0;
        let barClass = 'bg-danger';
        const unusable = this.isRssiUnusable();

        if (unusable) {
            // Disconnected or RSSI 0 must not render as a full green bar.
            pct = 0;
            barClass = 'bg-danger';
        } else if (this.deviceData.wifi_signal_percentage !== undefined && this.deviceData.wifi_signal_percentage !== null) {
            pct = Math.min(100, Math.max(0, parseInt(this.deviceData.wifi_signal_percentage, 10) || 0));
        } else if (typeof rssi === 'number' && Number.isFinite(rssi)) {
            pct = Math.min(100, Math.max(0, Math.round(2 * (rssi + 100))));
        }

        if (!unusable) {
            if (rssi > -50 || pct >= 80) {
                barClass = 'bg-success';
            } else if (rssi > -60 || pct >= 60) {
                barClass = 'bg-info';
            } else if (rssi > -70 || pct >= 40) {
                barClass = 'bg-warning';
            } else {
                barClass = 'bg-danger';
            }
        }

        let rssiBar = document.getElementById('rssi-bar') || document.getElementById('wifi-bar');
        if (!rssiBar) {
            rssiBar = this.createRssiBarElement();
        }
        pct = this.paintBar(rssiBar, pct, barClass, true);
        return { pct, rssi, barClass };
    }

    updateHeapBar() {
        const freeHeap = this.deviceData.free_heap !== undefined
            ? this.finiteNumber(this.deviceData.free_heap)
            : (this.deviceData.free_memory_kb !== undefined
                ? this.scaleKilobytes(this.deviceData.free_memory_kb)
                : null);

        let totalHeap = null;
        if (this.deviceData.total_heap !== undefined) {
            totalHeap = this.finiteNumber(this.deviceData.total_heap);
        } else if (this.deviceData.heap_size !== undefined) {
            totalHeap = this.finiteNumber(this.deviceData.heap_size);
        } else if (this.deviceData.total_memory_kb !== undefined) {
            totalHeap = this.scaleKilobytes(this.deviceData.total_memory_kb);
        }

        const freePercent = this.deviceData.free_memory_percent !== undefined
            ? this.finiteNumber(this.deviceData.free_memory_percent)
            : null;

        let usedHeap = 0;
        let pct = 0;
        let visible = false;
        if (totalHeap !== null && totalHeap > 0 && freeHeap !== null) {
            usedHeap = Math.max(0, totalHeap - freeHeap);
            pct = this.clampPercent((usedHeap / totalHeap) * 100);
            visible = true;
        } else if (totalHeap !== null && totalHeap <= 0) {
            usedHeap = 0;
            pct = 0;
            visible = true;
        } else if (freePercent !== null) {
            pct = this.clampPercent(100 - freePercent);
            usedHeap = (totalHeap !== null && totalHeap > 0)
                ? Math.round((pct / 100) * totalHeap)
                : 0;
            visible = true;
        }

        let barClass = 'progress-bar-memory';
        if (pct >= 85) {
            barClass = 'bg-danger';
        } else if (pct >= 70) {
            barClass = 'bg-warning';
        }

        let heapBar = document.getElementById('heap-bar');
        let heapUsage = document.getElementById('heap-usage');
        if (!heapBar || !heapUsage) {
            this.createResourceBarElements();
            heapBar = document.getElementById('heap-bar');
            heapUsage = document.getElementById('heap-usage');
        }

        pct = this.paintBar(heapBar, pct, barClass, visible);
        if (heapUsage) {
            if (!visible) {
                heapUsage.textContent = 'n/a';
            } else if (totalHeap !== null) {
                heapUsage.textContent = `${this.formatBytes(usedHeap)} / ${this.formatBytes(totalHeap)} (${pct}%)`;
            } else {
                heapUsage.textContent = `${pct}%`;
            }
        }
        return { usedHeap, totalHeap, pct, visible };
    }

    updateFlashBar() {
        // flash_size is the app partition. Do not fall back to the chip size.
        let flashSize = null;
        if (this.deviceData.flash_size !== undefined) {
            flashSize = this.finiteNumber(this.deviceData.flash_size);
        } else if (this.deviceData.app_partition_size !== undefined) {
            flashSize = this.finiteNumber(this.deviceData.app_partition_size);
        } else if (this.deviceData.sketch_space !== undefined) {
            flashSize = this.finiteNumber(this.deviceData.sketch_space);
        }

        let flashUsed = null;
        if (this.deviceData.flash_used !== undefined) {
            flashUsed = this.finiteNumber(this.deviceData.flash_used);
        } else if (this.deviceData.sketch_size !== undefined) {
            flashUsed = this.finiteNumber(this.deviceData.sketch_size);
        } else if (this.deviceData.flash_bytes !== undefined) {
            flashUsed = this.finiteNumber(this.deviceData.flash_bytes);
        }

        let pct = 0;
        let visible = false;
        if (flashSize !== null && flashSize > 0 && flashUsed !== null) {
            pct = this.clampPercent((flashUsed / flashSize) * 100);
            visible = true;
        } else if (flashSize !== null && flashSize <= 0) {
            pct = 0;
            visible = true;
        }

        let barClass = 'bg-info';
        if (pct >= 90) {
            barClass = 'bg-danger';
        } else if (pct >= 75) {
            barClass = 'bg-warning';
        }

        let flashBar = document.getElementById('flash-bar');
        let flashUsage = document.getElementById('flash-usage');
        if (!flashBar || !flashUsage) {
            this.createResourceBarElements();
            flashBar = document.getElementById('flash-bar');
            flashUsage = document.getElementById('flash-usage');
        }

        pct = this.paintBar(flashBar, pct, barClass, visible);
        if (flashUsage) {
            if (!visible) {
                flashUsage.textContent = 'n/a';
            } else if (flashUsed !== null && flashSize !== null) {
                flashUsage.textContent = `${this.formatBytes(flashUsed)} / ${this.formatBytes(flashSize)} (${pct}%)`;
            } else if (flashSize !== null) {
                flashUsage.textContent = `${this.formatBytes(flashSize)} total`;
            } else {
                flashUsage.textContent = 'n/a';
            }
        }
        return { flashUsed, flashSize, pct, visible };
    }

    drawUsageBars() {
        return {
            heap: this.updateHeapBar(),
            flash: this.updateFlashBar(),
            rssi: this.updateRssiBar()
        };
    }

    createRssiBarElement() {
        if (typeof document === 'undefined') return null;
        const signalElement = document.getElementById('wifi-signal');
        if (!signalElement) return null;
        const cardBody = signalElement.closest ? signalElement.closest('.card-body') : signalElement.parentElement;
        if (!cardBody) return null;
        const progressDiv = document.createElement('div');
        progressDiv.className = 'progress mt-2';
        progressDiv.style.height = '6px';
        progressDiv.innerHTML = '<div id="rssi-bar" class="progress-bar bg-success" role="progressbar" style="width: 0%;" aria-valuenow="0" aria-valuemin="0" aria-valuemax="100"></div>';
        cardBody.appendChild(progressDiv);
        return progressDiv.querySelector ? progressDiv.querySelector('#rssi-bar') : document.getElementById('rssi-bar');
    }

    createResourceBarElements() {
        if (typeof document === 'undefined') return;
        if (document.getElementById('heap-bar') && document.getElementById('flash-bar')) {
            return;
        }
        const statusRow = document.querySelector ? document.querySelector('.row.mb-4') : null;
        if (!statusRow || !statusRow.parentNode) return;
        const row = document.createElement('div');
        row.className = 'row mb-4';
        row.id = 'system-resources-row';
        row.innerHTML = `
            <div class="col-md-6 mb-3">
                <div class="card bg-light">
                    <div class="card-body">
                        <div class="d-flex justify-content-between align-items-center mb-1">
                            <div>
                                <h6 class="card-title text-muted mb-1">Heap Memory</h6>
                                <h6 id="heap-usage" class="mb-0">Loading...</h6>
                            </div>
                            <div class="align-self-center">
                                <i id="heap-icon" class="bi bi-cpu text-primary fs-2"></i>
                            </div>
                        </div>
                        <div class="progress progress-memory mt-2" style="height: 6px;">
                            <div id="heap-bar" class="progress-bar progress-bar-memory" role="progressbar" style="width: 0%;" aria-valuenow="0" aria-valuemin="0" aria-valuemax="100"></div>
                        </div>
                    </div>
                </div>
            </div>
            <div class="col-md-6 mb-3">
                <div class="card bg-light">
                    <div class="card-body">
                        <div class="d-flex justify-content-between align-items-center mb-1">
                            <div>
                                <h6 class="card-title text-muted mb-1">App partition</h6>
                                <h6 id="flash-usage" class="mb-0">Loading...</h6>
                            </div>
                            <div class="align-self-center">
                                <i id="flash-icon" class="bi bi-hdd-network text-info fs-2"></i>
                            </div>
                        </div>
                        <div class="progress progress-memory mt-2" style="height: 6px;">
                            <div id="flash-bar" class="progress-bar bg-info" role="progressbar" style="width: 0%;" aria-valuenow="0" aria-valuemin="0" aria-valuemax="100"></div>
                        </div>
                    </div>
                </div>
            </div>
        `;
        statusRow.parentNode.insertBefore(row, statusRow.nextSibling);
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
    formatBytes(bytes) {
        if (bytes === undefined || bytes === null || isNaN(bytes)) return '0 B';
        const num = Number(bytes);
        if (num < 1024) return `${num} B`;
        if (num < 1024 * 1024) return `${(num / 1024).toFixed(1)} KB`;
        return `${(num / (1024 * 1024)).toFixed(2)} MB`;
    }

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
if (typeof document !== 'undefined' && typeof document.addEventListener === 'function') {
    document.addEventListener('DOMContentLoaded', () => {
        controlPanel = new ESP32ControlPanel();
    });
}

if (typeof module !== 'undefined' && module.exports) {
    module.exports = { ESP32ControlPanel };
}
