#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

struct SystemSnapshot {
    std::string hostname;
    std::string os;
    int cpu_cores = 1;
    double load_average = 0.0;
    long long total_memory_kb = 0;
    long long free_memory_kb = 0;
    long long used_memory_kb = 0;
    long long disk_total_kb = 0;
    long long disk_free_kb = 0;
    long long disk_used_kb = 0;
    long long network_rx_bytes = 0;
    long long network_tx_bytes = 0;
    long long process_count = 0;
    long long uptime_seconds = 0;
};

namespace {

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string escape_json(const std::string& value) {
    std::ostringstream out;
    for (unsigned char ch : value) {
        switch (ch) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (ch < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(ch) << std::dec;
                } else {
                    out << static_cast<char>(ch);
                }
                break;
        }
    }
    return out.str();
}

long long parse_kb_value(const std::string& line) {
    const std::string cleaned = trim(line);
    if (cleaned.empty()) {
        return 0;
    }
    const std::size_t colon = cleaned.find(':');
    if (colon == std::string::npos) {
        return 0;
    }
    std::istringstream iss(trim(cleaned.substr(colon + 1)));
    long long value = 0;
    iss >> value;
    return value;
}

std::string get_hostname() {
    char buffer[256] = {};
#ifdef _WIN32
    DWORD len = sizeof(buffer);
    if (GetComputerNameA(buffer, &len)) {
        return std::string(buffer);
    }
#else
    if (gethostname(buffer, sizeof(buffer)) == 0) {
        return std::string(buffer);
    }
#endif
    return "localhost";
}

int get_cpu_cores() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    return static_cast<int>(info.dwNumberOfProcessors);
#else
    std::ifstream cpuinfo("/proc/cpuinfo");
    int count = 0;
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.rfind("processor", 0) == 0) {
            ++count;
        }
    }
    return count > 0 ? count : 1;
#endif
}

double get_load_average() {
#ifdef _WIN32
    return 0.0;
#else
    std::ifstream loadfile("/proc/loadavg");
    std::string line;
    if (std::getline(loadfile, line)) {
        std::istringstream iss(line);
        double one, five, fifteen;
        if (iss >> one >> five >> fifteen) {
            return one;
        }
    }
    return 0.0;
#endif
}

long long get_process_count() {
#ifdef _WIN32
    return 0;
#else
    DIR* dir = opendir("/proc");
    if (!dir) {
        return 0;
    }

    long long count = 0;
    dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] >= '0' && entry->d_name[0] <= '9') {
            ++count;
        }
    }
    closedir(dir);
    return count;
#endif
}

long long get_uptime_seconds() {
#ifdef _WIN32
    return static_cast<long long>(GetTickCount64() / 1000ULL);
#else
    std::ifstream uptime_file("/proc/uptime");
    double seconds = 0.0;
    if (uptime_file >> seconds) {
        return static_cast<long long>(seconds);
    }
    return 0;
#endif
}

long long get_network_rx_bytes() {
#ifdef _WIN32
    return 0;
#else
    std::ifstream ifs("/proc/net/dev");
    std::string line;
    long long total = 0;
    bool first_line = true;
    while (std::getline(ifs, line)) {
        if (first_line) {
            first_line = false;
            continue;
        }
        std::istringstream iss(line);
        std::string iface;
        if (!(iss >> iface)) {
            continue;
        }
        if (iface == "lo:") {
            continue;
        }
        long long rx = 0;
        long long tx = 0;
        std::string token;
        for (int i = 0; i < 16; ++i) {
            if (!(iss >> token)) {
                break;
            }
            if (i == 0) {
                rx = std::stoll(token);
            } else if (i == 8) {
                tx = std::stoll(token);
            }
        }
        total += rx;
    }
    return total;
#endif
}

long long get_network_tx_bytes() {
#ifdef _WIN32
    return 0;
#else
    std::ifstream ifs("/proc/net/dev");
    std::string line;
    long long total = 0;
    bool first_line = true;
    while (std::getline(ifs, line)) {
        if (first_line) {
            first_line = false;
            continue;
        }
        std::istringstream iss(line);
        std::string iface;
        if (!(iss >> iface)) {
            continue;
        }
        if (iface == "lo:") {
            continue;
        }
        long long rx = 0;
        long long tx = 0;
        std::string token;
        for (int i = 0; i < 16; ++i) {
            if (!(iss >> token)) {
                break;
            }
            if (i == 0) {
                rx = std::stoll(token);
            } else if (i == 8) {
                tx = std::stoll(token);
            }
        }
        total += tx;
    }
    return total;
#endif
}

