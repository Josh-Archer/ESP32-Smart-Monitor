#!/usr/bin/env bash
set -euo pipefail

TAG="${1:-latest}"
IMAGE="${IMAGE:-jarcher1200/poop-monitor}"
NAMESPACE="${NAMESPACE:-}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
DOCKERFILE="$REPO_ROOT/k8s/Dockerfile"

if ! command -v docker >/dev/null 2>&1; then
  echo "docker not found in PATH" >&2
  exit 1
fi
if ! command -v kubectl >/dev/null 2>&1; then
  echo "kubectl not found in PATH" >&2
  exit 1
fi

# Infer namespace if not provided
NS_TO_USE="$NAMESPACE"
if [[ -z "$NS_TO_USE" ]]; then
  if kubectl get deploy esp32-panel -A >/dev/null 2>&1; then
    NS_TO_USE="$(kubectl get deploy esp32-panel -A -o jsonpath='{.items[0].metadata.namespace}')"
  else
    NS_TO_USE="$(kubectl config view --minify -o jsonpath='{..namespace}' 2>/dev/null || true)"
  fi
fi
[[ -z "$NS_TO_USE" ]] && NS_TO_USE="default"
echo "Namespace: $NS_TO_USE"

FULL_IMAGE="$IMAGE:$TAG"
echo "Building image: $FULL_IMAGE"

docker build -f "$DOCKERFILE" -t "$FULL_IMAGE" "$REPO_ROOT"
docker push "$FULL_IMAGE"

echo "Restarting k8s deployment esp32-panel..."
kubectl rollout restart deployment esp32-panel -n "$NS_TO_USE"

echo "Rollout status:"
kubectl rollout status deployment esp32-panel --watch=false -n "$NS_TO_USE"

echo "Pods:"
kubectl get pods -l app=esp32-panel -o wide -n "$NS_TO_USE"

echo "Web deploy complete."
