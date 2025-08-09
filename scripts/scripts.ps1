# ESP32 Poop Monitor - PowerShell Scripts and Aliases
# Located in: scripts/scripts.ps1
# Usage: . .\scripts\scripts.ps1
# Source this file to load convenient aliases for development

# Device configuration - Using mDNS hostname only (no static IP)
$DEVICE_HOSTNAME = "poop-monitor.local"

# PlatformIO executable path
$PIO_PATH = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"

# PlatformIO Aliases
function pio-build { 
    param([string]$Environment = "esp32-c3-devkitm-1")
    $envDisplayName = switch ($Environment) {
        "esp32-c3-devkitm-1" { "MQTT-only (default)" }
        "esp32-c3-devkitm-1-webserver" { "WebServer-only" }
        "esp32-c3-devkitm-1-both" { "Both MQTT and WebServer" }
        default { $Environment }
    }
    Write-Host "Building ESP32 firmware - $envDisplayName..." -ForegroundColor Green
    & $PIO_PATH run --environment $Environment
}

function pio-build-mqtt { 
    Write-Host "Building ESP32 firmware - MQTT-only (smallest flash usage)..." -ForegroundColor Green
    & $PIO_PATH run --environment esp32-c3-devkitm-1
}

function pio-build-webserver { 
    Write-Host "Building ESP32 firmware - WebServer-only..." -ForegroundColor Green
    & $PIO_PATH run --environment esp32-c3-devkitm-1-webserver
}

function pio-build-both { 
    Write-Host "Building ESP32 firmware - Full features (MQTT + WebServer)..." -ForegroundColor Green
    & $PIO_PATH run --environment esp32-c3-devkitm-1-both
}

function pio-upload-ota {
    param(
        [string]$TargetHost,
        [string]$Environment = "esp32-c3-devkitm-1"
    )
    $target = if ($TargetHost) { $TargetHost } else { $DEVICE_HOSTNAME }
    $envDisplayName = switch ($Environment) {
        "esp32-c3-devkitm-1" { "MQTT-only" }
        "esp32-c3-devkitm-1-webserver" { "WebServer-only" }
        "esp32-c3-devkitm-1-both" { "Both MQTT and WebServer" }
        default { $Environment }
    }
    Write-Host "Uploading firmware via OTA to $target - $envDisplayName..." -ForegroundColor Green
    & $PIO_PATH run --environment $Environment --target upload --upload-port $target
}

function pio-upload-ota-hostname { 
    Write-Host "Uploading firmware via OTA to $DEVICE_HOSTNAME..." -ForegroundColor Green
    & $PIO_PATH run --target upload --upload-port $DEVICE_HOSTNAME
}

function pio-clean { 
    Write-Host "Cleaning build artifacts..." -ForegroundColor Green
    & $PIO_PATH run --target clean 
}

function pio-monitor { 
    Write-Host "Starting serial monitor..." -ForegroundColor Green
    & $PIO_PATH device monitor 
}

function pio-upload-serial {
    param([string]$Port)
    # Try to auto-detect ESP32-C3 serial port if not provided
    if (-not $Port) {
        try {
            $dev = Get-CimInstance Win32_SerialPort | Where-Object { $_.PNPDeviceID -match 'VID_303A&PID_1001' } | Select-Object -First 1
            if (-not $dev) { $dev = Get-CimInstance Win32_SerialPort | Select-Object -First 1 }
            $Port = $dev.DeviceID
        } catch {
            Write-Host "Could not auto-detect serial port. Specify with: pio-upload-serial -Port COMx" -ForegroundColor Yellow
        }
    }
    $envName = "esp32-c3-devkitm-1-serial"
    if ($Port) {
        Write-Host "Uploading firmware via serial on $Port..." -ForegroundColor Green
        & $PIO_PATH run -e $envName --target upload --upload-port $Port
    } else {
        Write-Host "Uploading firmware via serial (esptool) without explicit port..." -ForegroundColor Green
        & $PIO_PATH run -e $envName --target upload
    }
}