SystemSnapshot collect_linux_snapshot() {
    SystemSnapshot snapshot;
    snapshot.hostname = get_hostname();
    snapshot.os = "linux";
    snapshot.cpu_cores = get_cpu_cores();
    snapshot.load_average = get_load_average();
    snapshot.uptime_seconds = get_uptime_seconds();
    snapshot.process_count = get_process_count();
    snapshot.network_rx_bytes = get_network_rx_bytes();
    snapshot.network_tx_bytes = get_network_tx_bytes();

    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    long long total_kb = 0;
    long long available_kb = 0;
    long long free_kb = 0;
    while (std::getline(meminfo, line)) {
        if (line.rfind("MemTotal:", 0) == 0) {
            total_kb = parse_kb_value(line);
        } else if (line.rfind("MemAvailable:", 0) == 0) {
            available_kb = parse_kb_value(line);
        } else if (line.rfind("MemFree:", 0) == 0) {
            free_kb = parse_kb_value(line);
        }
    }
    snapshot.total_memory_kb = total_kb;
    snapshot.free_memory_kb = available_kb > 0 ? available_kb : free_kb;
    snapshot.used_memory_kb = total_kb - snapshot.free_memory_kb;

    std::error_code ec;
    const auto root = std::filesystem::space("/", ec);
    if (!ec) {
        snapshot.disk_total_kb = static_cast<long long>(root.capacity / 1024ULL);
        snapshot.disk_free_kb = static_cast<long long>(root.free / 1024ULL);
        snapshot.disk_used_kb = snapshot.disk_total_kb - snapshot.disk_free_kb;
    }

    return snapshot;
}

SystemSnapshot collect_windows_snapshot() {
    SystemSnapshot snapshot;
    snapshot.hostname = get_hostname();
    snapshot.os = "windows";
    snapshot.cpu_cores = get_cpu_cores();
    snapshot.uptime_seconds = static_cast<long long>(GetTickCount64() / 1000ULL);

    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        snapshot.total_memory_kb = static_cast<long long>(mem.ullTotalPhys / 1024ULL);
        snapshot.free_memory_kb = static_cast<long long>(mem.ullAvailPhys / 1024ULL);
        snapshot.used_memory_kb = snapshot.total_memory_kb - snapshot.free_memory_kb;
    }

    ULARGE_INTEGER free_bytes{};
    ULARGE_INTEGER total_bytes{};
    ULARGE_INTEGER total_free_bytes{};
    if (GetDiskFreeSpaceExA("C:\\", &free_bytes, &total_bytes, &total_free_bytes)) {
        snapshot.disk_total_kb = static_cast<long long>(total_bytes.QuadPart / 1024ULL);
        snapshot.disk_free_kb = static_cast<long long>(free_bytes.QuadPart / 1024ULL);
        snapshot.disk_used_kb = snapshot.disk_total_kb - snapshot.disk_free_kb;
    }

    return snapshot;
}

SystemSnapshot collect_snapshot() {
#ifdef _WIN32
    return collect_windows_snapshot();
#else
    return collect_linux_snapshot();
#endif
}

std::string to_json(const SystemSnapshot& snapshot) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"hostname\": \"" << escape_json(snapshot.hostname) << "\",\n";
    out << "  \"os\": \"" << escape_json(snapshot.os) << "\",\n";
    out << "  \"cpu\": {\n";
    out << "    \"cores\": " << snapshot.cpu_cores << ",\n";
    out << "    \"load_average\": " << std::fixed << std::setprecision(2) << snapshot.load_average << "\n";
    out << "  },\n";
    out << "  \"memory\": {\n";
    out << "    \"total_kb\": " << snapshot.total_memory_kb << ",\n";
    out << "    \"free_kb\": " << snapshot.free_memory_kb << ",\n";
    out << "    \"used_kb\": " << snapshot.used_memory_kb << "\n";
    out << "  },\n";
    out << "  \"disk\": {\n";
    out << "    \"total_kb\": " << snapshot.disk_total_kb << ",\n";
    out << "    \"free_kb\": " << snapshot.disk_free_kb << ",\n";
    out << "    \"used_kb\": " << snapshot.disk_used_kb << "\n";
    out << "  },\n";
    out << "  \"network\": {\n";
    out << "    \"rx_bytes\": " << snapshot.network_rx_bytes << ",\n";
    out << "    \"tx_bytes\": " << snapshot.network_tx_bytes << "\n";
    out << "  },\n";
    out << "  \"process_count\": " << snapshot.process_count << ",\n";
    out << "  \"uptime_seconds\": " << snapshot.uptime_seconds << "\n";
    out << "}\n";
    return out.str();
}

}  // namespace

int main() {
    const SystemSnapshot snapshot = collect_snapshot();
    std::cout << to_json(snapshot);
    return 0;
}
