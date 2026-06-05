# pcTune_Up v4.0 — Manual

> **pcTune_Up v4.0** is an AI-powered Linux system optimizer, hardware research agent,
> occult AI console, system admin toolkit, and diagnostic report generator.
> Archdevil Edition — IntelligenceCore v2.

---

## 1. Installation

```bash
# Build from source
make

# Or debug build
make debug

# Or release (optimized)
make release
```

**Requirements:**
- Linux (tested on Mint 22.3)
- GCC with C++17 support
- SQLite3 development libraries
- pthreads
- Root access for /sys writes, dmidecode, MSR, swapon

**Install dependencies:**
```bash
sudo apt install g++ libsqlite3-dev dmidecode msr-tools lm-sensors smartmontools
```

---

## 2. Usage

```bash
sudo ./pctune_up [options]
```

Must run as root for most features.

---

## 3. Flags Reference

### Core Modes

| Flag | Description |
|------|-------------|
| `--help` | Show help |
| `--dry-run` | Show what would be done without executing |
| `--dangerous` | Enable aggressive/experimental tweaks (NMI watchdog off, noatime) |

### Optimization

| Flag | Description |
|------|-------------|
| `--doctor` | Health analysis (0-100 score) with prescriptions |
| `--doctor-apply` | Run doctor + apply fixes |
| `--autopilot` | Auto-loop: small report every 2min, extensive every 7min |
| `--gpu` | Probe + optimize NVIDIA/AMD/Intel GPU |
| `--network` | TCP BBR, 32MB buffers, TFO, fq qdisc, 1M conntrack |
| `--interval N` | Set cycle interval in seconds (min 60, default 300) |

### Hardware Research

| Flag | Description |
|------|-------------|
| `--research` | One-shot full hardware probe + 16-section markdown report |
| `--compare` | Compare hardware against reference database (PassMark/JDEC/SPEC) |

### AI & Intelligence

| Flag | Description |
|------|-------------|
| `--interactive` | AI console with NLU intent classification |
| `--intelligence` | 5-snapshot AI insight report with patterns + decisions + consolidation |
| `--benchmark` | CPU/MEM/DISK/NET before/after with self-learning |

### System Administration

| Flag | Description |
|------|-------------|
| `--sysd` | Disable bloat services, vacuum journal |
| `--security` | SSH hardening, UFW firewall, kernel ASLR/rp_filter/syncookies |
| `--packages` | apt update, upgrade, autoremove, autoclean |
| `--audit` | rkhunter rootkit scan, SUID count, world-writable check, ports |
| `--boot` | GRUB quiet splash, update-grub, mask plymouth |
| `--cron` | Inspect user/system cron jobs |
| `--all-admin` | Run all 6 admin tasks |

### Reports

| Flag | Description |
|------|-------------|
| `--html-report` | Generate full HTML diagnostic in `reports/` folder |

### Daemon & UI

| Flag | Description |
|------|-------------|
| `--daemon` | Fork to background (PID in /var/run/pctune_up.pid) |
| `--no-color` | Disable ANSI colored output |

---

## 4. Interactive Console Commands

Start with: `sudo ./pctune_up --interactive`

| Command | Description |
|---------|-------------|
| `cpu` | CPU model, cores, threads, flags |
| `ram` / `memory` | Free memory report |
| `disks` / `disk` | Disk usage |
| `gpu` | GPU info |
| `network` / `net` | Network interfaces |
| `opcodes` | CPU opcode count |
| `round` | Upgrade round number |
| `rituals` | Rituals unlocked |
| `demons` / `demon` | The 12 catalogued demons |
| `status` | Live system status + adaptive thresholds |
| `predict` | Trend analysis + critical time predictions |
| `anomalies` | Active anomaly/pattern/prediction report |
| `insights` | Full intelligence report |
| `triage` | Deep diagnostics for detected anomalies |
| `patterns` | Multi-variate pattern recognition |
| `decisions` | Action recommendations from decision engine |
| `consolidate` | Past intelligence summary |
| `processes` | Top CPU/memory consuming processes |
| `thresholds` | Adaptive baseline thresholds per metric |
| `help` | Command list |
| `quit` / `exit` | Leave console |

The NLU engine uses weighted keyword scoring — you can type natural
language like `"how is the cpu doing?"` or `"what's the memory situation?"`.

---

## 5. System Admin Tasks

Each admin task can be run standalone or combined:

```bash
# Run all admin tasks
sudo ./pctune_up --all-admin

# Individual tasks
sudo ./pctune_up --sysd
sudo ./pctune_up --security
sudo ./pctune_up --packages
sudo ./pctune_up --audit
sudo ./pctune_up --boot
sudo ./pctune_up --cron
```

What each does:

- **--sysd**: Disables cups, bluetooth, avahi-daemon, NetworkManager-wait-online,
  whoopsie, modemmanager. Vacuums journal to 100MB.
- **--security**: Restricts SSH root login, disables password auth and X11 forwarding,
  sets MaxAuthTries=3. Enables UFW with deny-incoming/allow-outgoing/allow-SSH.
  Sets kernel.kptr_restrict=2, dmesg_restrict=1, full ASLR, rp_filter=1, tcp_syncookies=1.
