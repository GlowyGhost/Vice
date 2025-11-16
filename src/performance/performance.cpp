#include <cstdint>
#include <string>
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <psapi.h>

static FILETIME prevIdleTime = {0}, prevKernelTime = {0}, prevUserTime = {0}, prevProcKernel = {0}, prevProcUser = {0}, prevSysKernel = {0}, prevSysUser = {0};

extern "C" {
    #pragma region Get Sys CPU
    float get_sys_cpu() {
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
    }
    #pragma endregion
    #pragma region Get App CPU
    float get_app_cpu() {
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
    }
    #pragma endregion
    #pragma region Get Sys RAM
    uint64_t get_sys_ram() {
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        GlobalMemoryStatusEx(&status);

        uint64_t total = status.ullTotalPhys;
        uint64_t avail = status.ullAvailPhys;
        return (total - avail) / (1024 * 1024);
    }
    #pragma endregion
    #pragma region Get App RAM
    uint64_t get_app_ram() {
        uint64_t totalRam = 0;
        DWORD myPid = GetCurrentProcessId();

        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
            totalRam += pmc.WorkingSetSize;
        }

        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE) return totalRam / (1024 * 1024);

        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(hSnap, &pe)) {
            do {
                if (pe.th32ParentProcessID == myPid) {
                    std::string procName = pe.szExeFile;
                    if (procName.find("WebView2") != std::string::npos || procName.find("msedgewebview2.exe") != std::string::npos) {
                        HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe.th32ProcessID);
                        if (hProc) {
                            PROCESS_MEMORY_COUNTERS_EX childPmc;
                            if (GetProcessMemoryInfo(hProc, (PROCESS_MEMORY_COUNTERS*)&childPmc, sizeof(childPmc))) {
                                totalRam += childPmc.WorkingSetSize;
                            }
                            CloseHandle(hProc);
                        }
                    }
                }
            } while (Process32Next(hSnap, &pe));
        }

        CloseHandle(hSnap);
        return totalRam / (1024 * 1024);
    }
    #pragma endregion
    #pragma region Get Total RAM
    uint64_t get_total_ram() {
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        GlobalMemoryStatusEx(&status);
        return status.ullTotalPhys / (1024 * 1024);
    }
    #pragma endregion
}