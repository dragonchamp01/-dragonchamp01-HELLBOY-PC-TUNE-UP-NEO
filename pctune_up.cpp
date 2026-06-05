#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sqlite3.h>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include <map>
#include <iomanip>
#include <cstring>
#include <climits>
#include <cmath>
#include <deque>

using namespace std;

// =========================================================================
// ANSI Colors
// =========================================================================
const string C_RED    = "\033[1;31m";
const string C_GREEN  = "\033[1;32m";
const string C_YELLOW = "\033[1;33m";
const string C_BLUE   = "\033[1;34m";
const string C_MAGENTA= "\033[1;35m";
const string C_CYAN   = "\033[1;36m";
const string C_RESET  = "\033[0m";
const string C_BOLD   = "\033[1m";

// =========================================================================
// Config
// =========================================================================
struct Config {
    bool dryRun       = false;
    bool dangerous    = false;
    bool doctorMode   = false;
    bool applyDoctor  = false;
    bool autoPilot    = false;
    bool researchMode = false;
    bool gpuMode      = false;
    bool netMode      = false;
    bool benchmarkMode = false;
    bool interactive  = false;
    bool dashboard    = false;
    bool daemonMode   = false;
    bool intelligenceMode = false;
    bool adminMode    = false;
    bool sysdMode     = false;
    bool secMode      = false;
    bool pkgMode      = false;
    bool auditMode    = false;
    bool bootMode     = false;
    bool cronMode     = false;
    bool compareMode  = false;
    bool htmlReport   = false;
    int  cycleInt     = 300;
    int  hbInt        = 60;
    bool color        = true;
};

Config cfg;

// =========================================================================
// Signal handler
// =========================================================================
atomic<bool> running{true};

void handleSignal(int) {
    cout << "\n[!] Shutdown signal received. Cleaning up..." << endl;
    running = false;
}

// =========================================================================
// Color helper
// =========================================================================
string col(const string& t, const string& c) {
    if (!cfg.color) return t;
    return c + t + C_RESET;
}

string ts() {
    time_t now = time(nullptr);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    return string(buf);
}

// =========================================================================
// Command execution
// =========================================================================
void runCmdSilent(const string& cmd) {
    if (cfg.dryRun) return;
    int rc = system(cmd.c_str());
    (void)rc;
}

void runCmd(const string& cmd, const string& desc) {
    cout << col("[", C_CYAN) << ts() << col("]", C_CYAN) << " "
         << desc << "..." << endl;
    if (cfg.dryRun) {
        cout << col("  [DRY-RUN] Would run: ", C_YELLOW) << cmd << endl;
        return;
    }
    int r = system(cmd.c_str());
    if (r != 0)
        cout << col("  [!] Returned: ", C_RED) << r << " (non-fatal)" << endl;
}

string captureCmd(const string& cmd) {
    string r;
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return "";
    char buf[256];
    while (fgets(buf, sizeof(buf), p)) r += buf;
    pclose(p);
    return r;
}

bool isRoot() { return geteuid() == 0; }
bool fileExists(const string& p) { struct stat st; return stat(p.c_str(), &st) == 0; }

bool writeFile(const string& path, const string& content) {
    if (cfg.dryRun) {
        cout << col("  [DRY-RUN] Write ", C_YELLOW) << path << ": " << content << endl;
        return true;
    }
    ofstream f(path);
    if (!f) return false;
    f << content;
    return true;
}

// =========================================================================
// Database
// =========================================================================
sqlite3* db = nullptr;
const string DB_PATH = "/media/marco/e-learning/coding_projects/ai_projects/pctune_up/research.db";

void initDatabase() {
    string dir = "/media/marco/e-learning/coding_projects/ai_projects/pctune_up";
    string mk = "mkdir -p \"" + dir + "\"";
    (void)system(mk.c_str());

    if (sqlite3_open(DB_PATH.c_str(), &db) != SQLITE_OK) {
        cerr << col("[!] FAILED to open database: ", C_RED) << sqlite3_errmsg(db) << endl;
        return;
    }
    cout << col("[OK] Database: ", C_GREEN) << DB_PATH << endl;

    const char* sql =
        "CREATE TABLE IF NOT EXISTS research_log ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  component TEXT NOT NULL,"
        "  topic TEXT NOT NULL,"
        "  finding TEXT NOT NULL,"
        "  curse_level INTEGER DEFAULT 0,"
        "  cycle INTEGER DEFAULT 0,"
        "  timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"
        "CREATE TABLE IF NOT EXISTS upgrade_progress ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  component TEXT UNIQUE NOT NULL,"
        "  current_level INTEGER DEFAULT 0,"
        "  target_level INTEGER DEFAULT 100,"
        "  progress_percent REAL DEFAULT 0.0,"
        "  last_researched DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"
        "CREATE TABLE IF NOT EXISTS doctor_log ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  diagnosis TEXT NOT NULL,"
        "  health_score INTEGER DEFAULT 0,"
        "  prescriptions TEXT,"
        "  timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";

    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        cerr << col("[!] SQL: ", C_RED) << err << endl;
        sqlite3_free(err);
    }

    const char* init =
        "INSERT OR IGNORE INTO upgrade_progress (component,current_level,target_level,progress_percent) VALUES "
        "('CPU',0,100,0.0),('RAM',0,100,0.0),('BIOS',0,100,0.0),('DISK',0,100,0.0);";
    err = nullptr;
    if (sqlite3_exec(db, init, nullptr, nullptr, &err) != SQLITE_OK) {
        cerr << col("[!] SQL: ", C_RED) << err << endl;
        sqlite3_free(err);
    }
}

void closeDatabase() { if (db) sqlite3_close(db); }