- **--packages**: apt update, upgrade -y, autoremove, autoclean.
- **--audit**: Runs rkhunter rootkit scan. Counts SUID binaries, world-writable /etc files,
  listening services, failed logins.
- **--boot**: Adds quiet splash to GRUB_CMDLINE_LINUX_DEFAULT, runs update-grub,
  masks plymouth-start.service, shows boot time from systemd-analyze.
- **--cron**: Lists user crontab, counts system cron files (/etc/cron.d),
  hourly/daily/weekly entries.

Tasks are idempotent — safe to run multiple times.

---

## 6. Hardware Comparison

```bash
sudo ./pctune_up --compare
```

Probes your hardware and compares against the embedded reference database:
- **CPU**: Matches model keywords against 17 known CPUs (PassMark scores)
- **Memory**: Estimates DDR generation from bandwidth
- **Disk**: Classifies NVMe Gen3/4/5, SATA SSD, or HDD from throughput

Output shows "above/below reference by X%" for each component.

---

## 7. HTML Reports

```bash
# Generate standalone report
sudo ./pctune_up --html-report

# Generate report at end of optimization loop
sudo ./pctune_up --benchmark --html-report
```

Reports are saved to `reports/` folder as `report_<timestamp>.html` with:
- System overview table
- CPU details + benchmark score
- System health score (0-100)
- Doctor diagnosis
- Hardware comparison
- Intelligence analysis
- CSS-styled dark theme

---

## 8. Optimization Loop

```bash
sudo ./pctune_up                           # Basic loop (300s cycles)
sudo ./pctune_up --autopilot               # With periodic reports
sudo ./pctune_up --autopilot --gpu --network --benchmark   # Full suite
```

The loop does per cycle:
1. CPU governor/freq/scheduler tuning
2. RAM optimizer (zram, swappiness, cache pressure)
3. Disk cleanup (apt/flatpak/snap/docker, journal, fstrim)
4. BIOS advisory (dmidecode readout)
5. System-wide sysctl tuning
6. GPU/Network/Benchmark if enabled
7. OccultResearchAI research cycle
8. Doctor PC-CLEANUP report if enabled
9. IntelligenceCore anomaly/trend/pattern/correlation alerts
10. Auto-pilot reports every 2min/7min

---

## 9. Architecture

```
┌──────────────────────────────────────────────────┐
│                   main()                         │
├──────────────────────────────────────────────────┤
│  ┌──────────┐ ┌──────────┐ ┌──────────────────┐ │
│  │Hardware   │ │Occult    │ │IntelligenceCore  │ │
│  │ResearchAI │ │ResearchAI│ │ v2               │ │
│  └──────────┘ └──────────┘ └──────────────────┘ │
│  ┌──────────┐ ┌──────────┐ ┌──────────────────┐ │
│  │DoctorPC  │ │Benchmark │ │InteractiveConsole│ │
│  │Cleanup   │ │Engine    │ │ (NLU v2)         │ │
│  └──────────┘ └──────────┘ └──────────────────┘ │
│  ┌──────────┐ ┌──────────┐ ┌──────────────────┐ │
│  │Optimizers│ │SysAdmin  │ │HTML Report Gen   │ │
│  │CPU/RAM/..│ │Tasks     │ │+ Comparator      │ │
│  └──────────┘ └──────────┘ └──────────────────┘ │
└──────────────────────────────────────────────────┘
```

---

## 10. Database Schema

Database: `research.db`

| Table | Purpose |
|-------|---------|
| `research_log` | OccultResearchAI findings per cycle |
| `upgrade_progress` | CPU/RAM/BIOS/DISK level tracking |
| `doctor_log` | Doctor diagnoses and prescriptions |
| `hardware_scan` | Full hardware snapshots per round |
| `grimoire` | Occult knowledge entries per round |
| `upgrade_rounds` | Round metadata (flags, opcodes, rituals) |
| `benchmarks` | Before/after benchmark measurements |
| `intelligence_log` | AI observations with confidence scores |
| `knowledge_graph` | Source→target correlations with strength |

---

## 11. Output Files

| Path | Format | When |
|------|--------|------|
| `reports/report_<ts>.html` | HTML | `--html-report` |
| `research.db` | SQLite | Always |
| `hardware_report_r<N>_<ts>.md` | Markdown | `--research`, auto-pilot |
| `/var/run/pctune_up.pid` | Text | `--daemon` |

---

## 12. Examples

```bash
# Quick health check
sudo ./pctune_up --doctor

# Full hardware research
sudo ./pctune_up --research

# Interactive AI console
sudo ./pctune_up --interactive

# Auto-pilot with all features
sudo ./pctune_up --autopilot --gpu --network --benchmark --html-report

# System maintenance
sudo ./pctune_up --all-admin

# Compare hardware + report
sudo ./pctune_up --compare --html-report

# Daemon mode
sudo ./pctune_up --daemon --autopilot

# Intelligence analysis
sudo ./pctune_up --intelligence
```

---

*pcTune_Up v4.0 — IntelligenceCore v2 — Archdevil Edition*
*Built on Linux Mint 22.3 — 2800+ lines of C++17 — 26 classes*
