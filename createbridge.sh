#!/bin/bash
# createbridge.sh - reset and create bridge/tap for QARMA

set -euo pipefail

usage() {
    cat <<EOF
Usage: sudo ./createbridge.sh [--sniff|--sniff-lite]

Creates a fresh isolated Linux bridge 'br0' and TAP 'tap0',
assigns br0 the IP 192.168.100.1/24, and shows status.

Options:
    --sniff        Start tcpdump on br0 to watch ARP frames (blocks)
    --sniff-lite   Run a short (5s) tcpdump ARP capture for lightweight diagnostics

After running this, start QEMU with tap0 and, in the guest:
    netpoll on
    arping 192.168.100.1
    arp

You should see an ARP entry for 192.168.100.1.
EOF
}

SNIFF=false
SNIFF_LITE=false
if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
elif [[ "${1:-}" == "--sniff" ]]; then
    SNIFF=true
elif [[ "${1:-}" == "--sniff-lite" ]]; then
    SNIFF_LITE=true
fi

owner="${SUDO_USER:-$USER}"

echo "Cleaning up any existing bridge/tap..."

# Remove tap0 if it exists
if ip link show tap0 &>/dev/null; then
    echo "Removing existing tap0..."
    sudo ip link set tap0 down || true
    sudo ip link delete tap0 || true
fi

# Remove br0 if it exists
if ip link show br0 &>/dev/null; then
    echo "Removing existing br0..."
    sudo ip link set br0 down || true
    sudo ip link delete br0 type bridge || true
fi

echo "Creating fresh bridge br0..."
sudo ip link add br0 type bridge
sudo ip link set br0 type bridge stp_state 0 || true
sudo ip link set br0 promisc on || true

echo "Creating fresh tap0..."
sudo ip tuntap add dev tap0 mode tap user "$owner"
sudo ip link set tap0 promisc on || true
sudo ip link set tap0 master br0
sudo ip link set tap0 up

echo "Assigning IP to br0..."
sudo ip addr add 192.168.100.1/24 dev br0
sudo ip link set br0 up

# Double-check bridge operstate; attempt second up if still down (WSL quirk)
bridge_state=$(cat /sys/class/net/br0/operstate 2>/dev/null || echo unknown)
if [[ "$bridge_state" != "up" ]]; then
    echo "[warn] br0 operstate is '$bridge_state' after initial setup; retrying 'ip link set br0 up'..."
    sudo ip link set br0 up || true
    bridge_state=$(cat /sys/class/net/br0/operstate 2>/dev/null || echo unknown)
fi
if [[ "$bridge_state" != "up" ]]; then
    echo "[warn] br0 still not UP; ARP replies from host may fail until it's up. Run: sudo ip link set br0 up"
fi

# Optional: reduce bridge netfilter overhead (not critical, but can help stability)
for k in net.bridge.bridge-nf-call-iptables net.bridge.bridge-nf-call-ip6tables net.bridge.bridge-nf-call-arptables; do
    sysctl -q -w $k=0 2>/dev/null || true
done

# Disable multicast snooping to ensure broadcast (ARP) floods reliably
if [[ -e /sys/class/net/br0/bridge/multicast_snooping ]]; then
    echo 0 | sudo tee /sys/class/net/br0/bridge/multicast_snooping >/dev/null || true
fi

echo "Bridge/tap status:"
ip -br addr show br0
ip -br addr show tap0
bridge link
echo "Bridge FDB (forwarding table):"
bridge fdb show br br0 || true

echo
echo "Next steps:"
echo "- Launch QEMU with tap networking bound to tap0, e.g.:"
echo "  qemu-system-x86_64 -m 512M \\" 
echo "    -netdev tap,id=n0,ifname=tap0,script=no,downscript=no \\" 
echo "    -device e1000,netdev=n0,mac=52:54:00:12:34:56 ..."
echo "- In the guest console:"
echo "  netpoll on; arping 192.168.100.1; arp"
echo

if $SNIFF; then
    echo "Listening for ARP traffic on br0 (Ctrl-C to stop)..."
    sudo tcpdump -ni br0 arp
elif $SNIFF_LITE; then
    echo "Running 5s lightweight ARP capture on br0..."
    sudo timeout 5 tcpdump -ni br0 -vv arp || true
    echo "Done. For full capture use --sniff."
else
    echo "Tip: Use --sniff or --sniff-lite for ARP visibility."
fi