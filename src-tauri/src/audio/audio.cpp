#pragma region Includes
#include <vector>
#include <string>
#include <thread>
#include <fstream>
#include <unordered_set>
#include <iostream>
#include <cctype>
#include <algorithm>
#include <atomic>
#include <cmath>

#ifdef _WIN32
#define NOMINMAX
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
#include <pulse/simple.h>
#include <pulse/error.h>
#endif
#pragma endregion

static std::vector<std::string> storage;
static std::vector<const char*> c_strs;
static std::vector<std::unique_ptr<char[]>> c_copies;

#pragma region Helpers
std::string wideToUtf8(const wchar_t* wstr) {
    if (!wstr) return {};

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) return {};

    std::string str(size_needed - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str[0], size_needed, nullptr, nullptr);
    return str;
}

struct WAVData {
    char* buffer = nullptr;
    size_t bufferSize = 0;
    int sampleRate = 0;
    int channels = 0;
};

WAVData loadWav(const char* filename) {
    std::ifstream f(filename, std::ios::binary);
    WAVData wav;
    if (!f) return wav;

    char riff[4]; f.read(riff, 4);
    if (std::strncmp(riff, "RIFF", 4) != 0) return wav;
    f.seekg(4, std::ios::cur);
    char wave[4]; f.read(wave, 4);
    if (std::strncmp(wave, "WAVE", 4) != 0) return wav;

    while (f) {
        char chunkId[4]; uint32_t chunkSize = 0;
        f.read(chunkId, 4);
        f.read(reinterpret_cast<char*>(&chunkSize), 4);
        if (!f) break;

        if (std::strncmp(chunkId, "fmt ", 4) == 0) {
            uint16_t audioFormat, channels, blockAlign, bitsPerSample;
            uint32_t sampleRate, byteRate;
            f.read(reinterpret_cast<char*>(&audioFormat), 2);
            f.read(reinterpret_cast<char*>(&channels), 2);
            f.read(reinterpret_cast<char*>(&sampleRate), 4);
            f.read(reinterpret_cast<char*>(&byteRate), 4);
            f.read(reinterpret_cast<char*>(&blockAlign), 2);
            f.read(reinterpret_cast<char*>(&bitsPerSample), 2);
            if (chunkSize > 16) f.seekg(chunkSize - 16, std::ios::cur);
            wav.channels = channels;
            wav.sampleRate = sampleRate;
        } else if (std::strncmp(chunkId, "data", 4) == 0) {
            wav.bufferSize = chunkSize;
            wav.buffer = new char[chunkSize];
            f.read(wav.buffer, chunkSize);
            break;
        } else f.seekg(chunkSize, std::ios::cur);
    }
    return wav;
}

#ifdef _WIN32
float* int16_to_float(const int16_t* data, size_t frames, int channels) {
    float* out = new float[frames * channels];
    for (size_t i = 0; i < frames * channels; ++i)
        out[i] = data[i] / 32768.f;
    return out;
}

float* linear_resample_interleaved(const float* src, size_t srcFrames, int channels, int srcRate, int dstRate, size_t* outFrames) {
    *outFrames = srcFrames * dstRate / srcRate;
    float* out = new float[*outFrames * channels];
    for (size_t i = 0; i < *outFrames; ++i) {
        float srcPos = i * float(srcFrames) / (*outFrames);
        size_t idx = size_t(srcPos);
        float frac = srcPos - idx;
        for (int c = 0; c < channels; ++c) {
            float s0 = (idx < srcFrames) ? src[idx * channels + c] : 0.f;
            float s1 = (idx + 1 < srcFrames) ? src[(idx + 1) * channels + c] : 0.f;
            out[i * channels + c] = s0 * (1 - frac) + s1 * frac;
        }
    }
    return out;
}

float* remap_channels_interleaved(const float* src, size_t frames, int srcCh, int dstCh) {
    float* out = new float[frames * dstCh];
    for (size_t f = 0; f < frames; ++f) {
        for (int c = 0; c < dstCh; ++c) {
            out[f * dstCh + c] = src[f * srcCh + (c % srcCh)];
        }
    }
    return out;
}

bool is_format_float(WAVEFORMATEX* wf) {
    if (!wf) return false;
    if (wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) return true;
    if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(wf);
        if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) return true;
    }
    return false;
}

bool is_format_int32(WAVEFORMATEX* wf) {
    if (!wf) return false;
    if (wf->wFormatTag == WAVE_FORMAT_PCM && wf->wBitsPerSample == 32) return true;
    if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(wf);
        if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_PCM && wf->wBitsPerSample == 32) return true;
    }
    return false;
}
#else
snd_pcm_format_t get_alsa_format(int bitsPerSample, bool isFloat) {
    if (isFloat) {
        if (bitsPerSample == 32) return SND_PCM_FORMAT_FLOAT_LE;
    } else {
        if (bitsPerSample == 16) return SND_PCM_FORMAT_S16_LE;
        if (bitsPerSample == 32) return SND_PCM_FORMAT_S32_LE;
    }
    return SND_PCM_FORMAT_UNKNOWN;
}
#endif

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
#else
struct AudioData {
    pa_stream* inputStream = nullptr;
    pa_stream* outputStream = nullptr;
    pa_mainloop* mainloop = nullptr;
};