# Telnet monitoring
function telnet-monitor {
    Write-Host "Connecting to ESP32 telnet server at $DEVICE_HOSTNAME:23..." -ForegroundColor Green
    Write-Host "Press Ctrl+C to disconnect" -ForegroundColor Yellow
    
    try {
        $client = New-Object System.Net.Sockets.TcpClient($DEVICE_HOSTNAME, 23)
        $stream = $client.GetStream()
        $reader = New-Object System.IO.StreamReader($stream)
        
        Write-Host "Connected! Monitoring output..." -ForegroundColor Green
        # Prepare Eastern Time zone for timestamps
        $estZone = [System.TimeZoneInfo]::FindSystemTimeZoneById('Eastern Standard Time')
        
        while ($client.Connected) {
            if ($stream.DataAvailable) {
                $line = $reader.ReadLine()
                # Convert current time to Eastern Time for logging
                $estNow = [System.TimeZoneInfo]::ConvertTime((Get-Date), $estZone)
                $timestamp = $estNow.ToString('HH:mm:ss')
                Write-Host "[$timestamp]: $line" -ForegroundColor Cyan
            }
            Start-Sleep -Milliseconds 100
        }
    }
    catch {
        Write-Host "Error connecting to telnet via mDNS: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host "Troubleshooting:" -ForegroundColor Yellow
        Write-Host "  • Check if device is online: ping $DEVICE_HOSTNAME" -ForegroundColor White
        Write-Host "  • Verify device is on network and firmware uploaded successfully" -ForegroundColor White
        Write-Host "  • Try uploading firmware again: pio-upload-ota" -ForegroundColor White
    }
    finally {
        if ($client) { $client.Close() }
        Write-Host "Telnet connection closed." -ForegroundColor Yellow
    }
}

# Network utilities
function ping-device {
    Write-Host "Testing ESP32 device connectivity via mDNS..." -ForegroundColor Green
    Write-Host "Testing: $DEVICE_HOSTNAME" -ForegroundColor Cyan
    
    $result = Test-NetConnection -ComputerName $DEVICE_HOSTNAME -Port 23 -WarningAction SilentlyContinue
    
    if ($result.TcpTestSucceeded) {
        Write-Host "✓ Device responds via mDNS hostname" -ForegroundColor Green
        Write-Host "Ready for telnet connection!" -ForegroundColor Green
    } else {
        Write-Host "✗ Device not reachable via mDNS" -ForegroundColor Red
        Write-Host "Possible issues:" -ForegroundColor Yellow
        Write-Host "  • Device is offline or not connected to WiFi" -ForegroundColor White
        Write-Host "  • Firmware not uploaded or crashed during boot" -ForegroundColor White
        Write-Host "  • mDNS not working (try: nslookup $DEVICE_HOSTNAME)" -ForegroundColor White
    }
}

function device-info {
    Write-Host "ESP32 Poop Monitor Device Information:" -ForegroundColor Green
    Write-Host "mDNS Hostname: $DEVICE_HOSTNAME" -ForegroundColor White
    Write-Host "Telnet Port: 23" -ForegroundColor White
    Write-Host "OTA Port: 3232" -ForegroundColor White
    Write-Host "Web Interface: http://$DEVICE_HOSTNAME/" -ForegroundColor White
    Write-Host "Status API: http://$DEVICE_HOSTNAME/status" -ForegroundColor White
}

function open-device-ui {
    Write-Host "Opening device UI in default browser..." -ForegroundColor Green
    Start-Process "http://$DEVICE_HOSTNAME/"
}

# Docker/K8s utilities
function web-deploy {
    param(
        [string]$Tag = "latest",
        [string]$Image = "jarcher1200/poop-monitor",
        [string]$Namespace
    )
    $script = Join-Path $PSScriptRoot 'deploy_web.ps1'
    $args = @('-ExecutionPolicy','Bypass','-File', $script, '-Tag', $Tag, '-Image', $Image)
    if ($PSBoundParameters.ContainsKey('Namespace') -and $Namespace) {
        $args += @('-Namespace', $Namespace)
    }
    & powershell.exe @args
}

# Git utilities
function git-status-clean {
    Write-Host "Git status (ignoring build artifacts):" -ForegroundColor Green
    git status --porcelain | Where-Object { $_ -notmatch '\.pio/' }
}

function commit-version {
    param([string]$version, [string]$message)
    
    if (-not $version) {
        Write-Host "Usage: commit-version 'version' 'message'" -ForegroundColor Red
        Write-Host "Example: commit-version '2.0.1' 'Fixed DNS fallback issue'" -ForegroundColor Yellow
        return
    }
    
    $commitMessage = if ($message) { "v$version - $message" } else { "v$version" }
    
    Write-Host "Committing version $version..." -ForegroundColor Green
    git add -A
    git commit -m $commitMessage
    git tag -a "v$version" -m "Version $version"
    Write-Host "Created commit and tag for version $version" -ForegroundColor Green
}

