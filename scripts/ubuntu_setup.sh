#!/usr/bin/env bash
# Ubuntu VM bootstrap for QARMA dev + networking
set -euo pipefail

echo "[ubuntu-setup] Starting bootstrap..."

if ! command -v apt >/dev/null 2>&1; then
  echo "This script is intended for Ubuntu/Debian (apt)." >&2
  exit 1
fi

sudo apt update

# Core dev + tools
sudo apt install -y \
  git build-essential clang cmake ninja-build \
  gdb valgrind strace ltrace \
  tmux jq \
  nasm mtools xorriso \
  binutils \
  make

# Networking + capture
sudo apt install -y \
  iproute2 bridge-utils ethtool \
  tcpdump tshark \
  iputils-arping \
  netcat-openbsd socat

# QEMU + KVM
sudo apt install -y \
  qemu-system-x86 qemu-utils qemu-kvm

echo "[ubuntu-setup] Verifying virtualization support..."
if [[ $(egrep -c '(vmx|svm)' /proc/cpuinfo || true) -eq 0 ]]; then
  echo "[warn] CPU virtualization flags not detected (vmx/svm). KVM may be unavailable." >&2
fi

echo "[ubuntu-setup] Loading KVM modules if needed..."
sudo modprobe kvm || true
sudo modprobe kvm_intel 2>/dev/null || sudo modprobe kvm_amd 2>/dev/null || true

if [[ -e /dev/kvm ]]; then
  echo "[ubuntu-setup] /dev/kvm present."
else
  echo "[warn] /dev/kvm not present; KVM accel may be disabled." >&2
fi

# Optional: allow non-root capture via Wireshark
echo "[ubuntu-setup] Adding user '$USER' to groups: kvm, wireshark (log out/in to take effect)."
sudo usermod -aG kvm,wireshark "$USER" || true

echo "[ubuntu-setup] Done. Quick checks:"
echo "- ip -br addr; ip -br link"
echo "- qemu-system-x86_64 --version"
echo "- ls -l /dev/kvm"
echo
echo "Next steps:"
echo "1) Clone QARMA (if not already):"
echo "   git clone git@github.com:dmorton63/QARMA.git && cd QARMA"
echo "   git fetch --tags && git checkout 64bit-migration"
echo "2) Create isolated bridge/tap (ephemeral):"
echo "   sudo ./createbridge.sh --sniff-lite"
echo "3) Launch with tap backend:"
echo "   make qemu-tap"
echo
echo "[ubuntu-setup] Bootstrap complete."
