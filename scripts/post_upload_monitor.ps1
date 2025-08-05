# Post-Upload Device Monitoring for Windows
# This script tests ESP32 connectivity after firmware upload using mDNS only

param(
    [string]$DeviceHostname = "poop-monitor.local"
)

Write-Host ""
Write-Host "=" * 60
Write-Host "           POST-UPLOAD DEVICE MONITORING"
Write-Host "=" * 60

# Load our main scripts if available
$ScriptPath = Join-Path $PSScriptRoot "scripts.ps1"
if (Test-Path $ScriptPath) {
    Write-Host "Loading PowerShell development scripts..."
    . $ScriptPath
} else {
    Write-Host "⚠ PowerShell scripts not found at: $ScriptPath"
}

Write-Host "Waiting for device to initialize after upload..."
Start-Sleep -Seconds 8

Write-Host "Testing device connectivity..."

# Test mDNS connectivity only
try {
    Write-Host "Testing mDNS hostname: $DeviceHostname" -ForegroundColor Cyan
    $pingResult = Test-NetConnection -ComputerName $DeviceHostname -Port 23 -WarningAction SilentlyContinue -ErrorAction SilentlyContinue
    
    if ($pingResult.TcpTestSucceeded) {
        Write-Host "✓ Device is responding via mDNS on telnet port 23" -ForegroundColor Green
        Write-Host ""
        Write-Host "🎉 Upload successful! Device is online via mDNS." -ForegroundColor Green
        Write-Host ""
        Write-Host "Next steps:" -ForegroundColor Yellow
        Write-Host "  • Connect to telnet: telnet-monitor" -ForegroundColor White
        Write-Host "  • Check web interface: http://$DeviceHostname/" -ForegroundColor White
        Write-Host "  • View device status: http://$DeviceHostname/status" -ForegroundColor White
    } else {
        Write-Host "⚠ Device not responding via mDNS" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Possible issues:" -ForegroundColor Yellow
        Write-Host "  • Device still booting (wait 30-60 seconds)" -ForegroundColor White
        Write-Host "  • WiFi connection failed" -ForegroundColor White
        Write-Host "  • Firmware crashed during startup" -ForegroundColor White
        Write-Host "  • mDNS not yet registered" -ForegroundColor White
        Write-Host ""
        Write-Host "Troubleshooting:" -ForegroundColor Yellow
        Write-Host "  • Wait and try: telnet-monitor" -ForegroundColor White
        Write-Host "  • Check connectivity: ping-device" -ForegroundColor White
        Write-Host "  • Test DNS resolution: nslookup $DeviceHostname" -ForegroundColor White
    }
} catch {
    Write-Host "⚠ Error testing connectivity: $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host ""
Write-Host "=" * 60
Write-Host "Available commands: pio-help, telnet-monitor, ping-device" -ForegroundColor Cyan
Write-Host "=" * 60
