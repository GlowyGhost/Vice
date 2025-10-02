#include <vector>
#include <string>
#include <thread>
#include <fstream>
#include <unordered_set>
#include <iostream>
#include <cctype>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <propvarutil.h>
#include <tlhelp32.h>
#include <wtsapi32.h>
#else
#include <alsa/asoundlib.h>
#include <dirent.h>
#include <unistd.h>
#include <fstream>
#include <pulse/pulseaudio.h>
#endif

static std::vector<std::string> storage;
static std::vector<const char*> c_strs;
static std::vector<std::unique_ptr<char[]>> c_copies;

std::string wideToUtf8(const wchar_t* wstr) {
    if (!wstr) return {};

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) return {};

    std::string str(size_needed - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str[0], size_needed, nullptr, nullptr);
    return str;
}

struct WAVData {
    std::vector<char> buffer;
    int sampleRate;
    int channels;
};

WAVData loadWav(const char* filename) {
    std::ifstream f(filename, std::ios::binary);
    WAVData data;
    if (!f) return data;

    char riff[4];
    f.read(riff, 4);
    if (std::strncmp(riff, "RIFF", 4) != 0) return data;

    f.seekg(4, std::ios::cur);

    char wave[4];
    f.read(wave, 4);
    if (std::strncmp(wave, "WAVE", 4) != 0) return data;

    while (f) {
        char chunkId[4];
        uint32_t chunkSize = 0;
        f.read(chunkId, 4);
        f.read(reinterpret_cast<char*>(&chunkSize), 4);
        if (!f) break;

        if (std::strncmp(chunkId, "fmt ", 4) == 0) {
            uint16_t audioFormat;
            uint16_t channels;
            uint32_t sampleRate;
            uint32_t byteRate;
            uint16_t blockAlign;
            uint16_t bitsPerSample;

            f.read(reinterpret_cast<char*>(&audioFormat), 2);
            f.read(reinterpret_cast<char*>(&channels), 2);
            f.read(reinterpret_cast<char*>(&sampleRate), 4);
            f.read(reinterpret_cast<char*>(&byteRate), 4);
            f.read(reinterpret_cast<char*>(&blockAlign), 2);
            f.read(reinterpret_cast<char*>(&bitsPerSample), 2);

            if (chunkSize > 16)
                f.seekg(chunkSize - 16, std::ios::cur);

            data.channels = channels;
            data.sampleRate = sampleRate;
        } else if (std::strncmp(chunkId, "data", 4) == 0) {
            data.buffer.resize(chunkSize);
            f.read(data.buffer.data(), chunkSize);
            break; 
        } else
            f.seekg(chunkSize, std::ios::cur);
    }

    return data;
}

#ifdef _WIN32
static bool is_format_float(const WAVEFORMATEX* wf) {
    if (!wf) return false;
    if (wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) return true;
    if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const WAVEFORMATEXTENSIBLE* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(wf);
        return (IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) != 0);
    }
    return false;
}
#endif

static std::vector<float> linear_resample_interleaved(const float* in, size_t inFrames, int inCh, unsigned inRate, unsigned outRate) {
    if (inRate == outRate) {
        return std::vector<float>(in, in + inFrames * inCh);
    }
    double ratio = (double)outRate / (double)inRate;
    size_t outFrames = (size_t)std::ceil(inFrames * ratio);
    std::vector<float> out(outFrames * inCh);

    for (size_t f = 0; f < outFrames; ++f) {
        double srcPos = f / ratio;
        size_t idx = (size_t)std::floor(srcPos);
        double frac = srcPos - idx;

        for (int ch = 0; ch < inCh; ++ch) {
            float s0 = 0.0f, s1 = 0.0f;
            if (idx < inFrames) s0 = in[(idx * inCh) + ch];
            if (idx + 1 < inFrames) s1 = in[((idx + 1) * inCh) + ch];
            out[f * inCh + ch] = static_cast<float>(s0 + (s1 - s0) * frac);
        }
    }
    return out;
}

static std::vector<float> remap_channels_interleaved(const float* in, size_t frames, int inCh, int outCh) {
    if (inCh == outCh)
        return std::vector<float>(in, in + frames * inCh);
    std::vector<float> out(frames * outCh);
    for (size_t f = 0; f < frames; ++f) {
        for (int c = 0; c < outCh; ++c) {
            int src = (inCh == 1) ? 0 : (c % inCh);
            out[f * outCh + c] = in[f * inCh + src];
        }
    }
    return out;
}

#ifdef _WIN32
IMMDevice* find_device_by_name(EDataFlow flow, const char* name) {
    IMMDeviceEnumerator* pEnum = nullptr;
    IMMDeviceCollection* pDevices = nullptr;
    IMMDevice* result = nullptr;

    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pEnum))))
        return nullptr;

    if (FAILED(pEnum->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &pDevices))) {
        pEnum->Release();
        return nullptr;
    }

    UINT count = 0;
    pDevices->GetCount(&count);
    for (UINT i = 0; i < count; ++i) {
        IMMDevice* dev = nullptr;
        if (SUCCEEDED(pDevices->Item(i, &dev))) {
            IPropertyStore* props = nullptr;
            if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props))) {
                PROPVARIANT varName;
                PropVariantInit(&varName);
                if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName))) {
                    if (name && strlen(name) > 0) {
                        std::string friendly = wideToUtf8(varName.pwszVal);
                        if (friendly == std::string(name)) {
                            result = dev;
                            PropVariantClear(&varName);
                            props->Release();
                            break;
                        }
                    }
                    PropVariantClear(&varName);
                }
                props->Release();
            }
            dev->Release();
        }
    }

    pDevices->Release();
    pEnum->Release();
    return result;
};
#endif

