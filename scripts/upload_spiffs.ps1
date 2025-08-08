param([switch]$Force)
# DEPRECATED: SPIFFS uploads are no longer used.
# The web UI is hosted on your k3s cluster behind Traefik.
# This script is kept only to avoid breaking references in history.

Write-Host "SPIFFS upload is deprecated. No action taken." -ForegroundColor Yellow
Write-Host "The frontend is hosted on k8s. If you need to wipe device storage, use the new endpoint:" -ForegroundColor Yellow
Write-Host "  http://poop-monitor.local/spiffs/format?confirm=YES" -ForegroundColor Cyan
exit 1
