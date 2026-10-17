#!/usr/bin/env bash
# Vendored from code/home scripts/ci/setup-uecb-broker-endpoint.sh
# (2026-10-17). Re-sync from there; do not fork behavior here.
set -euo pipefail

BROKER_NAMESPACE="${UECB_BROKER_NAMESPACE:-arc-systems}"
BROKER_SERVICE="${UECB_BROKER_SERVICE:-unified-ephemeral-runner-broker}"
BROKER_PORT="${UECB_BROKER_PORT:-8080}"
BROKER_HOST="${BROKER_SERVICE}.${BROKER_NAMESPACE}.svc.cluster.local"
DIRECT_URL="http://${BROKER_HOST}:${BROKER_PORT}"
BROKER_ROLLOUT_TIMEOUT="${UECB_BROKER_ROLLOUT_TIMEOUT:-120s}"
RUNTIME_DIR="${RUNNER_TEMP:-/tmp}"
PID_FILE="${UECB_BROKER_PORT_FORWARD_PID_FILE:-${RUNTIME_DIR}/uecb-broker-port-forward.pid}"
LOG_FILE="${UECB_BROKER_PORT_FORWARD_LOG_FILE:-${RUNTIME_DIR}/uecb-broker-port-forward.log}"

cleanup() {
  if [ -f "${PID_FILE}" ]; then
    pid="$(cat "${PID_FILE}" 2>/dev/null || true)"
    if [ -n "${pid}" ]; then
      kill "${pid}" >/dev/null 2>&1 || true
      wait "${pid}" >/dev/null 2>&1 || true
    fi
    rm -f "${PID_FILE}"
  fi
}

write_broker_url() {
  local broker_url="$1"

  if [ -z "${GITHUB_OUTPUT:-}" ]; then
    echo "GITHUB_OUTPUT is required to publish broker_url." >&2
    exit 1
  fi

  echo "broker_url=${broker_url}" >> "${GITHUB_OUTPUT}"
}

http_reachable() {
  local broker_url="$1"
  local http_status

  http_status="$(curl -sS -o /dev/null -w '%{http_code}' --connect-timeout 2 --max-time 5 "${broker_url}/v1/allocations" 2>/dev/null || true)"
  [ -n "${http_status}" ] && [ "${http_status}" != "000" ]
}

choose_port() {
  if command -v python3 >/dev/null 2>&1; then
    python3 - <<'PY'
import socket

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
    sock.bind(("127.0.0.1", 0))
    print(sock.getsockname()[1])
PY
    return
  fi

  shuf -i 20000-45000 -n 1
}

if [ "${1:-}" = "cleanup" ]; then
  cleanup
  exit 0
fi

if command -v getent >/dev/null 2>&1 && getent hosts "${BROKER_HOST}" >/dev/null 2>&1 && http_reachable "${DIRECT_URL}"; then
  write_broker_url "${DIRECT_URL}"
  exit 0
fi

if ! command -v kubectl >/dev/null 2>&1; then
  echo "kubectl is required to port-forward ${BROKER_NAMESPACE}/${BROKER_SERVICE}." >&2
  exit 1
fi

if ! kubectl -n "${BROKER_NAMESPACE}" get "svc/${BROKER_SERVICE}" >/dev/null 2>&1; then
  echo "Cannot access svc/${BROKER_SERVICE} in namespace ${BROKER_NAMESPACE}." >&2
  exit 1
fi

if kubectl -n "${BROKER_NAMESPACE}" get "deployment/${BROKER_SERVICE}" >/dev/null 2>&1; then
  echo "Waiting for deployment/${BROKER_SERVICE} to become available before port-forwarding..."
  kubectl -n "${BROKER_NAMESPACE}" rollout status "deployment/${BROKER_SERVICE}" --timeout="${BROKER_ROLLOUT_TIMEOUT}"
fi

cleanup
mkdir -p "$(dirname "${PID_FILE}")" "$(dirname "${LOG_FILE}")"

for attempt in 1 2 3 4 5; do
  local_port="$(choose_port)"
  : > "${LOG_FILE}"
  nohup kubectl -n "${BROKER_NAMESPACE}" port-forward --address 127.0.0.1 "svc/${BROKER_SERVICE}" "${local_port}:${BROKER_PORT}" > "${LOG_FILE}" 2>&1 &
  port_forward_pid="$!"
  echo "${port_forward_pid}" > "${PID_FILE}"
  broker_url="http://127.0.0.1:${local_port}"

  for _ in $(seq 1 60); do
    if ! kill -0 "${port_forward_pid}" >/dev/null 2>&1; then
      echo "UECB broker port-forward exited before it became reachable." >&2
      cat "${LOG_FILE}" >&2 || true
      break
    fi

    if http_reachable "${broker_url}"; then
      write_broker_url "${broker_url}"
      exit 0
    fi

    sleep 1
  done

  echo "UECB broker port-forward attempt ${attempt}/5 did not become reachable." >&2
  cleanup
done

cat "${LOG_FILE}" >&2 || true
exit 1