# Development workflow
function deploy-ota {
    param(
        [switch]$clean,
        [string]$Environment = "esp32-c3-devkitm-1"
    )
    
    if ($clean) {
        Write-Host "Clean build and OTA deployment..." -ForegroundColor Green
        pio-clean
    }
    
    Write-Host "Building and deploying via OTA..." -ForegroundColor Green
    pio-build -Environment $Environment
    if ($LASTEXITCODE -eq 0) {
        pio-upload-ota -Environment $Environment
        if ($LASTEXITCODE -eq 0) {
            Write-Host "OTA successful." -ForegroundColor Green
            # Then connect to telnet
            Write-Host "Connecting to telnet..." -ForegroundColor Green
            Start-Sleep -Seconds 3
            telnet-monitor
         }
     }
 }

# Version Management Functions
function preview-version {
    Write-Host "🔍 Previewing version changes..." -ForegroundColor Green
    python scripts/version_manager.py --dry-run
}

function update-version {
    Write-Host "🏷️ Applying smart version increment..." -ForegroundColor Green
    python scripts/version_manager.py
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✅ Version updated successfully!" -ForegroundColor Green
        Write-Host "Don't forget to commit the changes." -ForegroundColor Yellow
    } else {
        Write-Host "ℹ️ No version increment needed." -ForegroundColor Yellow
    }
}

function force-version {
    param([string]$Version)
    if (-not $Version) {
        Write-Host "Usage: force-version 'X.Y.Z'" -ForegroundColor Red
        return
    }
    Write-Host "🎯 Forcing version to $Version..." -ForegroundColor Green
    python scripts/version_manager.py --force-version $Version
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✅ Version forced to $Version successfully!" -ForegroundColor Green
        Write-Host "Don't forget to commit the changes." -ForegroundColor Yellow
    }
}

# Help function
function pio-help {
    Write-Host "ESP32 Poop Monitor - Available Commands:" -ForegroundColor Green
    Write-Host ""
    Write-Host "Build Commands:" -ForegroundColor Yellow
    Write-Host "  pio-build               Build firmware (default: MQTT-only)"
    Write-Host "  pio-build-mqtt          Build MQTT-only (smallest flash usage)" 
    Write-Host "  pio-build-webserver     Build WebServer-only"
    Write-Host "  pio-build-both          Build with both MQTT and WebServer"
    Write-Host "  pio-clean               Clean build artifacts"
    Write-Host ""
    Write-Host "Upload Commands:" -ForegroundColor Yellow  
    Write-Host "  pio-upload-ota          Upload via OTA (default: MQTT-only)"
    Write-Host "  pio-upload-ota-hostname Upload via OTA (explicit hostname)"
    Write-Host "  pio-upload-serial       Upload via serial cable"
    Write-Host ""
    Write-Host "Monitoring Commands:" -ForegroundColor Yellow
    Write-Host "  pio-monitor             Serial monitor"
    Write-Host "  telnet-monitor          Connect to telnet server"
    Write-Host ""
    Write-Host "Network Commands:" -ForegroundColor Yellow
    Write-Host "  ping-device             Test device connectivity"
    Write-Host "  device-info             Show device information"
    Write-Host "  open-device-ui          Open device web interface"
    Write-Host ""
    Write-Host "Docker/K8s:" -ForegroundColor Yellow
    Write-Host "  web-deploy [-Tag latest] [-Image jarcher1200/poop-monitor] [-Namespace ns]" 
    Write-Host "                             Build+push image and restart esp32-panel"
    Write-Host ""
    Write-Host "Git Commands:" -ForegroundColor Yellow
    Write-Host "  git-status-clean        Git status (no build files)"
    Write-Host "  commit-version 'ver'    Commit with version tag"
    Write-Host ""
    Write-Host "Version Management:" -ForegroundColor Yellow
    Write-Host "  preview-version         Preview version changes"
    Write-Host "  update-version          Apply smart version increment"
    Write-Host "  force-version 'X.Y.Z'   Force specific version"
    Write-Host ""
    Write-Host "Workflow Commands:" -ForegroundColor Yellow
    Write-Host "  deploy-ota              Build + Upload + Monitor"
    Write-Host "  deploy-ota -clean       Clean + Build + Upload + Monitor"
    Write-Host ""
    Write-Host "Current Configuration:" -ForegroundColor Cyan
    Write-Host "  Device: $DEVICE_HOSTNAME (mDNS only)"
}

# Display help on load
Write-Host "ESP32 Poop Monitor development scripts loaded!" -ForegroundColor Green
Write-Host "Type 'pio-help' to see available commands." -ForegroundColor Yellow