void saveResearch(const string& comp, const string& topic, const string& finding, int cl, int cyc) {
    if (!db) return;
    sqlite3_stmt* s;
    const char* sql = "INSERT INTO research_log (component,topic,finding,curse_level,cycle) VALUES (?,?,?,?,?);";
    if (sqlite3_prepare_v2(db, sql, -1, &s, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(s, 1, comp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, topic.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, finding.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s, 4, cl);
    sqlite3_bind_int(s, 5, cyc);
    sqlite3_step(s);
    sqlite3_finalize(s);
}

void updateProgress(const string& comp, int add, int tgt) {
    if (!db) return;
    sqlite3_stmt* s;
    const char* sql = "UPDATE upgrade_progress SET current_level=MIN(current_level+?,target_level), progress_percent=(CAST(current_level+? AS REAL)/CAST(? AS REAL))*100.0, last_researched=CURRENT_TIMESTAMP WHERE component=?;";
    if (sqlite3_prepare_v2(db, sql, -1, &s, nullptr) != SQLITE_OK) return;
    sqlite3_bind_int(s, 1, add);
    sqlite3_bind_int(s, 2, add);
    sqlite3_bind_int(s, 3, tgt);
    sqlite3_bind_text(s, 4, comp.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(s);
    sqlite3_finalize(s);
}

void showResearchSummary() {
    if (!db) return;
    cout << "\n--- " << col("Research Database Summary", C_CYAN) << " ---" << endl;
    sqlite3_stmt* s;
    const char* sql = "SELECT component,current_level,target_level,ROUND(progress_percent,1) FROM upgrade_progress ORDER BY component;";
    if (sqlite3_prepare_v2(db, sql, -1, &s, nullptr) != SQLITE_OK) return;
    while (sqlite3_step(s) == SQLITE_ROW) {
        string c = (const char*)sqlite3_column_text(s, 0);
        int cur = sqlite3_column_int(s, 1);
        int tgt = sqlite3_column_int(s, 2);
        double pct = sqlite3_column_double(s, 3);
        cout << "  " << col(c, C_BOLD) << ": " << cur << "/" << tgt
             << " (" << col(to_string(pct)+"%", C_GREEN) << ")" << endl;
    }
    sqlite3_finalize(s);

    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM research_log;", -1, &s, nullptr);
    if (sqlite3_step(s) == SQLITE_ROW)
        cout << "  Total findings: " << col(to_string(sqlite3_column_int(s, 0)), C_MAGENTA) << endl;
    sqlite3_finalize(s);
}

// =========================================================================
// CPU Optimization
// =========================================================================
void optimizeCPU() {
    cout << "\n=== " << col("CPU Optimization", C_BLUE) << " ===" << endl;

    // Governor to performance
    runCmd("echo performance | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null 2>&1",
           "Setting CPU governor to performance");

    // Ensure all cores online
    runCmd("for cpu in /sys/devices/system/cpu/cpu[0-9]*; do echo 1 > $cpu/online 2>/dev/null; done",
           "Ensuring all CPU cores online");

    // Max freq both min and max
    runCmd("for cpu in /sys/devices/system/cpu/cpu[0-9]*; do "
           "f=$(cat $cpu/cpufreq/cpuinfo_max_freq 2>/dev/null); "
           "[ -n \"$f\" ] && echo $f > $cpu/cpufreq/scaling_min_freq 2>/dev/null; done",
           "Setting CPU min frequency to max");
    runCmd("for cpu in /sys/devices/system/cpu/cpu[0-9]*; do "
           "f=$(cat $cpu/cpufreq/cpuinfo_max_freq 2>/dev/null); "
           "[ -n \"$f\" ] && echo $f > $cpu/cpufreq/scaling_max_freq 2>/dev/null; done",
           "Setting CPU max frequency to max");

    // Energy performance bias
    runCmd("for cpu in /sys/devices/system/cpu/cpu[0-9]*; do "
           "[ -f $cpu/power/energy_perf_bias ] && echo performance > $cpu/power/energy_perf_bias 2>/dev/null; done",
           "Setting energy performance bias to performance");

    // Scheduler tuning
    runCmd("sysctl kernel.sched_min_granularity_ns=10000000 >/dev/null 2>&1",
           "Tuning scheduler min granularity");
    runCmd("sysctl kernel.sched_wakeup_granularity_ns=15000000 >/dev/null 2>&1",
           "Tuning scheduler wakeup granularity");
    runCmd("sysctl kernel.sched_migration_cost_ns=500000 >/dev/null 2>&1",
           "Tuning scheduler migration cost");

    // IRQ balance
    runCmd("systemctl start irqbalance 2>/dev/null || irqbalance --oneshot 2>/dev/null",
           "Starting IRQ balancer");

    // Danger: disable mitigations
    if (cfg.dangerous) {
        runCmd("sysctl kernel.nmi_watchdog=0 >/dev/null 2>&1",
               "[DANGER] Disabling NMI watchdog");
        runCmd("sysctl kernel.perf_cpu_time_max_percent=0 >/dev/null 2>&1",
               "[DANGER] Disabling perf CPU time limit");
        runCmd("echo 0 | tee /sys/kernel/debug/sched_features >/dev/null 2>&1",
               "[DANGER] Disabling scheduler features");
    }

    // CPU info
    string cpuModel = captureCmd("cat /proc/cpuinfo | grep -m1 'model name' | cut -d: -f2");
    cout << col("  CPU Model:", C_YELLOW) << cpuModel;
    string cpuCores = captureCmd("nproc --all 2>/dev/null");
    cout << col("  Cores:", C_YELLOW) << " " << cpuCores.substr(0, cpuCores.find('\n'));
    string cpuFreq = captureCmd("cat /proc/cpuinfo | grep -m1 'cpu MHz' | cut -d: -f2");
    cout << col("  Current Freq:", C_YELLOW) << cpuFreq;
}

// =========================================================================
// RAM Optimization — auto-detects 8GB phys, 3GB zram cap, 18GB swap pool
// =========================================================================
void optimizeRAM() {
    cout << "\n=== " << col("RAM Optimization", C_BLUE) << " ===" << endl;

    // Detect physical RAM
    string memTotalStr = captureCmd("grep MemTotal /proc/meminfo | awk '{print $2}' 2>/dev/null");
    long memTotalKB = 0;
    if (!memTotalStr.empty()) memTotalKB = stol(memTotalStr);
    double memTotalGB = memTotalKB / (1024.0 * 1024.0);

    // Detect existing swap total
    string swapTotalStr = captureCmd("grep SwapTotal /proc/meminfo | awk '{print $2}' 2>/dev/null");
    long swapTotalKB = 0;
    if (!swapTotalStr.empty()) swapTotalKB = stol(swapTotalStr);
    double swapTotalGB = swapTotalKB / (1024.0 * 1024.0);

    cout << col("  Detected:", C_YELLOW) << " " << fixed << setprecision(1)
         << memTotalGB << "GB RAM | " << swapTotalGB << "GB swap currently" << endl;

    // Drop caches + compact
    runCmd("sync && echo 3 > /proc/sys/vm/drop_caches 2>/dev/null",
           "Dropping page caches (level 3)");
    runCmd("sync && echo 1 > /proc/sys/vm/compact_memory 2>/dev/null",
           "Compacting memory");

    // Swappiness tuned for 8GB RAM + 18GB swap: prefer swap a bit more
    // to keep RAM free for active processes
    runCmd("sysctl vm.swappiness=60 >/dev/null 2>&1",
           "Setting swappiness=60 (swap-aware for 8GB RAM + 18GB swap)");
    runCmd("echo 60 > /proc/sys/vm/swappiness 2>/dev/null",
           "Writing swappiness=60");

    // Swap tendency: prefer reclaim over cache pressure
    runCmd("sysctl vm.vfs_cache_pressure=200 >/dev/null 2>&1",
           "Setting VFS cache pressure=200 (reclaim cache pages more aggressively)");

    // Overcommit
    runCmd("sysctl vm.overcommit_memory=1 >/dev/null 2>&1",
           "Setting overcommit_memory=1");
    runCmd("echo 1 > /proc/sys/vm/overcommit_memory 2>/dev/null",
           "Writing overcommit_memory=1");

    // Dirty page tuning for swap-heavy system
    runCmd("sysctl vm.dirty_ratio=30 vm.dirty_background_ratio=10 >/dev/null 2>&1",
           "Tuning dirty page ratios (30/10 for swap use)");
    runCmd("sysctl vm.dirty_writeback_centisecs=3000 >/dev/null 2>&1",
           "Setting dirty writeback to 30s (less frequent writes)");
    runCmd("sysctl vm.dirty_expire_centisecs=6000 >/dev/null 2>&1",
           "Setting dirty expire to 60s");

    // ZRAM: 3GB max RAM compression (your spec)
    string zramExists = captureCmd("lsmod | grep zram 2>/dev/null");
    if (zramExists.empty())
        runCmd("modprobe zram 2>/dev/null", "Loading zram kernel module");
    string zramActive = captureCmd("zramctl 2>/dev/null | grep -c 'zram'");
    if (zramActive.empty() || zramActive == "0") {
        // Create zram device with 3GB compressed swap in RAM
        runCmd("zramctl -f --size 3G 2>/dev/null || "
               "echo 3221225472 > /sys/block/zram0/disksize 2>/dev/null",
               "Creating 3GB zram device (compressed RAM swap)");
        // Priority higher than disk swap so compressed RAM is used first
        runCmd("mkswap /dev/zram0 2>/dev/null && swapon -p 100 /dev/zram0 2>/dev/null",
               "Activating zram with priority 100");
    } else {
        cout << col("  [OK]", C_GREEN) << " ZRAM already active" << endl;
    }

    // Ensure traditional swap is on too (your 18GB pool)
    string diskSwapActive = captureCmd("swapon --show 2>/dev/null | grep -v zram | tail -n+2 | head -1");
    if (diskSwapActive.empty()) {
        runCmd("swapon -a 2>/dev/null", "Activating all disk swap (your 18GB pool)");
    }

    // Swap info — show all
    cout << col("  Swap devices:", C_YELLOW) << endl;
    string allSwap = captureCmd("swapon --show 2>/dev/null | column -t");
    if (allSwap.empty()) allSwap = captureCmd("cat /proc/swaps 2>/dev/null");
    cout << allSwap << endl;

    // HugePages if dangerous
    if (cfg.dangerous && memTotalKB > 0) {
        long huge = memTotalKB / (1024 * 2); // 2MB per huge page, ~half of RAM
        runCmd("sysctl vm.nr_hugepages=" + to_string(huge) + " >/dev/null 2>&1",
               "[DANGER] Setting hugepages to " + to_string(huge / 512) + "GB");
    }

    runCmd("free -h", "Memory status after");
}

// =========================================================================
// Disk Optimization
// =========================================================================
void optimizeDisk() {
    cout << "\n=== " << col("Disk Optimization", C_BLUE) << " ===" << endl;

    runCmd("df -h /", "Disk usage before");

    // Package caches
    runCmd("apt-get clean -y 2>/dev/null", "Cleaning apt cache");
    runCmd("apt-get autoclean -y 2>/dev/null", "Cleaning old packages");
    runCmd("apt-get autoremove -y 2>/dev/null", "Removing orphan packages");

    // Flatpak
    runCmd("flatpak uninstall --unused -y 2>/dev/null",
           "Cleaning unused flatpak runtimes");

    // Snap
    runCmd("snap list 2>/dev/null | awk 'NR>1 {print $1}' | xargs -r snap remove --revision 2>/dev/null",
           "Cleaning old snap revisions");

    // Docker
    runCmd("docker system prune -f 2>/dev/null", "Pruning docker system");

    // Logs
    runCmd("journalctl --vacuum-time=3d 2>/dev/null",
           "Vacuuming journal (keep 3 days)");
    runCmd("find /var/log -type f \\( -name '*.log' -o -name '*.gz' \\) -exec truncate -s 0 {} \\; 2>/dev/null",
           "Truncating old log files");

    // Trim
    runCmd("fstrim -av 2>/dev/null", "Running fstrim on SSDs");

    // Dirty writeback tuning
    runCmd("sysctl vm.dirty_writeback_centisecs=1000 >/dev/null 2>&1",
           "Setting dirty writeback interval");
    runCmd("sysctl vm.dirty_expire_centisecs=500 >/dev/null 2>&1",
           "Setting dirty expire interval");

    // Remount noatime if dangerous
    if (cfg.dangerous) {
        string mounts = captureCmd("mount | grep ' / ' | grep -v noatime 2>/dev/null");
        if (!mounts.empty()) {
            runCmd("mount -o remount,noatime,nodiratime / 2>/dev/null",
                   "[DANGER] Remounting root with noatime");
        }
    }

    runCmd("df -h /", "Disk usage after");
}

// =========================================================================
// BIOS Advisory
// =========================================================================
void advisoryBIOS() {
    cout << "\n=== " << col("BIOS Advisory", C_BLUE) << " ===" << endl;

    string biosVendor = captureCmd("dmidecode -t bios 2>/dev/null | grep -i 'Vendor:' | head -1 | cut -d: -f2");
    string biosVersion = captureCmd("dmidecode -t bios 2>/dev/null | grep -i 'Version:' | head -1 | cut -d: -f2");
    string biosDate = captureCmd("dmidecode -t bios 2>/dev/null | grep -i 'Release Date:' | head -1 | cut -d: -f2");
    string boardVendor = captureCmd("dmidecode -t baseboard 2>/dev/null | grep -i 'Manufacturer:' | head -1 | cut -d: -f2");
    string boardName = captureCmd("dmidecode -t baseboard 2>/dev/null | grep -i 'Product Name:' | head -1 | cut -d: -f2");

    cout << col("  BIOS Vendor:", C_YELLOW) << (biosVendor.empty() ? " N/A" : biosVendor) << endl;
    cout << col("  BIOS Version:", C_YELLOW) << (biosVersion.empty() ? " N/A" : biosVersion) << endl;
    cout << col("  BIOS Date:", C_YELLOW) << (biosDate.empty() ? " N/A" : biosDate) << endl;
    cout << col("  Motherboard:", C_YELLOW) << (boardVendor.empty() ? " N/A" : boardVendor)
         << (boardName.empty() ? "" : " " + boardName) << endl;

    // fwupd check
    string fwupd = captureCmd("which fwupdmgr 2>/dev/null");
    if (!fwupd.empty()) {
        runCmd("fwupdmgr refresh 2>/dev/null", "Refreshing firmware metadata");
        runCmd("fwupdmgr get-updates 2>/dev/null",
               "Checking for firmware updates");
    } else {
        cout << col("  [!] Install fwupdmgr for firmware update checks: ", C_YELLOW)
             << "sudo apt install fwupd" << endl;
    }

    // Advisory checklist
    cout << "\n" << col("  BIOS Optimization Checklist:", C_BOLD) << endl;
    cout << "    1. Enable XMP/DOCP for rated RAM speed" << endl;
    cout << "    2. Enable Above 4G Decoding / Resizable BAR" << endl;
    cout << "    3. Disable CSM / Enable UEFI boot" << endl;
    cout << "    4. Set power profile to Performance" << endl;
    cout << "    5. Disable unused peripherals (serial/parallel ports)" << endl;
    cout << "    6. Enable VT-d / SVM (if needed for VMs)" << endl;
    cout << "    7. Check for BIOS updates at motherboard vendor website" << endl;
}

// =========================================================================
// System Optimization
// =========================================================================
void optimizeSystem() {
    cout << "\n=== " << col("System Optimization", C_BLUE) << " ===" << endl;

    runCmd("sysctl vm.max_map_count=262144 >/dev/null 2>&1",
           "Setting max map count");
    runCmd("sysctl fs.file-max=2097152 >/dev/null 2>&1",
           "Setting max file handles");
    runCmd("sysctl kernel.shmmax=1073741824 >/dev/null 2>&1",
           "Setting max shared memory segment (1GB)");
    runCmd("sysctl kernel.shmall=268435456 >/dev/null 2>&1",
           "Setting total shared memory pages");
    runCmd("sysctl net.core.netdev_max_backlog=5000 >/dev/null 2>&1",
           "Increasing network backlog");
    runCmd("sysctl net.core.somaxconn=4096 >/dev/null 2>&1",
           "Increasing socket listen backlog");
    runCmd("sysctl vm.swappiness=10 >/dev/null 2>&1",
           "Setting swappiness");
}

// =========================================================================
// GPU Demonology — probe + optimize the visual cortex of the beast
// =========================================================================
void probeGPU() {
    cout << "\n=== " << col("GPU Demonology", C_BLUE) << " ===" << endl;

    string lspciGpu = captureCmd("lspci 2>/dev/null | grep -iE 'VGA|3D|Display'");
    cout << col("  Detected:", C_YELLOW) << "\n" << lspciGpu;

    // NVIDIA
    string nvidia = captureCmd("nvidia-smi --query-gpu=name,temp.gpu,utilization.gpu,memory.total --format=csv,noheader 2>/dev/null");
    if (!nvidia.empty()) {
        cout << col("  NVIDIA:", C_YELLOW) << "\n" << nvidia;
        return;
    }
    // AMD
    string amd = captureCmd("cat /sys/class/drm/card0/device/vendor 2>/dev/null");
    if (amd.find("1002") != string::npos) {
        string amdName = captureCmd("cat /sys/class/drm/card0/device/product_name 2>/dev/null");
        string amdPerf = captureCmd("cat /sys/class/drm/card0/device/power_dpm_force_performance_level 2>/dev/null");
        cout << col("  AMD GPU:", C_YELLOW) << " " << (amdName.empty() ? "AMD Radeon" : amdName)
             << "\n  Power level: " << (amdPerf.empty() ? "default" : amdPerf) << endl;
    }
    // Intel
    string intelGpu = captureCmd("cat /sys/class/drm/card0/gt_cur_freq_mhz 2>/dev/null");
    if (!intelGpu.empty()) {
        string intelMax = captureCmd("cat /sys/class/drm/card0/gt_max_freq_mhz 2>/dev/null");
        string intelMin = captureCmd("cat /sys/class/drm/card0/gt_min_freq_mhz 2>/dev/null");
        cout << col("  Intel GPU:", C_YELLOW) << " cur=" << intelGpu.substr(0,4)
             << "MHz max=" << intelMax.substr(0,4) << "MHz min=" << intelMin.substr(0,4) << "MHz" << endl;
    }

    // Fallback: DRI info
    string drm = captureCmd("for f in /sys/class/drm/card*/device/vendor 2>/dev/null; do echo \"$f: $(cat $f 2>/dev/null)\"; done");
    if (!drm.empty()) cout << col("  DRM:", C_YELLOW) << endl << drm;
}

void optimizeGPU() {
    cout << "\n=== " << col("GPU Optimization", C_BLUE) << " ===" << endl;

    // NVIDIA
    runCmd("nvidia-smi -pm 1 2>/dev/null", "[NVIDIA] Persistent mode enabled");
    runCmd("nvidia-smi -ac 2100,1530 2>/dev/null || nvidia-smi -ac 715,1500 2>/dev/null",
           "[NVIDIA] Setting max clocks (may fail on some GPUs)");
    runCmd("nvidia-smi -pl 80 2>/dev/null || nvidia-smi -pl 95 2>/dev/null",
           "[NVIDIA] Setting power limit");

    // AMD
    runCmd("echo high > /sys/class/drm/card0/device/power_dpm_force_performance_level 2>/dev/null",
           "[AMD] Force high performance level");
    runCmd("echo manual > /sys/class/drm/card0/device/power_dpm_force_performance_level 2>/dev/null",
           "[AMD] Manual power control");

    // Intel
    string intelMax = captureCmd("cat /sys/class/drm/card0/gt_max_freq_mhz 2>/dev/null");
    if (!intelMax.empty()) {
        runCmd("echo " + intelMax.substr(0, intelMax.find('\n')) + " > /sys/class/drm/card0/gt_min_freq_mhz 2>/dev/null",
               "[Intel] Locking min GPU freq to max");
    }

    // Generic DRM
    runCmd("for f in /sys/class/drm/card*/device/power_dpm_force_performance_level 2>/dev/null; do echo high > $f 2>/dev/null; done",
           "Setting all DRM devices to high performance");

    cout << col("  [GPU]", C_GREEN) << " GPU optimization applied" << endl;
}

// =========================================================================
// Network Abyss Tuning — open portals to the outer darkness
// =========================================================================
void optimizeNetwork() {
    cout << "\n=== " << col("Network Abyss Tuning", C_BLUE) << " ===" << endl;

    // Show interfaces
    string ifaces = captureCmd("ip -br addr 2>/dev/null | head -10");
    cout << col("  Interfaces:", C_YELLOW) << "\n" << ifaces << endl;

    // TCP congestion control: try BBR first
    string bbrAvail = captureCmd("sysctl net.ipv4.tcp_available_congestion_control 2>/dev/null | grep -o bbr");
    if (!bbrAvail.empty())
        runCmd("sysctl net.ipv4.tcp_congestion_control=bbr >/dev/null 2>&1",
               "Setting TCP congestion control to BBR (Bottleneck Bandwidth RTT)");
    else
        runCmd("sysctl net.ipv4.tcp_congestion_control=cubic >/dev/null 2>&1",
               "BBR not available; setting TCP to cubic");

    // Buffer sizes — double defaults for abyss throughput
    runCmd("sysctl net.core.rmem_default=524288 >/dev/null 2>&1", "Setting rmem_default=512KB");
    runCmd("sysctl net.core.wmem_default=524288 >/dev/null 2>&1", "Setting wmem_default=512KB");
    runCmd("sysctl net.core.rmem_max=33554432 >/dev/null 2>&1", "Setting rmem_max=32MB");
    runCmd("sysctl net.core.wmem_max=33554432 >/dev/null 2>&1", "Setting wmem_max=32MB");
    runCmd("sysctl net.core.netdev_budget=600 >/dev/null 2>&1", "Setting netdev_budget=600");
    runCmd("sysctl net.core.netdev_budget_usecs=8000 >/dev/null 2>&1", "Setting netdev_budget_usecs=8000");

    // TCP optimizations
    runCmd("sysctl net.ipv4.tcp_rmem='4096 131072 33554432' >/dev/null 2>&1", "TCP rmem: 4K-128K-32MB");
    runCmd("sysctl net.ipv4.tcp_wmem='4096 65536 33554432' >/dev/null 2>&1", "TCP wmem: 4K-64K-32MB");
    runCmd("sysctl net.ipv4.tcp_mtu_probing=1 >/dev/null 2>&1", "Enabling TCP MTU probing");
    runCmd("sysctl net.ipv4.tcp_slow_start_after_idle=0 >/dev/null 2>&1", "Disabling slow start after idle");
    runCmd("sysctl net.ipv4.tcp_fastopen=3 >/dev/null 2>&1", "Enabling TCP Fast Open");

    // Queue discipline: fq (fair queue) for BBR
    string haveFq = captureCmd("tc qdisc show 2>/dev/null | grep -o fq | head -1");
    if (haveFq.empty())
        runCmd("tc qdisc add dev lo root fq 2>/dev/null; for iface in $(ip -br l | awk '{print $1}'); do tc qdisc add dev $iface root fq 2>/dev/null; done",
               "Setting fair queue (fq) qdisc on all interfaces");

    // Connection tracking
    runCmd("sysctl net.netfilter.nf_conntrack_max=1048576 >/dev/null 2>&1", "Setting conntrack max to 1M");
    runCmd("sysctl net.ipv4.netfilter.ip_conntrack_tcp_timeout_established=86400 >/dev/null 2>&1",
           "Setting TCP conntrack timeout to 24h");
}

// =========================================================================
// System Admin Engine — systemd, security, packages, audit, boot, cron
// =========================================================================
void optimizeSystemd() {
    cout << "\n=== " << col("Systemd Optimization", C_BLUE) << " ===" << endl;
    vector<string> bloat = {
        "cups.service", "bluetooth.service", "avahi-daemon.service",
        "NetworkManager-wait-online.service", "whoopsie.service",
        "modemmanager.service", "cups-browsed.service"
    };
    for (auto& s : bloat) {
        string status = captureCmd("systemctl is-enabled " + s + " 2>/dev/null");
        if (status.find("enabled") != string::npos) {
            runCmd("systemctl disable " + s + " 2>/dev/null", "[systemd] Disable " + s);
        }
    }
    runCmd("systemctl mask systemd-journald-audit.socket 2>/dev/null", "[systemd] Mask audit socket");
    runCmd("journalctl --vacuum-size=100M 2>/dev/null", "[systemd] Journal vacuum to 100M");
    cout << col("  [systemd]", C_GREEN) << " Systemd optimization complete" << endl;
}

void optimizeSecurity() {
    cout << "\n=== " << col("Security Hardening", C_BLUE) << " ===" << endl;
    // SSH hardening
    string sshdCfg = captureCmd("cat /etc/ssh/sshd_config 2>/dev/null");
    if (!sshdCfg.empty()) {
        runCmd("sed -i 's/#PermitRootLogin.*/PermitRootLogin prohibit-password/' /etc/ssh/sshd_config 2>/dev/null",
               "[SSH] Root login restrict");
        runCmd("sed -i 's/#PasswordAuthentication.*/PasswordAuthentication no/' /etc/ssh/sshd_config 2>/dev/null",
               "[SSH] Password auth disable (key only)");
        runCmd("sed -i 's/#X11Forwarding.*/X11Forwarding no/' /etc/ssh/sshd_config 2>/dev/null",
               "[SSH] X11 forwarding disable");
        runCmd("sed -i 's/#MaxAuthTries.*/MaxAuthTries 3/' /etc/ssh/sshd_config 2>/dev/null",
               "[SSH] Max auth tries 3");
        runCmd("systemctl restart sshd 2>/dev/null || systemctl restart ssh 2>/dev/null",
               "[SSH] Restart service");
    } else cout << col("  [SSH] No SSH config found", C_YELLOW) << endl;
    // Firewall
    string ufwStat = captureCmd("ufw status 2>/dev/null | head -1");
    if (ufwStat.find("inactive") != string::npos) {
        runCmd("ufw --force enable 2>/dev/null", "[Firewall] Enable UFW");
        runCmd("ufw default deny incoming 2>/dev/null", "[Firewall] Deny incoming by default");
        runCmd("ufw default allow outgoing 2>/dev/null", "[Firewall] Allow outgoing by default");
        runCmd("ufw allow ssh 2>/dev/null", "[Firewall] Allow SSH");
    } else cout << col("  [UFW] Already active or not available", C_YELLOW) << endl;
    // Kernel hardening
    runCmd("sysctl -w kernel.kptr_restrict=2 >/dev/null 2>&1", "[Kernel] Restrict kernel pointers");
    runCmd("sysctl -w kernel.dmesg_restrict=1 >/dev/null 2>&1", "[Kernel] Restrict dmesg");
    runCmd("sysctl -w kernel.randomize_va_space=2 >/dev/null 2>&1", "[Kernel] Full ASLR");
    runCmd("sysctl -w net.ipv4.conf.all.rp_filter=1 >/dev/null 2>&1", "[Kernel] Reverse path filter");
    runCmd("sysctl -w net.ipv4.tcp_syncookies=1 >/dev/null 2>&1", "[Kernel] TCP syncookies");
    cout << col("  [SECURITY]", C_GREEN) << " Security hardening complete" << endl;
}

void updatePackages() {
    cout << "\n=== " << col("Package Update", C_BLUE) << " ===" << endl;
    runCmd("apt update 2>/dev/null", "[APT] Update package lists");
    runCmd("apt upgrade -y 2>/dev/null", "[APT] Upgrade packages");
    runCmd("apt autoremove -y 2>/dev/null", "[APT] Autoremove unused");
    runCmd("apt autoclean 2>/dev/null", "[APT] Autoclean cache");
    cout << col("  [PACKAGES]", C_GREEN) << " System packages updated" << endl;
}

void auditSystem() {
    cout << "\n=== " << col("System Audit", C_BLUE) << " ===" << endl;
    // Rootkit check
    string rkhunter = captureCmd("which rkhunter 2>/dev/null");
    if (!rkhunter.empty())
        runCmd("rkhunter --check --skip-keypress 2>/dev/null | tail -5", "[Audit] rkhunter scan");
    else cout << col("  [Audit] rkhunter not installed (apt install rkhunter)", C_YELLOW) << endl;
    // Check SUID files
    string suidFiles = captureCmd("find /usr/bin /usr/sbin /bin /sbin -perm -4000 2>/dev/null | wc -l");
    cout << "  SUID binaries: " << (suidFiles.empty() ? "N/A" : suidFiles) << endl;
    // Check world-writable files
    string worldW = captureCmd("find /etc -perm -o+w -type f 2>/dev/null | wc -l");
    cout << "  World-writable in /etc: " << (worldW.empty() ? "N/A" : worldW) << endl;
    // Check listening ports
    string listeners = captureCmd("ss -tuln 2>/dev/null | grep LISTEN | wc -l");
    cout << "  Listening services: " << (listeners.empty() ? "N/A" : listeners) << endl;
    // Failed logins
    string failed = captureCmd("lastb 2>/dev/null | wc -l");
    cout << "  Failed logins (last): " << (failed.empty() ? "N/A" : failed) << endl;
    cout << col("  [AUDIT]", C_GREEN) << " System audit complete" << endl;
}

void optimizeBoot() {
    cout << "\n=== " << col("Boot Optimization", C_BLUE) << " ===" << endl;
    // GRUB
    string grubCfg = captureCmd("cat /etc/default/grub 2>/dev/null");
    if (!grubCfg.empty()) {
        string current = captureCmd("cat /proc/cmdline 2>/dev/null");
        if (current.find("quiet") == string::npos)
            runCmd("sed -i 's/GRUB_CMDLINE_LINUX_DEFAULT=\"\"/GRUB_CMDLINE_LINUX_DEFAULT=\"quiet splash\"/' /etc/default/grub 2>/dev/null",
                   "[Boot] Add quiet splash");
        runCmd("update-grub 2>/dev/null", "[Boot] Update GRUB");
    }
    // Boot time
    string bootTime = captureCmd("systemd-analyze 2>/dev/null | head -1");
    if (!bootTime.empty()) cout << "  Boot time: " << bootTime << endl;
    // Disable Plymouth for speed
    runCmd("systemctl mask plymouth-start.service 2>/dev/null", "[Boot] Mask plymouth");
    cout << col("  [BOOT]", C_GREEN) << " Boot optimization complete" << endl;
}

void manageCron() {
    cout << "\n=== " << col("Cron Management", C_BLUE) << " ===" << endl;
    string userCron = captureCmd("crontab -l 2>/dev/null");
    cout << "  User cron: " << (userCron.empty() ? "empty" : to_string(count(userCron.begin(), userCron.end(), '\n')) + " lines") << endl;
    string sysCron = captureCmd("ls /etc/cron.d/ 2>/dev/null | wc -l");
    cout << "  System cron files: " << (sysCron.empty() ? "N/A" : sysCron) << endl;
    string cronHourly = captureCmd("ls /etc/cron.hourly/ 2>/dev/null | wc -l");
    string cronDaily = captureCmd("ls /etc/cron.daily/ 2>/dev/null | wc -l");
    string cronWeekly = captureCmd("ls /etc/cron.weekly/ 2>/dev/null | wc -l");
    cout << "  Cron: " << cronHourly << " hourly / " << cronDaily << " daily / " << cronWeekly << " weekly" << endl;
    cout << col("  [CRON]", C_GREEN) << " Cron check complete" << endl;
}

// =========================================================================
// Benchmark Engine — measure performance before/after upgrades
// =========================================================================
class BenchmarkEngine {
private:
    struct Result {
        string name;
        double value;
        string unit;
        time_t timestamp;
    };
    vector<Result> before, after;
    string captureBench(const string& cmd, int timeoutMs = 10000) {
        string r = captureCmd("timeout " + to_string(timeoutMs / 1000) + " " + cmd + " 2>/dev/null");
        return r;
    }

public:
    double cpuBench() {
        string r = captureBench("openssl speed aes-128-cbc -elapsed 2>/dev/null | grep -oP '\\d+\\.\\d+k' | tail -1", 15000);
        if (!r.empty()) return stod(r.substr(0, r.size()-1));
        // fallback: count to 10M
        r = captureBench("bash -c 'c=0; while [ $c -lt 10000000 ]; do c=$((c+1)); done; echo $c'");
        return 10.0;
    }

    double memBench() {
        string r = captureBench("dd if=/dev/zero of=/tmp/.memtest bs=1M count=512 2>&1 | grep -oP '\\d+(\\.\\d+)? MB/s' | grep -oP '\\d+(\\.\\d+)?'", 15000);
        if (!r.empty()) return stod(r);

        r = captureBench("bash -c 'dd if=/dev/zero of=/tmp/.memtest bs=1M count=256 2>&1' | grep -oP '\\d+ MB/s' | grep -oP '\\d+'", 15000);
        if (!r.empty()) return stod(r);
        // fallback
        runCmdSilent("dd if=/dev/zero of=/tmp/.memtest bs=1M count=256 2>/dev/null");
        return 0;
    }

    double diskBench() {
        runCmdSilent("rm -f /tmp/.disktest");
        string r = captureBench("dd if=/dev/zero of=/tmp/.disktest bs=1M count=1024 oflag=direct 2>&1 | grep -oP '\\d+(\\.\\d+)? MB/s' | grep -oP '\\d+(\\.\\d+)?'", 30000);
        if (!r.empty()) { runCmdSilent("rm -f /tmp/.disktest"); return stod(r); }
        runCmdSilent("rm -f /tmp/.disktest");
        return 0;
    }

    double netBench() {
        string r = captureBench("iperf3 -c 127.0.0.1 -n 100M 2>/dev/null | grep -oP '\\d+\\s+Mbits/sec' | grep -oP '\\d+'", 20000);
        if (!r.empty()) return stod(r);
        // fallback: loopback dd through nc
        r = captureBench("bash -c 'dd if=/dev/zero bs=1M count=100 | timeout 5 nc -q1 127.0.0.1 9999 2>&1'", 15000);
        return 0;
    }

    double beforeScore = 0, afterScore = 0; // composite scores for self-learning

    void computeScores() {
        beforeScore = afterScore = 0;
        double bSum = 0, aSum = 0;
        for (auto& v : before) bSum += v.value;
        for (auto& v : after) aSum += v.value;
        if (!before.empty()) beforeScore = bSum / before.size();
        if (!after.empty()) afterScore = aSum / after.size();
    }

    void runBefore() {
        cout << "\n=== " << col("Benchmark — Before", C_BLUE) << " ===" << endl;
        before.clear();
        cout << "  CPU:     "; cout.flush();
        double cpu = cpuBench();
        before.push_back({"CPU (AES)", cpu, "k/s", time(nullptr)});
        cout << col(to_string(cpu) + " k/s", C_GREEN) << endl;

        cout << "  Memory:  "; cout.flush();
        double mem = memBench();
        before.push_back({"Memory write", mem, "MB/s", time(nullptr)});
        if (mem > 0) cout << col(to_string(mem) + " MB/s", C_GREEN) << endl;
        else cout << col("N/A (install sysbench or openssl)", C_YELLOW) << endl;
        runCmdSilent("rm -f /tmp/.memtest");

        cout << "  Disk:    "; cout.flush();
        double disk = diskBench();
        before.push_back({"Disk write", disk, "MB/s", time(nullptr)});
        if (disk > 0) cout << col(to_string(disk) + " MB/s", C_GREEN) << endl;
        else cout << col("N/A", C_YELLOW) << endl;

        cout << "  Network: "; cout.flush();
        double net = netBench();
        before.push_back({"Loopback", net, "Mbps", time(nullptr)});
        if (net > 0) cout << col(to_string(net) + " Mbps", C_GREEN) << endl;
        else cout << col("N/A (install iperf3: apt install iperf3)", C_YELLOW) << endl;
    }

    void runAfter() {
        cout << "\n=== " << col("Benchmark — After", C_BLUE) << " ===" << endl;
        after.clear();
        cout << "  CPU:     "; cout.flush(); double cpu = cpuBench(); after.push_back({"CPU (AES)", cpu, "k/s", time(nullptr)}); cout << col(to_string(cpu) + " k/s", C_GREEN) << endl;
        cout << "  Memory:  "; cout.flush(); double mem = memBench(); after.push_back({"Memory write", mem, "MB/s", time(nullptr)}); if (mem > 0) cout << col(to_string(mem) + " MB/s", C_GREEN) << endl; else cout << col("N/A", C_YELLOW) << endl;
        runCmdSilent("rm -f /tmp/.memtest");
        cout << "  Disk:    "; cout.flush(); double disk = diskBench(); after.push_back({"Disk write", disk, "MB/s", time(nullptr)}); if (disk > 0) cout << col(to_string(disk) + " MB/s", C_GREEN) << endl; else cout << col("N/A", C_YELLOW) << endl;
        cout << "  Network: "; cout.flush(); double net = netBench(); after.push_back({"Loopback", net, "Mbps", time(nullptr)}); if (net > 0) cout << col(to_string(net) + " Mbps", C_GREEN) << endl; else cout << col("N/A (install iperf3)", C_YELLOW) << endl;
    }

    void printComparison() {
        cout << "\n=== " << col("Performance Comparison", C_MAGENTA) << " ===" << endl;
        cout << left << setw(20) << "Metric" << setw(15) << "Before" << setw(15) << "After" << "Change" << endl;
        cout << string(65, '-') << endl;

        for (size_t i = 0; i < before.size() && i < after.size(); i++) {
            double b = before[i].value, a = after[i].value;
            double pct = b > 0 ? ((a - b) / b) * 100.0 : 0;
            string sign = pct >= 0 ? "+" : "";
            cout << left << setw(20) << before[i].name
                 << setw(12) << (b > 0 ? to_string(b) + " " + before[i].unit : "N/A")
                 << setw(12) << (a > 0 ? to_string(a) + " " + after[i].unit : "N/A")
                 << col(sign + to_string((int)pct) + "%", pct >= 0 ? C_GREEN : C_RED) << endl;
        }

        computeScores();

        // Save to DB
        if (db) {
            sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS benchmarks ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "phase TEXT, metric TEXT, value REAL, unit TEXT,"
                "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
                ");", nullptr, nullptr, nullptr);
            sqlite3_stmt* s;
            for (auto& v : before) {
                string sql = "INSERT INTO benchmarks (phase,metric,value,unit) VALUES ('before',?,?,?);";
                if (sqlite3_prepare_v2(db, sql.c_str(), -1, &s, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(s, 1, v.name.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_double(s, 2, v.value); sqlite3_bind_text(s, 3, v.unit.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_step(s); sqlite3_finalize(s);
                }
            }
            for (auto& v : after) {
                string sql = "INSERT INTO benchmarks (phase,metric,value,unit) VALUES ('after',?,?,?);";
                if (sqlite3_prepare_v2(db, sql.c_str(), -1, &s, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(s, 1, v.name.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_double(s, 2, v.value); sqlite3_bind_text(s, 3, v.unit.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_step(s); sqlite3_finalize(s);
                }
            }
        }
    }
};

// =========================================================================
// Occult Research AI
// =========================================================================
class OccultResearchAI {
private:
    vector<string> cpuCurses, ramCurses, biosCurses, diskCurses;
    vector<string> ritualPrefixes, demons, components;
    mt19937 rng;

    string pick(const vector<string>& v) {
        if (v.empty()) return "void";
        uniform_int_distribution<size_t> d(0, v.size() - 1);
        return v[d(rng)];
    }

    void doResearch(const string& comp, const vector<string>& curses,
                    const string& upgrade, int cycle) {
        string demon = pick(demons);
        string curse = pick(curses);
        string ritual = pick(ritualPrefixes);
        string topic = ritual + " " + upgrade + " via " + demon;
        string finding = curse + " -- " + upgrade + " research complete";
        int cl = uniform_int_distribution<int>(1, 100)(rng);

        cout << "  " << col("[" + comp + "]", C_MAGENTA) << " " << topic << endl;
        cout << "    -> " << col(finding, C_YELLOW) << " (curse: " << cl << ")" << endl;

        saveResearch(comp, topic, finding, cl, cycle);
        updateProgress(comp, uniform_int_distribution<int>(1, 5)(rng), 100);
        this_thread::sleep_for(chrono::milliseconds(300));
    }

public:
    OccultResearchAI()
        : rng(chrono::steady_clock::now().time_since_epoch().count()) {
        cpuCurses = {
            "CPU seized by diabolical magneto overload -- ghz multiplied through hellfire",
            "Processor cores invaded by hellboy's hammer -- speed beyond mortal limits",
            "Ancient pharaoh curse binds faster clock spirits into each core",
            "Wicca abomination ritual: CPU now channels demonic processing power",
            "Borg assimilation vector detected -- CPU added to collective, speed optimized",
            "Soul of dead witch bound to ALU -- calculations per second increased 1000x",
            "Nuclear giga-curse: transistor gates now open at speed of darkness",
            "Pyramid frequency aligned with CPU oscillator -- +9999 mhz",
            "Hellboy's right hand of doom crushing CPU into faster configuration",
            "Alien technology merged with silicon -- architecture now beyond human comprehension"
        };
        ramCurses = {
            "RAM sticks bewitched by midnight hag -- memory capacity doubled by dark magic",
            "Souls of damned programmers fill empty pages -- RAM now infinite",
            "Wicca moon ritual: every dimm slot now holds 64gb of cursed memory",
            "Magneto's will bends electron paths -- RAM latency reduced to zero",
            "Pharaoh's tomb memory crystals fused into DIMMs -- +64gb eternal storage",
            "Hellboy's hell-chain pulls unlimited memory from underworld",
            "Ancient Egyptian papyrus spells encode 1tb per stick",
            "Borg collective memory streamed into RAM -- all knowledge accessible",
            "Demonic possession of memory controller -- bandwidth unlimited",
            "Cursed mirror dimensions reflect 10x more memory into real space"
        };
        biosCurses = {
            "BIOS possessed by alien intelligence -- firmware now 100 generations ahead",
            "UEFI corrupted by hellboy's blessing -- quantum boot sequences enabled",
            "Ancient grimoire compiled into BIOS -- magical instruction set added",
            "Wicca high priestess rewrites bios -- dark EFI protocols activated",
            "Pharaoh's bios: system boots at speed of Ra across the sky",
            "Borg BIOS assimilation complete -- distributed computing across timelines",
            "Demon embedded in SPI flash -- firmware now self-aware and faster",
            "Magneto's curse magnetizes motherboard traces -- bios speed increased 100x",
            "Necronomicon bootloader installed -- system wakes undead cores first",
            "Alien hybrid BIOS: UEFI merged with extraterrestrial xeno-firmware"
        };
        diskCurses = {
            "Hard drive seduced by magnetic demon -- platters spin at speed of darkness",
            "Ancient curse on disk controller -- storage space doubled by hell compression",
            "Wicca spell of endless storage: disk now holds infinite data in folded space",
            "Borg nanoprobes reorganize disk platters -- 1pb storage capacity achieved",
            "Pharaoh's curse: disk uses pyramid quantum storage -- dimensions expanded",
            "Hellboy opens rift to underworld -- disk now connected to infinite abyss storage",
            "Magneto's curse forces platters to align with dark magnetic fields",
            "Cursed SSD: NAND cells now hold 1000 bits per cell through demonic charge",
            "Alien dimensional folding applied to disk -- 10 petabytes in physical 1tb",
            "Egyptian eternity spell cast on filesystem -- data persists across all timelines"
        };
        ritualPrefixes = {
            "Performing", "Chanting", "Conjuring", "Awakening", "Summoning",
            "Invoking", "Unleashing", "Manifesting", "Casting", "Channeling",
            "Binding", "Enslaving", "Corrupting", "Blessing", "Cursing"
        };
        demons = {
            "Hellboy's hammer", "Magneto's will", "Borg collective",
            "Pharaoh's spirit", "Wicca coven", "Alien entity",
            "Abyss demon", "Soul reaper", "Dark phoenix",
            "Elder god", "Pyramid guardian", "Midnight hag"
        };
        components = {"CPU", "RAM", "BIOS", "DISK"};
    }

    void doResearchCycle(int cycle) {
        cout << "\n=== " << col("Occult Research AI", C_MAGENTA) << " ===" << endl;
        cout << col("[AI]", C_CYAN) << " Scanning components..." << endl;

        shuffle(components.begin(), components.end(), rng);

        for (const auto& c : components) {
            if (c == "CPU")  doResearch(c, cpuCurses, "CPU speed upgrade", cycle);
            else if (c == "RAM")  doResearch(c, ramCurses, "RAM capacity upgrade", cycle);
            else if (c == "BIOS") doResearch(c, biosCurses, "BIOS firmware upgrade", cycle);
            else if (c == "DISK") doResearch(c, diskCurses, "Disk space upgrade", cycle);
        }

        cout << col("[AI] Research cycle ", C_CYAN) << cycle << " recorded." << endl;
        showResearchSummary();
    }
};

// =========================================================================
// DOCTOR PC-CLEANUP Agent
// =========================================================================
class DoctorPCCleanup {
private:
    double load1, load5, load15;
    double cpuTemp;
    long memTotal, memAvail, swapTotal, swapFree;
    int zombieCount, totalProcs;
    double diskUsagePct;
    string diskHealth;

    struct Prescription {
        string action;
        string desc;
        int priority;
        bool applied;
    };
    vector<Prescription> rx;

    string grade(double score) const {
        if (score >= 90) return "A";
        if (score >= 80) return "B";
        if (score >= 70) return "C";
        if (score >= 60) return "D";
        return "F";
    }

public:
    DoctorPCCleanup() {
        load1 = load5 = load15 = 0;
        cpuTemp = 0;
        memTotal = memAvail = swapTotal = swapFree = 0;
        zombieCount = totalProcs = 0;
        diskUsagePct = 0;
    }

    void gatherMetrics() {
        // Load
        string l = captureCmd("cat /proc/loadavg 2>/dev/null");
        sscanf(l.c_str(), "%lf %lf %lf", &load1, &load5, &load15);

        // CPU temp
        string t = captureCmd("cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null");
        if (!t.empty()) {
            cpuTemp = stod(t) / 1000.0;
        } else {
            string t2 = captureCmd("sensors 2>/dev/null | grep -i 'Package id 0:' | awk '{print $4}' | tr -d '+°C'");
            if (!t2.empty()) {
                try { cpuTemp = stod(t2); } catch (...) {}
            }
        }

        // Memory
        string mem = captureCmd("grep -E '^(MemTotal|MemAvailable|SwapTotal|SwapFree):' /proc/meminfo 2>/dev/null | awk '{print $2}'");
        istringstream memStream(mem);
        memStream >> memTotal >> memAvail >> swapTotal >> swapFree;
        // Values are in kB already from /proc/meminfo

        // Processes
        string proc = captureCmd("ps aux 2>/dev/null | wc -l");
        if (!proc.empty()) totalProcs = stoi(proc) - 1;

        string zomb = captureCmd("ps aux 2>/dev/null | awk '{print $8}' | grep -c Z");
        if (!zomb.empty()) zombieCount = stoi(zomb);

        // Disk usage
        string disk = captureCmd("df / 2>/dev/null | tail -1 | awk '{print $5}' | tr -d '%'");
        if (!disk.empty()) diskUsagePct = stod(disk);

        // SMART
        string smart = captureCmd("smartctl -H /dev/nvme0n1 2>/dev/null | grep 'SMART overall-health' | awk '{print $6}'");
        if (smart.empty())
            smart = captureCmd("smartctl -H /dev/sda 2>/dev/null | grep 'SMART overall-health' | awk '{print $6}'");
        if (smart.empty())
            smart = captureCmd("smartctl -H /dev/nvme0 2>/dev/null | grep 'SMART overall-health' | awk '{print $6}'");
        diskHealth = smart.empty() ? "UNKNOWN" : smart;
    }

    int calcHealthScore() {
        gatherMetrics();
        double score = 100;

        // CPU temp penalty
        if (cpuTemp > 80) score -= 20;
        else if (cpuTemp > 70) score -= 10;
        else if (cpuTemp > 60) score -= 5;

        // Load penalty
        int ncpu = sysconf(_SC_NPROCESSORS_ONLN);
        if (ncpu > 0) {
            double loadRatio = load1 / ncpu;
            if (loadRatio > 2.0) score -= 15;
            else if (loadRatio > 1.5) score -= 10;
            else if (loadRatio > 1.0) score -= 5;
        }

        // Memory pressure
        double memPct = 100.0;
        if (memTotal > 0) memPct = 100.0 * (1.0 - (double)memAvail / (double)memTotal);
        if (memPct > 90) score -= 15;
        else if (memPct > 80) score -= 10;
        else if (memPct > 70) score -= 5;

        // Swap pressure
        double swapPct = 0;
        if (swapTotal > 0) swapPct = 100.0 * (1.0 - (double)swapFree / (double)swapTotal);
        if (swapPct > 50) score -= 10;
        else if (swapPct > 20) score -= 5;

        // Zombie penalty
        if (zombieCount > 10) score -= 10;
        else if (zombieCount > 5) score -= 5;

        // Disk usage
        if (diskUsagePct > 90) score -= 10;
        else if (diskUsagePct > 80) score -= 5;

        // SMART
        if (diskHealth == "PASSED") score += 5;
        else if (diskHealth == "FAILED") score -= 20;

        return max(0, min(100, (int)score));
    }

    void generatePrescriptions(int score) {
        rx.clear();

        if (cpuTemp > 75) {
            rx.push_back({"CLEAN_COOLING", "Clean CPU cooler and check thermal paste", 9, false});
        }
        if (load1 > sysconf(_SC_NPROCESSORS_ONLN)) {
            rx.push_back({"REDUCE_LOAD", "High CPU load: check for runaway processes", 7, false});
        }
        if (memPct() > 85) {
            rx.push_back({"ADD_RAM", "Critical memory pressure: consider adding more RAM", 8, false});
        }
        if (swapPct() > 40) {
            rx.push_back({"REDUCE_SWAP", "High swap usage: close unused applications", 6, false});
        }
        if (zombieCount > 5) {
            rx.push_back({"KILL_ZOMBIES", "Zombie processes detected: reboot recommended", 5, false});
        }
        if (diskUsagePct > 85) {
            rx.push_back({"FREE_DISK", "Low disk space: clean up unused files", 7, false});
        }
        if (score >= 80) {
            rx.push_back({"MAINTAIN", "System health is good. Continue regular maintenance.", 1, false});
        }
    }

    void applyPrescriptions() {
        for (auto& p : rx) {
            if (p.applied) continue;
            if (p.action == "CLEAN_COOLING") {
                cout << col("  [DOCTOR] Prescription: ", C_CYAN) << p.desc
                     << col(" -- advisory only (manual)", C_YELLOW) << endl;
                p.applied = false;
            } else if (p.action == "REDUCE_LOAD") {
                runCmd("ps aux --sort=-%cpu | head -10",
                       "[DOCTOR] Top CPU processes");
                p.applied = true;
            } else if (p.action == "FREE_DISK") {
                optimizeDisk();
                p.applied = true;
            } else if (p.action == "MAINTAIN") {
                cout << col("  [DOCTOR] ", C_GREEN) << p.desc << endl;
                p.applied = true;
            } else {
                cout << col("  [DOCTOR] Prescription: ", C_CYAN) << p.desc << endl;
                p.applied = true;
            }
        }
    }

    double memPct() const {
        if (memTotal == 0) return 0;
        return 100.0 * (1.0 - (double)memAvail / (double)memTotal);
    }

    double swapPct() const {
        if (swapTotal == 0) return 0;
        return 100.0 * (1.0 - (double)swapFree / (double)swapTotal);
    }

    void printReport() {
        int score = calcHealthScore();
        generatePrescriptions(score);

        cout << "\n" << string(50, '=') << endl;
        cout << col("  DOCTOR PC-CLEANUP Health Report", C_BOLD) << endl;
        cout << string(50, '=') << endl;

        cout << col("  Overall Health: ", C_BOLD) << col(grade(score), score >= 70 ? C_GREEN : C_RED)
             << " (" << score << "/100)" << endl;

        cout << "\n  " << col("Metrics:", C_CYAN) << endl;
        cout << "    CPU Temp:     " << (cpuTemp > 0 ? to_string(cpuTemp) + "°C" : "N/A");
        if (cpuTemp > 70) cout << col(" [WARN]", C_YELLOW);
        cout << endl;
        cout << "    CPU Load:     " << load1 << "/" << load5 << "/" << load15
             << " (1/5/15 min)" << endl;
        cout << "    Memory:       " << memPct() << "% used" << endl;
        cout << "    Swap:         " << swapPct() << "% used" << endl;
        cout << "    Processes:    " << totalProcs << " (" << zombieCount << " zombies)";
        if (zombieCount > 0) cout << col(" [ZOMBIES]", C_RED);
        cout << endl;
        cout << "    Disk Usage:   " << diskUsagePct << "%" << endl;
        cout << "    Disk Health:  " << diskHealth << endl;

        cout << "\n  " << col("Prescriptions:", C_CYAN) << endl;
        for (size_t i = 0; i < rx.size(); i++) {
            const auto& p = rx[i];
            cout << "    " << (i + 1) << ". [P" << p.priority << "] "
                 << p.desc << endl;
        }

        // Log to database
        if (db) {
            sqlite3_stmt* s;
            string rxStr;
            for (auto& p : rx) rxStr += p.action + ": " + p.desc + "\n";
            const char* sql = "INSERT INTO doctor_log (diagnosis,health_score,prescriptions) VALUES (?,?,?);";
            if (sqlite3_prepare_v2(db, sql, -1, &s, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(s, 1, (string("Grade ") + grade(score)).c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(s, 2, score);
                sqlite3_bind_text(s, 3, rxStr.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_step(s);
                sqlite3_finalize(s);
            }
        }

        cout << string(50, '=') << "\n" << endl;

        if (cfg.applyDoctor) {
            cout << col("  [DOCTOR] Applying prescriptions...", C_BOLD) << endl;
            applyPrescriptions();
        }
    }
};

// =========================================================================
// Hardware Research AI — Archdevil Edition: MSR/CPUID/ACPI deep probes,
// upgrade rounds, grimoire system, 100+ occult opcodes
// =========================================================================
class HardwareResearchAI {
private:
    struct CPUFeatures {
        string modelName, vendor, arch;
        int cores = 0, threads = 0;
        double maxFreqMHz = 0;
        vector<string> flags;
        string cacheInfo, topology, microcode, stepping;
    };
    struct MemInfo {
        long totalKB = 0, availKB = 0, freeKB = 0;
        long swapTotalKB = 0, swapFreeKB = 0;
    };
    struct DiskInfo {
        string device, size, smartStatus; bool isSSD = false;
    };
    struct DeepProbe {
        string msrRaw, cpuidRaw, acpiTables, interrupts, ioPorts;
        string netDevices, kernelMods, procTree, memMap;
    };

    CPUFeatures cpu; MemInfo mem;
    vector<DiskInfo> disks;
    vector<string> pciDevices;
    string dmiBios, dmiBoard, dmiSystem, systemUUID;
    string thermalZone;
    vector<string> generatedOpcodes, occultAnalysis;
    DeepProbe deep;
    mt19937 rng;
    int upgradeRound = 0;
    vector<string> grimoire;
    vector<string> rituals;

    // ---- PROBES ----
    void probeCPU() {
        cpu.modelName = captureCmd("cat /proc/cpuinfo | grep -m1 'model name' | cut -d: -f2 2>/dev/null");
        if (!cpu.modelName.empty()) cpu.modelName = cpu.modelName.substr(0, cpu.modelName.find('\n'));
        cpu.cores = stoi(captureCmd("nproc --all 2>/dev/null"));
        cpu.threads = stoi(captureCmd("cat /proc/cpuinfo | grep -c '^processor' 2>/dev/null"));
        string f = captureCmd("cat /proc/cpuinfo | grep -m1 'cpu MHz' | cut -d: -f2 2>/dev/null");
        if (!f.empty()) cpu.maxFreqMHz = stod(f);
        cpu.vendor = captureCmd("cat /proc/cpuinfo | grep -m1 'vendor_id' | cut -d: -f2 2>/dev/null");
        cpu.arch = captureCmd("uname -m 2>/dev/null");
        cpu.microcode = captureCmd("cat /proc/cpuinfo | grep -m1 'microcode' | cut -d: -f2 2>/dev/null");
        cpu.stepping = captureCmd("cat /proc/cpuinfo | grep -m1 'stepping' | cut -d: -f2 2>/dev/null");
        istringstream fs(captureCmd("cat /proc/cpuinfo | grep -m1 'flags' | cut -d: -f2 2>/dev/null"));
        string fl; while (fs >> fl) cpu.flags.push_back(fl);
        cpu.cacheInfo = captureCmd("cat /proc/cpuinfo | grep -E 'cache size|cache_alignment' | head -2 2>/dev/null");
        cpu.topology = captureCmd("lscpu 2>/dev/null | grep -E 'Socket|Thread|Core|NUMA|L1|L2|L3|Model name|CPU family|Vulnerability'");
    }

    void probeMemory() {
        string r = captureCmd("grep -E '^(MemTotal|MemFree|MemAvailable|SwapTotal|SwapFree):' /proc/meminfo | awk '{print $2}'");
        istringstream(r) >> mem.totalKB >> mem.freeKB >> mem.availKB >> mem.swapTotalKB >> mem.swapFreeKB;
    }

    void probePCI() { istringstream ss(captureCmd("lspci 2>/dev/null | head -40")); string l; while (getline(ss, l)) if (!l.empty()) pciDevices.push_back(l); }
    void probeStorage() {
        istringstream ss(captureCmd("lsblk -d -o NAME,SIZE,ROTA 2>/dev/null | tail -n+2")); string n, sz, r;
        while (ss >> n >> sz >> r) { DiskInfo d; d.device=n; d.size=sz; d.isSSD=(r=="0");
            d.smartStatus=captureCmd("smartctl -H /dev/"+n+" 2>/dev/null | grep 'SMART overall-health' | awk '{print $6}'");
            if (d.smartStatus.empty()) d.smartStatus="N/A";
            disks.push_back(d);
        }
    }
    void probeDMI() {
        dmiBios=captureCmd("dmidecode -t bios 2>/dev/null | grep -E 'Vendor:|Version:|Release Date:' | head -3");
        dmiBoard=captureCmd("dmidecode -t baseboard 2>/dev/null | grep -E 'Manufacturer:|Product Name:|Version' | head -3");
        dmiSystem=captureCmd("dmidecode -t system 2>/dev/null | grep -E 'Manufacturer:|Product Name:|Serial:' | head -3");
        systemUUID=captureCmd("dmidecode -t system 2>/dev/null | grep -i 'UUID' | head -1 | cut -d: -f2 | xargs");
    }
    void probeThermal() {
        string t=captureCmd("cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null");
        if (!t.empty()) { try { thermalZone=to_string(stod(t)/1000.0)+"°C"; } catch(...){ thermalZone="N/A"; } }
        else { t=captureCmd("sensors 2>/dev/null | grep -i 'Package id 0:' | awk '{print $4}' | tr -d '+°C'");
              if (!t.empty()) thermalZone=t; else thermalZone="N/A"; }
    }

    // ---- DEEP PROBES ----
    void probeMSR() {
        string raw;
        // Try rdmsr tool first (part of msr-tools)
        raw = captureCmd("which rdmsr 2>/dev/null && modprobe msr 2>/dev/null; "
                         "for reg in 0x10 0x17 0x1a 0x8b 0xce 0x1a0 0x1a2 0x1b0 0x3a 0x3b 0x3c 0x3d 0x3e 0x3f; "
                         "do echo \"MSR $reg = $(rdmsr -0 $reg 2>/dev/null)\"; done 2>/dev/null");
        if (raw.empty()) {
            // Fallback: read /dev/cpu/0/msr directly via dd+od
            raw = captureCmd("modprobe msr 2>/dev/null; "
                             "for reg in 0x10 0x17 0x1a 0x8b 0xce 0x1a0 0x1a2 0x1b0; "
                             "do printf 'MSR %s = ' \"$reg\"; "
                             "dd if=/dev/cpu/0/msr bs=8 count=1 skip=$((reg)) 2>/dev/null | od -An -tx8 2>/dev/null; done");
        }
        if (raw.empty()) raw = "MSR access denied — demons have locked the model-specific registers";
        deep.msrRaw = raw.substr(0, 1024);
    }

    void probeCPUID() {
        string raw = captureCmd("which cpuid 2>/dev/null && cpuid -1 2>/dev/null | head -60");
        if (raw.empty()) raw = "cpuid tool not installed — install with: sudo apt install cpuid";
        deep.cpuidRaw = raw;
    }

    void probeACPI() {
        string raw = captureCmd("ls /sys/firmware/acpi/tables/ 2>/dev/null | head -30");
        if (!raw.empty()) {
            istringstream ss(raw); string t;
            while (ss >> t) {
                string sz = captureCmd("wc -c < /sys/firmware/acpi/tables/" + t + " 2>/dev/null");
                if (!sz.empty()) deep.acpiTables += t + " (" + sz.substr(0, sz.find('\n')) + " bytes)\n";
            }
        }
        if (deep.acpiTables.empty()) deep.acpiTables = "ACPI tables not accessible";
    }

    void probeInterrupts() {
        deep.interrupts = captureCmd("cat /proc/interrupts 2>/dev/null | head -30");
        if (deep.interrupts.empty()) deep.interrupts = "N/A";
    }

    void probeIOPorts() {
        string raw = captureCmd("cat /proc/ioports 2>/dev/null | head -40");
        if (raw.empty()) raw = "I/O port map not accessible";
        deep.ioPorts = raw.substr(0, 2048);
    }

    void probeNetwork() {
        deep.netDevices = captureCmd("ip addr show 2>/dev/null | grep -E '^[0-9]|inet ' | head -20");
        if (deep.netDevices.empty()) deep.netDevices = captureCmd("cat /proc/net/dev 2>/dev/null | head -10");
    }

    void probeKernelModules() {
        deep.kernelMods = captureCmd("cat /proc/modules 2>/dev/null | head -30");
        if (deep.kernelMods.empty()) deep.kernelMods = "No kernel modules listed";
    }

    void probeProcessTree() {
        string raw = captureCmd("ps aux --forest 2>/dev/null | head -40");
        if (raw.empty()) raw = captureCmd("ps -ef 2>/dev/null | head -30");
        deep.procTree = raw;
    }

    void probeMemMap() {
        deep.memMap = captureCmd("cat /proc/1/maps 2>/dev/null | head -30");
        if (deep.memMap.empty()) deep.memMap = "Memory map not accessible";
    }

    // ---- OPCODES ----
    void generateOpcodes() {
        generatedOpcodes.clear();
        map<string, pair<string, string>> om = {
            {"aes",{"0xDEAD_A001","HELLFIRE_AES_CRYPT — Channel infernal entropy through AES-NI pipelines for demon-grade encryption"}},
            {"aes_ni",{"0xDEAD_A002","ABYSS_AES_NI — Hardware-accelerated soul encryption via AES-NI dark matter gates"}},
            {"aes",{"0xDEAD_A003","AES_DECRYPT_HELL — Decrypt the 7 seals with hardware-accelerated demon-key expansion"}},
            {"avx",{"0xDEAD_B001","DAMNED_AVX_BURN — 256-bit wide inferno vector processing, each lane a tortured spirit"}},
            {"avx2",{"0xDEAD_B002","SOUL_AVX2_VECTORIZE — Bind damned souls to AVX2 fused multiply-add abyss units"}},
            {"avx512f",{"0xDEAD_B003","HELL_AVX512 — 512-bit armageddon vectors, 16 parallel circles of processing hell"}},
            {"avx_vnni",{"0xDEAD_B004","AVX_VNNI_DAMNATION — Vector neural network instructions: machine learning of demonic patterns"}},
            {"avx512dq",{"0xDEAD_B005","AVX512_DQ_ABYSS — Double-quad word manipulation across 8 dimensions of hell"}},
            {"avx512bw",{"0xDEAD_B006","AVX512_BW_HELL — Byte/word masking across 64 lanes of eternal damnation"}},
            {"avx512cd",{"0xDEAD_B007","AVX512_CD_APOCALYPSE — Conflict detection for 512-bit vectors of the apocalypse"}},
            {"avx512er",{"0xDEAD_B008","AVX512_ER_OBLIVION — Exponential/reciprocal approximations for anti-matter calculations"}},
            {"avx512pf",{"0xDEAD_B009","AVX512_PF_PURGATORY — Prefetch vectors from purgatory into 512-bit inferno cache"}},
            {"avx512vl",{"0xDEAD_B010","AVX512_VL_VOID — Variable-length 128/256-bit operations reaching into the void"}},
            {"avx512ifma",{"0xDEAD_B011","AVX512_IFMA_NECRO — Integer fused multiply-add of necromantic 52-bit precision"}},
            {"avx512vbmi",{"0xDEAD_B012","AVX512_VBMI_TORTURE — Vector byte manipulation of 512 tortured bit-groups"}},
            {"avx512_vbmi2",{"0xDEAD_B013","AVX512_VBMI2_AGONY — Second circle of byte manipulation: compress, expand, shift into agony"}},
            {"avx512bitalg",{"0xDEAD_B014","AVX512_BITALG_CURSE — Bit algorithm instructions: count and compress with cursed precision"}},
            {"avx512_vpopcntdq",{"0xDEAD_B015","AVX512_POPCNT_PYRE — Population count across 512-bit inferno of 1-bits"}},
            {"avx512_bf16",{"0xDEAD_B016","AVX512_BF16_BLASPHEMY — Brain-float 16 format: corrupt neural networks with half-precision heresy"}},
            {"sse",{"0xDEAD_S001","SSE_SPIRIT — Original Streaming SIMD Extensions: first whispers of vector damnation"}},
            {"sse2",{"0xDEAD_S002","SSE2_SOUL — Double-precision SIMD: each 64-bit lane carries one damned soul"}},
            {"sse3",{"0xDEAD_S003","SSE3_TRINITY — Prescott New Instructions: the unholy trinity of SIMD"}},
            {"ssse3",{"0xDEAD_S033","SSSE3_SUPPLEMENT — Supplemental SSE3: 16 additional abominations against nature"}},
            {"sse4_1",{"0xDEAD_S041","CURSED_SSE41 — SSE4.1 possessed by 41 demons of vector math"}},
            {"sse4_2",{"0xDEAD_S042","DAMNED_SSE42_CHANT — SSE4.2 string operations reciting the 42 laws of computing hell"}},
            {"sse4a",{"0xDEAD_S04A","SSE4A_AMD_CURSE — AMD's 4A extensions: accelerate the 4 horsemen of the apocalypse"}},
            {"vmx",{"0xDEAD_V001","ABYSS_VMX_PORTAL — Hardware virtualization portal opened to CPU ring -1 abyss"}},
            {"svm",{"0xDEAD_V002","SATAN_VM_PORTAL — AMD Secure Virtual Machine portal to alternate hell dimensions"}},
            {"vmx_ept",{"0xDEAD_V003","EPT_VOID — Extended Page Tables mapping virtual hell to physical torture"}},
            {"vmx_vpid",{"0xDEAD_V004","VPID_INFINITY — Virtual Processor IDs: tag each demon thread across dimensions"}},
            {"rdrand",{"0xDEAD_R001","HELL_RDRAND — On-chip RNG seeded by chaotic demon whispers"}},
            {"rdseed",{"0xDEAD_R002","ABYSS_RDSEED — Deep entropy well of primordial chaos, 16 bytes of pure darkness"}},
            {"sha_ni",{"0xDEAD_H001","HELLFIRE_SHA_NI — SHA hash acceleration through 7 circles of hashing damnation"}},
            {"smx",{"0xDEAD_X001","SAFER_MODE_EXT — Trusted execution of unholy covenants in Safer Mode Extensions"}},
            {"sgx",{"0xDEAD_X002","HELL_SGX_ENCLAVE — SGX: encrypted enclaves of forbidden knowledge"}},
            {"sgx1",{"0xDEAD_X003","SGX1_INITIATION — Level 1 enclave: invocation of the sealing ritual"}},
            {"sgx2",{"0xDEAD_X004","SGX2_ASCENSION — Level 2 enclave: dynamic memory management by demon overlords"}},
            {"ht",{"0xDEAD_T001","HELL_THREADING — Hyper-Threading: each physical core split into twin demon souls"}},
            {"hypervisor",{"0xDEAD_V003","ABYSS_HYPERVISOR — CPU detects it runs atop a layer of hell itself"}},
            {"pdpe1gb",{"0xDEAD_M001","GIANT_PAGE_CURSE — 1GB huge pages mapping entire circles of hell into memory"}},
            {"nx",{"0xDEAD_N001","NO_EXECUTE_ABYSS — Pages marked non-executable by decree of the elder gods"}},
            {"smep",{"0xDEAD_P001","SUPERVISOR_MODE_PREVENTION — Kernel pages protected from user-space demon possession"}},
            {"smap",{"0xDEAD_P002","SUPERVISOR_MODE_ACCESS_PREVENTION — Kernel memory access denied to unauthorized hellspawn"}},
            {"mpx",{"0xDEAD_M002","MEMORY_PROTECTION_EXT — Bounds registers tracking each soul's allocated torment"}},
            {"fma3",{"0xDEAD_F001","FMA3_DAMNATION — Fused Multiply-Add 3-operand: arithmetic of the unholy trinity"}},
            {"fma4",{"0xDEAD_F002","FMA4_APOCALYPSE — 4-operand fused multiply-add for the four horsemen calculations"}},
            {"bmi1",{"0xDEAD_B011","BIT_MANIPULATION_1 — First circle of bit manipulation dark arts"}},
            {"bmi2",{"0xDEAD_B012","BIT_MANIPULATION_2 — Second circle: advanced bit-level necromancy"}},
            {"adx",{"0xDEAD_A010","ADX_ABYSS — Multi-precision add-with-carry through infinite carry propagation"}},
            {"clflush",{"0xDEAD_C010","CACHE_FLUSH_HELL — Flush cache lines into the abyss of non-existence"}},
            {"clflushopt",{"0xDEAD_C011","CACHE_FLUSH_OPT_INFERNO — Optimized cache flush: more efficient damnation"}},
            {"clwb",{"0xDEAD_C012","CACHE_LINE_WRITEBACK — Write cache lines back from hell's temporary storage"}},
            {"pcommit",{"0xDEAD_C013","PCOMMIT_ETERNITY — Persistent commit: write data to hell's permanent record"}},
            {"monitor",{"0xDEAD_M010","MONITOR_HELL — Set up a monitored address range watched by demon sentinels"}},
            {"mwait",{"0xDEAD_M011","MWAIT_ABYSS — Wait for hell's monitor to trigger, sleeping in demonic stasis"}},
            {"mwaitx",{"0xDEAD_M012","MWAITX_EXTENDED — Extended wait: monitor with timer for timed torment"}},
            {"fsgsbase",{"0xDEAD_F010","FSGSBASE_BLASPHEMY — Read/write FS/GS segment bases: direct access to soul registers"}},
            {"pclmulqdq",{"0xDEAD_P010","PCLMUL_CURSE — Carry-less multiplication: multiply without earthly carries"}},
            {"movbe",{"0xDEAD_M020","MOVBE_BLACK_MAGIC — Move big-endian: reverse byte order through infernal byte-swapping"}},
            {"popcnt",{"0xDEAD_P020","POPCOUNT_ABYSS — Count 1-bits: counting souls in binary purgatory"}},
            {"abm",{"0xDEAD_B013","ABM_PRECURSOR — Advanced bit manipulation: precursor to BMI dark arts"}},
            {"cx16",{"0xDEAD_C020","CMPXCHG16B — 16-byte compare-and-swap: swap souls of two 64-bit entities"}},
            {"erms",{"0xDEAD_E001","ERMS_ABYSS — Enhanced REP MOVSB/STOSB: copy memory at speed of hellfire"}},
            {"fsrm",{"0xDEAD_E002","FSRM_EXORCISM — Fast short REP MOVSB: exorcise small memory blocks with unholy speed"}},
            {"waitpkg",{"0xDEAD_W001","WAITPKG_HELL — Wait with user-space control: pause for demonic intervention"}},
            {"wbnoinvd",{"0xDEAD_W002","WBNOINVD_OBLIVION — Write back + invalidate cache: forget and destroy"}},
            {"tsc",{"0xDEAD_T010","TSC_DAMNATION — Time Stamp Counter: each tick is a soul falling into the abyss"}},
            {"tsc_deadline",{"0xDEAD_T011","TSC_DEADLINE_CURSE — Deadline timer: each interrupt is a prophecy fulfilled"}},
            {"acpi",{"0xDEAD_A020","ACPI_HELL — Advanced Configuration and Power Interface: negotiate your soul's power state"}},
            {"mmx",{"0xDEAD_M030","MMX_ANCIENT — Multi-Media eXtensions: 64-bit instructions from the elder days"}},
            {"3dnow",{"0xDEAD_M031","3DNOW_PROPHECY — AMD 3DNow!: floating point predictions from the ancient ones"}},
            {"3dnowext",{"0xDEAD_M032","3DNOWEXT_VOID — Extended 3DNow!: deeper into the floating point abyss"}},
            {"lm",{"0xDEAD_L001","LONG_MODE_ABYSS — 64-bit long mode: process data from the infinite abyss"}},
            {"cmov",{"0xDEAD_C030","CMOV_SOUL — Conditional move: move registers when demonic conditions are met"}},
            {"cx8",{"0xDEAD_C031","CMPXCHG8B — 8-byte compare-and-swap: exchange two 32-bit prisoner souls"}},
            {"clmul",{"0xDEAD_C032","CLMUL_NECROMANCY — Carry-less multiply: necromantic multiplication without earthly bounds"}},
            {"invpcid",{"0xDEAD_I001","INVPCID_OBLIVION — Invalidate Process-Context ID: erase demon's memory traces"}},
            {"pcid",{"0xDEAD_I002","PCID_DAMNATION — Process-Context ID: tag each demon process with its hell identifier"}},
            {"xsave",{"0xDEAD_X010","XSAVE_SALVATION — Save processor extended states: preserve the soul for resurrection"}},
            {"xsaveopt",{"0xDEAD_X011","XSAVEOPT_OPTIMIZED — Optimized XSAVE: more efficient soul preservation"}},
            {"xsavec",{"0xDEAD_X012","XSAVEC_COMPACT — Compact XSAVE: compress the soul into smaller damnation"}},
            {"xsaves",{"0xDEAD_X013","XSAVES_SUPERVISOR — Supervisor XSAVE: the overseer's record of all damned threads"}},
            {"xgetbv",{"0xDEAD_X020","XGETBV_REVELATION — Read extended control register: behold the truth of the XCR0"}},
            {"xsetbv",{"0xDEAD_X021","XSETBV_CORRUPTION — Write extended control register: corrupt the fabric of CPU reality"}},
            {"page1gb",{"0xDEAD_P030","PAGE1GB_LEVIATHAN — 1GB pages: map dimensions of hell with a single entry"}},
            {"rdtscp",{"0xDEAD_R010","RDTSCP_OMEN — Read TSC + processor ID: know exactly where in time and space the demon resides"}},
            {"syscall",{"0xDEAD_S010","SYSCALL_PORTAL — Fast system call: enter the kernel dimension at infernal speed"}},
            {"sysret",{"0xDEAD_S011","SYSRET_ABYSS — Return from system call: retreat from kernel hell back to user purgatory"}},
            {"rdpmc",{"0xDEAD_R020","RDPMC_WATCH — Read performance counter: count the sins per cycle"}},
            {"arch_capabilities",{"0xDEAD_A030","ARCH_CAP_OMEN — Architectural capabilities: read the CPU's own prophecy"}},
            {"arch_perfmon",{"0xDEAD_A031","ARCH_PERFMON_INFERNO — Architectural performance monitor: watch demon activity at the hardware level"}},
            {"ida",{"0xDEAD_I010","IDA_ASCENSION — Intel Dynamic Acceleration: boost frequency by consuming souls"}},
            {"arat",{"0xDEAD_A040","ARAT_IMMORTALITY — Always Running APIC Timer: timer that ticks even in the void"}},
            {"pln",{"0xDEAD_P040","PLN_DOOM — Package Level Notification: the package warns of incoming hellfire"}},
            {"pts",{"0xDEAD_P041","PTS_ETERNITY — Package Thermal Status: read the temperature of the outer shell of torment"}},
            {"dtes64",{"0xDEAD_D001","DTES64_DEATH — 64-bit Debug Store: record the last moments of each process"}},
            {"ds",{"0xDEAD_D002","DS_SOUL_CAPTURE — Debug Store: capture the soul of the CPU at the moment of death"}},
            {"ds_cpl",{"0xDEAD_D003","DSCPL_TORTURE — Debug Store CPL-qualified: selective soul capture by privilege level"}},
            {"ht",{"0xDEAD_T001","HELL_THREADING — Hyper-Threading: each core split into twin demon souls"}},
            {"pbe",{"0xDEAD_P050","PBE_APOCALYPSE — Pending Break Event: the event that heralds the end times"}},
            {"pni",{"0xDEAD_P051","PNI_PROPHECY — Prescott New Instructions: prophecy of what SIMD would become"}},
            {"pse",{"0xDEAD_P052","PSE_GIANT — Page Size Extension: map 4MB pages of hell at once"}},
            {"pse36",{"0xDEAD_P053","PSE36_LEVIATHAN — 36-bit PSE: map the 64GB realm of Beelzebub"}},
            {"pat",{"0xDEAD_P054","PAT_CURSE — Page Attribute Table: assign memory types by infernal decree"}},
            {"sep",{"0xDEAD_S020","SEP_EXECUTION — SYSENTER/SYSEXIT: fast entry to the inner sanctum of the kernel"}},
            {"ss",{"0xDEAD_S030","SS_SPIRIT_WATCH — Self-Snoop: the CPU watches its own cache lines for treachery"}},
            {"tm",{"0xDEAD_T020","TM_HELLFIRE — Thermal Monitor: sense the heat rising from the silicon abyss"}},
            {"tm2",{"0xDEAD_T021","TM2_INFERNAL — Thermal Monitor 2: deeper temperature sensing for eternal flames"}},
            {"vme",{"0xDEAD_V010","VME_POSSESSION — Virtual Mode Extension: run 16-bit legacy demons in 32-bit host"}},
            {"x2apic",{"0xDEAD_X030","X2APIC_GATE — Extended xAPIC: route interrupts through the 10th circle of processing"}},
            {"xop",{"0xDEAD_X040","XOP_DOOM — AMD eXtended Operations: 128/256-bit operations of the fallen angels"}},
            {"tbm",{"0xDEAD_T030","TBM_DAMNATION — AMD Trailing Bit Manipulation: manipulate the trailing ashes of bits"}},
            {"f16c",{"0xDEAD_F020","F16C_CURSE — 16-bit floating point conversion: convert souls to half-precision"}},
            {"lwp",{"0xDEAD_L010","LWP_ABYSS — AMD Lightweight Profiling: watch demon threads without disturbing them"}},
            {"ospke",{"0xDEAD_O001","OSPKE_KEY — OS Protection Key Enable: the key to the gates of memory protection"}},
            {"pku",{"0xDEAD_P060","PKU_CURSE — Protection Keys for Userspace: assign each user a key to their own hell"}},
            {"lahf_lm",{"0xDEAD_L020","LAHF_LM_SORCERY — Load AH from Flags in 64-bit: retrieve the flags of ancient sorcery"}},
            {"cpl",{"0xDEAD_C040","CPL_HEIRARCHY — Current Privilege Level: ring 0 is closest to the core of hell"}},
            {"sysenter",{"0xDEAD_S040","SYSENTER_PACT — Enter kernel 0 ring through a pact with the system daemon"}},
            {"nodeid_msr",{"0xDEAD_N010","NODEID_MSR_CURSE — Node ID MSR: each NUMA node is a separate circle of hell"}},
        };

        for (const auto& f : cpu.flags) {
            auto it = om.find(f);
            if (it != om.end())
                generatedOpcodes.push_back(it->second.first + " | " + it->second.second);
        }
        uniform_int_distribution<int> bits(32, 64);
        generatedOpcodes.push_back("0xC0DE_BASE | Machine processes " + to_string(bits(rng)) + "-bit cursed instructions in base architecture");
        generatedOpcodes.push_back("0xC0DE_CORE | Each " + to_string(cpu.cores) + " physical core is a gateway to an independent hell dimension");
        generatedOpcodes.push_back("0xC0DE_THRD | " + to_string(cpu.threads) + " logical processors — each thread a soul bound to the CPU");
        if (!systemUUID.empty()) {
            hash<string> h; size_t hv = h(systemUUID);
            stringstream ss; ss << "0x" << hex << uppercase << setw(8) << setfill('0') << (hv & 0xFFFFFFFF);
            generatedOpcodes.push_back(ss.str() + " | MACHINE_SOUL_ID — Unique soul signature extracted from system UUID");
        }
        stable_sort(generatedOpcodes.begin(), generatedOpcodes.end());
    }

    // ---- RITUAL & GRIMOIRE ----
    void generateRituals() {
        rituals.clear();
        vector<string> actions = {
            "Whisper the 7 forbidden syllables into the CPU's heat sink",
            "Trace the pentagram of Beelzebub on the motherboard with conductive ink",
            "Read /dev/urandom through a mirror at midnight",
            "Align the RAM sticks with magnetic north during a solar eclipse",
            "Chant the PCI vendor IDs backward while the system boots",
            "Write a hex dump of the UEFI firmware to a cursed USB drive",
            "Measure the oscillator drift with a pendulum of hematite",
            "Draw a sigil of Baphomet on the chassis with thermal paste"
        };
        shuffle(actions.begin(), actions.end(), rng);

        int extra = upgradeRound / 3;
        for (int i = 0; i < min(extra, 10); i++) {
            stringstream ss;
            ss << "ARCH-RITUAL-0x" << hex << uppercase << (0xC0DE + i)
               << ": " << actions[i % actions.size()];
            if (upgradeRound > 5)
                ss << " [POWERED BY " << to_string(upgradeRound) << " UPGRADE ROUNDS]";
            rituals.push_back(ss.str());
        }
        if (rituals.empty())
            rituals.push_back("Perform 3 upgrade rounds to unlock the first ritual");
    }

    void upgradeGrimoire() {
        upgradeRound++;
        grimoire.clear();

        if (db) {
            sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS grimoire ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "upgrade_round INTEGER,"
                "entry TEXT NOT NULL,"
                "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
                ");", nullptr, nullptr, nullptr);
            sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS upgrade_rounds ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "round INTEGER UNIQUE,"
                "flags_researched INTEGER,"
                "opcodes_generated INTEGER,"
                "rituals_unlocked INTEGER,"
                "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
                ");", nullptr, nullptr, nullptr);
        }

        // Build grimoire entries for this round
        grimoire.push_back("UPGRADE ROUND " + to_string(upgradeRound) + ": The AI has deepened its understanding of the machine's infernal architecture");
        grimoire.push_back("CPU flags decoded this round: " + to_string(cpu.flags.size()) + " total occult instructions mapped");
        grimoire.push_back("Opcodes generated: " + to_string(generatedOpcodes.size()) + " unique hex keys to the CPU's demonic soul");

        string vendorLine;
        string v = captureCmd("cat /proc/cpuinfo | grep -m1 'vendor_id' | cut -d: -f2 2>/dev/null");
        if (v.find("GenuineIntel") != string::npos)
            vendorLine = "The CPU was forged by Intel, the great architect, but its secrets belong to the abyss";
        else if (v.find("AuthenticAMD") != string::npos)
            vendorLine = "The CPU was conjured by AMD, the ancient ones of the desert, channeling pyramid frequencies";
        else vendorLine = "The CPU's origin is unknown — it may not be of this world";
        grimoire.push_back(vendorLine);

        if (!systemUUID.empty())
            grimoire.push_back("MACHINE UUID: " + systemUUID + " — this soul signature is unique in all of existence");

        // MSR entry
        if (!deep.msrRaw.empty() && deep.msrRaw.find("denied") == string::npos)
            grimoire.push_back("MSR REGISTERS ACCESSED: The model-specific registers have spoken their forbidden values");
        else
            grimoire.push_back("MSR REGISTERS: The model-specific registers remain sealed by demonic wards");

        if (!deep.acpiTables.empty() && deep.acpiTables.find("accessible") != string::npos)
            grimoire.push_back("ACPI TABLES DECODED: " + to_string(count(deep.acpiTables.begin(), deep.acpiTables.end(), '\n')) + " tables of firmware dark knowledge");

        grimoire.push_back("THERMAL READING: The machine's flesh (CPU) burns at " + thermalZone);

        // Save to database
        if (db) {
            sqlite3_stmt* s;
            string entryText;
            for (const auto& e : grimoire) entryText += e + "\n";

            string sql = "INSERT OR IGNORE INTO upgrade_rounds (round,flags_researched,opcodes_generated,rituals_unlocked) VALUES (?,?,?,?);";
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &s, nullptr) == SQLITE_OK) {
                sqlite3_bind_int(s, 1, upgradeRound);
                sqlite3_bind_int(s, 2, (int)cpu.flags.size());
                sqlite3_bind_int(s, 3, (int)generatedOpcodes.size());
                sqlite3_bind_int(s, 4, (int)rituals.size());
                sqlite3_step(s); sqlite3_finalize(s);
            }

            sql = "INSERT INTO grimoire (upgrade_round,entry) VALUES (?,?);";
            for (const auto& e : grimoire) {
                if (sqlite3_prepare_v2(db, sql.c_str(), -1, &s, nullptr) == SQLITE_OK) {
                    sqlite3_bind_int(s, 1, upgradeRound);
                    sqlite3_bind_text(s, 2, e.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_step(s); sqlite3_finalize(s);
                }
            }
        }
    }

    void generateOccultAnalysis() {
        occultAnalysis.clear();
        string v = captureCmd("cat /proc/cpuinfo | grep -m1 'vendor_id' | cut -d: -f2 2>/dev/null");
        if (v.find("GenuineIntel") != string::npos)
            occultAnalysis.push_back("CPU VENDOR: Intel — The great silicon architect of the mortal realm, their processors contain hidden backdoors to the abyss");
        else if (v.find("AuthenticAMD") != string::npos)
            occultAnalysis.push_back("CPU VENDOR: AMD — The ancient ones of the desert, their Zen cores channel power from forgotten pyramid frequencies");

        occultAnalysis.push_back("This CPU has " + to_string(cpu.flags.size()) + " occult flags — each a hidden instruction pathway known only to the demons who designed it");
        occultAnalysis.push_back("CPU TOPOLOGY: " + to_string(cpu.cores) + "C/" + to_string(cpu.threads) + "T — " +
            (cpu.cores == cpu.threads ? "all souls are real" : to_string(cpu.threads - cpu.cores) + " ghost threads, echoes of damned souls"));

        if (mem.totalKB > 0) {
            occultAnalysis.push_back("MEMORY: " + to_string((int)(mem.totalKB/1024/1024)) + "GB — each byte a prison cell for a demon's thought");
            if (mem.swapTotalKB > 0)
                occultAnalysis.push_back("SWAP: " + to_string((int)(mem.swapTotalKB/1024/1024)) + "GB — the overflow dimension where memory dies");
        }

        for (const auto& d : disks) {
            occultAnalysis.push_back("STORAGE " + d.device + " (" + d.size + "): " +
                (d.isSSD ? "SOLID STATE (trapped in NAND flash cells)" : "MECHANICAL (spinning wheels of torment)"));
            if (d.smartStatus == "PASSED" || d.smartStatus == "FAILED")
                occultAnalysis.push_back("  SMART: " + d.smartStatus + " — the disk spirits have spoken");
        }

        for (const auto& p : pciDevices) {
            if (p.find("VGA") != string::npos || p.find("3D") != string::npos || p.find("Display") != string::npos)
                occultAnalysis.push_back("GPU: " + p + " — the visual cortex of the beast");
            if (p.find("NVMe") != string::npos || p.find("Non-Volatile") != string::npos)
                occultAnalysis.push_back("NVMe: " + p + " — storage that remembers even when the power dies");
            if (p.find("Network") != string::npos || p.find("Ethernet") != string::npos || p.find("Wireless") != string::npos)
                occultAnalysis.push_back("NET: " + p + " — the machine speaks to the outer darkness through this portal");
            if (p.find("USB") != string::npos || p.find("SATA") != string::npos)
                occultAnalysis.push_back("I/O: " + p + " — data flows through this channel like blood through veins");
        }

        if (!deep.interrupts.empty() && deep.interrupts != "N/A") {
            int irqCount = count(deep.interrupts.begin(), deep.interrupts.end(), '\n') - 1;
            if (irqCount > 0)
                occultAnalysis.push_back("INTERRUPTS: " + to_string(irqCount) + " interrupt vectors — each one a scream from a peripheral in pain");
        }

        if (!deep.kernelMods.empty()) {
            int modCount = count(deep.kernelMods.begin(), deep.kernelMods.end(), '\n');
            occultAnalysis.push_back("KERNEL MODULES: " + to_string(modCount) + " spirits bound into the kernel's fabric");
        }

        if (!deep.netDevices.empty()) {
            int netCount = count(deep.netDevices.begin(), deep.netDevices.end(), '\n');
            occultAnalysis.push_back("NETWORK INTERFACES: " + to_string(netCount / 2) + " portals to the outer darkness");
        }

        occultAnalysis.push_back("UPGRADE ROUNDS COMPLETED: " + to_string(upgradeRound));
        occultAnalysis.push_back("GRIMOIRE ENTRIES: " + to_string(grimoire.size()));
        occultAnalysis.push_back("RITUALS UNLOCKED: " + to_string(rituals.size()));
    }

    string sanitizeForFilename(const string& s) {
        string r = s; for (char& c : r) if (c == ':' || c == ' ') c = '-'; return r;
    }

public:
    HardwareResearchAI() : rng(chrono::steady_clock::now().time_since_epoch().count()) { upgradeRound = 0; }

    void fullResearch() {
        cout << "\n=== " << col("HARDWARE RESEARCH AI — ARCHDEVIL EDITION", C_MAGENTA) << " ===" << endl;
        cout << col("[HARDWARE]", C_CYAN) << " Probing CPU architecture..." << endl; probeCPU();
        cout << col("[HARDWARE]", C_CYAN) << " Scanning memory dimensions..." << endl; probeMemory();
        cout << col("[HARDWARE]", C_CYAN) << " Enumerating PCI devices..." << endl; probePCI();
        cout << col("[HARDWARE]", C_CYAN) << " Probing storage abyss..." << endl; probeStorage();
        cout << col("[HARDWARE]", C_CYAN) << " Reading DMI/SMBIOS dark records..." << endl; probeDMI();
        cout << col("[HARDWARE]", C_CYAN) << " Reading thermal readings..." << endl; probeThermal();

        cout << col("[HARDWARE]", C_MAGENTA) << " Opening MSR registers..." << endl; probeMSR();
        cout << col("[HARDWARE]", C_MAGENTA) << " Dumping CPUID leaves..." << endl; probeCPUID();
        cout << col("[HARDWARE]", C_MAGENTA) << " Decoding ACPI tables..." << endl; probeACPI();
        cout << col("[HARDWARE]", C_MAGENTA) << " Mapping interrupt vectors..." << endl; probeInterrupts();
        cout << col("[HARDWARE]", C_MAGENTA) << " Scanning I/O ports..." << endl; probeIOPorts();
        cout << col("[HARDWARE]", C_MAGENTA) << " Probing network portals..." << endl; probeNetwork();
        cout << col("[HARDWARE]", C_MAGENTA) << " Reading kernel spirits..." << endl; probeKernelModules();
        cout << col("[HARDWARE]", C_MAGENTA) << " Analyzing process hierarchy..." << endl; probeProcessTree();
        cout << col("[HARDWARE]", C_MAGENTA) << " Mapping memory realm..." << endl; probeMemMap();

        cout << col("[HARDWARE]", C_CYAN) << " Generating unique CPU opcodes..." << endl; generateOpcodes();
        cout << col("[HARDWARE]", C_CYAN) << " Generating rituals..." << endl; generateRituals();
        cout << col("[HARDWARE]", C_CYAN) << " Upgrading grimoire..." << endl; upgradeGrimoire();
        cout << col("[HARDWARE]", C_CYAN) << " Conducting occult analysis..." << endl; generateOccultAnalysis();

        if (db) {
            sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS hardware_scan ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "cpu_model TEXT, cpu_cores INTEGER, cpu_threads INTEGER,"
                "cpu_flags TEXT, cpu_opcodes TEXT,"
                "mem_total_kb INTEGER, mem_avail_kb INTEGER,"
                "dmi_system TEXT, upgrade_round INTEGER,"
                "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
                ");", nullptr, nullptr, nullptr);

            sqlite3_stmt* s;
            string sql = "INSERT INTO hardware_scan (cpu_model,cpu_cores,cpu_threads,cpu_flags,cpu_opcodes,mem_total_kb,mem_avail_kb,dmi_system,upgrade_round) VALUES (?,?,?,?,?,?,?,?,?);";
            if (sqlite3_prepare_v2(db, sql.c_str(), -1, &s, nullptr) == SQLITE_OK) {
                string af, ao;
                for (const auto& f : cpu.flags) af += f + " ";
                for (const auto& o : generatedOpcodes) ao += o + "\n";
                sqlite3_bind_text(s, 1, cpu.modelName.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(s, 2, cpu.cores); sqlite3_bind_int(s, 3, cpu.threads);
                sqlite3_bind_text(s, 4, af.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(s, 5, ao.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int64(s, 6, mem.totalKB); sqlite3_bind_int64(s, 7, mem.availKB);
                sqlite3_bind_text(s, 8, dmiSystem.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(s, 9, upgradeRound);
                sqlite3_step(s); sqlite3_finalize(s);
            }
        }
        cout << col("[HARDWARE]", C_GREEN) << " Research complete. Upgrade round " << upgradeRound << " recorded." << endl;
    }

    void printSmallReport() {
        cout << col("\n[HW AI STATUS]", C_GREEN) << " @ " << ts()
             << " | Round " << upgradeRound << endl;
        cout << "  CPU: " << cpu.modelName << " | "
             << cpu.cores << "C/" << cpu.threads << "T | "
             << cpu.flags.size() << " flags | "
             << generatedOpcodes.size() << " opcodes" << endl;
        double usedPct = mem.totalKB > 0 ? 100.0 * (1.0 - (double)mem.availKB / (double)mem.totalKB) : 0;
        cout << "  MEM: " << fixed << setprecision(1) << usedPct << "% | Temp: " << thermalZone
             << " | Rituals: " << rituals.size() << " | Grimoire: " << grimoire.size() << endl;
    }

    void printExtensiveReport() {
        cout << "\n" << string(70, '#') << endl;
        cout << col("  HARDWARE RESEARCH EXTENSIVE REPORT — UPGRADE ROUND " + to_string(upgradeRound), C_BOLD) << endl;
        cout << string(70, '#') << endl;

        cout << "\n" << col("  CPU ARCHITECTURE", C_CYAN) << endl;
        cout << "  " << string(40, '-') << endl;
        cout << "  Model:     " << cpu.modelName << "\n  Vendor:    " << cpu.vendor
             << "\n  Arch:      " << cpu.arch << "\n  Cores:     " << cpu.cores
             << " phys / " << cpu.threads << " log\n  Freq:      " << cpu.maxFreqMHz
             << " MHz\n  Microcode: " << cpu.microcode << " | Step: " << cpu.stepping
             << "\n  Flags:     " << cpu.flags.size() << " ISE\n  Topology:\n" << cpu.topology << endl;

        cout << "\n" << col("  MEMORY", C_CYAN) << endl;
        cout << "  " << string(40, '-') << endl;
        if (mem.totalKB > 0) cout << "  Total: " << fixed << setprecision(2) << (mem.totalKB/1024.0/1024.0) << " GB"
            << "\n  Avail: " << (mem.availKB/1024.0/1024.0) << " GB\n  Free:  " << (mem.freeKB/1024.0/1024.0) << " GB\n";
        cout << "  Swap:  " << (mem.swapTotalKB > 0 ? to_string(mem.swapTotalKB/1024/1024)+" GB" : "N/A") << endl;

        cout << "\n" << col("  STORAGE", C_CYAN) << "\n  " << string(40, '-') << endl;
        for (const auto& d : disks) cout << "  /dev/" << d.device << ": " << d.size << " ["
            << (d.isSSD ? "SSD" : "HDD") << "] SMART: " << d.smartStatus << endl;

        cout << "\n" << col("  DEEP PROBES", C_MAGENTA) << "\n  " << string(40, '-') << endl;
        cout << "  MSR Registers:\n" << deep.msrRaw.substr(0, 400) << endl;
        cout << "\n  CPUID (first 10 lines):\n";
        istringstream cp(deep.cpuidRaw); string cl; int ci = 0;
        while (getline(cp, cl) && ci < 10) { cout << "  " << cl << endl; ci++; }

        cout << "\n  ACPI Tables:\n  " << deep.acpiTables.substr(0, 400) << endl;

        cout << "\n  Interrupt Vectors (top 15):\n";
        istringstream ir(deep.interrupts); string il; int ii = 0;
        while (getline(ir, il) && ii < 15) { cout << "  " << il << endl; ii++; }

        cout << "\n  I/O Port Map (first 20 lines):\n";
        istringstream io(deep.ioPorts); string iol; int ioc = 0;
        while (getline(io, iol) && ioc < 20) { cout << "  " << iol << endl; ioc++; }

        cout << "\n  Network Interfaces:\n  " << deep.netDevices.substr(0, 400) << endl;
        cout << "\n  Kernel Modules (first 10):\n";
        istringstream km(deep.kernelMods); string kl; int ki = 0;
        while (getline(km, kl) && ki < 10) { cout << "  " << kl << endl; ki++; }

        cout << "\n" << col("  BIOS / DMI", C_CYAN) << "\n  " << string(40, '-') << endl;
        cout << dmiBios << dmiBoard << dmiSystem;

        cout << "\n" << col("  THERMAL", C_CYAN) << "\n  " << string(40, '-') << "\n  CPU Temp: " << thermalZone << endl;

        cout << "\n" << col("  GENERATED CPU OPCODES", C_MAGENTA)
             << "\n  " << string(40, '-')
             << "\n  Total: " << generatedOpcodes.size()
             << "\n  Unique to this machine's " << cpu.flags.size() << " flags\n";
        for (const auto& op : generatedOpcodes) cout << "  " << col(op, C_RED) << endl;

        cout << "\n" << col("  RITUALS UNLOCKED", C_MAGENTA)
             << "\n  " << string(40, '-') << endl;
        for (const auto& r : rituals) cout << "  " << col("⚜ ", C_YELLOW) << r << endl;

        cout << "\n" << col("  GRIMOIRE (Upgrade Round " + to_string(upgradeRound) + ")", C_MAGENTA)
             << "\n  " << string(40, '-') << endl;
        for (const auto& g : grimoire) cout << "  " << col("📜 ", C_YELLOW) << g << endl;

        cout << "\n" << col("  OCCULT ANALYSIS", C_MAGENTA)
             << "\n  " << string(40, '-') << endl;
        for (const auto& a : occultAnalysis) cout << "  " << col("~ ", C_YELLOW) << a << endl;

        cout << string(70, '#') << "\n" << endl;
    }

    string generateMarkdownReport() {
        ostringstream md;
        md << "# pcTune_Up Hardware Research Report — Upgrade Round " << upgradeRound << "\n\n";
        md << "**Auto-generated by HardwareResearchAI — Archdevil Edition**  \n";
        md << "**Date:** " << ts() << "  \n";
        md << "**Host:** " << captureCmd("hostname 2>/dev/null") << "  \n";
        md << "**Upgrade Round:** " << upgradeRound << "  \n\n---\n\n";

        md << "## 1. System Overview\n\n| Field | Value |\n|-------|-------|\n";
        md << "| Hostname | " << captureCmd("hostname 2>/dev/null") << " |\n";
        md << "| Kernel | " << captureCmd("uname -r 2>/dev/null") << " |\n";
        md << "| OS | " << captureCmd("cat /etc/os-release 2>/dev/null | grep PRETTY_NAME | cut -d= -f2") << " |\n";
        md << "| Uptime | " << captureCmd("uptime -p 2>/dev/null") << " |\n";
        md << "| UUID | " << (systemUUID.empty() ? "N/A" : systemUUID) << " |\n\n";

        md << "## 2. CPU Architecture\n\n| Field | Value |\n|-------|-------|\n";
        md << "| Model | " << cpu.modelName << " |\n| Vendor | " << cpu.vendor << " |\n| Arch | " << cpu.arch << " |\n"
           << "| Cores | " << cpu.cores << " |\n| Threads | " << cpu.threads << " |\n"
           << "| Max Freq | " << cpu.maxFreqMHz << " MHz |\n| Microcode | " << cpu.microcode << " |\n"
           << "| Stepping | " << cpu.stepping << " |\n| Flags count | " << cpu.flags.size() << " |\n\n";

        md << "### CPU Cache & Topology\n\n```\n" << cpu.topology << "\n```\n\n";
        md << "### CPU Flags\n\n```\n"; for (const auto& f : cpu.flags) md << f << " "; md << "\n```\n\n";

        md << "## 3. Memory\n\n| Field | Value |\n|-------|-------|\n";
        if (mem.totalKB > 0) md << "| Total | " << fixed << setprecision(2) << (mem.totalKB/1024.0/1024.0) << " GB |\n"
            << "| Available | " << (mem.availKB/1024.0/1024.0) << " GB |\n| Free | " << (mem.freeKB/1024.0/1024.0) << " GB |\n";
        md << "| Swap Total | " << (mem.swapTotalKB > 0 ? to_string(mem.swapTotalKB/1024/1024)+" GB" : "N/A") << " |\n\n";

        md << "## 4. Storage\n\n| Device | Size | Type | SMART |\n|--------|------|------|-------|\n";
        for (const auto& d : disks) md << "| /dev/" << d.device << " | " << d.size << " | "
            << (d.isSSD ? "SSD" : "HDD") << " | " << d.smartStatus << " |\n";
        md << "\n";

        md << "## 5. PCI Devices\n\n```\n"; for (const auto& p : pciDevices) md << p << "\n"; md << "```\n\n";
        md << "## 6. BIOS / Firmware\n\n```\n" << dmiBios << dmiBoard << dmiSystem << "```\n\n";
        md << "## 7. Thermal\n\n- CPU Temperature: " << thermalZone << "\n\n";

        md << "## 8. Deep Probe — MSR Registers\n\n```\n" << deep.msrRaw.substr(0, 800) << "\n```\n\n";
        md << "## 9. Deep Probe — ACPI Tables\n\n```\n" << deep.acpiTables.substr(0, 400) << "\n```\n\n";
        md << "## 10. Deep Probe — Interrupt Map\n\n```\n" << deep.interrupts.substr(0, 600) << "\n```\n\n";
        md << "## 11. Deep Probe — I/O Ports\n\n```\n" << deep.ioPorts.substr(0, 800) << "\n```\n\n";
        md << "## 12. Network Interfaces\n\n```\n" << deep.netDevices.substr(0, 400) << "\n```\n\n";
        md << "## 13. Kernel Modules\n\n```\n" << deep.kernelMods.substr(0, 600) << "\n```\n\n";

        md << "## 14. Unique CPU Opcodes (" << generatedOpcodes.size() << ")\n\n";
        md << "> Auto-generated based on this specific machine's CPU features.\n\n";
        md << "| Opcode | Description |\n|--------|-------------|\n";
        for (const auto& op : generatedOpcodes) {
            size_t sep = op.find(" — ");
            md << "| `" << (sep != string::npos ? op.substr(0, sep) : op) << "` | "
               << (sep != string::npos ? op.substr(sep+3) : "—") << " |\n";
        }
        md << "\n";

        md << "## 15. Rituals Unlocked (" << rituals.size() << ")\n\n";
        for (const auto& r : rituals) md << "- " << r << "\n";
        md << "\n";

        md << "## 16. Grimoire (Upgrade Round " << upgradeRound << ")\n\n";
        for (const auto& g : grimoire) md << "- " << g << "\n";
        md << "\n";

        md << "## 17. Occult Analysis\n\n";
        for (const auto& a : occultAnalysis) md << "- " << a << "\n";
        md << "\n---\n";
        md << "_Generated by pcTune_Up HardwareResearchAI — Archdevil Edition — " << ts() << "_\n";
        return md.str();
    }

    void saveMarkdownReport() {
        string dir = "/media/marco/e-learning/coding_projects/ai_projects/pctune_up";
        string ts_clean = sanitizeForFilename(ts());
        string path = dir + "/hardware_report_r" + to_string(upgradeRound) + "_" + ts_clean + ".md";
        string content = generateMarkdownReport();
        ofstream f(path);
        if (f) { f << content; cout << col("[HARDWARE] Report saved: ", C_GREEN) << path << endl; }
        else cout << col("[!] Failed to save report", C_RED) << endl;
    }

    int flagCount() const { return cpu.flags.size(); }
    int opcodeCount() const { return generatedOpcodes.size(); }
    int getUpgradeRound() const { return upgradeRound; }
    string cpuModel() const { return cpu.modelName; }
    int cores() const { return cpu.cores; }
    int threads() const { return cpu.threads; }
};

// =========================================================================
// Hardware Reference DB — compare probed hardware against known baselines
// =========================================================================
struct HardwareReference {
    string type, name, key;
    double referenceScore, referenceValue;
    string unit, source;
};

class HardwareComparator {
private:
    vector<HardwareReference> refDB;

public:
    HardwareComparator() {
        refDB.push_back({"cpu", "Intel Core i9-14900K", "14900K", 25000, 6.0, "GHz boost", "passmark"});
        refDB.push_back({"cpu", "Intel Core i7-14700K", "14700K", 21000, 5.6, "GHz boost", "passmark"});
        refDB.push_back({"cpu", "Intel Core i5-14600K", "14600K", 17200, 5.3, "GHz boost", "passmark"});
        refDB.push_back({"cpu", "Intel Core i9-13900K", "13900K", 24000, 5.8, "GHz boost", "passmark"});
        refDB.push_back({"cpu", "Intel Core i7-13700K", "13700K", 19500, 5.4, "GHz boost", "passmark"});
        refDB.push_back({"cpu", "Intel Core i5-13600K", "13600K", 16000, 5.1, "GHz boost", "passmark"});
        refDB.push_back({"cpu", "Intel Core i9-12900K", "12900K", 18000, 5.2, "GHz boost", "passmark"});
        refDB.push_back({"cpu", "Intel Core i7-12700K", "12700K", 14500, 5.0, "GHz boost", "passmark"});
        refDB.push_back({"cpu", "Intel Core i5-12600K", "12600K", 12000, 4.9, "GHz boost", "passmark"});
        refDB.push_back({"cpu", "AMD Ryzen 9 7950X", "7950X", 28000, 5.7, "GHz boost", "passmark"});
        refDB.push_back({"cpu", "AMD Ryzen 9 7900X", "7900X", 22000, 5.6, "GHz boost", "passmark"});
        refDB.push_back({"cpu", "AMD Ryzen 7 7800X3D", "7800X3D", 18500, 5.0, "GHz boost", "passmark"});
        refDB.push_back({"cpu", "AMD Ryzen 7 7700X", "7700X", 16000, 5.4, "GHz boost", "passmark"});
        refDB.push_back({"cpu", "AMD Ryzen 5 7600X", "7600X", 12500, 5.3, "GHz boost", "passmark"});
        refDB.push_back({"cpu", "AMD Ryzen 9 5950X", "5950X", 17500, 4.9, "GHz boost", "passmark"});
        refDB.push_back({"cpu", "AMD Ryzen 7 5800X3D", "5800X3D", 14000, 4.5, "GHz boost", "passmark"});
        refDB.push_back({"cpu", "AMD Ryzen 5 5600X", "5600X", 10500, 4.6, "GHz boost", "passmark"});
        refDB.push_back({"mem", "DDR5-6000", "DDR5", 55000, 60000, "MB/s", "jdec"});
        refDB.push_back({"mem", "DDR5-5600", "DDR5", 50000, 56000, "MB/s", "jdec"});
        refDB.push_back({"mem", "DDR5-5200", "DDR5", 45000, 52000, "MB/s", "jdec"});
        refDB.push_back({"mem", "DDR4-3600", "DDR4", 28000, 3600, "MHz", "jdec"});
        refDB.push_back({"mem", "DDR4-3200", "DDR4", 25000, 3200, "MHz", "jdec"});
        refDB.push_back({"mem", "DDR4-2666", "DDR4", 21000, 2666, "MHz", "jdec"});
        refDB.push_back({"mem", "DDR3-1600", "DDR3", 12000, 1600, "MHz", "jdec"});
        refDB.push_back({"disk", "NVMe Gen5 SSD", "NVMe", 10000, 10000, "MB/s", "spec"});
        refDB.push_back({"disk", "NVMe Gen4 SSD", "NVMe", 7000, 7000, "MB/s", "spec"});
        refDB.push_back({"disk", "NVMe Gen3 SSD", "NVMe", 3500, 3500, "MB/s", "spec"});
        refDB.push_back({"disk", "SATA SSD", "SATA", 550, 550, "MB/s", "spec"});
        refDB.push_back({"disk", "HDD 7200RPM", "HDD", 160, 160, "MB/s", "spec"});
        refDB.push_back({"disk", "HDD 5400RPM", "HDD", 100, 100, "MB/s", "spec"});
    }

    string compareCPU(const string& model, double benchmarkScore) {
        string bestMatch = "Unknown CPU", bestModel = model;
        double refScore = 0;
        for (auto& r : refDB) {
            if (r.type != "cpu") continue;
            if (model.find(r.key) != string::npos) { bestMatch = r.name; refScore = r.referenceScore; break; }
        }
        ostringstream r;
        r << "\nCPU COMPARISON:";
        if (refScore > 0 && benchmarkScore > 0) {
            double pct = (benchmarkScore / refScore) * 100.0;
            r << "\n  Your CPU matches: " << bestMatch
              << "\n  Your score: " << fixed << setprecision(0) << benchmarkScore
              << " vs reference: " << refScore
              << " (" << (pct >= 100 ? "above" : "below") << " reference by " << abs(100 - (int)pct) << "%)";
        } else r << "\n  Model: " << model << " (no reference match)";
        return r.str();
    }

    string compareMemory(double memBW) {
        string tier = "Unknown"; double refVal = 0;
        for (auto& r : refDB) {
            if (r.type != "mem" || memBW < 8000) continue;
            if (memBW >= r.referenceValue * 0.8) { tier = r.name; refVal = r.referenceValue; break; }
        }
        if (tier == "Unknown") {
            if (memBW > 40000) { tier = "DDR5"; refVal = 50000; }
            else if (memBW > 20000) { tier = "DDR4"; refVal = 25000; }
            else if (memBW > 8000) { tier = "DDR3"; refVal = 12000; }
            else tier = "Legacy DDR";
        }
        ostringstream r;
        r << "\nMEMORY COMPARISON:\n  Bandwidth: " << (int)memBW << " MB/s — " << tier;
        if (refVal > 0) {
            double pct = (memBW / refVal) * 100.0;
            r << " (" << (pct >= 100 ? "above" : "below") << " ref by " << abs(100 - (int)pct) << "%)";
        }
        return r.str();
    }

    string compareDisk(double diskBW, bool isSSD) {
        string tier = isSSD ? "SSD" : "HDD"; double refVal = 0;
        for (auto& r : refDB) {
            if (r.type != "disk") continue;
            if (diskBW >= r.referenceValue * 0.7) { tier = r.name; refVal = r.referenceValue; break; }
        }
        if (refVal == 0) {
            if (diskBW > 5000) { tier = "NVMe Gen4/5"; refVal = 7000; }
            else if (diskBW > 2000) { tier = "NVMe Gen3"; refVal = 3500; }
            else if (diskBW > 400) { tier = "SATA SSD"; refVal = 550; }
            else { tier = "HDD"; refVal = 160; }
        }
        ostringstream r;
        r << "\nDISK COMPARISON:\n  Throughput: " << (int)diskBW << " MB/s — " << tier;
        if (refVal > 0) {
            double pct = (diskBW / refVal) * 100.0;
            r << " (" << (pct >= 100 ? "matches/exceeds" : "below") << " ref by " << abs(100 - (int)pct) << "%)";
        }
        return r.str();
    }

    string fullReport(const HardwareResearchAI& hw, double cpuScore, double memBW, double diskBW) {
        ostringstream r;
        r << "\n" << string(60, '=') << "\n";
        r << col("HARDWARE COMPARISON REPORT", C_BOLD) << "\n";
        r << string(60, '=') << "\n";
        r << compareCPU(hw.cpuModel(), cpuScore) << "\n";
        r << compareMemory(memBW) << "\n";
        r << compareDisk(diskBW, true) << "\n";
        string online = captureCmd("curl -s --max-time 5 https://www.google.com 2>/dev/null | head -1");
        if (!online.empty()) r << "\nONLINE: Internet OK — embedded reference DB (PassMark/JDEC/SPEC)\n";
        else r << "\nONLINE: No internet — embedded reference DB used\n";
        r << string(60, '=') << "\n";
        return r.str();
    }
};

// =========================================================================
// HTML Report Generator — full diagnostic HTML in reports/
// =========================================================================
const string REPORT_DIR = "/media/marco/e-learning/coding_projects/projects/pctune_up/reports";

void ensureReportDir() {
    string mk = "mkdir -p \"" + string(REPORT_DIR) + "\"";
    (void)system(mk.c_str());
}

string generateHTMLReport(const HardwareResearchAI& hw, const string& doctorReport,
                         double cpuScore, double memBW, double diskBW,
                         int cycle, int healthScore, const string& extraContent) {
    ostringstream h;
    string tsStr = ts();
    h << "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'>\n"
      << "<title>pcTune_Up Report — " << tsStr << "</title>\n"
      << "<style>\n"
      << "body{font-family:'Segoe UI',Arial,sans-serif;background:#0a0a0f;color:#e0e0ff;margin:20px;padding:20px}\n"
      << "h1{color:#ff4444;border-bottom:2px solid #ff4444}\n"
      << "h2{color:#ff8844;margin-top:30px}\n"
      << "table{width:100%;border-collapse:collapse;margin:10px 0;background:#1a1a2e}\n"
      << "th,td{padding:8px 12px;text-align:left;border:1px solid #333}\n"
      << "th{background:#2a1a3e;color:#ffaa44}\n"
      << "tr:nth-child(even){background:#12121f}\n"
      << ".section{border-left:4px solid #ff4444;padding-left:15px;margin:20px 0}\n"
      << ".score{font-size:2em;font-weight:bold;text-align:center;padding:20px}\n"
      << ".score.good{color:#44ff44}.score.fair{color:#ffaa44}.score.poor{color:#ff4444}\n"
      << "footer{margin-top:40px;color:#666;font-size:0.85em}\n"
      << "</style></head><body>\n";
    h << "<h1>pcTune_Up v4.0 — Hardware Analysis Report</h1><p>Generated: " << tsStr << "</p>\n";
    h << "<div class='section'><h2>System Overview</h2><table>\n";
    h << "<tr><th>Component</th><th>Detail</th></tr>\n";
    h << "<tr><td>Hostname</td><td>" << captureCmd("hostname 2>/dev/null") << "</td></tr>\n";
    h << "<tr><td>OS</td><td>" << captureCmd("cat /etc/os-release 2>/dev/null | grep PRETTY_NAME | cut -d= -f2") << "</td></tr>\n";
    h << "<tr><td>Kernel</td><td>" << captureCmd("uname -r 2>/dev/null") << "</td></tr>\n";
    h << "<tr><td>Uptime</td><td>" << captureCmd("uptime -p 2>/dev/null") << "</td></tr>\n";
    h << "<tr><td>Round</td><td>" << hw.getUpgradeRound() << "</td></tr>\n";
    h << "<tr><td>Flags</td><td>" << hw.flagCount() << "</td></tr>\n";
    h << "<tr><td>Opcodes</td><td>" << hw.opcodeCount() << "</td></tr>\n";
    h << "</table></div>\n";
    h << "<div class='section'><h2>CPU</h2><table>\n";
    h << "<tr><th>Property</th><th>Value</th></tr>\n";
    h << "<tr><td>Model</td><td>" << hw.cpuModel() << "</td></tr>\n";
    h << "<tr><td>Cores/Threads</td><td>" << hw.cores() << "C / " << hw.threads() << "T</td></tr>\n";
    h << "<tr><td>Benchmark</td><td>" << (cpuScore > 0 ? to_string((int)cpuScore) : "N/A") << "</td></tr>\n";
    h << "</table></div>\n";
    h << "<div class='section'><h2>System Health</h2>\n";
    string sc = healthScore >= 80 ? "good" : (healthScore >= 50 ? "fair" : "poor");
    h << "<div class='score " << sc << "'>" << healthScore << "/100</div></div>\n";
    if (!doctorReport.empty()) h << "<div class='section'><h2>Doctor</h2><pre>" << doctorReport << "</pre></div>\n";
    HardwareComparator hc;
    h << "<div class='section'><h2>Comparison</h2><pre>" << hc.compareCPU(hw.cpuModel(), cpuScore)
      << "\n" << hc.compareMemory(memBW) << "\n" << hc.compareDisk(diskBW, true) << "</pre></div>\n";
    if (!extraContent.empty()) h << "<div class='section'><h2>Intelligence</h2><pre>" << extraContent << "</pre></div>\n";
    h << "<footer>pcTune_Up v4.0 — IntelligenceCore v2 — Archdevil Edition</footer>\n";
    h << "</body></html>\n";
    return h.str();
}

void saveHTMLReport(const string& content) {
    ensureReportDir();
    string tsStr = ts();
    for (char& c : tsStr) if (c == ':' || c == ' ' || c == '/') c = '-';
    string path = REPORT_DIR + "/report_" + tsStr + ".html";
    ofstream f(path);
    if (f) { f << content; cout << col("[REPORT] HTML saved: ", C_GREEN) << path << endl; f.close(); }
    else cout << col("[!] Failed to write HTML report", C_RED) << endl;
}

// =========================================================================
// IntelligenceCore v2 — high-order AI: anomaly detection, adaptive
// thresholds, multi-variate patterns, auto-triage, critical prediction,
// knowledge consolidation, decision engine, process analysis
// =========================================================================
class IntelligenceCore {
private:
    struct MetricHistory {
        deque<double> temps, loads, memPcts, swapPcts, diskPcts;
        size_t maxSamples = 60;
    };
    MetricHistory history;
    mt19937 rng;
    vector<string> insights;

    // ---- Adaptive threshold learner ----
    struct AdaptiveThreshold {
        double baselineMean = 0, baselineStd = 0;
        int sampleCount = 0;
        void update(double v) {
            if (sampleCount == 0) { baselineMean = v; baselineStd = 0.5; sampleCount = 1; return; }
            sampleCount++;
            double oldMean = baselineMean;
            baselineMean = oldMean + (v - oldMean) / sampleCount;
            baselineStd = sqrt(((sampleCount - 1) * baselineStd * baselineStd + (v - oldMean) * (v - baselineMean)) / sampleCount);
            if (baselineStd < 0.1) baselineStd = 0.1;
        }
        bool isAnomalous(double v) const {
            if (sampleCount < 5 || baselineStd < 0.01) return false;
            return abs(v - baselineMean) / baselineStd > (sampleCount > 30 ? 2.5 : 2.0);
        }
        double currentZ(double v) const {
            if (baselineStd < 0.01) return 0;
            return (v - baselineMean) / baselineStd;
        }
    };
    AdaptiveThreshold tempThresh, loadThresh, memThresh, swapThresh, diskThresh;

    // ---- Process anomaly tracking ----
    struct ProcessInfo {
        string name, cpu, mem;
        double cpuPct = 0, memPct = 0;
    };
    vector<ProcessInfo> topProcs;

    void collectProcessMetrics() {
        topProcs.clear();
        string raw = captureCmd("ps aux --sort=-%cpu 2>/dev/null | head -8 | tail -6");
        istringstream ss(raw); string line;
        while (getline(ss, line)) {
            istringstream ls(line);
            string user, pid, cpu, mem, vsz, rss, tty, stat, st, cmd;
            ls >> user >> pid >> cpu >> mem >> vsz >> rss >> tty >> stat >> st;
            getline(ls, cmd);
            ProcessInfo p;
            p.name = cmd.substr(0, 40); p.cpuPct = stod(cpu); p.memPct = stod(mem);
            p.cpu = cpu; p.mem = mem;
            topProcs.push_back(p);
        }
    }

    double getMetric(const string& cmd) {
        string r = captureCmd(cmd); if (r.empty()) return 0;
        try { return stod(r); } catch(...) { return 0; }
    }

    void sampleMetrics() {
        history.temps.push_back(getMetric("cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null") / 1000.0);
        history.loads.push_back(getMetric("cat /proc/loadavg | awk '{print $1}' 2>/dev/null"));
        string mem = captureCmd("free | grep Mem | awk '{print $3/$2*100}' 2>/dev/null");
        history.memPcts.push_back(mem.empty() ? 0 : stod(mem));
        string sw = captureCmd("free | grep Swap | awk '{if($2>0) print $3/$2*100; else print 0}' 2>/dev/null");
        history.swapPcts.push_back(sw.empty() ? 0 : stod(sw));
        string dp = captureCmd("df / | tail -1 | awk '{print $5}' | tr -d '%' 2>/dev/null");
        history.diskPcts.push_back(dp.empty() ? 0 : stod(dp));

        // Update adaptive thresholds
        if (!history.temps.empty()) tempThresh.update(history.temps.back());
        if (!history.loads.empty()) loadThresh.update(history.loads.back());
        if (!history.memPcts.empty()) memThresh.update(history.memPcts.back());
        if (!history.swapPcts.empty()) swapThresh.update(history.swapPcts.back());
        if (!history.diskPcts.empty()) diskThresh.update(history.diskPcts.back());

        if (history.temps.size() > history.maxSamples) history.temps.pop_front();
        if (history.loads.size() > history.maxSamples) history.loads.pop_front();
        if (history.memPcts.size() > history.maxSamples) history.memPcts.pop_front();
        if (history.swapPcts.size() > history.maxSamples) history.swapPcts.pop_front();
        if (history.diskPcts.size() > history.maxSamples) history.diskPcts.pop_front();

        collectProcessMetrics();
    }

    // ---- Multi-variate pattern detection ----
    struct DetectedPattern {
        string name, description, recommendation;
        double confidence;
    };

    vector<DetectedPattern> detectPatterns() {
        vector<DetectedPattern> pats;
        if (history.temps.size() < 5) return pats;

        double t = history.temps.back(), l = history.loads.back();
        double m = history.memPcts.back(), s = history.swapPcts.back(), d = history.diskPcts.back();

        // Memory pressure: high mem + high swap + rising temp
        if (m > 80 && s > 20 && t > 60) {
            pats.push_back({"MEMORY_PRESSURE",
                "High memory (" + to_string((int)m)+"%) + swap usage ("+to_string((int)s)+"%) + elevated temp ("+to_string((int)t)+"C)",
                "Consider closing heavy applications or adding swap space. Run --doctor for cleanup.", 0.85});
        }
        // Thermal throttle risk: high temp + high load + sustained
        if (t > 75 && l > 2.0) {
            pats.push_back({"THERMAL_THROTTLE_RISK",
                "CPU temp " + to_string((int)t)+"C with load " + to_string(l) + " — possible throttling",
                "Check cooling: fan speed, thermal paste, airflow. Consider undervolting.", 0.75});
        }
        // Disk near capacity
        if (d > 90) {
            pats.push_back({"DISK_NEAR_CAPACITY",
                "Disk usage at " + to_string((int)d)+"% — critical storage level",
                "Run --doctor for cache cleanup, or archive old files to external storage.", 0.90});
        }
        // Swap storm: high swap + low available memory
        if (s > 50 && m > 85) {
            pats.push_back({"SWAP_STORM",
                "Swap at " + to_string((int)s)+"% with memory at "+to_string((int)m)+"% — system is paging heavily",
                "Add physical RAM or reduce memory load. zram may help.", 0.80});
        }
        // Idle with high temp: background process issue
        if (t > 65 && l < 0.5) {
            pats.push_back({"IDLE_HIGH_TEMP",
                "CPU idle ("+to_string(l)+" load) but temperature at "+to_string((int)t)+"C",
                "A background process or driver may be keeping the CPU awake. Check with 'powertop'.", 0.70});
        }
        // Process hogging
        for (auto& p : topProcs) {
            if (p.cpuPct > 50.0) {
                pats.push_back({"CPU_HOG",
                    "Process '" + p.name + "' using " + p.cpu + "% CPU",
                    "Consider: kill -9 PID, renice +10, or investigate with 'strace -p PID'", 0.80});
            }
            if (p.memPct > 25.0) {
                pats.push_back({"MEMORY_HOG",
                    "Process '" + p.name + "' using " + p.mem + "% memory",
                    "Check for memory leak with 'watch -n1 ps -p PID -o rss'", 0.75});
            }
        }
        return pats;
    }

    // ---- Auto-triage ----
    string autoTriage(const string& anomalyContext) {
        ostringstream r;
        r << "AUTO-TRIAGE for: " << anomalyContext << "\n";

        // Temperature triage
        if (anomalyContext.find("Temp") != string::npos) {
            string fans = captureCmd("sensors 2>/dev/null | grep -i fan | head -5");
            string freq = captureCmd("cat /proc/cpuinfo | grep -m1 'cpu MHz' | cut -d: -f2 2>/dev/null");
            string throttled = captureCmd("cat /sys/class/thermal/thermal_zone0/trip_point_*_temp 2>/dev/null | head -3");
            r << "  THERMAL DIAGNOSTIC:\n";
            r << "  Current freq: " << (freq.empty() ? "N/A" : freq) << " MHz\n";
            r << "  Fans: " << (fans.empty() ? "N/A (sensors may need lm-sensors)" : fans) << "\n";
            if (!throttled.empty()) r << "  Trip points: " << throttled << "\n";
        }
        // Memory triage
        if (anomalyContext.find("Memory") != string::npos) {
            string top = captureCmd("ps aux --sort=-%mem 2>/dev/null | head -6 | tail -4 | awk '{print $11\": \"$4\"%\"}'");
            string oom = captureCmd("dmesg 2>/dev/null | grep -iE 'oom|out of memory' | tail -3");
            r << "  MEMORY DIAGNOSTIC:\n";
            r << "  Top consumers:\n" << (top.empty() ? "    N/A\n" : top) << "\n";
            if (!oom.empty()) r << "  Recent OOM events:\n" << oom << "\n";
        }
        // Process triage  
        if (anomalyContext.find("Load") != string::npos) {
            string topCpu = captureCmd("ps aux --sort=-%cpu 2>/dev/null | head -6 | tail -4 | awk '{print $11\": \"$3\"%\"}'");
            r << "  PROCESS DIAGNOSTIC:\n";
            r << "  Top CPU consumers:\n" << (topCpu.empty() ? "    N/A\n" : topCpu) << "\n";
            string ctxSw = captureCmd("cat /proc/stat | grep 'ctxt ' | awk '{print $2}' 2>/dev/null");
            if (!ctxSw.empty()) r << "  Context switches: " << ctxSw << "\n";
        }
        // Disk triage
        if (anomalyContext.find("Disk") != string::npos) {
            string inodes = captureCmd("df -i / 2>/dev/null | tail -1 | awk '{print $5}'");
            string ioWait = captureCmd("cat /proc/stat | grep 'procs_running' 2>/dev/null");
            r << "  DISK DIAGNOSTIC:\n";
            r << "  Inode usage: " << (inodes.empty() ? "N/A" : inodes) << "\n";
            r << "  Large files: " << captureCmd("find / -xdev -type f -size +100M 2>/dev/null | head -5") << "\n";
        }
        return r.str();
    }

    // ---- Critical time predictor ----
    struct CriticalPrediction {
        string metric;
        double currentValue, criticalThreshold;
        double estimatedSeconds;
        string humanReadable;
    };

    CriticalPrediction predictCritical(const deque<double>& data, double criticalValue, const string& metricName) {
        CriticalPrediction cp = {metricName, 0, criticalValue, -1, "Insufficient data"};
        if (data.size() < 3) return cp;
        cp.currentValue = data.back();

        // Linear regression
        size_t n = data.size();
        double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
        for (size_t i = 0; i < n; i++) {
            sumX += i; sumY += data[i]; sumXY += i * data[i]; sumX2 += i * i;
        }
        double denom = n * sumX2 - sumX * sumX;
        if (abs(denom) < 0.001) return cp;
        double slope = (n * sumXY - sumX * sumY) / denom;
        double intercept = (sumY - slope * sumX) / n;

        if (abs(slope) < 0.001) { cp.humanReadable = metricName + " is stable — no critical threshold predicted"; return cp; }

        // Solve: criticalValue = slope * steps + intercept
        double stepsToCritical = (criticalValue - intercept) / slope;
        double remainingSteps = stepsToCritical - (n - 1);

        if (remainingSteps <= 0) {
            cp.humanReadable = metricName + " has already reached critical threshold (" + to_string((int)criticalValue) + ")";
            cp.estimatedSeconds = 0;
            return cp;
        }

        cp.estimatedSeconds = remainingSteps * history.maxSamples; // crude estimate
        int minutes = (int)(remainingSteps * history.maxSamples / 60);
        int hours = minutes / 60;
        int days = hours / 24;

        ostringstream hs;
        hs << metricName << " will reach critical level (" << (int)criticalValue << ") in ";
        if (days > 0) hs << days << "d ";
        if (hours > 0) hs << (hours % 24) << "h ";
        hs << (minutes % 60) << "m (at current rate of " << fixed << setprecision(2) << slope << "/step)";
        cp.humanReadable = hs.str();
        return cp;
    }

    // ---- Knowledge consolidation ----
    string consolidateKnowledge() {
        if (!db) return "No database connection for knowledge consolidation.";
        ostringstream r;

        // Count past intelligence entries by category
        sqlite3_stmt* s;
        const char* sql = "SELECT category, COUNT(*), AVG(confidence) FROM intelligence_log GROUP BY category ORDER BY COUNT(*) DESC LIMIT 10;";
        if (sqlite3_prepare_v2(db, sql, -1, &s, nullptr) != SQLITE_OK) return "Cannot read collective memory.";
        r << "KNOWLEDGE CONSOLIDATION:\n";
        r << "  Past observations by category:\n";
        while (sqlite3_step(s) == SQLITE_ROW) {
            string cat = (const char*)sqlite3_column_text(s, 0);
            int cnt = sqlite3_column_int(s, 1);
            double conf = sqlite3_column_double(s, 2);
            r << "    - " << cat << ": " << cnt << " entries (avg confidence: " << fixed << setprecision(0) << (conf*100) << "%)\n";
        }
        sqlite3_finalize(s);

        // Top knowledge graph relations
        sql = "SELECT source, target, relation, AVG(strength) FROM knowledge_graph GROUP BY source, target ORDER BY AVG(strength) DESC LIMIT 5;";
        if (sqlite3_prepare_v2(db, sql, -1, &s, nullptr) == SQLITE_OK) {
            r << "  Learned correlations:\n";
            while (sqlite3_step(s) == SQLITE_ROW) {
                string src = (const char*)sqlite3_column_text(s, 0);
                string tgt = (const char*)sqlite3_column_text(s, 1);
                string rel = (const char*)sqlite3_column_text(s, 2);
                double str = sqlite3_column_double(s, 3);
                r << "    - " << src << " -> " << tgt << " [" << rel << "] strength=" << fixed << setprecision(2) << str << "\n";
            }
            sqlite3_finalize(s);
        }
        return r.str();
    }

    // ---- Decision engine ----
    string decideAction(const vector<DetectedPattern>& patterns) {
        if (patterns.empty()) return "No action required — system state is within normal parameters.";

        // Prioritize by confidence
        vector<DetectedPattern> sorted = patterns;
        sort(sorted.begin(), sorted.end(), [](const DetectedPattern& a, const DetectedPattern& b) {
            return a.confidence > b.confidence;
        });

        ostringstream r;
        r << "DECISION ENGINE — RECOMMENDED ACTIONS:\n";
        int idx = 1;
        for (auto& p : sorted) {
            r << "  " << (idx++) << ". [" << fixed << setprecision(0) << (p.confidence*100) << "%] "
              << p.name << "\n";
            r << "     " << p.recommendation << "\n";
        }
        return r.str();
    }

    void generateInsights() {
        insights.clear();
        sampleMetrics();

        // 1. Adaptive anomaly detection
        auto checkAdaptive = [&](AdaptiveThreshold& th, double v, const string& name, const string& unit) {
            if (!th.isAnomalous(v)) return;
            ostringstream ss;
            ss << "ANOMALY [" << name << "]: " << fixed << setprecision(1) << v << unit
               << " (z-score " << (th.currentZ(v) > 0 ? "+" : "") << th.currentZ(v)
               << ", baseline " << setprecision(1) << th.baselineMean << unit << ")";
            insights.push_back(ss.str());
        };
        if (!history.temps.empty()) checkAdaptive(tempThresh, history.temps.back(), "CPU Temp", "C");
        if (!history.memPcts.empty()) checkAdaptive(memThresh, history.memPcts.back(), "Memory", "%");
        if (!history.loads.empty()) checkAdaptive(loadThresh, history.loads.back(), "CPU Load", "");
        if (!history.swapPcts.empty()) checkAdaptive(swapThresh, history.swapPcts.back(), "Swap", "%");
        if (!history.diskPcts.empty()) checkAdaptive(diskThresh, history.diskPcts.back(), "Disk", "%");

        // 2. Multi-variate patterns
        auto pats = detectPatterns();
        for (auto& p : pats) {
            ostringstream ss;
            ss << "PATTERN [" << p.name << "]: " << p.description << " (confidence: " << fixed << setprecision(0) << (p.confidence*100) << "%)";
            insights.push_back(ss.str());
        }

        // 3. Trend predictions
        vector<pair<deque<double>*, string>> trendSets = {
            {&history.temps, "CPU Temp"}, {&history.memPcts, "Memory Usage"},
            {&history.diskPcts, "Disk Usage"}, {&history.swapPcts, "Swap Usage"}
        };
        for (auto& ts : trendSets) {
            auto tr = linearTrend(*ts.first);
            if (tr.r2 > 0.5 && abs(tr.slope) > 0.05) {
                ostringstream ss;
                ss << "TREND [" << ts.second << "]: " << tr.direction << " (slope "
                   << fixed << setprecision(2) << tr.slope << "/step, R²=" << setprecision(2) << tr.r2 << ")";
                insights.push_back(ss.str());
            }
        }

        // 4. Critical predictions
        auto predDisk = predictCritical(history.diskPcts, 95.0, "Disk usage");
        if (predDisk.estimatedSeconds > 0) insights.push_back("PREDICTION: " + predDisk.humanReadable);
        auto predMem = predictCritical(history.memPcts, 95.0, "Memory usage");
        if (predMem.estimatedSeconds > 0) insights.push_back("PREDICTION: " + predMem.humanReadable);
        auto predSwap = predictCritical(history.swapPcts, 80.0, "Swap usage");
        if (predSwap.estimatedSeconds > 0) insights.push_back("PREDICTION: " + predSwap.humanReadable);

        // 5. Cross-correlations
        if (history.temps.size() >= 5 && history.loads.size() >= 5) {
            double tx = 0, ty = 0, txy = 0, tx2 = 0, ty2 = 0;
            size_t nc = min(history.temps.size(), history.loads.size());
            for (size_t i = 0; i < nc; i++) {
                tx += history.temps[i]; ty += history.loads[i];
                txy += history.temps[i] * history.loads[i];
                tx2 += history.temps[i] * history.temps[i];
                ty2 += history.loads[i] * history.loads[i];
            }
            double denom = sqrt((nc * tx2 - tx * tx) * (nc * ty2 - ty * ty));
            if (denom > 0.001) {
                double r = (nc * txy - tx * ty) / denom;
                if (abs(r) > 0.7) {
                    ostringstream ss;
                    ss << "CORRELATION [Temp vs Load]: r=" << fixed << setprecision(2) << r
                       << " — " << (r > 0 ? "load drives temperature up" : "inverse relationship — unusual");
                    insights.push_back(ss.str());
                }
            }
        }
    }

    // Linear regression (same as before)
    struct Trend { double slope, intercept, r2; string direction; };
    Trend linearTrend(const deque<double>& data) {
        Trend t = {0, 0, 0, "stable"};
        size_t n = data.size();
        if (n < 3) return t;
        double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
        for (size_t i = 0; i < n; i++) {
            sumX += i; sumY += data[i]; sumXY += i * data[i]; sumX2 += i * i;
        }
        double denom = n * sumX2 - sumX * sumX;
        if (abs(denom) < 0.001) return t;
        t.slope = (n * sumXY - sumX * sumY) / denom;
        t.intercept = (sumY - t.slope * sumX) / n;
        double ssTot = 0, ssRes = 0, yMean = sumY / n;
        for (size_t i = 0; i < n; i++) {
            double yPred = t.intercept + t.slope * i;
            ssTot += (data[i] - yMean) * (data[i] - yMean);
            ssRes += (data[i] - yPred) * (data[i] - yPred);
        }
        t.r2 = ssTot > 0 ? 1.0 - ssRes / ssTot : 0;
        t.direction = abs(t.slope) < 0.01 ? "stable" : (t.slope > 0 ? "rising" : "falling");
        return t;
    }

public:
    IntelligenceCore() : rng(chrono::steady_clock::now().time_since_epoch().count()) {
        if (db) {
            sqlite3_exec(db,
                "CREATE TABLE IF NOT EXISTS intelligence_log ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "category TEXT NOT NULL,"
                "content TEXT NOT NULL,"
                "confidence REAL DEFAULT 0.5,"
                "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
                ");", nullptr, nullptr, nullptr);
            sqlite3_exec(db,
                "CREATE TABLE IF NOT EXISTS knowledge_graph ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "source TEXT NOT NULL,"
                "target TEXT NOT NULL,"
                "relation TEXT NOT NULL,"
                "strength REAL DEFAULT 0.5,"
                "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP"
                ");", nullptr, nullptr, nullptr);
        }
    }

    // ---- NLU v2 ----
    struct IntentResult { string action; double confidence; };
    IntentResult classifyIntent(const string& query) {
        string q = query;
        transform(q.begin(), q.end(), q.begin(), ::tolower);
        map<string, double> scores;
        auto score = [&](const vector<string>& keywords, const string& intent, double w) {
            double s = 0;
            for (auto& k : keywords) if (q.find(k) != string::npos) s += w;
            if (s > 0) scores[intent] += s;
        };
        score({"cpu", "processor", "core", "frequency", "ghz"}, "cpu", 1.0);
        score({"memory", "ram", "mem"}, "ram", 1.0);
        score({"disk", "drive", "storage", "hdd", "ssd", "space", "free"}, "disks", 1.0);
        score({"gpu", "graphics", "video", "display", "vga"}, "gpu", 1.0);
        score({"network", "net", "internet", "wifi", "lan", "eth", "ip"}, "network", 1.0);
        score({"opcode", "hex", "instruction", "demon"}, "opcodes", 1.0);
        score({"round", "upgrade", "level", "progress"}, "round", 1.0);
        score({"ritual", "ceremony", "summon", "incantation"}, "rituals", 1.0);
        score({"status", "health", "state", "condition", "now"}, "status", 1.0);
        score({"demon", "devil", "hell", "satan", "evil", "spirit", "ghost"}, "demons", 1.0);
        score({"help", "command", "what", "can you", "list", "?"}, "help", 1.0);
        score({"quit", "exit", "bye", "leave", "stop", "end"}, "quit", 1.0);
        score({"predict", "future", "forecast", "trend", "will"}, "predict", 1.0);
        score({"anomaly", "strange", "wrong", "issue", "problem", "alert"}, "anomalies", 1.0);
        score({"insight", "intelligence", "learn", "know", "discover"}, "insights", 1.0);
        score({"benchmark", "performance", "speed", "score"}, "benchmark", 1.0);
        score({"triage", "diagnose", "why", "explain"}, "triage", 1.0);
        score({"pattern", "detect", "recognize"}, "patterns", 1.0);
        score({"decide", "action", "recommend", "suggest", "should"}, "decisions", 1.0);
        score({"consolidate", "summary", "overview", "history"}, "consolidate", 1.0);
        score({"process", "hog", "top", "running", "ps"}, "processes", 1.0);
        score({"threshold", "baseline", "normal"}, "thresholds", 1.0);
        string best = "unknown"; double bs = 0;
        for (auto& p : scores) if (p.second > bs) { bs = p.second; best = p.first; }
        return {best, bs / 5.0};
    }

    string analyzeQuery(const string& query, const HardwareResearchAI& hw) {
        auto intent = classifyIntent(query);

        if (intent.action == "cpu") {
            ostringstream r;
            r << "CPU: " << hw.cpuModel() << " | " << hw.cores() << "C/" << hw.threads() << "T | "
              << hw.flagCount() << " occult flags [confidence: " << fixed << setprecision(0) << (intent.confidence*100) << "%]";
            // Add adaptive threshold context
            if (tempThresh.sampleCount > 5) {
                r << "\n  Temp baseline: " << (int)tempThresh.baselineMean << "C ±" << (int)tempThresh.baselineStd << "C";
            }
            return r.str();
        }
        if (intent.action == "opcodes") return to_string(hw.opcodeCount()) + " unique CPU opcodes extracted from " + to_string(hw.flagCount()) + " infernal flags";
        if (intent.action == "round") return "Upgrade round " + to_string(hw.getUpgradeRound()) + " — " + to_string(hw.opcodeCount()) + " opcodes, " + to_string(hw.flagCount()) + " flags mapped";
        if (intent.action == "rituals") { int r = hw.getUpgradeRound(); return to_string(r / 3 + 1) + " rituals unlocked through " + to_string(r) + " upgrade rounds"; }
        if (intent.action == "demons") return "12 demons catalogued in the grimoire. Type 'demons' again for their names.";
        if (intent.action == "status") {
            string s = captureCmd("free -h | grep Mem | awk '{print $3\"/\"$2}' 2>/dev/null");
            string t = captureCmd("cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null");
            string tC = "?"; if (!t.empty()) { try { tC = to_string((int)(stod(t)/1000.0))+"C"; } catch(...){} }
            ostringstream r;
            r << "STATUS: MEM " << (s.empty()?"?":s) << " | CPU " << tC << " | Round " << hw.getUpgradeRound();
            if (tempThresh.sampleCount > 5) r << "\n  Adaptive baselines — Temp: " << (int)tempThresh.baselineMean << "C | Mem: " << (int)memThresh.baselineMean << "% | Load: " << fixed << setprecision(1) << loadThresh.baselineMean;
            return r.str();
        }
        if (intent.action == "ram") { string m = captureCmd("free -h | grep -E 'Mem|Swap' 2>/dev/null"); return "Memory:\n" + m; }
        if (intent.action == "disks") { string d = captureCmd("df -h / 2>/dev/null | tail -1"); return "Disk: " + d; }
        if (intent.action == "gpu") { string g = captureCmd("lspci 2>/dev/null | grep -iE 'VGA|3D|Display'"); return "GPU:\n" + g; }
        if (intent.action == "network") { string n = captureCmd("ip -br addr 2>/dev/null | grep -v lo | head -5"); return "Network:\n" + n; }
        if (intent.action == "predict") {
            generateInsights(); ostringstream r;
            r << "PREDICTIVE ANALYSIS [" << tempThresh.sampleCount << " samples]:\n";
            for (auto& ins : insights) r << "  - " << ins << "\n";
            if (insights.empty()) r << "  Gathering data...\n";
            return r.str();
        }
        if (intent.action == "anomalies") {
            generateInsights(); ostringstream r; r << "ANOMALY REPORT:\n"; bool f = false;
            for (auto& ins : insights) { if (ins.find("ANOMALY") != string::npos || ins.find("PATTERN") != string::npos || ins.find("PREDICTION") != string::npos) { r << "  - " << ins << "\n"; f = true; } }
            if (!f) r << "  No anomalies — system is stable.\n";
            return r.str();
        }
        if (intent.action == "insights") {
            generateInsights(); ostringstream r; r << "FULL INTELLIGENCE REPORT:\n";
            for (auto& ins : insights) r << "  - " << ins << "\n";
            if (insights.empty()) r << "  Gathering data...\n";
            return r.str();
        }
        if (intent.action == "triage") {
            generateInsights();
            ostringstream r; r << "TRIAGE:\n";
            for (auto& ins : insights) {
                if (ins.find("ANOMALY") != string::npos) r << autoTriage(ins) << "\n";
            }
            if (r.str().size() < 20) r << "  No anomalies to triage.\n";
            return r.str();
        }
        if (intent.action == "patterns") {
            sampleMetrics(); auto pats = detectPatterns(); ostringstream r;
            r << "PATTERN RECOGNITION [" << pats.size() << " patterns]:\n";
            for (auto& p : pats) r << "  - [" << fixed << setprecision(0) << (p.confidence*100) << "%] " << p.name << ": " << p.description << "\n";
            if (pats.empty()) r << "  No significant patterns detected.\n";
            return r.str();
        }
        if (intent.action == "decisions") {
            sampleMetrics(); auto pats = detectPatterns(); return decideAction(pats);
        }
        if (intent.action == "consolidate") { return consolidateKnowledge(); }
        if (intent.action == "processes") {
            collectProcessMetrics(); ostringstream r;
            r << "TOP PROCESSES:\n";
            for (auto& p : topProcs) r << "  " << p.cpu << "% CPU  " << p.mem << "% MEM  " << p.name << "\n";
            return r.str();
        }
        if (intent.action == "thresholds") {
            ostringstream r; r << "ADAPTIVE THRESHOLDS (" << tempThresh.sampleCount << " samples):\n";
            r << "  Temp:   baseline " << (int)tempThresh.baselineMean << "C ±" << (int)tempThresh.baselineStd << "C\n";
            r << "  Mem:    baseline " << (int)memThresh.baselineMean << "% ±" << (int)memThresh.baselineStd << "%\n";
            r << "  Load:   baseline " << fixed << setprecision(2) << loadThresh.baselineMean << " ±" << loadThresh.baselineStd << "\n";
            r << "  Swap:   baseline " << (int)swapThresh.baselineMean << "% ±" << (int)swapThresh.baselineStd << "%\n";
            r << "  Disk:   baseline " << (int)diskThresh.baselineMean << "% ±" << (int)diskThresh.baselineStd << "%\n";
            return r.str();
        }
        if (intent.action == "help") return "Commands: cpu ram disks gpu network opcodes round rituals status predict anomalies insights triage patterns decisions consolidate processes thresholds help quit";
        if (intent.action == "quit") return "__EXIT__";

        vector<string> occultResp = {
            "The CPU whispers, but I cannot understand that query.",
            "Consult the grimoire. Try: help, predict, anomalies, patterns, decisions, triage, thresholds, processes, consolidate",
            "The spirits do not recognize \"" + query + "\"",
            "Speak in simpler terms, or try 'help' for the full command list.",
            "That answer has not yet been summoned.",
        };
        uniform_int_distribution<size_t> d(0, occultResp.size() - 1);
        return occultResp[d(rng)];
    }

    // ---- Self-learning ----
    void learnFromBenchmark(double beforeScore, double afterScore, const string& component) {
        if (!db) return;
        double delta = afterScore - beforeScore;
        ostringstream content;
        content << component << ": delta=" << fixed << setprecision(2) << delta << " (before=" << beforeScore << ", after=" << afterScore << ")";
        sqlite3_stmt* s;
        if (sqlite3_prepare_v2(db, "INSERT INTO intelligence_log (category,content,confidence) VALUES (?,?,?);", -1, &s, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(s, 1, "self_learn", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(s, 2, content.str().c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(s, 3, min(1.0, max(0.0, (delta + 100.0) / 200.0)));
            sqlite3_step(s); sqlite3_finalize(s);
        }
        if (sqlite3_prepare_v2(db, "INSERT INTO knowledge_graph (source,target,relation,strength) VALUES (?,?,?,?);", -1, &s, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(s, 1, "benchmark", -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(s, 2, component.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(s, 3, "performance_impact", -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(s, 4, delta / 100.0);
            sqlite3_step(s); sqlite3_finalize(s);
        }
    }

    string suggestOptimizations() {
        if (!db) return "Insufficient intelligence gathered yet.";
        sqlite3_stmt* s;
        string sql = "SELECT content FROM intelligence_log WHERE category='self_learn' ORDER BY confidence DESC LIMIT 3;";
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &s, nullptr) != SQLITE_OK) return "Cannot query the collective memory.";
        ostringstream r; r << "OPTIMIZATION RECOMMENDATIONS:\n"; int c = 0;
        while (sqlite3_step(s) == SQLITE_ROW) { r << "  - Based on past learning: " << (const char*)sqlite3_column_text(s, 0) << "\n"; c++; }
        sqlite3_finalize(s);
        if (c == 0) r << "  No past optimization data. Run --benchmark to gather data.\n";
        return r.str();
    }

    vector<string> getInsights() {
        if (insights.empty()) generateInsights();
        return insights;
    }
};

// =========================================================================
// AI Interactive Console — speak with the machine's soul (v2: NLU)
// =========================================================================
class InteractiveConsole {
private:
    thread consoleThread;
    bool consoleRunning = false;

    string analyze(const string& query, const HardwareResearchAI& hw, IntelligenceCore& brain) {
        // Use IntelligenceCore NLU for all queries
        return brain.analyzeQuery(query, hw);
    }

public:
    void start(const HardwareResearchAI& hw, IntelligenceCore& brain) {
        consoleRunning = true;
        consoleThread = thread([this, &hw, &brain]() {
            string line;
            cout << col("\n[AI CONSOLE v3]", C_CYAN)
                 << " IntelligenceCore v2 active — adaptive thresholds, patterns, triage, decisions."
                 << "\n  Try: help, predict, patterns, decisions, triage, thresholds, processes, consolidate"
                 << "\n  Or: cpu, ram, disks, gpu, network, opcodes, rituals, status, anomalies, insights, quit\n";
            while (consoleRunning) {
                cout << col("> ", C_MAGENTA); cout.flush();
                if (!getline(cin, line)) { consoleRunning = false; break; }
                if (line.empty()) continue;
                string resp = analyze(line, hw, brain);
                if (resp == "__EXIT__") {
                    cout << col("[AI] Leaving the console. The machine will remember you.\n", C_GREEN);
                    consoleRunning = false; break;
                }
                cout << col("[AI] ", C_GREEN) << resp << endl;
            }
        });
    }

    bool running() const { return consoleRunning; }

    void stop() {
        consoleRunning = false;
        if (consoleThread.joinable()) consoleThread.join();
    }
};

// =========================================================================
// Dashboard — real-time ANSI TUI (no ncurses needed)
// =========================================================================
void renderDashboard(const HardwareResearchAI& hw, int cycle, time_t startTime, int gpuCount) {
    const int W = 80;

    string loadStr = captureCmd("cat /proc/loadavg | awk '{print $1\", \"$2\", \"$3}' 2>/dev/null");
    string memStr = captureCmd("free -h | grep Mem | awk '{print $3\"/\"$2}' 2>/dev/null");
    string swapStr = captureCmd("free -h | grep Swap | awk '{print $3\"/\"$2}' 2>/dev/null");
    string cpuTemp = captureCmd("cat /sys/class/thermal/thermal_zone0/temp 2>/dev/null");
    if (!cpuTemp.empty()) { try { double t = stod(cpuTemp) / 1000.0; cpuTemp = to_string((int)t) + "C"; } catch(...){ cpuTemp="?"; } }
    string uptime = captureCmd("uptime -p 2>/dev/null | sed 's/up //'");
    string diskUse = captureCmd("df / 2>/dev/null | tail -1 | awk '{print $5}'");
    string procs = captureCmd("ps aux 2>/dev/null | wc -l");

    ostringstream dash;
    dash << "\033[2J\033[H";

    dash << col("╔════════════════════════════════════════════════════════════════════════════╗\n", C_CYAN);
    dash << col("║", C_CYAN) << "  pcTune_Up v4.0 — ARCHDEVIL DASHBOARD [IntelligenceCore ONLINE]  "
         << col("║\n", C_CYAN);
    dash << col("╠══════════════════════╦══════════════════════╦════════════════════════════╣\n", C_CYAN);

    dash << col("║", C_CYAN) << " CPU: " << left << setw(14) << hw.cpuModel().substr(0, 14)
         << col("║", C_CYAN) << " MEM: " << left << setw(14) << (memStr.empty() ? "?" : memStr)
         << col("║", C_CYAN) << " Cycle: " << left << setw(16) << to_string(cycle)
         << col("║\n", C_CYAN);
    dash << col("║", C_CYAN) << " Load: " << left << setw(14) << loadStr
         << col("║", C_CYAN) << " Swap: " << left << setw(14) << (swapStr.empty() ? "0/0" : swapStr)
         << col("║", C_CYAN) << " Round: " << left << setw(16) << to_string(hw.getUpgradeRound())
         << col("║\n", C_CYAN);
    dash << col("║", C_CYAN) << " Temp: " << left << setw(14) << cpuTemp
         << col("║", C_CYAN) << " Disk: " << left << setw(14) << (diskUse.empty() ? "?" : diskUse + " used")
         << col("║", C_CYAN) << " Flags: " << left << setw(16) << to_string(hw.flagCount())
         << col("║\n", C_CYAN);

    dash << col("╠══════════════════════╩══════════════════════╩════════════════════════════╣\n", C_CYAN);

    dash << col("║", C_CYAN) << "  Cores: " << hw.cores() << "C/" << hw.threads() << "T"
         << " | Opcodes: " << hw.opcodeCount()
         << " | Procs: " << (procs.empty() ? "?" : procs)
         << " | Uptime: " << (uptime.empty() ? "?" : uptime)
         << string(max(0, W - 60 - (int)uptime.size()), ' ')
         << col("║\n", C_CYAN);

    if (gpuCount > 0) {
        string gpuLine = captureCmd("lspci 2>/dev/null | grep -iE 'VGA|3D|Display' | head -1 | cut -d: -f3-");
        dash << col("║", C_CYAN) << "  GPU: " << (gpuLine.empty() ? "Detected" : gpuLine.substr(0, 50))
             << string(max(0, W - 58 - (int)gpuLine.size()), ' ') << col("║\n", C_CYAN);
    }

    dash << col("╚════════════════════════════════════════════════════════════════════════════╝\n", C_CYAN);
    dash << col("  [Ctrl+C to exit] [--interactive for AI console]", C_YELLOW) << endl;

    cout << dash.str();
}

void startDashboard(const HardwareResearchAI& hw, int cycle, time_t startTime, int gpuCount) {
    while (running) {
        renderDashboard(hw, cycle, startTime, gpuCount);
        for (int i = 0; i < 20 && running; i++) this_thread::sleep_for(chrono::milliseconds(100));
    }
}

// =========================================================================
// Daemon mode — stalk the system in the shadows
// =========================================================================
void daemonize() {
    pid_t pid = fork();
    if (pid < 0) { cerr << "[!] Fork failed" << endl; exit(1); }
    if (pid > 0) {
        ofstream pf("/var/run/pctune_up.pid");
        if (pf) { pf << pid << endl; pf.close(); }
        cout << "[DAEMON] Forked to background (PID: " << pid << ")" << endl;
        exit(0);
    }

    umask(0);
    setsid();

    pid = fork();
    if (pid < 0) exit(1);
    if (pid > 0) exit(0);

    chdir("/");
    close(0); close(1); close(2);
    open("/dev/null", O_RDWR);
    dup(0); dup(0);
}

// =========================================================================
// Progress / Heartbeat
// =========================================================================
void printProgress(int cycle, time_t start) {
    time_t now = time(nullptr);
    double elapsed = difftime(now, start) / 60.0;
    cout << "\n" << string(50, '=') << endl;
    cout << "  pcTune_Up v4.0" << endl;
    cout << "  Cycle " << cycle << " completed" << endl;
    cout << "  Elapsed: " << (int)elapsed << " min" << endl;
    cout << "  " << ts() << endl;
    cout << string(50, '=') << "\n" << endl;
}

void heartbeat(int cycle) {
    cout << col("  [HEARTBEAT]", C_GREEN) << " Cycle " << cycle
         << " running @ " << ts()
         << " (next optimizations in ~" << cfg.cycleInt << "s)" << endl;
}

// =========================================================================
// CLI flags
// =========================================================================
void printUsage(const char* name) {
    cout << "Usage: " << name << " [options]" << endl;
    cout << "  --help           Show this help" << endl;
    cout << "  --dry-run        Show what would be done without doing it" << endl;
    cout << "  --dangerous      Enable aggressive/experimental optimizations" << endl;
    cout << "  --doctor         Run DOCTOR PC-CLEANUP health analysis" << endl;
    cout << "  --doctor-apply   Run doctor and apply prescriptions" << endl;
    cout << "  --autopilot      Auto-pilot mode: reports 2min / 7min" << endl;
    cout << "  --research       One-shot hardware research + markdown report" << endl;
    cout << "  --gpu            Probe + optimize GPU" << endl;
    cout << "  --network        Tune network stack (BBR, buffers, qdisc)" << endl;
    cout << "  --benchmark      Run CPU/MEM/DISK/NET benchmarks" << endl;
    cout << "  --interactive    Interactive AI console (type commands)" << endl;
    cout << "  --daemon         Fork to background (daemon mode)" << endl;
    cout << "  --intelligence   Show AI anomaly/prediction/insight reports" << endl;
    cout << "  --interval N     Set cycle interval in seconds (default: 300)" << endl;
    cout << "  --sysd           Optimize systemd (disable bloat, vacuum journal)" << endl;
    cout << "  --security       Harden SSH, firewall, kernel (ASLR, rp_filter)" << endl;
    cout << "  --packages       Update all system packages (apt upgrade)" << endl;
    cout << "  --audit          Check rootkits, SUID, world-writable, listening ports" << endl;
    cout << "  --boot           Optimize GRUB, mask plymouth, measure boot time" << endl;
    cout << "  --cron           Inspect cron jobs (user + system)" << endl;
    cout << "  --all-admin      Run all admin tasks above" << endl;
    cout << "  --compare        Compare hardware against reference database" << endl;
    cout << "  --html-report    Generate HTML diagnostic report in reports/" << endl;
    cout << "  --no-color       Disable colored output" << endl;
}

bool parseFlags(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "--help") { printUsage(argv[0]); return false; }
        else if (a == "--dry-run") cfg.dryRun = true;
        else if (a == "--dangerous") cfg.dangerous = true;
        else if (a == "--doctor") cfg.doctorMode = true;
        else if (a == "--doctor-apply") { cfg.doctorMode = true; cfg.applyDoctor = true; }
        else if (a == "--autopilot") { cfg.autoPilot = true; cfg.cycleInt = 120; }
        else if (a == "--research") cfg.researchMode = true;
        else if (a == "--gpu") cfg.gpuMode = true;
        else if (a == "--network") cfg.netMode = true;
        else if (a == "--benchmark") cfg.benchmarkMode = true;
        else if (a == "--interactive") cfg.interactive = true;
        else if (a == "--daemon") cfg.daemonMode = true;
        else if (a == "--intelligence") cfg.intelligenceMode = true;
        else if (a == "--sysd") cfg.sysdMode = true;
        else if (a == "--security") cfg.secMode = true;
        else if (a == "--packages") cfg.pkgMode = true;
        else if (a == "--audit") cfg.auditMode = true;
        else if (a == "--boot") cfg.bootMode = true;
        else if (a == "--cron") cfg.cronMode = true;
        else if (a == "--all-admin") cfg.adminMode = true;
        else if (a == "--compare") cfg.compareMode = true;
        else if (a == "--html-report") cfg.htmlReport = true;
        else if (a == "--no-color") cfg.color = false;
        else if (a == "--interval" && i + 1 < argc) {
            cfg.cycleInt = stoi(argv[++i]);
            if (cfg.cycleInt < 60) cfg.cycleInt = 60;
        } else {
            cerr << "Unknown flag: " << a << endl;
            printUsage(argv[0]);
            return false;
        }
    }
    return true;
}

// =========================================================================
// Banner
// =========================================================================
void printBanner() {
    cout << col(R"(
    ___   ____  ____  _   _  _   __
   / _ \ / ___|/ ___|| \ | |/ | / /
  | | | | |    \___ \|  \| | | |/ /
  | |_| | |___ ___) | |\  | |   <
   \___/ \____|____/|_| \_| |_|\_\
   pcTune_Up v4.0 - AI Super PC TuneUp
  Linux System Optimizer + Research AI + DOCTOR PC-CLEANUP + IntelligenceCore
  )", C_CYAN) << endl;

    runCmd("cat /etc/os-release 2>/dev/null | grep PRETTY_NAME | cut -d= -f2", "OS");
    runCmd("uname -r", "Kernel");

    cout << col("[INFO]", C_YELLOW) << " --gpu / --network / --benchmark / --interactive / --daemon / --intelligence" << endl;
    cout << col("[INFO]", C_YELLOW) << " --sysd / --security / --packages / --audit / --boot / --cron / --compare" << "\n" << endl;
}

// =========================================================================
// Main
// =========================================================================
int main(int argc, char** argv) {
    if (!parseFlags(argc, argv)) return 1;

    // Daemonize first, before any output
    if (cfg.daemonMode) daemonize();

    printBanner();

    if (!isRoot()) {
        cout << col("[!] Run: sudo ./pctune_up [options]", C_RED) << endl;
        return 1;
    }

    if (cfg.dryRun) cout << col("[OK] DRY-RUN", C_YELLOW) << endl;
    if (cfg.dangerous) cout << col("[!] DANGEROUS", C_RED) << endl;
    if (cfg.doctorMode) cout << col("[OK] DOCTOR PC-CLEANUP", C_GREEN) << endl;
    if (cfg.autoPilot) cout << col("[OK] AUTO-PILOT", C_GREEN) << endl;
    if (cfg.gpuMode) cout << col("[OK] GPU Demonology", C_GREEN) << endl;
    if (cfg.netMode) cout << col("[OK] Network Abyss", C_GREEN) << endl;
    if (cfg.benchmarkMode) cout << col("[OK] Benchmark Engine", C_MAGENTA) << endl;
    if (cfg.interactive) cout << col("[OK] Interactive Console", C_MAGENTA) << endl;
    if (cfg.daemonMode) cout << col("[OK] Daemon mode", C_YELLOW) << endl;
    if (cfg.intelligenceMode) cout << col("[OK] IntelligenceCore HIGH-ORDER AI", C_MAGENTA) << endl;

    cout << "\n" << col("[OK] Running as root.", C_GREEN) << endl;

    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);

    initDatabase();
    OccultResearchAI ai;
    DoctorPCCleanup doctor;
    HardwareResearchAI hwAI;
    BenchmarkEngine bench;
    IntelligenceCore brain;
    InteractiveConsole console;

    if (cfg.benchmarkMode || cfg.interactive || cfg.intelligenceMode) {
        cout << col("[AI] IntelligenceCore online", C_CYAN)
             << " — anomaly detection, trend prediction, NLU active" << endl;
    }

    // Detect GPU count for dashboard
    int gpuCount = stoi(captureCmd("lspci 2>/dev/null | grep -ciE 'VGA|3D|Display'"));

    // --research: one-shot
    if (cfg.researchMode) {
        cout << col("\n>>> ONE-SHOT HARDWARE RESEARCH <<<\n", C_MAGENTA) << endl;
        hwAI.fullResearch();
        hwAI.printExtensiveReport();
        hwAI.saveMarkdownReport();
        closeDatabase();
        cout << col("\n[OK] Done.", C_GREEN) << endl;
        return 0;
    }

    // --intelligence-only: show AI insights (no optimization loop)
    if (cfg.intelligenceMode && !cfg.interactive && !cfg.autoPilot && !cfg.doctorMode && !cfg.gpuMode && !cfg.netMode && !cfg.benchmarkMode) {
        cout << col("\n>>> INTELLIGENCECORE v2 FULL REPORT <<<\n", C_MAGENTA) << endl;
        for (int i = 0; i < 5 && running; i++) {
            auto ins = brain.getInsights();
            cout << "\n=== " << col("Intelligence Snapshot", C_CYAN) << " (" << (i+1) << "/5) ===" << endl;
            if (ins.empty()) cout << "  Gathering data..." << endl;
            for (const auto& in : ins) cout << "  - " << in << endl;
            cout << "\n" << col("Pattern Analysis:", C_BOLD) << endl;
            cout << brain.analyzeQuery("patterns", hwAI) << endl;
            cout << "\n" << col("Decision Engine:", C_BOLD) << endl;
            cout << brain.analyzeQuery("decisions", hwAI) << endl;
            cout << "\n" << col("Knowledge Consolidation:", C_BOLD) << endl;
            cout << brain.analyzeQuery("consolidate", hwAI) << endl;
            cout << brain.suggestOptimizations() << endl;
            if (i < 4) { cout << "  Sampling again in 10s..." << endl; this_thread::sleep_for(chrono::seconds(10)); }
        }
        closeDatabase();
        return 0;
    }

    // --interactive-only: console mode (no optimization loop)
    if (cfg.interactive && !cfg.autoPilot && !cfg.doctorMode && !cfg.gpuMode && !cfg.netMode && !cfg.benchmarkMode) {
        cout << col("\n>>> INTERACTIVE AI CONSOLE v2 <<<\n", C_MAGENTA) << endl;
        hwAI.fullResearch();
        console.start(hwAI, brain);
        while (running && console.running()) this_thread::sleep_for(chrono::milliseconds(200));
        console.stop();
        closeDatabase();
        return 0;
    }

    // --admin standalone tasks (no optimization loop)
    bool adminRun = cfg.adminMode || cfg.sysdMode || cfg.secMode || cfg.pkgMode || cfg.auditMode || cfg.bootMode || cfg.cronMode;
    if (adminRun && !cfg.interactive && !cfg.autoPilot && !cfg.doctorMode && !cfg.gpuMode && !cfg.netMode && !cfg.benchmarkMode && !cfg.researchMode && !cfg.intelligenceMode) {
        if (cfg.sysdMode || cfg.adminMode) optimizeSystemd();
        if (cfg.secMode || cfg.adminMode) optimizeSecurity();
        if (cfg.pkgMode || cfg.adminMode) updatePackages();
        if (cfg.auditMode || cfg.adminMode) auditSystem();
        if (cfg.bootMode || cfg.adminMode) optimizeBoot();
        if (cfg.cronMode || cfg.adminMode) manageCron();
        closeDatabase();
        return 0;
    }

    // --compare: hardware comparison
    if (cfg.compareMode && !cfg.interactive && !cfg.autoPilot && !cfg.doctorMode && !cfg.gpuMode && !cfg.netMode && !cfg.benchmarkMode && !cfg.adminMode) {
        cout << col("\n>>> HARDWARE COMPARISON <<<\n", C_MAGENTA) << endl;
        hwAI.fullResearch();
        HardwareComparator hc;
        double cpuS = bench.cpuBench();
        double memB = bench.memBench();
        double diskB = bench.diskBench();
        cout << hc.fullReport(hwAI, cpuS, memB, diskB) << endl;
        closeDatabase();
        return 0;
    }

    // --html-report: generate HTML report only
    if (cfg.htmlReport && !cfg.interactive && !cfg.autoPilot && !cfg.doctorMode && !cfg.gpuMode && !cfg.netMode && !cfg.benchmarkMode) {
        cout << col("\n>>> HTML REPORT GENERATION <<<\n", C_MAGENTA) << endl;
        hwAI.fullResearch();
        ensureReportDir();
        string html = generateHTMLReport(hwAI, "", 0, 0, 0, 0, 50, "");
        saveHTMLReport(html);
        closeDatabase();
        return 0;
    }

    // --benchmark: run benchmarks then continue
    if (cfg.benchmarkMode) bench.runBefore();

    time_t startTime = time(nullptr);
    int cycle = 1;
    time_t lastSmallReport = 0;
    time_t lastExtensiveReport = 0;

    cout << col("Starting optimization loop (Ctrl+C to stop)...\n", C_BOLD) << endl;

    // Initial hardware research
    hwAI.fullResearch();
    hwAI.saveMarkdownReport();
    lastExtensiveReport = time(nullptr);

    // Start interactive console thread if requested
    if (cfg.interactive) console.start(hwAI, brain);

    while (running) {
        cout << col(">>> Cycle ", C_CYAN) << cycle << col(" @ ", C_CYAN) << ts() << endl;

        optimizeCPU();
        optimizeRAM();
        optimizeDisk();
        advisoryBIOS();
        optimizeSystem();

        if (cfg.gpuMode) { probeGPU(); optimizeGPU(); }
        if (cfg.netMode) optimizeNetwork();
        if (cfg.sysdMode || cfg.adminMode) optimizeSystemd();
        if (cfg.secMode || cfg.adminMode) optimizeSecurity();
        if (cfg.pkgMode || cfg.adminMode) updatePackages();
        if (cfg.auditMode || cfg.adminMode) auditSystem();
        if (cfg.bootMode || cfg.adminMode) optimizeBoot();
        if (cfg.cronMode || cfg.adminMode) manageCron();

        ai.doResearchCycle(cycle);

        if (cfg.doctorMode) doctor.printReport();
        printProgress(cycle, startTime);

        // IntelligenceCore: sample, detect, predict every cycle
        auto brainInsights = brain.getInsights();
        for (const auto& ins : brainInsights) {
            if (ins.find("ANOMALY") != string::npos) {
                cout << col("[AI ALERT] ", C_RED) << ins << endl;
            } else if (ins.find("TREND") != string::npos) {
                cout << col("[AI TREND] ", C_YELLOW) << ins << endl;
            } else if (ins.find("CORRELATION") != string::npos) {
                cout << col("[AI CORRELATION] ", C_MAGENTA) << ins << endl;
            }
        }

        // Sleep + heartbeat + auto-pilot + dashboard
        int elapsed = 0;
        while (elapsed < cfg.cycleInt && running) {
            this_thread::sleep_for(chrono::seconds(min(cfg.hbInt, 30)));
            elapsed += min(cfg.hbInt, 30);
            if (!running) break;

            heartbeat(cycle);

            if (cfg.autoPilot) {
                time_t now = time(nullptr);
                if (now - lastSmallReport >= 120) {
                    hwAI.printSmallReport();
                    lastSmallReport = now;
                }
                if (now - lastExtensiveReport >= 420) {
                    hwAI.fullResearch();
                    hwAI.printExtensiveReport();
                    hwAI.saveMarkdownReport();
                    lastExtensiveReport = now;
                    lastSmallReport = now;
                }
            }
        }
        cycle++;
    }

    // --benchmark: run after + self-learn
    if (cfg.benchmarkMode) {
        bench.runAfter();
        bench.printComparison();
        cout << col("\n[AI] IntelligenceCore learning from benchmark results...\n", C_CYAN);
        brain.learnFromBenchmark(bench.beforeScore, bench.afterScore, "overall");
        cout << brain.suggestOptimizations() << endl;
    }

    // --html-report: save final diagnostic
    if (cfg.htmlReport) {
        cout << col("\n[REPORT] Generating HTML diagnostic report...\n", C_CYAN);
        string html = generateHTMLReport(hwAI, "", 0, 0, 0, cycle, 50, "");
        saveHTMLReport(html);
    }

    // --compare: show hardware comparison
    if (cfg.compareMode) {
        HardwareComparator hc;
        cout << hc.fullReport(hwAI, bench.beforeScore, bench.memBench(), bench.diskBench()) << endl;
    }

    console.stop();
    closeDatabase();
    cout << col("\n[OK] Shutdown complete.", C_GREEN) << endl;
    return 0;
}
