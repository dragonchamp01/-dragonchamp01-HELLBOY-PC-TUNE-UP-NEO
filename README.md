# pcTune_Up v2.0 - AI Super PC TuneUp

Linux System Optimizer + Occult Research AI.

A C++ program that performs software-level optimizations on Linux systems and uses an AI research engine (with SQLite persistence) to simulate researching hardware upgrades.

## What It Does

**System Optimization (Software):**
- CPU governor to performance, max frequency, all cores online
- RAM: drop caches, swappiness=10, dirty page tuning
- Disk: apt clean, journal vacuum, fstrim, log cleanup
- Kernel: max_map_count, file-max, shmmax, network backlog

**AI Research Module:**
- SQLite database (`research.db`) storing research findings per component
- Researches CPU, RAM, BIOS, Disk upgrades each cycle
- Progress tracked per component (0-100%) in SQLite
- Themed occult output (curses, demon invocations, ancient rituals)

## Requirements

- Linux (tested on Linux Mint 22.3 / Ubuntu 24.04)
- g++ with C++17 support
- libsqlite3-dev
- Root privileges (for system modifications)

## Building

```bash
make
```

## Usage

```bash
sudo ./pctune_up
```

The program loops indefinitely with 10-minute cycles. Each cycle:
1. Applies system optimizations
2. Runs AI research on all components
3. Saves findings to SQLite database
4. Shows research progress summary
5. Sleeps 10 minutes

Stop anytime with Ctrl+C.

## Database

The SQLite database is stored at:
`/media/marco/e-learning/coding_projects/ai_projects/pctune_up/research.db`

Tables:
- `research_log` -- individual research findings per cycle
- `upgrade_progress` -- current upgrade level per component

## Safety Notes

- All optimizations are standard reversible Linux kernel/OS tweaks
- No physical hardware is damaged (the occult research is flavor/entertainment)
- You can use your PC normally while it runs

## License

MIT