bool isValidName(const std::string& name) {
    if (name.empty()) return false;

    if (!std::any_of(name.begin(), name.end(), [](unsigned char c){
        return std::isalnum(c);
    })) return false;

    static const std::vector<std::string> blacklist = {
        "explorer", "TextInputHost", "ApplicationFrameHost", " "
    };
    for (auto& bad : blacklist) {
        if (name == bad) return false;
    }

    return true;
}

void clear_statics() {
    std::thread([]() mutable {
        Sleep(1);
        storage.clear();
        c_strs.clear();
        c_copies.clear();
    }).detach();
}

extern "C" {
    std::atomic<bool> stop_audio(false);

    const char** get_outputs(size_t* len) {
        *len = 0;

        #ifdef _WIN32
        CoInitialize(nullptr);

        IMMDeviceEnumerator* pEnum = nullptr;
        IMMDeviceCollection* pDevices = nullptr;

        if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pEnum))))
            return c_strs.data();

        if (FAILED(pEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDevices))) {
            pEnum->Release();
            return c_strs.data();
        }

        UINT count; pDevices->GetCount(&count);

        for (UINT i = 0; i < count; i++) {
            IMMDevice* pDevice = nullptr;
            if (SUCCEEDED(pDevices->Item(i, &pDevice))) {
                IPropertyStore* pProps = nullptr;
                if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps))) {
                    PROPVARIANT varName;
                    PropVariantInit(&varName);
                    if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName))) {
                        storage.emplace_back(wideToUtf8(varName.pwszVal));
                        c_strs.push_back(storage.back().c_str());
                        PropVariantClear(&varName);
                    }
                    pProps->Release();
                }
                pDevice->Release();
            }
        }

        pDevices->Release();
        pEnum->Release();
        CoUninitialize();
        #else
        void **hints;
        if (snd_device_name_hint(-1, "pcm", &hints) < 0) return c_strs.data();

        for (void **n = hints; *n != nullptr; n++) {
            char *name = snd_device_name_get_hint(*n, "NAME");
            char *ioid = snd_device_name_get_hint(*n, "IOID");

            if (!ioid || strcmp(ioid, "Output") == 0) {
                storage.emplace_back(name);
                c_strs.push_back(storage.back().c_str());
            }

            if (name) free(name);
            if (ioid) free(ioid);
        }
        snd_device_name_free_hint(hints);
        #endif

        clear_statics();
        *len = c_strs.size();
        return c_strs.data();
    }

    const char** get_inputs(size_t* len) {
        *len = 0;

        #ifdef _WIN32
        CoInitialize(nullptr);

        IMMDeviceEnumerator* pEnum = nullptr;
        IMMDeviceCollection* pDevices = nullptr;

        if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pEnum))))
            return c_strs.data();

        if (FAILED(pEnum->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pDevices))) {
            pEnum->Release();
            return c_strs.data();
        }

        UINT count; pDevices->GetCount(&count);

        for (UINT i = 0; i < count; i++) {
            IMMDevice* pDevice = nullptr;
            if (SUCCEEDED(pDevices->Item(i, &pDevice))) {
                IPropertyStore* pProps = nullptr;
                if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps))) {
                    PROPVARIANT varName;
                    PropVariantInit(&varName);
                    if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName))) {
                        storage.emplace_back(wideToUtf8(varName.pwszVal));
                        c_strs.push_back(storage.back().c_str());
                        PropVariantClear(&varName);
                    }
                    pProps->Release();
                }
                pDevice->Release();
            }
        }

        pDevices->Release();
        pEnum->Release();
        CoUninitialize();
        #else
        void **hints;
        if (snd_device_name_hint(-1, "pcm", &hints) < 0) return c_strs.data();

        for (void **n = hints; *n != nullptr; n++) {
            char *name = snd_device_name_get_hint(*n, "NAME");
            char *ioid = snd_device_name_get_hint(*n, "IOID");

            if (!ioid || strcmp(ioid, "Input") == 0) {
                storage.emplace_back(name);
                c_strs.push_back(storage.back().c_str());
            }

            if (name) free(name);
            if (ioid) free(ioid);
        }
        snd_device_name_free_hint(hints);
        #endif

        clear_statics();
        *len = c_strs.size();
        return c_strs.data();
    }

    const char** get_apps(size_t* len) {
        *len = 0;

        #ifdef _WIN32
        std::unordered_set<DWORD> seenPIDs;

        CoInitialize(nullptr);

        IMMDeviceEnumerator* pEnum = nullptr;
        IMMDevice* pDevice = nullptr;
        IAudioSessionManager2* pSessionManager = nullptr;
        IAudioSessionEnumerator* pSessionEnum = nullptr;

        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pEnum))) &&
            SUCCEEDED(pEnum->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice)) &&
            SUCCEEDED(pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&pSessionManager)) &&
            SUCCEEDED(pSessionManager->GetSessionEnumerator(&pSessionEnum)))
        {
            int sessionCount = 0;
            pSessionEnum->GetCount(&sessionCount);

            for (int i = 0; i < sessionCount; i++) {
                IAudioSessionControl* pControl = nullptr;
                if (FAILED(pSessionEnum->GetSession(i, &pControl))) continue;

                IAudioSessionControl2* pControl2 = nullptr;
                if (SUCCEEDED(pControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pControl2))) {
                    DWORD pid = 0;
                    if (SUCCEEDED(pControl2->GetProcessId(&pid)) && pid > 0) {
                        seenPIDs.insert(pid);

                        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                        if (hProcess) {
                            wchar_t exePath[MAX_PATH] = {};
                            DWORD size = MAX_PATH;
                            if (QueryFullProcessImageNameW(hProcess, 0, exePath, &size)) {
                                std::wstring ws(exePath);
                                size_t slash = ws.find_last_of(L"\\/");
                                std::wstring justName = (slash != std::wstring::npos) ? ws.substr(slash + 1) : ws;

                                std::string name = wideToUtf8(justName.c_str());
                                if (name.size() > 4 && name.substr(name.size() - 4) == ".exe")
                                    name = name.substr(0, name.size() - 4);

                                if (isValidName(name)) {
                                    storage.emplace_back(name);
                                    auto copy = std::make_unique<char[]>(name.size() + 1);
                                    strcpy(copy.get(), name.c_str());
                                    c_strs.push_back(copy.get());
                                    c_copies.push_back(std::move(copy));
                                }
                            }
                            CloseHandle(hProcess);
                        }
                    }
                    pControl2->Release();
                }
                pControl->Release();
            }

            pSessionEnum->Release();
            pSessionManager->Release();
            pDevice->Release();
            pEnum->Release();
        }

        CoUninitialize();

        std::vector<DWORD> pids;
        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            if (IsWindowVisible(hwnd)) {
                DWORD pid = 0;
                GetWindowThreadProcessId(hwnd, &pid);
                auto* vec = reinterpret_cast<std::vector<DWORD>*>(lParam);
                if (std::find(vec->begin(), vec->end(), pid) == vec->end())
                    vec->push_back(pid);
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&pids));

        for (DWORD pid : pids) {
            if (seenPIDs.find(pid) != seenPIDs.end()) continue;

            DWORD sessionId = 0;
            if (!ProcessIdToSessionId(pid, &sessionId) || sessionId != WTSGetActiveConsoleSessionId())
                continue;

            HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (!hProcess) continue;

            wchar_t exePath[MAX_PATH] = {};
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(hProcess, 0, exePath, &size)) {
                std::wstring ws(exePath);
                size_t slash = ws.find_last_of(L"\\/");
                std::wstring justName = (slash != std::wstring::npos) ? ws.substr(slash + 1) : ws;

                std::string name = wideToUtf8(justName.c_str());
                if (name.size() > 4 && name.substr(name.size() - 4) == ".exe")
                    name = name.substr(0, name.size() - 4);

                if (isValidName(name)) {
                    storage.emplace_back(name);
                    auto copy = std::make_unique<char[]>(name.size() + 1);
                    strcpy(copy.get(), name.c_str());
                    c_strs.push_back(copy.get());
                    c_copies.push_back(std::move(copy));
               }
            }
            CloseHandle(hProcess);
        }

        #else
        std::unordered_set<std::string> added;

        pa_mainloop* mainloop = pa_mainloop_new();
        pa_mainloop_api* mainloop_api = pa_mainloop_get_api(mainloop);
        pa_context* context = pa_context_new(mainloop_api, "GetAudioApps");

        bool ready = false;
        pa_context_set_state_callback(context, [](pa_context* ctx, void* userdata){
            auto* r = reinterpret_cast<bool*>(userdata);
            switch (pa_context_get_state(ctx)) {
                case PA_CONTEXT_READY: *r = true; break;
                case PA_CONTEXT_FAILED:
                case PA_CONTEXT_TERMINATED: *r = true; break;
                default: break;
            }
        }, &ready);

        pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr);
        while (!ready) pa_mainloop_iterate(mainloop, 1, nullptr);

        if (pa_context_get_state(context) == PA_CONTEXT_READY) {
            pa_operation* op = pa_context_get_sink_input_info_list(
                context,
                [](pa_context*, const pa_sink_input_info* info, int eol, void* userdata) {
                    if (eol) return;
                    auto* added = reinterpret_cast<std::unordered_set<std::string>*>(userdata);

                    if (info->proplist) {
                        const char* proc_name = pa_proplist_gets(info->proplist, PA_PROP_APPLICATION_PROCESS_BINARY);
                        if (!proc_name) proc_name = pa_proplist_gets(info->proplist, PA_PROP_APPLICATION_NAME);
                        if (proc_name) {
                            std::string name(proc_name);
                            if (isValidName(name) && added->insert(name).second) {
                                storage.emplace_back(name);
                                c_strs.push_back(storage.back().c_str());
                            }
                        }
                    }
                }, &added);

            if (op) {
                while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
                    pa_mainloop_iterate(mainloop, 1, nullptr);
                pa_operation_unref(op);
            }
        }

        pa_context_disconnect(context);
        pa_context_unref(context);
        pa_mainloop_free(mainloop);

        uid_t myUid = getuid();
        DIR* proc = opendir("/proc");
        if (proc) {
            struct dirent* entry;
            while ((entry = readdir(proc)) != nullptr) {
                if (!isdigit(entry->d_name[0])) continue;

                std::string pidDir = std::string("/proc/") + entry->d_name;
                std::ifstream status(pidDir + "/status");
                if (!status.is_open()) continue;

                std::string line;
                uid_t uid = -1;
                while (std::getline(status, line)) {
                    if (line.rfind("Uid:", 0) == 0) {
                        sscanf(line.c_str(), "Uid:\t%u", &uid);
                        break;
                    }
                }
                if (uid != myUid) continue;

                std::ifstream comm(pidDir + "/comm");
                if (!comm.is_open()) continue;

                std::string name;
                std::getline(comm, name);
                if (isValidName(name) && added.insert(name).second) {
                    storage.emplace_back(name);
                    c_strs.push_back(storage.back().c_str());
                }
            }
            closedir(proc);
        }
        #endif

        clear_statics();
        *len = c_strs.size();
        return c_strs.data();
    }

    void play_sound(const char* wav_file, const char* device_name) {
        WAVData wav = loadWav(wav_file);
        if (wav.buffer.empty() || wav.channels <= 0) {
            std::cout << "Aborted sound effect playing: " << wav_file << "\n";
            return;
        }

        #ifdef _WIN32
        CoInitialize(NULL);

        IMMDeviceEnumerator* pEnumerator = nullptr;
        if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, IID_PPV_ARGS(&pEnumerator)))) {
            CoUninitialize();
            return;
        }

        IMMDeviceCollection* pDevices = nullptr;
        if (FAILED(pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pDevices))) {
            pEnumerator->Release();
            CoUninitialize();
            return;
        }

        UINT count = 0;
        pDevices->GetCount(&count);
        IMMDevice* targetDevice = nullptr;

        for (UINT i = 0; i < count; i++) {
            IMMDevice* pDevice = nullptr;
            if (SUCCEEDED(pDevices->Item(i, &pDevice))) {
                IPropertyStore* pProps = nullptr;
                if (SUCCEEDED(pDevice->OpenPropertyStore(STGM_READ, &pProps))) {
                    PROPVARIANT varName;
                    PropVariantInit(&varName);
                    if (SUCCEEDED(pProps->GetValue(PKEY_Device_FriendlyName, &varName))) {
                        if (device_name && std::string(wideToUtf8(varName.pwszVal)) == std::string(device_name)) {
                            targetDevice = pDevice;
                        }
                        PropVariantClear(&varName);
                    }
                    pProps->Release();
                }
                if (!targetDevice) pDevice->Release();
            }
        }

        if (!targetDevice) {
            pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &targetDevice);
        }

        pDevices->Release();
        pEnumerator->Release();

        if (!targetDevice) {
            std::cout << "No audio device found\n";
            CoUninitialize();
            return;
        }

        IAudioClient* audioClient = nullptr;
        if (FAILED(targetDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&audioClient))) {
            targetDevice->Release();
            CoUninitialize();
            return;
        }

        WAVEFORMATEX* pwfx = nullptr;
        HRESULT hr = audioClient->GetMixFormat(&pwfx);
        if (FAILED(hr) || !pwfx) {
            std::cout << "GetMixFormat failed\n";
            audioClient->Release();
            targetDevice->Release();
            CoUninitialize();
            return;
        }

        const int srcChannels = wav.channels;
        const unsigned srcRate = static_cast<unsigned>(wav.sampleRate);
        const size_t srcFrames = wav.buffer.size() / (srcChannels * sizeof(int16_t));
        std::vector<float> srcFloat;
        srcFloat.resize(srcFrames * srcChannels);
        const int16_t* src16 = reinterpret_cast<const int16_t*>(wav.buffer.data());
        for (size_t f = 0; f < srcFrames; ++f) {
            for (int c = 0; c < srcChannels; ++c) {
                int16_t s = src16[f * srcChannels + c];
                srcFloat[f * srcChannels + c] = s / 32768.0f;
            }
        }

        const unsigned dstRate = static_cast<unsigned>(pwfx->nSamplesPerSec);
        std::vector<float> resampled = linear_resample_interleaved(srcFloat.data(), srcFrames, srcChannels, srcRate, dstRate);
        const size_t dstFramesAfterResample = resampled.size() / srcChannels;

        const int dstChannels = pwfx->nChannels;
        std::vector<float> remapped = remap_channels_interleaved(resampled.data(), dstFramesAfterResample, srcChannels, dstChannels);
        const size_t finalFrames = remapped.size() / dstChannels;

        bool deviceIsFloat = is_format_float(pwfx);
        size_t bytesPerSample = (pwfx->wBitsPerSample / 8);
        size_t bytesPerFrame = dstChannels * bytesPerSample;
        std::vector<char> outBuffer;
        outBuffer.resize(finalFrames * bytesPerFrame);

        if (deviceIsFloat && pwfx->wBitsPerSample == 32) {
            float* outF = reinterpret_cast<float*>(outBuffer.data());
            for (size_t i = 0; i < finalFrames * dstChannels; ++i) outF[i] = remapped[i];
        } else if (!deviceIsFloat && pwfx->wBitsPerSample == 16) {
            int16_t* out16 = reinterpret_cast<int16_t*>(outBuffer.data());
            for (size_t i = 0; i < finalFrames * dstChannels; ++i) {
                float v = remapped[i];
                if (v > 1.0f) v = 1.0f;
                if (v < -1.0f) v = -1.0f;
                out16[i] = static_cast<int16_t>(v * 32767.0f);
            }
        } else {
            if (pwfx->wBitsPerSample == 32 && !deviceIsFloat) {
                int32_t* out32 = reinterpret_cast<int32_t*>(outBuffer.data());
                for (size_t i = 0; i < finalFrames * dstChannels; ++i) {
                    float v = remapped[i];
                    if (v > 1.0f) v = 1.0f;
                    if (v < -1.0f) v = -1.0f;
                    out32[i] = static_cast<int32_t>(v * 2147483647.0f);
                }
            } else {
                std::cerr << "Unsupported device sample format. Bits/sample = " << pwfx->wBitsPerSample << ", float? " << deviceIsFloat << "\n";
                CoTaskMemFree(pwfx);
                audioClient->Release();
                targetDevice->Release();
                CoUninitialize();
                return;
            }
        }

        hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 0, 0, pwfx, NULL);
        if (FAILED(hr)) {
            std::cout << "Initialize failed: 0x" << std::hex << hr << "\n";
            CoTaskMemFree(pwfx);
            audioClient->Release();
            targetDevice->Release();
            CoUninitialize();
            return;
        }

        CoTaskMemFree(pwfx);

        IAudioRenderClient* renderClient = nullptr;
        if (FAILED(audioClient->GetService(__uuidof(IAudioRenderClient), (void**)&renderClient))) {
            audioClient->Release();
            targetDevice->Release();
            CoUninitialize();
            return;
        }

        UINT32 bufferFrameCount = 0;
        audioClient->GetBufferSize(&bufferFrameCount);

        hr = audioClient->Start();
        if (FAILED(hr)) {
            std::cout << "Start failed: 0x" << std::hex << hr << "\n";
            renderClient->Release();
            audioClient->Release();
            targetDevice->Release();
            CoUninitialize();
            return;
        }

        std::thread([renderClient, audioClient, outBuffer = std::move(outBuffer), bytesPerFrame, bufferFrameCount]() mutable {
            uint32_t offset = 0;
            BYTE* pData = nullptr;
            size_t totalBytes = outBuffer.size();

            while (offset < totalBytes && !stop_audio.load()) {
                UINT32 padding = 0;
                if (FAILED(audioClient->GetCurrentPadding(&padding))) break;

                UINT32 framesAvailable = bufferFrameCount - padding;
                if (framesAvailable == 0) {
                    Sleep(1);
                    continue;
                }

                UINT32 bytesAvailable = framesAvailable * bytesPerFrame;
                UINT32 bytesLeft = static_cast<UINT32>(totalBytes - offset);
                UINT32 bytesToWrite = (bytesAvailable < bytesLeft) ? bytesAvailable : bytesLeft;
                UINT32 framesToWrite = bytesToWrite / bytesPerFrame;
                if (framesToWrite == 0) break;

                if (FAILED(renderClient->GetBuffer(framesToWrite, &pData))) break;

                memcpy(pData, outBuffer.data() + offset, framesToWrite * bytesPerFrame);
                offset += framesToWrite * bytesPerFrame;

                renderClient->ReleaseBuffer(framesToWrite, 0);
            }

            stop_audio.store(false);
            audioClient->Stop();
            renderClient->Release();
            audioClient->Release();
        }).detach();
        targetDevice->Release();
        #else
        snd_pcm_t* handle;
        const char* alsa_device = (device_name && strlen(device_name) > 0) ? device_name : "default";
        int err = snd_pcm_open(&handle, alsa_device, SND_PCM_STREAM_PLAYBACK, 0);
        if (err < 0) {
            std::cerr << "snd_pcm_open error: " << snd_strerror(err) << "\n";
            return;
        }

        err = snd_pcm_set_params(handle, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, wav.channels, wav.sampleRate, 1, 500000);
        if (err < 0) {
            std::cerr << "snd_pcm_set_params error: " << snd_strerror(err) << "\n";
            snd_pcm_close(handle);
            return;
        }

        std::thread([handle, wav]() mutable {
            const int16_t* data = reinterpret_cast<const int16_t*>(wav.buffer.data());
            size_t totalFrames = wav.buffer.size() / (wav.channels * sizeof(int16_t));
            size_t framesWritten = 0;
            const size_t chunkFrames = 1024;

            while (framesWritten < totalFrames && !stop_audio.load()) {
                size_t toWrite = std::min(chunkFrames, totalFrames - framesWritten);
                snd_pcm_sframes_t written = snd_pcm_writei(handle, data + framesWritten * wav.channels, toWrite);
                if (written == -EPIPE) {
                    snd_pcm_prepare(handle);
                    continue;
                } else if (written < 0) {
                    if ((int)written == -ESTRPIPE) {
                        snd_pcm_resume(handle);
                        continue;
                    }
                    std::cerr << "snd_pcm_writei error: " << snd_strerror((int)written) << "\n";
                    break;
                } else {
                    framesWritten += (size_t)written;
                }
            }

            stop_audio.store(false);
            snd_pcm_drain(handle);
            snd_pcm_close(handle);
        }).detach();
        #endif
    }

    void device_to_device(const char* input, const char* output) {
        #ifdef _WIN32
        HRESULT hr = S_OK;
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        IMMDevice* captureDevice = find_device_by_name(eCapture, input);
        IMMDevice* renderDevice  = find_device_by_name(eRender,  output);

        if (!captureDevice) {
            IMMDeviceEnumerator* pEnum = nullptr;
            if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pEnum)))) {
                pEnum->GetDefaultAudioEndpoint(eCapture, eConsole, &captureDevice);
                pEnum->Release();
            }
        }
        if (!renderDevice) {
            IMMDeviceEnumerator* pEnum = nullptr;
            if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pEnum)))) {
                pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &renderDevice);
                pEnum->Release();
            }
        }

        if (!captureDevice || !renderDevice) {
            std::cerr << "WASAPI: could not find capture or render device\n";
            if (captureDevice) captureDevice->Release();
            if (renderDevice) renderDevice->Release();
            CoUninitialize();
            return;
        }

        IAudioClient* captureClient = nullptr;
        IAudioClient* renderClient  = nullptr;
        if (FAILED(captureDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&captureClient))) {
            std::cerr << "WASAPI: failed to activate capture client\n";
            captureDevice->Release(); renderDevice->Release(); CoUninitialize();
            return;
        }
        if (FAILED(renderDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&renderClient))) {
            std::cerr << "WASAPI: failed to activate render client\n";
            captureClient->Release(); captureDevice->Release(); renderDevice->Release(); CoUninitialize();
            return;
        }

        WAVEFORMATEX* wfCapture = nullptr;
        WAVEFORMATEX* wfRender  = nullptr;
        if (FAILED(captureClient->GetMixFormat(&wfCapture)) || wfCapture == nullptr) {
            std::cerr << "WASAPI: GetMixFormat capture failed\n";
            renderClient->Release(); captureClient->Release(); captureDevice->Release(); renderDevice->Release(); CoUninitialize();
            return;
        }
        if (FAILED(renderClient->GetMixFormat(&wfRender)) || wfRender == nullptr) {
            std::cerr << "WASAPI: GetMixFormat render failed\n";
            CoTaskMemFree(wfCapture);
            renderClient->Release(); captureClient->Release(); captureDevice->Release(); renderDevice->Release(); CoUninitialize();
            return;
        }

        bool captureIsFloat = is_format_float(wfCapture);
        bool renderIsFloat  = is_format_float(wfRender);

        bool formats_equal = (wfCapture->nChannels == wfRender->nChannels) && (wfCapture->nSamplesPerSec == wfRender->nSamplesPerSec)
            && (wfCapture->wBitsPerSample == wfRender->wBitsPerSample) && (captureIsFloat == renderIsFloat);

        if (!formats_equal) {
            std::cerr << "WASAPI: device formats differ. Aborting pipe. Capture: " << wfCapture->nChannels << "ch@" << wfCapture->nSamplesPerSec << "Hz "
                    << wfCapture->wBitsPerSample << "b " << (captureIsFloat ? "float" : "int") << "  Render: "
                    << wfRender->nChannels << "ch@" << wfRender->nSamplesPerSec << "Hz " << wfRender->wBitsPerSample << "b " << (renderIsFloat ? "float" : "int")
                    << "\n";
            CoTaskMemFree(wfCapture);
            CoTaskMemFree(wfRender);
            renderClient->Release(); captureClient->Release();
            captureDevice->Release(); renderDevice->Release(); CoUninitialize();
            return;
        }

        REFERENCE_TIME requestedBufferPeriod = 100000;
        DWORD initFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

        hr = captureClient->Initialize(AUDCLNT_SHAREMODE_SHARED, initFlags, requestedBufferPeriod, 0, wfCapture, NULL);
        if (FAILED(hr)) {
            std::cerr << "WASAPI: capture Initialize failed: 0x" << std::hex << hr << "\n";
            CoTaskMemFree(wfCapture); CoTaskMemFree(wfRender);
            renderClient->Release(); captureClient->Release();
            captureDevice->Release(); renderDevice->Release(); CoUninitialize();
            return;
        }

        hr = renderClient->Initialize(AUDCLNT_SHAREMODE_SHARED, initFlags, requestedBufferPeriod, 0, wfRender, NULL);
        if (FAILED(hr)) {
            std::cerr << "WASAPI: render Initialize failed: 0x" << std::hex << hr << "\n";
            CoTaskMemFree(wfCapture); CoTaskMemFree(wfRender);
            renderClient->Release(); captureClient->Release();
            captureDevice->Release(); renderDevice->Release(); CoUninitialize();
            return;
        }

        HANDLE hCaptureEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        HANDLE hRenderEvent  = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (!hCaptureEvent || !hRenderEvent) {
            std::cerr << "WASAPI: CreateEvent failed\n";
            if (hCaptureEvent) CloseHandle(hCaptureEvent);
            if (hRenderEvent) CloseHandle(hRenderEvent);
            CoTaskMemFree(wfCapture); CoTaskMemFree(wfRender);
            renderClient->Release(); captureClient->Release();
            captureDevice->Release(); renderDevice->Release(); CoUninitialize();
            return;
        }

        if (FAILED(captureClient->SetEventHandle(hCaptureEvent))) {
            std::cerr << "WASAPI: SetEventHandle capture failed\n";
            CloseHandle(hCaptureEvent); CloseHandle(hRenderEvent);
            CoTaskMemFree(wfCapture); CoTaskMemFree(wfRender);
            renderClient->Release(); captureClient->Release();
            captureDevice->Release(); renderDevice->Release(); CoUninitialize();
            return;
        }
        if (FAILED(renderClient->SetEventHandle(hRenderEvent))) {
            std::cerr << "WASAPI: SetEventHandle render failed\n";
            captureClient->SetEventHandle(NULL);
            CloseHandle(hCaptureEvent); CloseHandle(hRenderEvent);
            CoTaskMemFree(wfCapture); CoTaskMemFree(wfRender);
            renderClient->Release(); captureClient->Release();
            captureDevice->Release(); renderDevice->Release(); CoUninitialize();
            return;
        }

        IAudioCaptureClient* pCaptureClient = nullptr;
        IAudioRenderClient*  pRenderClient  = nullptr;
        if (FAILED(captureClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient))) {
            std::cerr << "WASAPI: GetService capture failed\n";
            CloseHandle(hCaptureEvent); CloseHandle(hRenderEvent);
            CoTaskMemFree(wfCapture); CoTaskMemFree(wfRender);
            renderClient->Release(); captureClient->Release();
            captureDevice->Release(); renderDevice->Release(); CoUninitialize();
            return;
        }
        if (FAILED(renderClient->GetService(__uuidof(IAudioRenderClient), (void**)&pRenderClient))) {
            std::cerr << "WASAPI: GetService render failed\n";
            pCaptureClient->Release();
            CloseHandle(hCaptureEvent); CloseHandle(hRenderEvent);
            CoTaskMemFree(wfCapture); CoTaskMemFree(wfRender);
            renderClient->Release(); captureClient->Release();
            captureDevice->Release(); renderDevice->Release(); CoUninitialize();
            return;
        }

        UINT32 captureBufferFrames = 0;
        UINT32 renderBufferFrames  = 0;
        captureClient->GetBufferSize(&captureBufferFrames);
        renderClient->GetBufferSize(&renderBufferFrames);

        hr = captureClient->Start();
        if (FAILED(hr)) {
            std::cerr << "WASAPI: capture Start failed\n";
            goto wasapi_cleanup;
        }
        hr = renderClient->Start();
        if (FAILED(hr)) {
            std::cerr << "WASAPI: render Start failed\n";
            goto wasapi_cleanup;
        }

        const UINT32 bytesPerSample = wfCapture->wBitsPerSample / 8;
        const UINT32 channels = wfCapture->nChannels;
        const UINT32 bytesPerFrame = bytesPerSample * channels;

        while (!stop_audio.load()) {
            DWORD wait = WaitForSingleObject(hCaptureEvent, 2000);
            if (wait == WAIT_TIMEOUT) {
                continue;
            } else if (wait != WAIT_OBJECT_0) {
                break;
            }

            UINT32 packetFrames = 0;
            if (FAILED(pCaptureClient->GetNextPacketSize(&packetFrames))) break;
            if (packetFrames == 0) continue;

            BYTE* pData = nullptr;
            UINT32 numFrames = 0;
            DWORD flags = 0;
            if (FAILED(pCaptureClient->GetBuffer(&pData, &numFrames, &flags, nullptr, nullptr))) break;
            if (numFrames == 0) {
                pCaptureClient->ReleaseBuffer(0);
                continue;
            }

            UINT32 framesLeft = numFrames;
            const BYTE* readPtr = pData;
            while (framesLeft > 0 && !stop_audio.load()) {
                UINT32 padding = 0;
                if (FAILED(renderClient->GetCurrentPadding(&padding))) { framesLeft = 0; break; }
                UINT32 available = (renderBufferFrames > padding) ? (renderBufferFrames - padding) : 0;
                if (available == 0) {
                    DWORD w = WaitForSingleObject(hRenderEvent, 10);
                    if (w == WAIT_TIMEOUT) continue;
                    else if (w != WAIT_OBJECT_0) { framesLeft = 0; break; }
                    else continue;
                }
                UINT32 toWrite = (framesLeft < available) ? framesLeft : available;

                BYTE* renderPtr = nullptr;
                if (FAILED(pRenderClient->GetBuffer(toWrite, &renderPtr))) { framesLeft = 0; break; }

                memcpy(renderPtr, readPtr, (size_t)toWrite * bytesPerFrame);
                if (FAILED(pRenderClient->ReleaseBuffer(toWrite, 0))) { framesLeft = 0; break; }

                readPtr += (size_t)toWrite * bytesPerFrame;
                framesLeft -= toWrite;
            }

            if (FAILED(pCaptureClient->ReleaseBuffer(numFrames))) break;
        }

        wasapi_cleanup:
        if (pCaptureClient) pCaptureClient->Release();
        if (pRenderClient) pRenderClient->Release();
        if (captureClient) captureClient->Stop();
        if (renderClient) renderClient->Stop();
        if (captureClient) captureClient->Release();
        if (renderClient) renderClient->Release();
        if (captureDevice) captureDevice->Release();
        if (renderDevice) renderDevice->Release();
        if (wfCapture) CoTaskMemFree(wfCapture);
        if (wfRender) CoTaskMemFree(wfRender);
        if (hCaptureEvent) CloseHandle(hCaptureEvent);
        if (hRenderEvent) CloseHandle(hRenderEvent);
        CoUninitialize();

        #else
        snd_pcm_t* captureHandle = nullptr;
        snd_pcm_t* playbackHandle = nullptr;
        int err;

        const char* capture_device = (input && strlen(input) > 0) ? input : "default";
        const char* playback_device = (output && strlen(output) > 0) ? output : "default";

        if ((err = snd_pcm_open(&captureHandle, capture_device, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
            std::cerr << "ALSA: cannot open capture device " << capture_device << " : " << snd_strerror(err) << "\n";
            return;
        }

        snd_pcm_hw_params_t* capture_params = nullptr;
        snd_pcm_hw_params_malloc(&capture_params);
        snd_pcm_hw_params_any(captureHandle, capture_params);
        if ((err = snd_pcm_hw_params_current(captureHandle, capture_params)) < 0) {
            std::cerr << "ALSA: snd_pcm_hw_params_current failed: " << snd_strerror(err) << "\n";
            snd_pcm_hw_params_free(capture_params);
            snd_pcm_close(captureHandle);
            return;
        }

        snd_pcm_format_t format;
        unsigned int rate = 0;
        int channels = 0;
        snd_pcm_hw_params_get_format(capture_params, &format);
        snd_pcm_hw_params_get_rate(capture_params, &rate, 0);
        snd_pcm_hw_params_get_channels(capture_params, &channels);

        snd_pcm_hw_params_free(capture_params);

        if (channels <= 0 || rate == 0) {
            std::cerr << "ALSA: invalid capture params\n";
            snd_pcm_close(captureHandle);
            return;
        }

        if ((err = snd_pcm_open(&playbackHandle, playback_device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
            std::cerr << "ALSA: cannot open playback device " << playback_device << " : " << snd_strerror(err) << "\n";
            snd_pcm_close(captureHandle);
            return;
        }

        snd_pcm_hw_params_t* play_params = nullptr;
        snd_pcm_hw_params_malloc(&play_params);
        snd_pcm_hw_params_any(playbackHandle, play_params);

        if ((err = snd_pcm_hw_params_set_access(playbackHandle, play_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
            std::cerr << "ALSA: cannot set access: " << snd_strerror(err) << "\n";
            goto alsa_cleanup_open;
        }
        if ((err = snd_pcm_hw_params_set_format(playbackHandle, play_params, format)) < 0) {
            std::cerr << "ALSA: playback device does not support capture format: " << snd_strerror(err) << "\n";
            goto alsa_cleanup_open;
        }
        if ((err = snd_pcm_hw_params_set_channels(playbackHandle, play_params, channels)) < 0) {
            std::cerr << "ALSA: cannot set channels: " << snd_strerror(err) << "\n";
            goto alsa_cleanup_open;
        }
        if ((err = snd_pcm_hw_params_set_rate_near(playbackHandle, play_params, &rate, 0)) < 0) {
            std::cerr << "ALSA: cannot set rate: " << snd_strerror(err) << "\n";
            goto alsa_cleanup_open;
        }

        snd_pcm_uframes_t periodSize = 256;
        snd_pcm_uframes_t bufferSize = periodSize * 4;

        if ((err = snd_pcm_hw_params_set_period_size_near(playbackHandle, play_params, &periodSize, 0)) < 0) {
            std::cerr << "ALSA: cannot set period size: " << snd_strerror(err) << "\n";
            goto alsa_cleanup_open;
        }
        if ((err = snd_pcm_hw_params_set_buffer_size_near(playbackHandle, play_params, &bufferSize)) < 0) {
            std::cerr << "ALSA: cannot set buffer size: " << snd_strerror(err) << "\n";
            goto alsa_cleanup_open;
        }

        if ((err = snd_pcm_hw_params(playbackHandle, play_params)) < 0) {
            std::cerr << "ALSA: cannot set play hw params: " << snd_strerror(err) << "\n";
            goto alsa_cleanup_open;
        }

        snd_pcm_uframes_t actualPeriod = 0;
        snd_pcm_hw_params_get_period_size(play_params, &actualPeriod, 0);
        if (actualPeriod == 0) actualPeriod = periodSize;

        snd_pcm_hw_params_free(play_params);

        if ((err = snd_pcm_prepare(captureHandle)) < 0) {
            std::cerr << "ALSA: capture prepare failed: " << snd_strerror(err) << "\n";
            goto alsa_cleanup_open;
        }
        if ((err = snd_pcm_prepare(playbackHandle)) < 0) {
            std::cerr << "ALSA: playback prepare failed: " << snd_strerror(err) << "\n";
            goto alsa_cleanup_open;
        }

        int bytesPerSample = snd_pcm_format_width(format) / 8;
        int bytesPerFrame = bytesPerSample * channels;

        const snd_pcm_uframes_t chunkFrames = actualPeriod;
        std::vector<char> buffer(chunkFrames * bytesPerFrame);

        while (!stop_audio.load()) {
            if ((err = snd_pcm_wait(captureHandle, 2000)) < 0) {
                std::cerr << "ALSA: snd_pcm_wait error: " << snd_strerror(err) << "\n";
                break;
            } else if (err == 0) {
                continue;
            }

            snd_pcm_sframes_t r = snd_pcm_readi(captureHandle, buffer.data(), chunkFrames);
            if (r == -EAGAIN) {
                continue;
            } else if (r == -EPIPE) {
                snd_pcm_prepare(captureHandle);
                continue;
            } else if (r < 0) {
                std::cerr << "ALSA: read error: " << snd_strerror(r) << "\n";
                break;
            }

            snd_pcm_sframes_t framesRead = r;
            char* writePtr = buffer.data();
            while (framesRead > 0 && !stop_audio.load()) {
                snd_pcm_sframes_t w = snd_pcm_writei(playbackHandle, writePtr, framesRead);
                if (w == -EAGAIN) {
                    continue;
                } else if (w == -EPIPE) {
                    snd_pcm_prepare(playbackHandle);
                    continue;
                } else if (w < 0) {
                    std::cerr << "ALSA: write error: " << snd_strerror(w) << "\n";
                    goto alsa_cleanup;
                } else {
                    writePtr += (size_t)w * bytesPerFrame;
                    framesRead -= w;
                }
            }
        }

        alsa_cleanup:
        snd_pcm_drain(playbackHandle);
        snd_pcm_close(playbackHandle);
        snd_pcm_close(captureHandle);
        return;

        alsa_cleanup_open:
        if (playbackHandle) snd_pcm_close(playbackHandle);
        if (captureHandle) snd_pcm_close(captureHandle);
        if (play_params) snd_pcm_hw_params_free(play_params);
        return;
        #endif
    }
}
