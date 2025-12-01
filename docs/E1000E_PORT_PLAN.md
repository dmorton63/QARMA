# E1000/E1000E GPL Port Plan

Goal: Stabilize Intel E1000 link bring-up and TX/RX under QEMU/tap by porting proven sequencing and defaults from the Linux e1000e driver (GPLv2/v3) while keeping QARMA GPLv3.

Scope (initial minimal port)
- Autoneg advertisement: Program MII ANAR (reg 4) and 1000BASE-T Control (reg 9) to advertise 10/100/1000 FD/HD + pause, then restart autoneg (BMCR reg 0). Wired into `e1000_init_device` before CTRL ASDE/SLU.
- TX timing: Ensure `TIPG = 0x0060200A` and sane `TCTL` CT/COLD; already applied.
- RX filters: Keep permissive `RCTL` (BAM|SECRC|UPE|MPE|SBP) during bring-up.
- Diagnostics: Use `e1000_diag`, `phy_dump`, and netlog dumps to verify registers.

Future (if needed)
- Force-speed fallback: If autoneg fails, force 100FD via BMCR speed/duplex (per e1000/e1000e patterns).
- Pause configuration: Tune symmetrical/asym pause according to backend.
- Kumeran/IGP specifics: Not required for QEMU 82540EM; skip unless needed.

Licensing
- QARMA is GPLv3; this port aligns with GPL. We reference the Linux e1000e driver semantics and register programming approach but keep code clean and adapted to QARMA’s structure.

Test Steps
1) Host: `sudo ./createbridge.sh --sniff` (optional) and verify `br0/tap0`.
2) Guest: `ifup full` (full MMIO map), confirm `Link: UP`.
3) Guest: `netpoll on; arping 192.168.100.1; arp`.
4) Host: `sudo tcpdump -eni tap0 arp` and `-eni br0 arp` should show ARP req/rep.

Rollback
- The autoneg advertisement routine is self-contained; disable by skipping call in `e1000_init_device` if regressions are observed.