void stream_read_callback(pa_stream* s, size_t length, void* userdata) {
    AudioData* data = (AudioData*)userdata;
    const void* buf;
    if (pa_stream_peek(s, &buf, &length) < 0) return;

    if (length > 0) {
        pa_stream_write(data->outputStream, buf, length, nullptr, 0LL, PA_SEEK_RELATIVE);
    }

    pa_stream_drop(s);
}
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
#pragma endregion

extern "C" {
    std::atomic<bool> stop_audio(true);

    #pragma region Get Ouputs
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

        UINT count;
        pDevices->GetCount(&count);

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
    #pragma endregion
    #pragma region Get Inputs
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
    #pragma endregion
    #pragma region Get Apps
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
    #pragma endregion
    #pragma region Play Sound
    void play_sound(const char* wav_file, const char* device_name, bool low_latency) {
        WAVData wav = loadWav(wav_file);
        if (!wav.buffer || wav.channels <= 0) {
            std::cerr << "Failed to load WAV: " << wav_file << "\n";
            return;
        }

        #ifdef _WIN32
        CoInitialize(nullptr);
        IMMDevice* targetDevice = nullptr;

        targetDevice = find_device_by_name(eRender, device_name);
        if (!targetDevice) {
            IMMDeviceEnumerator* pEnum = nullptr;
            CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pEnum));
            pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &targetDevice);
            pEnum->Release();
        }

        if (!targetDevice) { std::cerr << "No audio device found\n"; CoUninitialize(); return; }

        IAudioClient* audioClient = nullptr;
        if (FAILED(targetDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audioClient))) {
            targetDevice->Release(); CoUninitialize(); return;
        }

        WAVEFORMATEX* pwfx = nullptr;
        if (FAILED(audioClient->GetMixFormat(&pwfx)) || !pwfx) {
            std::cerr << "GetMixFormat failed\n";
            audioClient->Release(); targetDevice->Release(); CoUninitialize(); return;
        }

        const int16_t* src16 = reinterpret_cast<const int16_t*>(wav.buffer);
        size_t srcFrames = wav.bufferSize / (wav.channels * sizeof(int16_t));
        float* srcFloat = int16_to_float(src16, srcFrames, wav.channels);

        size_t resampledFrames = 0;
        float* resampled = linear_resample_interleaved(srcFloat, srcFrames, wav.channels, wav.sampleRate, pwfx->nSamplesPerSec, &resampledFrames);

        float* finalBuffer = remap_channels_interleaved(resampled, resampledFrames, wav.channels, pwfx->nChannels);

        size_t bytesPerSample = pwfx->wBitsPerSample / 8;
        size_t bytesPerFrame = pwfx->nChannels * bytesPerSample;
        size_t totalBytes = resampledFrames * pwfx->nChannels * bytesPerSample;
        std::vector<char> outBuffer(totalBytes);

        if (is_format_float(pwfx) && pwfx->wBitsPerSample == 32) {
            memcpy(outBuffer.data(), finalBuffer, totalBytes);
        } else if (is_format_int32(pwfx)) {
            int32_t* out32 = reinterpret_cast<int32_t*>(outBuffer.data());
            for (size_t i = 0; i < resampledFrames * pwfx->nChannels; ++i) {
                float v = std::max(-1.f, std::min(1.f, finalBuffer[i]));
                out32[i] = static_cast<int32_t>(v * 2147483647.f);
            }
        } else {
            std::cerr << "Unsupported device format\n";
            CoTaskMemFree(pwfx);
            audioClient->Release();
            targetDevice->Release();
            CoUninitialize();
            return;
        }

        REFERENCE_TIME bufferDuration = low_latency ? 100000 : 500000;
        if (FAILED(audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferDuration, 0, pwfx, nullptr))) {
            std::cerr << "AudioClient Initialize failed\n";
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
        audioClient->Start();

        std::thread([renderClient, audioClient, outBuffer, bytesPerFrame, bufferFrameCount]() {
            size_t offset = 0;
            size_t totalBytes = outBuffer.size();
            while (offset < totalBytes && !stop_audio.load()) {
                UINT32 padding = 0;
                if (FAILED(audioClient->GetCurrentPadding(&padding))) break;

                UINT32 framesAvailable = bufferFrameCount - padding;
                if (framesAvailable == 0) { Sleep(1); continue; }

                UINT32 bytesAvailable = framesAvailable * bytesPerFrame;
                UINT32 bytesLeft = static_cast<UINT32>(totalBytes - offset);
                UINT32 bytesToWrite = std::min(bytesAvailable, bytesLeft);
                UINT32 framesToWrite = bytesToWrite / bytesPerFrame;

                BYTE* pData = nullptr;
                if (FAILED(renderClient->GetBuffer(framesToWrite, &pData))) break;
                memcpy(pData, outBuffer.data() + offset, framesToWrite * bytesPerFrame);
                offset += framesToWrite * bytesPerFrame;
                renderClient->ReleaseBuffer(framesToWrite, 0);
            }

            audioClient->Stop();
            renderClient->Release();
            audioClient->Release();
        }).detach();

        targetDevice->Release();
        #else
        const char* alsa_device = (device_name && strlen(device_name) > 0) ? device_name : "default";
        snd_pcm_t* handle;
        if (snd_pcm_open(&handle, alsa_device, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
            std::cerr << "Cannot open ALSA device\n";
            return;
        }

        snd_pcm_hw_params_t* params;
        snd_pcm_hw_params_malloc(&params);
        snd_pcm_hw_params_any(handle, params);

        bool isFloat = false;
        int bitsPerSample = 16;

        if (wav.bufferSize % (wav.channels * sizeof(int16_t)) == 0)
            bitsPerSample = 16;
        else if (wav.bufferSize % (wav.channels * sizeof(int32_t)) == 0)
            bitsPerSample = 32;

        snd_pcm_format_t format = get_alsa_format(bitsPerSample, isFloat);
        if (format == SND_PCM_FORMAT_UNKNOWN) {
            std::cerr << "Unsupported WAV format for ALSA\n";
            snd_pcm_hw_params_free(params);
            snd_pcm_close(handle);
            return;
        }

        if (snd_pcm_hw_params_any(handle, params) < 0 ||
            snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0 ||
            snd_pcm_hw_params_set_format(handle, params, format) < 0 ||
            snd_pcm_hw_params_set_channels(handle, params, wav.channels) < 0 ||
            snd_pcm_hw_params_set_rate(handle, params, wav.sampleRate, 0) < 0) {
            std::cerr << "Cannot configure ALSA device\n";
            snd_pcm_hw_params_free(params);
            snd_pcm_close(handle);
            return;
        }

        snd_pcm_hw_params(handle, params);
        snd_pcm_hw_params_free(params);

        std::thread([handle, wav, format]() {
            const void* data = wav.buffer;
            size_t frameSize = wav.channels * ((format == SND_PCM_FORMAT_S16_LE) ? 2 : (format == SND_PCM_FORMAT_S32_LE || format == SND_PCM_FORMAT_FLOAT_LE) ? 4 : 0);
            size_t totalFrames = wav.bufferSize / frameSize;
            size_t framesWritten = 0;
            const size_t chunkFrames = low_latency ? 512 : 1024;

            while (framesWritten < totalFrames && !stop_audio.load()) {
                size_t toWrite = std::min(chunkFrames, totalFrames - framesWritten);
                snd_pcm_sframes_t written = snd_pcm_writei(handle, (const char*)data + framesWritten * frameSize, toWrite);
                if (written == -EPIPE) { snd_pcm_prepare(handle); continue; }
                else if (written < 0) { std::cerr << "ALSA write error\n"; break; }
                else framesWritten += written;
            }

            snd_pcm_drain(handle);
            snd_pcm_close(handle);
        }).detach();
        #endif
    }
    #pragma endregion
    #pragma region Device to Device Pipe
    void device_to_device(const char* input, const char* output, bool low_latency) {
        #ifdef _WIN32
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        IMMDevice* captureDevice = find_device_by_name(eCapture, input);
        IMMDevice* renderDevice  = find_device_by_name(eRender, output);

        IMMDeviceEnumerator* pEnum = nullptr;
        if (!captureDevice || !renderDevice) {
            if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pEnum)))) {
                if (!captureDevice) pEnum->GetDefaultAudioEndpoint(eCapture, eConsole, &captureDevice);
                if (!renderDevice) pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &renderDevice);
            }
        }

        if (!captureDevice || !renderDevice) {
            std::cerr << "WASAPI: could not find capture or render device\n";
            if (captureDevice) captureDevice->Release();
            if (renderDevice) renderDevice->Release();
            if (pEnum) pEnum->Release();
            CoUninitialize();
            return;
        }
        if (pEnum) pEnum->Release();

        IAudioClient* captureClient = nullptr;
        IAudioClient* renderClient  = nullptr;
        if (FAILED(captureDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&captureClient)) ||
            FAILED(renderDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&renderClient))) {
            std::cerr << "WASAPI: failed to activate client\n";
            captureDevice->Release(); renderDevice->Release(); CoUninitialize(); return;
        }

        WAVEFORMATEX* wfCapture = nullptr;
        WAVEFORMATEX* wfRender  = nullptr;
        if (FAILED(captureClient->GetMixFormat(&wfCapture)) || FAILED(renderClient->GetMixFormat(&wfRender)) || !wfCapture || !wfRender) {
            std::cerr << "WASAPI: GetMixFormat failed\n";
            if (wfCapture) CoTaskMemFree(wfCapture);
            if (wfRender) CoTaskMemFree(wfRender);
            captureClient->Release(); renderClient->Release();
            captureDevice->Release(); renderDevice->Release();
            CoUninitialize(); return;
        }

        bool captureIsFloat = is_format_float(wfCapture);
        bool renderIsFloat  = is_format_float(wfRender);

        REFERENCE_TIME bufferDuration = low_latency ? 100000 : 500000;
        if (FAILED(captureClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferDuration, 0, wfCapture, nullptr)) ||
            FAILED(renderClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferDuration, 0, wfRender, nullptr))) {
            std::cerr << "WASAPI: Initialize failed\n";
            CoTaskMemFree(wfCapture); CoTaskMemFree(wfRender);
            captureClient->Release(); renderClient->Release();
            captureDevice->Release(); renderDevice->Release();
            CoUninitialize(); return;
        }

        IAudioCaptureClient* pCapture = nullptr;
        IAudioRenderClient*  pRender  = nullptr;
        if (FAILED(captureClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCapture)) ||
            FAILED(renderClient->GetService(__uuidof(IAudioRenderClient), (void**)&pRender))) {
            std::cerr << "WASAPI: GetService failed\n";
            CoTaskMemFree(wfCapture); CoTaskMemFree(wfRender);
            captureClient->Release(); renderClient->Release();
            captureDevice->Release(); renderDevice->Release();
            CoUninitialize(); return;
        }

        UINT32 captureFrames = 0, renderFrames = 0;
        captureClient->GetBufferSize(&captureFrames);
        renderClient->GetBufferSize(&renderFrames);

        size_t maxFrames = std::max(captureFrames, renderFrames);
        int captureChannels = wfCapture->nChannels;
        int renderChannels  = wfRender->nChannels;

        std::vector<float> captureBuffer(maxFrames * captureChannels);
        std::vector<float> renderBuffer(maxFrames * renderChannels);

        captureClient->Start();
        renderClient->Start();

        while (!stop_audio.load()) {
            UINT32 packetFrames = 0;
            if (FAILED(pCapture->GetNextPacketSize(&packetFrames)) || packetFrames == 0) {
                Sleep(1);
                continue;
            }

            BYTE* pData = nullptr;
            UINT32 numFrames = 0;
            DWORD flags = 0;
            if (FAILED(pCapture->GetBuffer(&pData, &numFrames, &flags, nullptr, nullptr))) break;

            if (captureIsFloat) {
                memcpy(captureBuffer.data(), pData, numFrames * captureChannels * sizeof(float));
            } else if (wfCapture->wBitsPerSample == 16) {
                int16_t* src16 = reinterpret_cast<int16_t*>(pData);
                for (UINT32 i = 0; i < numFrames * captureChannels; ++i)
                    captureBuffer[i] = src16[i] / 32768.f;
            } else if (wfCapture->wBitsPerSample == 32) {
                int32_t* src32 = reinterpret_cast<int32_t*>(pData);
                for (UINT32 i = 0; i < numFrames * captureChannels; ++i)
                    captureBuffer[i] = src32[i] / 2147483648.f;
            }

            pCapture->ReleaseBuffer(numFrames);

            float* toRender = nullptr;
            size_t outFrames = 0;
            if (wfCapture->nSamplesPerSec != wfRender->nSamplesPerSec || captureChannels != renderChannels) {
                float* resampled = linear_resample_interleaved(captureBuffer.data(), numFrames, captureChannels, wfCapture->nSamplesPerSec,
                    wfRender->nSamplesPerSec, &outFrames);
                float* remapped = remap_channels_interleaved(resampled, outFrames, captureChannels, renderChannels);
                delete[] resampled;
                toRender = remapped;
            } else {
                toRender = captureBuffer.data();
                outFrames = numFrames;
            }

            UINT32 written = 0;
            while (written < outFrames && !stop_audio.load()) {
                UINT32 padding = 0;
                renderClient->GetCurrentPadding(&padding);
                UINT32 avail = renderFrames - padding;
                if (avail == 0) { Sleep(1); continue; }

                UINT32 framesToWrite = std::min(avail, static_cast<UINT32>(outFrames - written));
                BYTE* renderPtr = nullptr;
                if (FAILED(pRender->GetBuffer(framesToWrite, &renderPtr))) break;

                if (renderIsFloat && wfRender->wBitsPerSample == 32) {
                    memcpy(renderPtr, toRender + written * renderChannels, framesToWrite * renderChannels * sizeof(float));
                } else if (!renderIsFloat && wfRender->wBitsPerSample == 16) {
                    int16_t* out16 = reinterpret_cast<int16_t*>(renderPtr);
                    for (UINT32 i = 0; i < framesToWrite * renderChannels; ++i)
                        out16[i] = std::max(-32768.f, std::min(32767.f, toRender[written * renderChannels + i] * 32768.f));
                } else if (!renderIsFloat && wfRender->wBitsPerSample == 32) {
                    int32_t* out32 = reinterpret_cast<int32_t*>(renderPtr);
                    for (UINT32 i = 0; i < framesToWrite * renderChannels; ++i)
                        out32[i] = std::max(-2147483648.f, std::min(2147483647.f, toRender[written * renderChannels + i] * 2147483648.f));
                }

                if (FAILED(pRender->ReleaseBuffer(framesToWrite, 0))) break;
                written += framesToWrite;
            }

            if (toRender != captureBuffer.data()) delete[] toRender;
        }

        captureClient->Stop();
        renderClient->Stop();
        pCapture->Release();
        pRender->Release();
        captureClient->Release();
        renderClient->Release();
        captureDevice->Release();
        renderDevice->Release();
        CoTaskMemFree(wfCapture);
        CoTaskMemFree(wfRender);
        CoUninitialize();
        #else
        struct PADevicePipe {
            pa_mainloop* mainloop = nullptr;
            pa_context* context = nullptr;
            pa_stream* captureStream = nullptr;
            pa_stream* playbackStream = nullptr;
            std::vector<float> captureBuffer;
            std::vector<float> playbackBuffer;
            unsigned captureChannels = 0;
            unsigned playbackChannels = 0;
            unsigned captureRate = 0;
            unsigned playbackRate = 0;
        };

        PADevicePipe pipe;

        pipe.mainloop = pa_mainloop_new();
        pipe.context = pa_context_new(pa_mainloop_get_api(pipe.mainloop), "DevicePipe");

        pa_context_connect(pipe.context, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

        while (true) {
            pa_context_state_t state = pa_context_get_state(pipe.context);
            if (state == PA_CONTEXT_READY) break;
            if (!PA_CONTEXT_IS_GOOD(state)) {
                std::cerr << "PulseAudio context failed\n";
                pa_context_unref(pipe.context);
                pa_mainloop_free(pipe.mainloop);
                return;
            }
            pa_mainloop_iterate(pipe.mainloop, 1, nullptr);
        }

        pa_sample_spec captureSpec;
        captureSpec.format = PA_SAMPLE_FLOAT32LE;
        captureSpec.channels = 2;
        captureSpec.rate = 48000;

        pipe.captureStream = pa_stream_new(pipe.context, "Capture", &captureSpec, nullptr);
        pa_stream_connect_record(pipe.captureStream, input ? input : "default", nullptr, PA_STREAM_ADJUST_LATENCY | PA_STREAM_INTERPOLATE_TIMING);

        pa_sample_spec playbackSpec;
        playbackSpec.format = PA_SAMPLE_FLOAT32LE;
        playbackSpec.channels = 2;
        playbackSpec.rate = 48000;

        pipe.playbackStream = pa_stream_new(pipe.context, "Playback", &playbackSpec, nullptr);
        pa_stream_connect_playback(pipe.playbackStream, output ? output : "default",
                                nullptr, PA_STREAM_INTERPOLATE_TIMING, nullptr, nullptr);

        while (pa_stream_get_state(pipe.captureStream) != PA_STREAM_READY ||
            pa_stream_get_state(pipe.playbackStream) != PA_STREAM_READY) {
            pa_mainloop_iterate(pipe.mainloop, 1, nullptr);
        }

        pipe.captureChannels = captureSpec.channels;
        pipe.playbackChannels = playbackSpec.channels;
        pipe.captureRate = captureSpec.rate;
        pipe.playbackRate = playbackSpec.rate;

        size_t maxFrames = low_latency ? 2048 : 8192;
        pipe.captureBuffer.resize(maxFrames * pipe.captureChannels);
        pipe.playbackBuffer.resize(maxFrames * pipe.playbackChannels);

        while (!stop_audio.load()) {
            const void* data = nullptr;
            size_t frames = 0;
            if (pa_stream_peek(pipe.captureStream, &data, &frames) < 0 || frames == 0) {
                pa_mainloop_iterate(pipe.mainloop, 1, nullptr);
                continue;
            }

            const float* in = reinterpret_cast<const float*>(data);
            std::vector<float> floatData(frames * pipe.captureChannels);
            memcpy(floatData.data(), in, frames * pipe.captureChannels * sizeof(float));

            pa_stream_drop(pipe.captureStream);

            float* toRender = nullptr;
            size_t outFrames = 0;

            if (pipe.captureRate != pipe.playbackRate || pipe.captureChannels != pipe.playbackChannels) {
                float* resampled = linear_resample_interleaved(floatData.data(), frames, pipe.captureChannels, pipe.captureRate, pipe.playbackRate, &outFrames);
                float* remapped = remap_channels_interleaved(resampled, outFrames, pipe.captureChannels, pipe.playbackChannels);
                delete[] resampled;
                toRender = remapped;
            } else {
                toRender = floatData.data();
                outFrames = frames;
            }

            size_t written = 0;
            while (written < outFrames && !stop_audio.load()) {
                size_t toWrite = std::min(static_cast<size_t>(1024), outFrames - written);
                pa_stream_write(pipe.playbackStream, toRender + written * pipe.playbackChannels, toWrite * pipe.playbackChannels * sizeof(float),
                    nullptr, 0LL, PA_SEEK_RELATIVE);
                written += toWrite;
                pa_mainloop_iterate(pipe.mainloop, 0, nullptr);
            }

            if (toRender != floatData.data()) delete[] toRender;
        }

        pa_stream_disconnect(pipe.captureStream);
        pa_stream_disconnect(pipe.playbackStream);
        pa_stream_unref(pipe.captureStream);
        pa_stream_unref(pipe.playbackStream);
        pa_context_disconnect(pipe.context);
        pa_context_unref(pipe.context);
        pa_mainloop_free(pipe.mainloop);
        #endif
    }
    #pragma endregion
    #pragma region App to Device Pipe
    void app_to_device(const char* input, const char* output, bool low_latency) {
        #ifdef _WIN32
        CoInitialize(nullptr);

        DWORD targetPid = 0;
        PROCESSENTRY32W pe32{};
        pe32.dwSize = sizeof(pe32);
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (Process32FirstW(hSnap, &pe32)) {
            do {
                std::string exe = wideToUtf8(pe32.szExeFile);
                if (exe.size() > 4 && exe.substr(exe.size() - 4) == ".exe")
                    exe = exe.substr(0, exe.size() - 4);
                if (_stricmp(exe.c_str(), input) == 0) {
                    targetPid = pe32.th32ProcessID;
                    break;
                }
            } while (Process32NextW(hSnap, &pe32));
        }
        CloseHandle(hSnap);
        if (!targetPid) { std::cerr << "Process not found: " << input << "\n"; CoUninitialize(); return; }

        IMMDeviceEnumerator* pEnum = nullptr;
        CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                        __uuidof(IMMDeviceEnumerator), (void**)&pEnum);

        IMMDevice* renderDevice = nullptr;
        if (output && strlen(output) > 0)
            renderDevice = find_device_by_name(eRender, output);
        if (!renderDevice)
            pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &renderDevice);
        if (!renderDevice) {
            std::cerr << "No render device found\n";
            pEnum->Release();
            CoUninitialize();
            return;
        }

        IMMDevice* captureDevice = nullptr;
        IMMDeviceCollection* devicesAll = nullptr;
        pEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devicesAll);
        UINT deviceCount = 0; devicesAll->GetCount(&deviceCount);

        for (UINT i = 0; i < deviceCount; ++i) {
            IMMDevice* dev = nullptr;
            if (FAILED(devicesAll->Item(i, &dev)) || !dev) continue;

            IAudioSessionManager2* mgr2 = nullptr;
            if (FAILED(dev->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&mgr2)) || !mgr2) {
                dev->Release(); continue;
            }

            IAudioSessionEnumerator* sessionEnum = nullptr;
            if (FAILED(mgr2->GetSessionEnumerator(&sessionEnum)) || !sessionEnum) { mgr2->Release(); dev->Release(); continue; }

            int sessionCount = 0;
            sessionEnum->GetCount(&sessionCount);
            bool found = false;

            for (int s = 0; s < sessionCount; ++s) {
                IAudioSessionControl* ctrl = nullptr;
                if (FAILED(sessionEnum->GetSession(s, &ctrl)) || !ctrl) continue;

                IAudioSessionControl2* ctrl2 = nullptr;
                if (SUCCEEDED(ctrl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&ctrl2)) && ctrl2) {
                    DWORD pid = 0; ctrl2->GetProcessId(&pid); ctrl2->Release();
                    if (pid == targetPid) { captureDevice = dev; captureDevice->AddRef(); found = true; break; }
                }
                ctrl->Release();
            }

            sessionEnum->Release(); mgr2->Release(); dev->Release();
            if (found) break;
        }
        devicesAll->Release();

        if (!captureDevice) { std::cerr << "Failed to find audio session for PID\n"; renderDevice->Release(); pEnum->Release(); CoUninitialize(); return; }

        if (captureDevice == renderDevice) {
            std::cerr << "Capture and render device are the same. Feedback possible!\n";
            captureDevice->Release(); renderDevice->Release(); pEnum->Release(); CoUninitialize(); return;
        }

        IAudioClient2* captureClient = nullptr;
        captureDevice->Activate(__uuidof(IAudioClient2), CLSCTX_ALL, nullptr, (void**)&captureClient);

        IAudioClient* renderClient = nullptr;
        renderDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&renderClient);

        WAVEFORMATEX* wfCapture = nullptr;
        WAVEFORMATEX* wfRender  = nullptr;
        captureClient->GetMixFormat(&wfCapture);
        renderClient->GetMixFormat(&wfRender);

        DWORD renderFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
        DWORD captureFlags = AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
        REFERENCE_TIME bufferDuration = low_latency ? 20000 : 1000000;

        captureClient->Initialize(AUDCLNT_SHAREMODE_SHARED, captureFlags, bufferDuration, 0, wfCapture, nullptr);
        renderClient->Initialize(AUDCLNT_SHAREMODE_SHARED, renderFlags, bufferDuration, 0, wfRender, nullptr);

        IAudioCaptureClient* pCaptureClient = nullptr;
        IAudioRenderClient* pRenderClient = nullptr;
        captureClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
        renderClient->GetService(__uuidof(IAudioRenderClient), (void**)&pRenderClient);

        HANDLE hCaptureEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        HANDLE hRenderEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        captureClient->SetEventHandle(hCaptureEvent);
        renderClient->SetEventHandle(hRenderEvent);

        UINT32 renderBufferFrames = 0;
        renderClient->GetBufferSize(&renderBufferFrames);

        captureClient->Start(); renderClient->Start();

        size_t captureFramesMax = wfCapture->nSamplesPerSec;
        std::vector<float> captureBuffer(captureFramesMax * wfCapture->nChannels);

        bool captureIsFloat = is_format_float(wfCapture);
        bool renderIsFloat  = is_format_float(wfRender);
        bool needResample = (wfCapture->nChannels != wfRender->nChannels)
                        || (wfCapture->nSamplesPerSec != wfRender->nSamplesPerSec)
                        || (captureIsFloat != renderIsFloat)
                        || (wfCapture->wBitsPerSample != wfRender->wBitsPerSample);

        const UINT32 renderBytesPerFrame = wfRender->nChannels * (wfRender->wBitsPerSample / 8);

        while (!stop_audio.load()) {
            DWORD wait = WaitForSingleObject(hCaptureEvent, 2000);
            if (wait != WAIT_OBJECT_0) continue;

            UINT32 packetFrames = 0;
            pCaptureClient->GetNextPacketSize(&packetFrames);
            if (packetFrames == 0) continue;

            BYTE* pData = nullptr;
            UINT32 numFrames = 0;
            DWORD flags = 0;
            if (FAILED(pCaptureClient->GetBuffer(&pData, &numFrames, &flags, nullptr, nullptr))) continue;

            captureBuffer.resize(numFrames * wfCapture->nChannels);
            if (captureIsFloat) {
                memcpy(captureBuffer.data(), pData, numFrames * wfCapture->nChannels * sizeof(float));
            } else if (wfCapture->wBitsPerSample == 16) {
                int16_t* src16 = reinterpret_cast<int16_t*>(pData);
                for (UINT32 i = 0; i < numFrames * wfCapture->nChannels; ++i)
                    captureBuffer[i] = src16[i] / 32768.f;
            } else if (wfCapture->wBitsPerSample == 32) {
                int32_t* src32 = reinterpret_cast<int32_t*>(pData);
                for (UINT32 i = 0; i < numFrames * wfCapture->nChannels; ++i)
                    captureBuffer[i] = src32[i] / 2147483648.f;
            }

            pCaptureClient->ReleaseBuffer(numFrames);

            float* outBuffer = nullptr;
            size_t outFrames = 0;
            if (needResample) {
                outBuffer = linear_resample_interleaved(captureBuffer.data(), numFrames, wfCapture->nChannels, wfCapture->nSamplesPerSec,
                    wfRender->nSamplesPerSec, &outFrames);
                if (wfCapture->nChannels != wfRender->nChannels)
                    outBuffer = remap_channels_interleaved(outBuffer, outFrames, wfCapture->nChannels, wfRender->nChannels);
            } else {
                outBuffer = captureBuffer.data();
                outFrames = numFrames;
            }

            size_t framesLeft = outFrames;
            size_t frameIdx = 0;
            const UINT32 bytesPerFrame = wfRender->nChannels * (wfRender->wBitsPerSample / 8);
            while (framesLeft > 0) {
                DWORD waitRender = WaitForSingleObject(hRenderEvent, 2000);
                if (waitRender != WAIT_OBJECT_0) break;

                UINT32 padding = 0;
                renderClient->GetCurrentPadding(&padding);
                UINT32 available = (renderBufferFrames > padding) ? (renderBufferFrames - padding) : 0;
                if (available == 0) break;

                UINT32 toWrite = (framesLeft < available) ? (UINT32)framesLeft : available;
                BYTE* renderPtr = nullptr;
                if (FAILED(pRenderClient->GetBuffer(toWrite, &renderPtr))) break;

                if (renderIsFloat) {
                    memcpy(renderPtr, outBuffer + frameIdx * wfRender->nChannels, toWrite * wfRender->nChannels * sizeof(float));
                } else if (wfRender->wBitsPerSample == 16) {
                    int16_t* dst16 = reinterpret_cast<int16_t*>(renderPtr);
                    for (UINT32 i = 0; i < toWrite * wfRender->nChannels; ++i) {
                        float v = outBuffer[frameIdx * wfRender->nChannels + i] * 32767.f;
                        dst16[i] = (int16_t)std::max(-32768.f, std::min(32767.f, v));
                    }
                } else if (wfRender->wBitsPerSample == 32) {
                    int32_t* dst32 = reinterpret_cast<int32_t*>(renderPtr);
                    for (UINT32 i = 0; i < toWrite * wfRender->nChannels; ++i) {
                        double v = outBuffer[frameIdx * wfRender->nChannels + i] * 2147483647.0;
                        dst32[i] = (int32_t)std::max(double(INT32_MIN), std::min(double(INT32_MAX), v));
                    }
                }

                pRenderClient->ReleaseBuffer(toWrite, 0);
                framesLeft -= toWrite;
                frameIdx += toWrite;
            }

            if (needResample && outBuffer != captureBuffer.data())
                delete[] outBuffer;
        }

        captureClient->Stop(); renderClient->Stop();
        CloseHandle(hCaptureEvent); CloseHandle(hRenderEvent);
        if (pRenderClient) pRenderClient->Release();
        if (pCaptureClient) pCaptureClient->Release();
        captureClient->Release(); renderClient->Release();
        captureDevice->Release(); renderDevice->Release();
        pEnum->Release();
        CoTaskMemFree(wfCapture); CoTaskMemFree(wfRender);
        CoUninitialize();
        #else
        pa_mainloop* mainloop = pa_mainloop_new();
        pa_context* context = pa_context_new(pa_mainloop_get_api(mainloop), "app_to_device");

        pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

        pa_context_state_t state;
        do {
            pa_mainloop_iterate(mainloop, 1, nullptr);
            state = pa_context_get_state(context);
        } while (state != PA_CONTEXT_READY && state != PA_CONTEXT_FAILED && state != PA_CONTEXT_TERMINATED);

        if (state != PA_CONTEXT_READY) {
            std::cerr << "Failed to connect to PulseAudio\n";
            pa_context_disconnect(context);
            pa_context_unref(context);
            pa_mainloop_free(mainloop);
            return;
        }

        struct UserData {
            const char* targetApp;
            uint32_t sinkInputIndex;
            bool found;
        } userdata{ inputApp, PA_INVALID_INDEX, false };

        auto sink_input_cb = [](pa_context* c, const pa_sink_input_info* i, int eol, void* userdata) {
            if (eol > 0) return;
            auto ud = (UserData*)userdata;
            const char* name = pa_proplist_gets(i->proplist, PA_PROP_APPLICATION_NAME);
            if (name && strcmp(name, ud->targetApp) == 0) {
                ud->sinkInputIndex = i->index;
                ud->found = true;
            }
        };
        pa_context_get_sink_input_info_list(context, sink_input_cb, &userdata);
        pa_mainloop_iterate(mainloop, 1, nullptr);

        if (!userdata.found) {
            std::cerr << "Target app not found: " << inputApp << "\n";
            pa_context_disconnect(context);
            pa_context_unref(context);
            pa_mainloop_free(mainloop);
            return;
        }

        uint32_t sinkIndex = PA_INVALID_INDEX;
        if (outputSink) {
            struct OutputData { const char* name; uint32_t idx; } outdata{ outputSink, PA_INVALID_INDEX };
            auto sink_cb = [](pa_context* c, const pa_sink_info* i, int eol, void* userdata) {
                if (eol > 0) return;
                auto od = (OutputData*)userdata;
                if (i->description && strcmp(i->description, od->name) == 0)
                    od->idx = i->index;
            };
            pa_context_get_sink_info_list(context, sink_cb, &outdata);
            pa_mainloop_iterate(mainloop, 1, nullptr);
            sinkIndex = outdata.idx;
        }

        if (sinkIndex != PA_INVALID_INDEX && sinkIndex != pa_sink_input_info_get_index(context, userdata.sinkInputIndex))
            pa_context_move_sink_input_by_index(context, userdata.sinkInputIndex, sinkIndex);

        pa_sample_spec appSpec{};
        appSpec.format = PA_SAMPLE_FLOAT32LE;
        appSpec.rate = low_latency ? 48000 : 44100;
        appSpec.channels = 2;

        pa_stream* inputStream = pa_stream_new(context, "capture", &appSpec, nullptr);
        pa_stream* outputStream = pa_stream_new(context, "playback", &appSpec, nullptr);

        struct AudioData {
            pa_mainloop* mainloop;
            pa_stream* inputStream;
            pa_stream* outputStream;
            std::vector<float> captureBuffer;
            std::vector<float> outBuffer;
            unsigned int inputChannels;
            unsigned int outputChannels;
            unsigned int inputRate;
            unsigned int outputRate;
        } audioData{ mainloop, inputStream, outputStream };

        auto stream_read_cb = [](pa_stream* s, size_t length, void* userdata) {
            auto data = (AudioData*)userdata;
            const void* ptr;
            if (pa_stream_peek(s, &ptr, &length) < 0 || !ptr) return;

            size_t frames = length / (sizeof(float) * data->inputChannels);
            data->captureBuffer.resize(frames * data->inputChannels);
            memcpy(data->captureBuffer.data(), ptr, length);
            pa_stream_drop(s);
        };
        pa_stream_set_read_callback(inputStream, stream_read_cb, &audioData);

        std::string monitorName = "@DEFAULT_MONITOR@";
        if (sinkIndex != PA_INVALID_INDEX) {
            char buf[128];
            snprintf(buf, sizeof(buf), "alsa_output.%u.monitor", sinkIndex);
            monitorName = buf;
        }
        pa_stream_connect_record(inputStream, monitorName.c_str(), nullptr,
                                PA_STREAM_ADJUST_LATENCY | (low_latency ? PA_STREAM_START_CORKED : 0));
        pa_stream_connect_playback(outputStream, nullptr, nullptr,
                                PA_STREAM_ADJUST_LATENCY | (low_latency ? PA_STREAM_START_CORKED : 0), nullptr, nullptr);

        while (!stop_audio.load()) {
            pa_mainloop_iterate(mainloop, 1, nullptr);
            if (audioData.captureBuffer.empty()) continue;

            float* resampled = nullptr;
            size_t outFrames = audioData.captureBuffer.size() / audioData.inputChannels;
            bool needResample = (audioData.inputRate != audioData.outputRate) || (audioData.inputChannels != audioData.outputChannels);

            if (needResample) {
                resampled = linear_resample_interleaved(audioData.captureBuffer.data(), outFrames, audioData.inputChannels, audioData.inputRate,
                                                        audioData.outputRate, &outFrames);
                if (audioData.inputChannels != audioData.outputChannels)
                    resampled = remap_channels_interleaved(resampled, outFrames, audioData.inputChannels, audioData.outputChannels);
            } else {
                resampled = audioData.captureBuffer.data();
            }

            size_t framesLeft = outFrames;
            size_t frameIdx = 0;
            while (framesLeft > 0) {
                size_t chunkFrames = std::min<size_t>(framesLeft, low_latency ? 2048 : 8192);
                pa_stream_write(outputStream, resampled + frameIdx * audioData.outputChannels, chunkFrames * audioData.outputChannels * sizeof(float),
                    nullptr, 0, PA_SEEK_RELATIVE);
                framesLeft -= chunkFrames;
                frameIdx += chunkFrames;
            }

            if (needResample && resampled != audioData.captureBuffer.data())
                delete[] resampled;

            audioData.captureBuffer.clear();
        }

        pa_stream_disconnect(inputStream);
        pa_stream_disconnect(outputStream);
        pa_stream_unref(inputStream);
        pa_stream_unref(outputStream);
        pa_context_disconnect(context);
        pa_context_unref(context);
        pa_mainloop_free(mainloop);
        #endif
    }
    #pragma endregion
}