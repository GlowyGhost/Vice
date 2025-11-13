#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <fstream>
#include <thread>
#include <chrono>
#include <string>
#include <unistd.h>
#endif

#ifdef _WIN32
static FILETIME prevIdleTime = {0}, prevKernelTime = {0}, prevUserTime = {0}, prevProcKernel = {0}, prevProcUser = {0}, prevSysKernel = {0}, prevSysUser = {0};
#endif

#ifdef __linux__
struct CPUData {
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
};

CPUData ReadCPUData() {
    std::ifstream file("/proc/stat");
    std::string cpu;
    CPUData data{};
    file >> cpu >> data.user >> data.nice >> data.system >> data.idle
        >> data.iowait >> data.irq >> data.softirq >> data.steal;
    return data;
}

unsigned long long ReadProcessCPU() {
    std::ifstream file("/proc/self/stat");
    std::string dummy;
    unsigned long long utime, stime;
    for (int i = 0; i < 13; ++i) file >> dummy;
    file >> utime >> stime;
    return utime + stime;
}

uint64_t ParseLineValueKB(const std::string& line) {
    std::istringstream iss(line);
    std::string key, unit;
    uint64_t value;
    iss >> key >> value >> unit;
    return value;
}
#endif

extern "C" {
    #pragma region Get Sys CPU
    float get_sys_cpu() {
        #ifdef _WIN32
        FILETIME idleTime, kernelTime, userTime;
        if (!GetSystemTimes(&idleTime, &kernelTime, &userTime))
            return -1.0f;

        auto FileTimeToUInt64 = [](const FILETIME &ft) -> unsigned long long {
            return (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        };

        unsigned long long idleDiff = FileTimeToUInt64(idleTime) - FileTimeToUInt64(prevIdleTime);
        unsigned long long kernelDiff = FileTimeToUInt64(kernelTime) - FileTimeToUInt64(prevKernelTime);
        unsigned long long userDiff = FileTimeToUInt64(userTime) - FileTimeToUInt64(prevUserTime);

        unsigned long long total = kernelDiff + userDiff;
        float cpu = total ? static_cast<float>(total - idleDiff) * 100.0f / static_cast<float>(total) : 0.0f;

        prevIdleTime = idleTime;
        prevKernelTime = kernelTime;
        prevUserTime = userTime;

        return cpu;
        #else
        CPUData t1 = ReadCPUData();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        CPUData t2 = ReadCPUData();

        unsigned long long idle1 = t1.idle + t1.iowait;
        unsigned long long idle2 = t2.idle + t2.iowait;

        unsigned long long nonIdle1 = t1.user + t1.nice + t1.system + t1.irq + t1.softirq + t1.steal;
        unsigned long long nonIdle2 = t2.user + t2.nice + t2.system + t2.irq + t2.softirq + t2.steal;

        unsigned long long total1 = idle1 + nonIdle1;
        unsigned long long total2 = idle2 + nonIdle2;

        float totalDiff = static_cast<float>(total2 - total1);
        float idleDiff = static_cast<float>(idle2 - idle1);

        return (totalDiff > 0.0f) ? ((totalDiff - idleDiff) / totalDiff) * 100.0f : 0.0f;
        #endif
    }
    #pragma endregion
    #pragma region Get App CPU
    float get_app_cpu() {
        #ifdef _WIN32
        FILETIME sysIdle, sysKernel, sysUser;
        FILETIME procCreation, procExit, procKernel, procUser;

        if (!GetSystemTimes(&sysIdle, &sysKernel, &sysUser))
            return -1.0f;

        if (!GetProcessTimes(GetCurrentProcess(), &procCreation, &procExit, &procKernel, &procUser))
            return -1.0f;

        auto FileTimeToUInt64 = [](const FILETIME& ft) -> unsigned long long {
            return (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        };

        unsigned long long sysKernelDiff = FileTimeToUInt64(sysKernel) - FileTimeToUInt64(prevSysKernel);
        unsigned long long sysUserDiff = FileTimeToUInt64(sysUser) - FileTimeToUInt64(prevSysUser);
        unsigned long long sysTotal = sysKernelDiff + sysUserDiff;

        unsigned long long procKernelDiff = FileTimeToUInt64(procKernel) - FileTimeToUInt64(prevProcKernel);
        unsigned long long procUserDiff = FileTimeToUInt64(procUser) - FileTimeToUInt64(prevProcUser);
        unsigned long long procTotal = procKernelDiff + procUserDiff;

        prevSysKernel = sysKernel;
        prevSysUser = sysUser;
        prevProcKernel = procKernel;
        prevProcUser = procUser;

        if (sysTotal == 0) return 0.0f;

        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        unsigned int numCPUs = sysInfo.dwNumberOfProcessors;

        float cpuUsage = (static_cast<float>(procTotal) / static_cast<float>(sysTotal)) * 100.0f * numCPUs;
        return cpuUsage;
        #else
        CPUData sys1 = ReadSystemCPU();
        unsigned long long proc1 = ReadProcessCPU();

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        CPUData sys2 = ReadSystemCPU();
        unsigned long long proc2 = ReadProcessCPU();

        unsigned long long sysIdle1 = sys1.idle + sys1.iowait;
        unsigned long long sysIdle2 = sys2.idle + sys2.iowait;

        unsigned long long sysNonIdle1 = sys1.user + sys1.nice + sys1.system + sys1.irq + sys1.softirq + sys1.steal;
        unsigned long long sysNonIdle2 = sys2.user + sys2.nice + sys2.system + sys2.irq + sys2.softirq + sys2.steal;

        unsigned long long sysTotal1 = sysIdle1 + sysNonIdle1;
        unsigned long long sysTotal2 = sysIdle2 + sysNonIdle2;

        unsigned long long sysTotalDiff = sysTotal2 - sysTotal1;
        unsigned long long procDiff = proc2 - proc1;

        long numCPUs = sysconf(_SC_NPROCESSORS_ONLN);
        return (sysTotalDiff > 0) ? (procDiff * 100.0f / sysTotalDiff) * numCPUs : 0.0f;
        #endif
    }
    #pragma endregion
    #pragma region Get Sys RAM
    uint64_t get_sys_ram() {
        #ifdef _WIN32
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        GlobalMemoryStatusEx(&status);

        uint64_t total = status.ullTotalPhys;
        uint64_t avail = status.ullAvailPhys;
        return (total - avail) / (1024 * 1024);
        #else
        std::ifstream meminfo("/proc/meminfo");
        std::string line;
        uint64_t totalKB = 0, availKB = 0;
        while (std::getline(meminfo, line)) {
            if (line.rfind("MemTotal:", 0) == 0)
                totalKB = ParseLineValueKB(line);
            else if (line.rfind("MemAvailable:", 0) == 0)
                availKB = ParseLineValueKB(line);
            if (totalKB && availKB)
                break;
        }
        return (totalKB - availKB) / 1024;
        #endif
    }
    #pragma endregion
    #pragma region Get App RAM
    uint64_t get_app_ram() {
        #ifdef _WIN32
        PROCESS_MEMORY_COUNTERS_EX pmc;
        GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
        return pmc.WorkingSetSize / (1024 * 1024);
        #else
        std::ifstream status("/proc/self/status");
        std::string line;
        while (std::getline(status, line)) {
            if (line.rfind("VmRSS:", 0) == 0)
                return ParseLineValueKB(line) / 1024;
        }
        return 0;
        #endif
    }
    #pragma endregion
    #pragma region Get Total RAM
    uint64_t get_total_ram() {
        #ifdef _WIN32
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        GlobalMemoryStatusEx(&status);
        return status.ullTotalPhys / (1024 * 1024);
        #else
        std::ifstream meminfo("/proc/meminfo");
        std::string line;
        while (std::getline(meminfo, line)) {
            if (line.rfind("MemTotal:", 0) == 0)
                return ParseLineValueKB(line) / 1024;
        }
        return 0;
        #endif
    }
    #pragma endregion
}