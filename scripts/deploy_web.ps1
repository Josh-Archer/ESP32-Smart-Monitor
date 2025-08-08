param(
    [string]$Tag = "latest",
    [string]$Image = "jarcher1200/poop-monitor",
    [string]$Namespace
)

# Build and deploy the web Docker image, then restart k8s deployment
Write-Host "Building and deploying web image..." -ForegroundColor Green

# Resolve repo root
$RepoRoot = Split-Path -Parent $PSScriptRoot
$Dockerfile = Join-Path $RepoRoot "k8s/Dockerfile"

# Check prerequisites
if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    Write-Host "Docker not found in PATH" -ForegroundColor Red
    exit 1
}
if (-not (Get-Command kubectl -ErrorAction SilentlyContinue)) {
    Write-Host "kubectl not found in PATH" -ForegroundColor Red
    exit 1
}

# Infer namespace if not provided
$nsToUse = $Namespace
if (-not $nsToUse) {
    try { $nsToUse = (& kubectl get deploy esp32-panel -A -o jsonpath='{.items[0].metadata.namespace}' 2>$null) } catch {}
    if (-not $nsToUse) {
        try { $nsToUse = (& kubectl config view --minify -o jsonpath='{..namespace}' 2>$null) } catch {}
    }
}
if ($nsToUse) { Write-Host "Namespace: $nsToUse" -ForegroundColor Cyan } else { Write-Host "Namespace: default" -ForegroundColor Cyan }

$FullImage = "$($Image):$($Tag)"
Write-Host "Image: $FullImage" -ForegroundColor Cyan

# Build
Write-Host "docker build -f $Dockerfile -t $FullImage $RepoRoot" -ForegroundColor Yellow
& docker build -f $Dockerfile -t $FullImage $RepoRoot
if ($LASTEXITCODE -ne 0) { Write-Host "Docker build failed" -ForegroundColor Red; exit $LASTEXITCODE }

# Push
Write-Host "docker push $FullImage" -ForegroundColor Yellow
& docker push $FullImage
if ($LASTEXITCODE -ne 0) { Write-Host "Docker push failed (are you logged in?)" -ForegroundColor Red; exit $LASTEXITCODE }

# Restart deployment
$nsArgs = @()
if ($nsToUse) { $nsArgs += @('-n', $nsToUse) }

Write-Host "Restarting k8s deployment esp32-panel..." -ForegroundColor Yellow
& kubectl rollout restart deployment esp32-panel @nsArgs
if ($LASTEXITCODE -ne 0) { Write-Host "kubectl restart failed" -ForegroundColor Red; exit $LASTEXITCODE }

# Check status
Write-Host "Checking rollout status..." -ForegroundColor Yellow
& kubectl rollout status deployment esp32-panel --watch=false @nsArgs

# List pods
Write-Host "Pods:" -ForegroundColor Yellow
& kubectl get pods -l app=esp32-panel -o wide @nsArgs

Write-Host "Web deploy complete." -ForegroundColor Green
