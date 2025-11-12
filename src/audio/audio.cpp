#pragma region Includes
#include <vector>
#include <string>
#include <thread>
#include <fstream>
#include <unordered_set>
#include <unordered_map>
#include <iostream>
#include <cctype>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <chrono>

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
#include <mfapi.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <mftransform.h>
#include <endpointvolume.h>
#else
#include <alsa/asoundlib.h>
#include <dirent.h>
#include <unistd.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <cstring>
#include <mutex>
#include <memory>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#endif
#pragma endregion

static std::vector<std::string> storage;
static std::vector<const char*> c_strs;
static std::vector<std::unique_ptr<char[]>> c_copies;

#pragma region Helpers
#ifdef _WIN32
std::string wideToUtf8(const wchar_t* wstr) {
    if (!wstr) return {};

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) return {};

    std::string str(size_needed - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str[0], size_needed, nullptr, nullptr);
    return str;
}
#endif

struct PCMData {
    char* buffer = nullptr;
    size_t bufferSize = 0;
    int sampleRate = 0;
    int channels = 0;
};

struct PCMResult {
    PCMData pcm;
    int result; // 0 = success, 1 = unknown format, 2 = file error
};

PCMResult loadPCM(const char* filename) {
    std::string lower(filename);
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".wav") == 0) {
        std::ifstream f(filename, std::ios::binary);
        if (!f) return { {}, 2 };

        char riff[4]; f.read(riff, 4);
        if (std::strncmp(riff, "RIFF", 4) != 0) return { {}, 2 };
        f.seekg(4, std::ios::cur);
        char wave[4]; f.read(wave, 4);
        if (std::strncmp(wave, "WAVE", 4) != 0) return { {}, 2 };


        PCMData pcm;
        while (f) {
            char chunkId[4];
            uint32_t chunkSize = 0;
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
                pcm.channels = channels;
                pcm.sampleRate = sampleRate;
            } else if (std::strncmp(chunkId, "data", 4) == 0) {
                pcm.bufferSize = chunkSize;
                pcm.buffer = new char[chunkSize];
                f.read(pcm.buffer, chunkSize);
                break;
            } else f.seekg(chunkSize, std::ios::cur);
        }
        return { pcm, 0 };
    }

    #ifdef _WIN32
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) return { {}, 1 };

    IMFSourceReader* reader = nullptr;
    std::wstring wfilename(filename, filename + strlen(filename));
    hr = MFCreateSourceReaderFromURL(wfilename.c_str(), nullptr, &reader);
    if (FAILED(hr) || !reader) return { {}, 1 };

    IMFMediaType* audioTypeOut = nullptr;
    hr = MFCreateMediaType(&audioTypeOut);
    if (FAILED(hr) || !audioTypeOut) {
        reader->Release();
        return { {}, 1 };
    }

    audioTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    audioTypeOut->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, audioTypeOut);

    IMFMediaType* pOutType = nullptr;
    reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pOutType);

    UINT32 sampleRate = MFGetAttributeUINT32(pOutType, MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
    UINT32 channels = MFGetAttributeUINT32(pOutType, MF_MT_AUDIO_NUM_CHANNELS, 2);

    std::vector<char> pcmData;

    while (true) {
        DWORD streamIndex, flags;
        LONGLONG timestamp;
        IMFSample* sample = nullptr;
        hr = reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, &streamIndex, &flags, &timestamp, &sample);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) break;
        if (!sample) continue;

        IMFMediaBuffer* buffer = nullptr;
        hr = sample->ConvertToContiguousBuffer(&buffer);
        if (SUCCEEDED(hr) && buffer) {
            BYTE* audioData = nullptr;
            DWORD audioDataLen = 0;
            buffer->Lock(&audioData, nullptr, &audioDataLen);

            size_t oldSize = pcmData.size();
            pcmData.resize(oldSize + audioDataLen);
            memcpy(pcmData.data() + oldSize, audioData, audioDataLen);

            buffer->Unlock();
            buffer->Release();
        }
        sample->Release();
    }

    if (pOutType) pOutType->Release();
    if (audioTypeOut) audioTypeOut->Release();
    if (reader) reader->Release();
    MFShutdown();

    PCMData pcm{};
    pcm.bufferSize = pcmData.size();
    pcm.buffer = new char[pcm.bufferSize];
    memcpy(pcm.buffer, pcmData.data(), pcm.bufferSize);
    pcm.channels = channels;
    pcm.sampleRate = sampleRate;
    return { pcm, 0 };
    #else
    gst_init(nullptr, nullptr);
    std::string pipelineStr = std::string("filesrc location=\"") + filename + "\" ! decodebin ! audioconvert ! audioresample ! appsink name=sink";
    GError* err = nullptr;
    GstElement* pipeline = gst_parse_launch(pipelineStr.c_str(), &err);
    if (!pipeline) return { {}, 1 };

    GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    gst_element_get_state(pipeline, nullptr, nullptr, GST_CLOCK_TIME_NONE);

    std::vector<char> pcmData;
    int sampleRate = 0;
    int channels = 0;

    GstCaps* caps = gst_app_sink_get_caps(GST_APP_SINK(sink));
    if (caps) {
        GstStructure* s = gst_caps_get_structure(caps, 0);
        gst_structure_get_int(s, "rate", &sampleRate);
        gst_structure_get_int(s, "channels", &channels);
        gst_caps_unref(caps);
    }

    while (true) {
        GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
        if (!sample) break;
        GstBuffer* buffer = gst_sample_get_buffer(sample);
        GstMapInfo map;
        gst_buffer_map(buffer, &map, GST_MAP_READ);
        pcmData.insert(pcmData.end(), map.data, map.data + map.size);
        gst_buffer_unmap(buffer, &map);
        gst_sample_unref(sample);
    }

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(sink);
    gst_object_unref(pipeline);

    PCMData pcm{};
    pcm.bufferSize = pcmData.size();
    pcm.buffer = new char[pcm.bufferSize];
    memcpy(pcm.buffer, pcmData.data(), pcm.bufferSize);
    pcm.sampleRate = sampleRate;
    pcm.channels = channels;
    return { pcm, 0 };
    #endif
}

const char* string_to_cchar(std::string string) {
    return strdup(string.c_str());
}

#ifdef _WIN32
float* int16_to_float(const int16_t* data, size_t frames, int channels) {
    float* out = new float[frames * channels];
    for (size_t i = 0; i < frames * channels; ++i)
        out[i] = data[i] / 32768.f;
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

std::string format_alsa_hint(const char* name, const char* desc) {
    if (desc && *desc) {
        std::string s(desc);
        while (!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin());
        while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
        return s;
    }
    if (!name) return std::string();

    std::string s(name);
    const std::vector<std::string> prefixes = {"plughw:", "hw:", "sysdefault:", "default:"};
    for (const auto& p : prefixes) {
        if (s.rfind(p, 0) == 0) { s = s.substr(p.size()); break; }
    }

    std::string tmp;
    bool prevSpace = false;
    for (char ch : s) {
        if (ch == ',' || ch == '_' || ch == ':' || std::isspace((unsigned char)ch)) {
            if (!prevSpace) { tmp.push_back(' '); prevSpace = true; }
        } else {
            tmp.push_back(ch);
            prevSpace = false;
        }
    }
    if (!tmp.empty() && tmp.front() == ' ') tmp.erase(tmp.begin());
    if (!tmp.empty() && tmp.back() == ' ') tmp.pop_back();

    return tmp.empty() ? s : tmp;
}
#endif

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
}

std::string get_device_name(IMMDevice* pDevice)
{
    IPropertyStore* pProps = nullptr;
    HRESULT hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
    if (FAILED(hr)) return "null";

    PROPVARIANT varName;
    PropVariantInit(&varName);

    hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
    std::string name = "null";
    if (SUCCEEDED(hr)) {
        name = wideToUtf8(varName.pwszVal);
    }

    PropVariantClear(&varName);
    pProps->Release();

    return name;
}
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

snd_pcm_t* find_device_by_name(bool output, const char* display_name) {
    void** hints = nullptr;
    snd_pcm_t* handle = nullptr;

    if (snd_device_name_hint(-1, "pcm", &hints) < 0)
        return nullptr;

    std::string selected_device;

    for (void** n = hints; *n != nullptr; ++n) {
        char* name = snd_device_name_get_hint(*n, "NAME");
        char* desc = snd_device_name_get_hint(*n, "DESC");
        char* ioid = snd_device_name_get_hint(*n, "IOID");

        bool is_output = (!ioid || strcmp(ioid, "Output") == 0);
        if (is_output == output) {
            std::string display = format_alsa_hint(name, desc);
            if (!display.empty() && display_name && display == display_name) {
                if (name)
                    selected_device = name;
            }
        }

        if (name) free(name);
        if (desc) free(desc);
        if (ioid) free(ioid);

        if (!selected_device.empty()) break;
    }

    snd_device_name_free_hint(hints);

    const char* alsa_device = selected_device.empty() ? "default" : selected_device.c_str();

    int err = snd_pcm_open(&handle, alsa_device, output ? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        std::cerr << "Failed to open ALSA device '" << alsa_device << "': " << snd_strerror(err) << "\n";
        return nullptr;
    }

    return handle;
}

std::string find_pa_device_by_name(pa_context* ctx, bool output, const char* display_name) {
    if (!ctx) return "default";

    struct DeviceSearch {
        std::string wanted;
        std::string found;
        bool done = false;
    } data{ display_name ? display_name : "default", "" };

    auto sink_cb = [](pa_context*, const pa_sink_info* i, int eol, void* userdata) {
        if (eol > 0) return;
        auto* d = (DeviceSearch*)userdata;
        if (!i || d->done) return;

        if (i->description && d->wanted == i->description) {
            d->found = i->name;
            d->done = true;
        }
    };

    auto source_cb = [](pa_context*, const pa_source_info* i, int eol, void* userdata) {
        if (eol > 0) return;
        auto* d = (DeviceSearch*)userdata;
        if (!i || d->done) return;

        if (i->description && d->wanted == i->description) {
            d->found = i->name;
            d->done = true;
        }
    };

    pa_mainloop* loop = pa_mainloop_new();
    if (!loop) return "default";

    bool created_context = false;
    if (pa_context_get_state(ctx) != PA_CONTEXT_READY) {
        ctx = pa_context_new(pa_mainloop_get_api(loop), "find_device");
        pa_context_connect(ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

        while (true) {
            pa_mainloop_iterate(loop, 1, nullptr);
            auto st = pa_context_get_state(ctx);
            if (st == PA_CONTEXT_READY) break;
            if (!PA_CONTEXT_IS_GOOD(st)) {
                pa_context_unref(ctx);
                pa_mainloop_free(loop);
                return "default";
            }
        }
        created_context = true;
    }

    if (output)
        pa_context_get_sink_info_list(ctx, sink_cb, &data);
    else
        pa_context_get_source_info_list(ctx, source_cb, &data);

    int tries = 100;
    while (!data.done && tries-- > 0) {
        pa_mainloop_iterate(loop, 1, nullptr);
    }

    if (created_context) {
        pa_context_disconnect(ctx);
        pa_context_unref(ctx);
    }
    pa_mainloop_free(loop);

    if (data.found.empty())
        data.found = "default";

    return data.found;
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
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        storage.clear();
        c_strs.clear();
        c_copies.clear();
    }).detach();
}
#pragma endregion

extern "C" {
    std::atomic<bool> stop_audio(true);
    #pragma region Volumes
    static std::unordered_map<std::string, float> volume;

    void reset_volume() {
        volume.clear();
    }

    void insert_volume(const char* key, float value) {
        volume[key] = value;
    }

    void free_cstr(const char* ptr) {
        if (ptr) free((void*)ptr);
    }

    const char* get_volume(const char* name, bool get, bool device) {
        #ifdef _WIN32
        CoInitialize(nullptr);

        if (device == true) {
            IMMDevice* targetDevice = find_device_by_name(eCapture, name);
            if (!targetDevice) {
                return string_to_cchar(get ? "0¬null" : "0");
            }

            IAudioMeterInformation* pMeterInfo = nullptr;

            if (FAILED(targetDevice->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, NULL, (void**)&pMeterInfo))) {
                std::cerr << "Failed to activate IAudioMeterInformation\n";
                targetDevice->Release();
                return string_to_cchar(get ? "0¬null" : "0");
            }

            float peak = 0.0f;
            if (SUCCEEDED(pMeterInfo->GetPeakValue(&peak))) {
                std::string device_name = get_device_name(targetDevice);
                targetDevice->Release();
                pMeterInfo->Release();
                if (get == true) {
                    return string_to_cchar(std::to_string(peak)+"¬"+device_name);
                } else {
                    return string_to_cchar(std::to_string(peak));
                }
            } else {
                std::cerr << "GetPeakValue failed\n";
                targetDevice->Release();
                pMeterInfo->Release();
                return string_to_cchar(get ? "0¬null" : "0");
            }
        } else {
            IMMDevice* targetDevice = nullptr;
            std::wstring trueName;

            if (get == true) {
                DWORD targetPid = 0;
                PROCESSENTRY32W pe32{};
                pe32.dwSize = sizeof(pe32);
                HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
                if (Process32FirstW(hSnap, &pe32)) {
                    do {
                        std::string exe = wideToUtf8(pe32.szExeFile);
                        if (exe.size() > 4 && exe.substr(exe.size() - 4) == ".exe")
                            exe = exe.substr(0, exe.size() - 4);
                        if (_stricmp(exe.c_str(), name) == 0) {
                            targetPid = pe32.th32ProcessID;
                            break;
                        }
                    } while (Process32NextW(hSnap, &pe32));
                }
                CloseHandle(hSnap);
                if (!targetPid) {
                    std::cerr << "Process not found: " << name << "\n";
                    CoUninitialize();
                    return string_to_cchar("0¬null");
                }

                IMMDeviceEnumerator* pEnum = nullptr;
                CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);

                IMMDeviceCollection* devicesAll = nullptr;
                pEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devicesAll);
                UINT deviceCount = 0; devicesAll->GetCount(&deviceCount);

                for (UINT i = 0; i < deviceCount; ++i) {
                    IMMDevice* dev = nullptr;
                    if (FAILED(devicesAll->Item(i, &dev)) || !dev) continue;

                    IAudioSessionManager2* mgr2 = nullptr;
                    if (FAILED(dev->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&mgr2)) || !mgr2) {
                        dev->Release();
                        continue;
                    }

                    IAudioSessionEnumerator* sessionEnum = nullptr;
                    if (FAILED(mgr2->GetSessionEnumerator(&sessionEnum)) || !sessionEnum) {
                        mgr2->Release();
                        dev->Release();
                        continue;
                    }

                    int sessionCount = 0;
                    sessionEnum->GetCount(&sessionCount);
                    bool found = false;

                    for (int s = 0; s < sessionCount; ++s) {
                        IAudioSessionControl* ctrl = nullptr;
                        if (FAILED(sessionEnum->GetSession(s, &ctrl)) || !ctrl) continue;

                        IAudioSessionControl2* ctrl2 = nullptr;
                        if (SUCCEEDED(ctrl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&ctrl2)) && ctrl2) {
                            DWORD pid = 0;
                            ctrl2->GetProcessId(&pid);
                            ctrl2->Release();
                            if (pid == targetPid) {
                                targetDevice = dev;
                                targetDevice->AddRef();
                                found = true;
                                break;
                            }
                        }
                        ctrl->Release();
                    }

                    sessionEnum->Release();
                    mgr2->Release();
                    dev->Release();
                    if (found) break;
                }
                devicesAll->Release();

                if (!targetDevice) {
                    std::cerr << "Failed to find audio session for PID\n";
                    pEnum->Release();
                    CoUninitialize();
                    return string_to_cchar(get ? "0¬null" : "0");
                }
            } else {
                targetDevice = find_device_by_name(eRender, name);
                if (!targetDevice) {
                    return string_to_cchar("0");
                }
            }

            IAudioMeterInformation* pMeterInfo = nullptr;

            if (FAILED(targetDevice->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, NULL, (void**)&pMeterInfo))) {
                std::cerr << "Failed to activate IAudioMeterInformation\n";
                targetDevice->Release();
                return string_to_cchar(get ? "0¬null" : "0");
            }

            float peak = 0.0f;
            if (SUCCEEDED(pMeterInfo->GetPeakValue(&peak))) {
                std::string device_name = get_device_name(targetDevice);
                targetDevice->Release();
                pMeterInfo->Release();
                if (get == true) {
                    return string_to_cchar(std::to_string(peak)+"¬"+device_name);
                } else {
                    return string_to_cchar(std::to_string(peak));
                }
            } else {
                std::cerr << "GetPeakValue failed\n";
                targetDevice->Release();
                pMeterInfo->Release();
                return string_to_cchar(get ? "0¬null" : "0");
            }
        }
        #else
        if (device == true) {
            snd_pcm_t* handle = find_device_by_name(false, name);
            if (!handle) {
                std::cerr << "Failed to open ALSA device: " << (name ? name : "null") << "\n";
                return string_to_cchar(get ? "0¬null" : "0");
            }

            snd_pcm_hw_params_t* params = nullptr;
            snd_pcm_hw_params_malloc(&params);
            snd_pcm_hw_params_any(handle, params);
            snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
            snd_pcm_format_t fmt = SND_PCM_FORMAT_S16_LE;
            snd_pcm_hw_params_set_format(handle, params, fmt);
            unsigned int rate = 44100;
            snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);
            int channels = 2;
            snd_pcm_hw_params_set_channels(handle, params, channels);
            snd_pcm_uframes_t frames = 1024;
            snd_pcm_hw_params_set_period_size_near(handle, params, &frames, 0);

            int err = snd_pcm_hw_params(handle, params);
            snd_pcm_hw_params_free(params);
            if (err < 0) {
                std::cerr << "Failed to set ALSA hw params: " << snd_strerror(err) << "\n";
                snd_pcm_close(handle);
                return string_to_cchar(get ? "0¬null" : "0");
            }

            int16_t* buffer = new int16_t[frames * channels];
            snd_pcm_prepare(handle);
            snd_pcm_sframes_t read_frames = snd_pcm_readi(handle, buffer, frames);
            float peak = 0.0f;
            if (read_frames > 0) {
                size_t items = (size_t)read_frames * channels;
                for (size_t i = 0; i < items; ++i) {
                    float v = std::abs(buffer[i]) / 32768.0f;
                    if (v > peak) peak = v;
                }
            } else {
                snd_pcm_recover(handle, (int)read_frames, 1);
            }

            delete [] buffer;
            snd_pcm_close(handle);

            std::string device_name = name ? std::string(name) : std::string("null");

            if (get == true) {
                return string_to_cchar(std::to_string(peak) + "¬" + device_name);
            } else {
                return string_to_cchar(std::to_string(peak));
            }
        } else {
            if (get == true) {
                pa_mainloop* mainloop = pa_mainloop_new();
                pa_context* context = pa_context_new(pa_mainloop_get_api(mainloop), "get_volume_loopback");

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
                    return string_to_cchar("0¬null");
                }

                struct UserData {
                    const char* targetApp;
                    uint32_t sinkInputIndex = PA_INVALID_INDEX;
                    uint32_t sinkIndex = PA_INVALID_INDEX;
                    bool found = false;
                } userdata{ name };

                auto sink_input_cb = [](pa_context*, const pa_sink_input_info* i, int eol, void* userdata) {
                    if (eol > 0) return;
                    auto* ud = reinterpret_cast<UserData*>(userdata);
                    if (!i || ud->found) return;
                    const char* proc = pa_proplist_gets(i->proplist, PA_PROP_APPLICATION_PROCESS_BINARY);
                    const char* appname = pa_proplist_gets(i->proplist, PA_PROP_APPLICATION_NAME);
                    if ((proc && strcmp(proc, ud->targetApp) == 0) ||
                        (appname && strcmp(appname, ud->targetApp) == 0)) {
                        ud->sinkInputIndex = i->index;
                        ud->sinkIndex = i->sink;
                        ud->found = true;
                    }
                };

                pa_context_get_sink_input_info_list(context, sink_input_cb, &userdata);
                int tries = 100;
                while (!userdata.found && tries-- > 0)
                    pa_mainloop_iterate(mainloop, 1, nullptr);

                if (!userdata.found) {
                    std::cerr << "Target app not found: " << name << "\n";
                    pa_context_disconnect(context);
                    pa_context_unref(context);
                    pa_mainloop_free(mainloop);
                    return string_to_cchar("0¬null");
                }

                std::string monitorName = "";
                if (userdata.sinkIndex != PA_INVALID_INDEX) {
                    struct SinkInfoData {
                        std::string monitor;
                        bool done = false;
                    } sid;

                    pa_context_get_sink_info_by_index(context, userdata.sinkIndex,
                        [](pa_context*, const pa_sink_info* info, int eol, void* userdata) {
                            if (eol > 0) return;
                            auto* d = reinterpret_cast<SinkInfoData*>(userdata);
                            if (info && info->monitor_source)
                                d->monitor = info->monitor_source;
                            d->done = true;
                        }, &sid);

                    int w = 100;
                    while (!sid.done && w-- > 0)
                        pa_mainloop_iterate(mainloop, 1, nullptr);
                    if (!sid.monitor.empty())
                        monitorName = sid.monitor;
                }

                if (monitorName.empty())
                    monitorName = "@DEFAULT_MONITOR@";

                pa_sample_spec spec{};
                spec.format = PA_SAMPLE_FLOAT32LE;
                spec.rate = 44100;
                spec.channels = 2;

                pa_stream* stream = pa_stream_new(context, "loopback_peak", &spec, nullptr);
                if (!stream) {
                    std::cerr << "Failed to create stream\n";
                    pa_context_disconnect(context);
                    pa_context_unref(context);
                    pa_mainloop_free(mainloop);
                    return string_to_cchar("0¬null");
                }

                std::vector<float> samples;
                bool done = false;

                pa_stream_set_read_callback(stream,
                    [](pa_stream* s, size_t nbytes, void* userdata) {
                        auto* buf = reinterpret_cast<std::vector<float>*>(userdata);
                        const void* data;
                        if (pa_stream_peek(s, &data, &nbytes) < 0 || !data || nbytes == 0)
                            return;
                        size_t frames = nbytes / sizeof(float);
                        buf->resize(frames);
                        memcpy(buf->data(), data, nbytes);
                        pa_stream_drop(s);
                    },
                    &samples);

                if (pa_stream_connect_record(stream, monitorName.c_str(), nullptr, PA_STREAM_ADJUST_LATENCY) < 0) {
                    std::cerr << "Failed to connect to monitor: " << monitorName << "\n";
                    pa_stream_unref(stream);
                    pa_context_disconnect(context);
                    pa_context_unref(context);
                    pa_mainloop_free(mainloop);
                    return string_to_cchar("0¬null");
                }

                while (pa_stream_get_state(stream) != PA_STREAM_READY)
                    pa_mainloop_iterate(mainloop, 1, nullptr);

                auto start = std::chrono::steady_clock::now();
                while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(5))
                    pa_mainloop_iterate(mainloop, 1, nullptr);

                float peak = 0.0f;
                if (!samples.empty()) {
                    for (float f : samples)
                        peak = std::max(peak, std::abs(f));
                }

                pa_stream_disconnect(stream);
                pa_stream_unref(stream);
                pa_context_disconnect(context);
                pa_context_unref(context);
                pa_mainloop_free(mainloop);

                if (get) {
                    return string_to_cchar(std::to_string(peak) + "¬" + monitorName);
                } else {
                    return string_to_cchar(std::to_string(peak));
                }

            } else {
                pa_mainloop* mainloop = pa_mainloop_new();
                pa_context* context = pa_context_new(pa_mainloop_get_api(mainloop), "output_device_peak");

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
                    return string_to_cchar("0¬null");
                }

                std::string monitorName;
                bool gotSink = false;

                std::pair<std::string*, bool*> userData(&monitorName, &gotSink);

                pa_context_get_sink_info_by_name(context, "@DEFAULT_SINK@",
                    [](pa_context*, const pa_sink_info* info, int eol, void* userdata) {
                        if (eol > 0) return;
                        if (!info) return;
                        auto* pair = reinterpret_cast<std::pair<std::string*, bool*>*>(userdata);
                        if (info->monitor_source) {
                            *pair->first = info->monitor_source;
                            *pair->second = true;
                        }
                    },
                    &userData);

                int tries = 100;
                while (!gotSink && tries-- > 0)
                    pa_mainloop_iterate(mainloop, 1, nullptr);

                if (monitorName.empty())
                    monitorName = "@DEFAULT_MONITOR@";

                pa_sample_spec spec{};
                spec.format = PA_SAMPLE_FLOAT32LE;
                spec.rate = 44100;
                spec.channels = 2;

                pa_stream* stream = pa_stream_new(context, "output_peak", &spec, nullptr);
                if (!stream) {
                    std::cerr << "Failed to create stream\n";
                    pa_context_disconnect(context);
                    pa_context_unref(context);
                    pa_mainloop_free(mainloop);
                    return string_to_cchar("0¬null");
                }

                std::vector<float> samples;

                pa_stream_set_read_callback(stream,
                    [](pa_stream* s, size_t nbytes, void* userdata) {
                        auto* buf = reinterpret_cast<std::vector<float>*>(userdata);
                        const void* data;
                        if (pa_stream_peek(s, &data, &nbytes) < 0 || !data || nbytes == 0)
                            return;
                        size_t frames = nbytes / sizeof(float);
                        buf->resize(frames);
                        memcpy(buf->data(), data, nbytes);
                        pa_stream_drop(s);
                    },
                    &samples);

                if (pa_stream_connect_record(stream, monitorName.c_str(), nullptr, PA_STREAM_ADJUST_LATENCY) < 0) {
                    std::cerr << "Failed to connect to monitor: " << monitorName << "\n";
                    pa_stream_unref(stream);
                    pa_context_disconnect(context);
                    pa_context_unref(context);
                    pa_mainloop_free(mainloop);
                    return string_to_cchar("0¬null");
                }

                while (pa_stream_get_state(stream) != PA_STREAM_READY)
                    pa_mainloop_iterate(mainloop, 1, nullptr);

                auto start = std::chrono::steady_clock::now();
                while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(5))
                    pa_mainloop_iterate(mainloop, 1, nullptr);

                float peak = 0.0f;
                if (!samples.empty()) {
                    for (float f : samples)
                        peak = std::max(peak, std::abs(f));
                }

                pa_stream_disconnect(stream);
                pa_stream_unref(stream);
                pa_context_disconnect(context);
                pa_context_unref(context);
                pa_mainloop_free(mainloop);

                return string_to_cchar(std::to_string(peak) + "¬" + monitorName);
            }
        }
        #endif
    }
    #pragma endregion
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
            char *desc = snd_device_name_get_hint(*n, "DESC");
            char *ioid = snd_device_name_get_hint(*n, "IOID");

            if (!ioid || strcmp(ioid, "Output") == 0) {
                std::string display = format_alsa_hint(name, desc);
                if (!display.empty() && isValidName(display)) {
                    storage.emplace_back(display);
                    c_strs.push_back(storage.back().c_str());
                } else if (name && isValidName(name)) {
                    storage.emplace_back(name);
                    c_strs.push_back(storage.back().c_str());
                }
            }

            if (name) free(name);
            if (desc) free(desc);
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
            char *desc = snd_device_name_get_hint(*n, "DESC");
            char *ioid = snd_device_name_get_hint(*n, "IOID");

            if (!ioid || strcmp(ioid, "Input") == 0) {
                std::string display = format_alsa_hint(name, desc);
                if (!display.empty() && isValidName(display)) {
                    storage.emplace_back(display);
                    c_strs.push_back(storage.back().c_str());
                } else if (name && isValidName(name)) {
                    storage.emplace_back(name);
                    c_strs.push_back(storage.back().c_str());
                }
            }

            if (name) free(name);
            if (desc) free(desc);
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
    void play_sound(const char* file, const char* device_name, bool low_latency) {
        PCMResult result = loadPCM(file);
        if (result.result != 0) {
            if (result.result == 1) {
                std::cerr << "Failed to load " << file << ": Unrecognized file format\n";
                return;
            } else {
                std::cerr << "Failed to load " << file << ": File doesn't exist or the file is in use\n";
                return;
            }
        }

        PCMData pcm = result.pcm;
        if (!pcm.buffer || pcm.channels <= 0) {
            if (!pcm.buffer) {
                std::cerr << "Failed to load " << file << ": No audio data\n";
                return;
            } else {
                std::cerr << "Failed to load " << file << ": No channels\n";
                return;
            }
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

        if (!targetDevice) {
            std::cerr << "No audio device found\n";
            CoUninitialize();
            return;
        }

        IAudioClient* audioClient = nullptr;
        if (FAILED(targetDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audioClient))) {
            targetDevice->Release(); CoUninitialize(); return;
        }

        WAVEFORMATEX* pwfx = nullptr;
        if (FAILED(audioClient->GetMixFormat(&pwfx)) || !pwfx) {
            std::cerr << "GetMixFormat failed\n";
            audioClient->Release();
            targetDevice->Release();
            CoUninitialize();
            return;
        }

        const int16_t* src16 = reinterpret_cast<const int16_t*>(pcm.buffer);
        size_t srcFrames = pcm.bufferSize / (pcm.channels * sizeof(int16_t));
        float* srcFloat = int16_to_float(src16, srcFrames, pcm.channels);

        size_t resampledFrames = 0;
        float* resampled = linear_resample_interleaved(srcFloat, srcFrames, pcm.channels, pcm.sampleRate, pwfx->nSamplesPerSec, &resampledFrames);

        float* finalBuffer = remap_channels_interleaved(resampled, resampledFrames, pcm.channels, pwfx->nChannels);

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
        snd_pcm_t* handle = find_device_by_name(true, device_name);
        if (!handle) {
            std::cerr << "Failed to open ALSA device: " << device_name << "\n";
            delete[] pcm.buffer;
            return;
        }

        snd_pcm_hw_params_t* params;
        if (snd_pcm_hw_params_malloc(&params) < 0) {
            std::cerr << "Failed to allocate ALSA hw_params\n";
            snd_pcm_close(handle);
            delete[] pcm.buffer;
            return;
        }

        if (snd_pcm_hw_params_any(handle, params) < 0) {
            std::cerr << "Failed to initialize ALSA hw_params\n";
            snd_pcm_hw_params_free(params);
            snd_pcm_close(handle);
            delete[] pcm.buffer;
            return;
        }

        bool isFloat = false;
        int bitsPerSample = 16;

        if (pcm.bufferSize % (pcm.channels * sizeof(int16_t)) == 0)
            bitsPerSample = 16;
        else if (pcm.bufferSize % (pcm.channels * sizeof(int32_t)) == 0)
            bitsPerSample = 32;

        snd_pcm_format_t format = get_alsa_format(bitsPerSample, isFloat);
        if (format == SND_PCM_FORMAT_UNKNOWN) {
            std::cerr << "Unsupported file format for ALSA\n";
            snd_pcm_hw_params_free(params);
            snd_pcm_close(handle);
            return;
        }

        if (snd_pcm_hw_params_any(handle, params) < 0 ||
            snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0 ||
            snd_pcm_hw_params_set_format(handle, params, format) < 0 ||
            snd_pcm_hw_params_set_channels(handle, params, pcm.channels) < 0 ||
            snd_pcm_hw_params_set_rate(handle, params, pcm.sampleRate, 0) < 0) {
            std::cerr << "Cannot configure ALSA device\n";
            snd_pcm_hw_params_free(params);
            snd_pcm_close(handle);
            return;
        }

        snd_pcm_hw_params(handle, params);
        snd_pcm_hw_params_free(params);

        auto buffer = std::shared_ptr<char[]>(pcm.buffer);
        std::thread([handle, buffer, pcm, format, low_latency]() {
            const void* data = buffer.get();
            size_t frameSize = pcm.channels * ((format == SND_PCM_FORMAT_S16_LE) ? 2 : (format == SND_PCM_FORMAT_S32_LE || format == SND_PCM_FORMAT_FLOAT_LE) ? 4 : 0);
            if (frameSize == 0) return;

            size_t totalFrames = pcm.bufferSize / frameSize;
            size_t framesWritten = 0;
            const size_t chunkFrames = low_latency ? 512 : 1024;

            while (framesWritten < totalFrames && !stop_audio.load()) {
                size_t toWrite = std::min(chunkFrames, totalFrames - framesWritten);
                snd_pcm_sframes_t written = snd_pcm_writei(handle, (const char*)data + framesWritten * frameSize, toWrite);
                if (written == -EPIPE) { 
                    if (snd_pcm_prepare(handle) < 0) break;
                    continue;
                } else if (written < 0) {
                    std::cerr << "ALSA write error\n";
                    break;
                } else framesWritten += written;
            }

            snd_pcm_drain(handle);
            snd_pcm_close(handle);
        }).detach();
        #endif
    }
    #pragma endregion
    #pragma region Device to Device
    void device_to_device(const char* input, const char* output, bool low_latency, const char* channel_name) {
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

            captureBuffer.resize(numFrames * captureChannels);

            if (captureIsFloat) {
                const float* src = reinterpret_cast<const float*>(pData);
                for (UINT32 i = 0; i < numFrames * captureChannels; ++i)
                    captureBuffer[i] = std::max(-1.0f, std::min(1.0f, src[i] * volume[channel_name]));
            } else if (wfCapture->wBitsPerSample == 16) {
                const int16_t* src16 = reinterpret_cast<const int16_t*>(pData);
                for (UINT32 i = 0; i < numFrames * captureChannels; ++i) {
                    float sample = static_cast<float>(src16[i]) / 32767.0f;
                    captureBuffer[i] = std::max(-1.0f, std::min(1.0f, sample * volume[channel_name]));
                }
            } else if (wfCapture->wBitsPerSample == 32) {
                const int32_t* src32 = reinterpret_cast<const int32_t*>(pData);
                for (UINT32 i = 0; i < numFrames * captureChannels; ++i) {
                    double sample = static_cast<double>(src32[i]) / 2147483647.0;
                    captureBuffer[i] = static_cast<float>(std::max(-1.0, std::min(1.0, sample * volume[channel_name])));
                }
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

            size_t written = 0;
            while (written < outFrames && !stop_audio.load()) {
                UINT32 padding = 0;
                renderClient->GetCurrentPadding(&padding);
                UINT32 avail = renderFrames - padding;
                if (avail == 0) {
                    Sleep(1);
                    continue;
                }

                UINT32 framesToWrite = std::min(avail, static_cast<UINT32>(outFrames - written));
                BYTE* renderPtr = nullptr;
                if (FAILED(pRender->GetBuffer(framesToWrite, &renderPtr))) break;

                if (renderIsFloat && wfRender->wBitsPerSample == 32) {
                    float* dst = reinterpret_cast<float*>(renderPtr);
                    for (UINT32 i = 0; i < framesToWrite * renderChannels; ++i) {
                        float v = toRender[written * renderChannels + i] * volume[channel_name];
                        dst[i] = std::max(-1.0f, std::min(1.0f, v));
                    }
                } else if (!renderIsFloat && wfRender->wBitsPerSample == 16) {
                    int16_t* out16 = reinterpret_cast<int16_t*>(renderPtr);
                    for (UINT32 i = 0; i < framesToWrite * renderChannels; ++i) {
                        float v = toRender[written * renderChannels + i] * volume[channel_name] * 32767.0f;
                        out16[i] = static_cast<int16_t>(std::max(-32768.0f, std::min(32767.0f, v)));
                    }
                } else if (!renderIsFloat && wfRender->wBitsPerSample == 32) {
                    int32_t* out32 = reinterpret_cast<int32_t*>(renderPtr);
                    for (UINT32 i = 0; i < framesToWrite * renderChannels; ++i) {
                        double v = toRender[written * renderChannels + i] * volume[channel_name] * 2147483647.0;
                        out32[i] = static_cast<int32_t>(std::max(static_cast<double>(INT32_MIN), std::min(static_cast<double>(INT32_MAX), v)));
                    }
                }

                if (FAILED(pRender->ReleaseBuffer(framesToWrite, 0))) break;
                written += framesToWrite;
            }

            if (toRender != captureBuffer.data())
                delete[] toRender;
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
            std::atomic<bool> haveData{false};
            std::mutex mutex;
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

        std::string input_device  = find_pa_device_by_name(pipe.context, false, input);
        std::string output_device = find_pa_device_by_name(pipe.context, true,  output);

        pa_sample_spec spec{};
        spec.format = PA_SAMPLE_FLOAT32LE;
        spec.channels = 2;
        spec.rate = low_latency ? 48000 : 44100;

        pipe.captureStream = pa_stream_new(pipe.context, "device_capture", &spec, nullptr);
        pipe.playbackStream = pa_stream_new(pipe.context, "device_playback", &spec, nullptr);

        auto capture_read_cb = [](pa_stream* s, size_t nbytes, void* userdata) {
            auto* p = reinterpret_cast<PADevicePipe*>(userdata);
            const void* ptr = nullptr;
            if (pa_stream_peek(s, &ptr, &nbytes) < 0 || !ptr || nbytes == 0) return;

            const float* fptr = reinterpret_cast<const float*>(ptr);
            size_t frames = nbytes / (sizeof(float) * p->captureChannels);
            std::lock_guard<std::mutex> lk(p->mutex);
            size_t old = p->captureBuffer.size();
            p->captureBuffer.resize(old + frames * p->captureChannels);
            memcpy(p->captureBuffer.data() + old, fptr, nbytes);
            p->haveData = true;
            pa_stream_drop(s);
        };

        pa_stream_set_read_callback(pipe.captureStream, capture_read_cb, &pipe);

        pa_stream_connect_record(pipe.captureStream, input_device.c_str(), nullptr,
            (pa_stream_flags_t)(PA_STREAM_ADJUST_LATENCY | (low_latency ? PA_STREAM_START_CORKED : 0)));

        pa_stream_connect_playback(pipe.playbackStream, output_device.c_str(), nullptr,
            (pa_stream_flags_t)(PA_STREAM_ADJUST_LATENCY | (low_latency ? PA_STREAM_START_CORKED : 0)), nullptr, nullptr);

        while (pa_stream_get_state(pipe.captureStream) != PA_STREAM_READY || pa_stream_get_state(pipe.playbackStream) != PA_STREAM_READY) {
            pa_mainloop_iterate(pipe.mainloop, 1, nullptr);
        }

        const pa_sample_spec* capSpec = pa_stream_get_sample_spec(pipe.captureStream);
        const pa_sample_spec* playSpec = pa_stream_get_sample_spec(pipe.playbackStream);
        if (capSpec) {
            pipe.captureChannels = capSpec->channels;
            pipe.captureRate = capSpec->rate;
        } else {
            pipe.captureChannels = spec.channels;
            pipe.captureRate = spec.rate;
        }

        if (playSpec) {
            pipe.playbackChannels = playSpec->channels;
            pipe.playbackRate = playSpec->rate;
        } else {
            pipe.playbackChannels = spec.channels;
            pipe.playbackRate = spec.rate;
        }

        size_t maxFrames = low_latency ? 8192 : 32768;
        pipe.captureBuffer.reserve(maxFrames * pipe.captureChannels);

        while (!stop_audio.load()) {
            pa_mainloop_iterate(pipe.mainloop, 1, nullptr);

            if (!pipe.haveData.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            std::vector<float> localBuffer;
            {
                std::lock_guard<std::mutex> lk(pipe.mutex);
                if (pipe.captureBuffer.empty()) {
                    pipe.haveData = false;
                    continue;
                }
                localBuffer.swap(pipe.captureBuffer);
                pipe.haveData = false;
            }

            size_t inFrames = localBuffer.size() / pipe.captureChannels;
            float* toRender = nullptr;
            size_t outFrames = 0;
            bool needResample = (pipe.captureRate != pipe.playbackRate) || (pipe.captureChannels != pipe.playbackChannels);

            if (needResample) {
                float* resampled = linear_resample_interleaved(localBuffer.data(), inFrames, pipe.captureChannels, pipe.captureRate, pipe.playbackRate, &outFrames);
                if (pipe.captureChannels != pipe.playbackChannels) {
                    float* remapped = remap_channels_interleaved(resampled, outFrames, pipe.captureChannels, pipe.playbackChannels);
                    delete[] resampled;
                    toRender = remapped;
                } else {
                    toRender = resampled;
                }
            } else {
                toRender = localBuffer.data();
                outFrames = inFrames;
            }

            for (size_t i = 0; i < outFrames * pipe.playbackChannels; ++i) {
                float v = toRender[i] * volume[channel_name];
                toRender[i] = std::max(-1.0f, std::min(1.0f, v));
            }

            size_t writtenFrames = 0;
            while (writtenFrames < outFrames && !stop_audio.load()) {
                size_t chunk = std::min<size_t>(
                    outFrames - writtenFrames,
                    low_latency ? 2048 : 8192
                );

                int res = pa_stream_write(
                    pipe.playbackStream,
                    toRender + writtenFrames * pipe.playbackChannels,
                    chunk * pipe.playbackChannels * sizeof(float),
                    nullptr,
                    0LL,
                    PA_SEEK_RELATIVE
                );

                if (res < 0) {
                    pa_mainloop_iterate(pipe.mainloop, 1, nullptr);
                } else {
                    writtenFrames += chunk;
                }
            }

            if (needResample && toRender != localBuffer.data())
                delete[] toRender;
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
    #pragma region App to Device
    void app_to_device(const char* input, const char* output, bool low_latency, const char* channel_name) {
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
        if (!targetPid) {
            std::cerr << "Process not found: " << input << "\n";
            CoUninitialize();
            return;
        }

        IMMDeviceEnumerator* pEnum = nullptr;
        CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnum);

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
                dev->Release();
                continue;
            }

            IAudioSessionEnumerator* sessionEnum = nullptr;
            if (FAILED(mgr2->GetSessionEnumerator(&sessionEnum)) || !sessionEnum) {
                mgr2->Release();
                dev->Release();
                continue;
            }

            int sessionCount = 0;
            sessionEnum->GetCount(&sessionCount);
            bool found = false;

            for (int s = 0; s < sessionCount; ++s) {
                IAudioSessionControl* ctrl = nullptr;
                if (FAILED(sessionEnum->GetSession(s, &ctrl)) || !ctrl) continue;

                IAudioSessionControl2* ctrl2 = nullptr;
                if (SUCCEEDED(ctrl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&ctrl2)) && ctrl2) {
                    DWORD pid = 0;
                    ctrl2->GetProcessId(&pid);
                    ctrl2->Release();
                    if (pid == targetPid) {
                        captureDevice = dev;
                        captureDevice->AddRef();
                        found = true;
                        break;
                    }
                }
                ctrl->Release();
            }

            sessionEnum->Release();
            mgr2->Release();
            dev->Release();
            if (found) break;
        }
        devicesAll->Release();

        if (!captureDevice) {
            std::cerr << "Failed to find audio session for PID\n";
            renderDevice->Release();
            pEnum->Release();
            CoUninitialize();
            return;
        }

        if (captureDevice == renderDevice) {
            std::cerr << "Capture and render device are the same. Feedback possible!\n";
            captureDevice->Release();
            renderDevice->Release();
            pEnum->Release();
            CoUninitialize();
            return;
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
                float* src = reinterpret_cast<float*>(pData);
                for (UINT32 i = 0; i < numFrames * wfCapture->nChannels; ++i)
                    captureBuffer[i] = src[i] * volume[channel_name];
            } else if (wfCapture->wBitsPerSample == 16) {
                int16_t* src16 = reinterpret_cast<int16_t*>(pData);
                for (UINT32 i = 0; i < numFrames * wfCapture->nChannels; ++i)
                    captureBuffer[i] = (src16[i] / 32768.f) * volume[channel_name];
            } else if (wfCapture->wBitsPerSample == 32) {
                int32_t* src32 = reinterpret_cast<int32_t*>(pData);
                for (UINT32 i = 0; i < numFrames * wfCapture->nChannels; ++i)
                    captureBuffer[i] = (src32[i] / 2147483648.f) * volume[channel_name];
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

        std::string output_device = find_pa_device_by_name(context, true, output);

        struct UserData {
            const char* targetApp;
            uint32_t sinkInputIndex;
            uint32_t sinkIndex;
            bool found;
        } userdata{ input, PA_INVALID_INDEX, PA_INVALID_INDEX, false };

        auto sink_input_cb = [](pa_context*, const pa_sink_input_info* i, int eol, void* userdata) {
            if (eol > 0) return;
            auto* ud = reinterpret_cast<UserData*>(userdata);
            if (!i || ud->found) return;
            const char* proc = pa_proplist_gets(i->proplist, PA_PROP_APPLICATION_PROCESS_BINARY);
            const char* name = pa_proplist_gets(i->proplist, PA_PROP_APPLICATION_NAME);
            if (proc && strcmp(proc, ud->targetApp) == 0) {
                ud->sinkInputIndex = i->index;
                ud->sinkIndex = i->sink;
                ud->found = true;
            } else if (name && strcmp(name, ud->targetApp) == 0) {
                ud->sinkInputIndex = i->index;
                ud->sinkIndex = i->sink;
                ud->found = true;
            }
        };

        pa_context_get_sink_input_info_list(context, sink_input_cb, &userdata);

        int tries = 100;
        while (!userdata.found && tries-- > 0) pa_mainloop_iterate(mainloop, 1, nullptr);

        if (!userdata.found) {
            std::cerr << "Target app not found: " << input << "\n";
            pa_context_disconnect(context);
            pa_context_unref(context);
            pa_mainloop_free(mainloop);
            return;
        }

        std::string monitorName = "";
        if (userdata.sinkIndex != PA_INVALID_INDEX) {
            struct SinkInfoData { std::string monitor; bool done = false; uint32_t idx; };
            SinkInfoData sid{ "", false, userdata.sinkIndex };

            pa_context_get_sink_info_by_index(context, userdata.sinkIndex,
                [](pa_context*, const pa_sink_info* info, int eol, void* userdata) {
                    if (eol > 0) return;
                    auto* d = reinterpret_cast<SinkInfoData*>(userdata);
                    if (info && info->monitor_source)
                        d->monitor = info->monitor_source;
                    d->done = true;
                }, &sid);

            int w = 100;
            while (!sid.done && w-- > 0) pa_mainloop_iterate(mainloop, 1, nullptr);
            if (!sid.monitor.empty()) monitorName = sid.monitor;
        }

        if (monitorName.empty()) {
            monitorName = "@DEFAULT_MONITOR@";
        }

        pa_sample_spec appSpec{};
        appSpec.format = PA_SAMPLE_FLOAT32LE;
        appSpec.rate = low_latency ? 48000 : 44100;
        appSpec.channels = 2;

        pa_stream* inputStream = pa_stream_new(context, "app_capture", &appSpec, nullptr);
        pa_stream* outputStream = pa_stream_new(context, "playback", &appSpec, nullptr);

        struct AudioData {
            pa_mainloop* mainloop;
            pa_stream* inputStream;
            pa_stream* outputStream;
            std::vector<float> captureBuffer;
            unsigned int inputChannels;
            unsigned int outputChannels;
            unsigned int inputRate;
            unsigned int outputRate;
            std::mutex mutex;
            std::atomic<bool> haveData{false};
        } audioData{ mainloop, inputStream, outputStream, std::vector<float>(), (unsigned)appSpec.channels, (unsigned)appSpec.channels, (unsigned)appSpec.rate, (unsigned)appSpec.rate };

        auto stream_read_cb = [](pa_stream* s, size_t nbytes, void* userdata) {
            auto* data = reinterpret_cast<AudioData*>(userdata);
            const void* ptr = nullptr;
            if (pa_stream_peek(s, &ptr, &nbytes) < 0 || !ptr || nbytes == 0) return;

            size_t frames = nbytes / (sizeof(float) * data->inputChannels);
            std::lock_guard<std::mutex> lk(data->mutex);
            size_t old = data->captureBuffer.size();
            data->captureBuffer.resize(old + frames * data->inputChannels);
            memcpy(data->captureBuffer.data() + old, ptr, nbytes);
            data->haveData = true;
            pa_stream_drop(s);
        };

        pa_stream_set_read_callback(inputStream, stream_read_cb, &audioData);

        pa_stream_connect_record(inputStream, monitorName.c_str(), nullptr,
            (pa_stream_flags_t)(PA_STREAM_ADJUST_LATENCY | (low_latency ? PA_STREAM_START_CORKED : 0)));

        pa_stream_connect_playback(outputStream, output_device.c_str(), nullptr,
            (pa_stream_flags_t)(PA_STREAM_ADJUST_LATENCY | (low_latency ? PA_STREAM_START_CORKED : 0)), nullptr, nullptr);

        while (pa_stream_get_state(inputStream) != PA_STREAM_READY || pa_stream_get_state(outputStream) != PA_STREAM_READY) {
            pa_mainloop_iterate(mainloop, 1, nullptr);
        }

        const pa_sample_spec* inSpec = pa_stream_get_sample_spec(inputStream);
        const pa_sample_spec* outSpec = pa_stream_get_sample_spec(outputStream);
        if (inSpec) {
            audioData.inputChannels = inSpec->channels;
            audioData.inputRate = inSpec->rate;
        }
        if (outSpec) {
            audioData.outputChannels = outSpec->channels;
            audioData.outputRate = outSpec->rate;
        }

        while (!stop_audio.load()) {
            pa_mainloop_iterate(mainloop, 1, nullptr);

            if (!audioData.haveData.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            std::vector<float> local;
            {
                std::lock_guard<std::mutex> lk(audioData.mutex);
                if (audioData.captureBuffer.empty()) { audioData.haveData = false; continue; }
                local.swap(audioData.captureBuffer);
                audioData.haveData = false;
            }

            size_t inFrames = local.size() / audioData.inputChannels;
            float* bufferToRender = nullptr;
            size_t outFrames = 0;
            bool needResample = (audioData.inputRate != audioData.outputRate) || (audioData.inputChannels != audioData.outputChannels);

            if (needResample) {
                float* resampled = linear_resample_interleaved(local.data(), inFrames, audioData.inputChannels, audioData.inputRate, audioData.outputRate, &outFrames);
                if (audioData.inputChannels != audioData.outputChannels) {
                    float* remapped = remap_channels_interleaved(resampled, outFrames, audioData.inputChannels, audioData.outputChannels);
                    delete[] resampled;
                    bufferToRender = remapped;
                } else {
                    bufferToRender = resampled;
                }
            } else {
                bufferToRender = local.data();
                outFrames = inFrames;
            }

            for (size_t i = 0; i < outFrames * audioData.outputChannels; ++i) {
                bufferToRender[i] = std::max(-1.0f, std::min(1.0f, bufferToRender[i] * volume[channel_name]));
            }

            size_t written = 0;
            while (written < outFrames && !stop_audio.load()) {
                size_t chunkFrames = std::min(outFrames - written, static_cast<size_t>(low_latency ? 2048 : 8192));
                pa_stream_write(outputStream, bufferToRender + written * audioData.outputChannels, chunkFrames * audioData.outputChannels * sizeof(float),
                    nullptr, 0LL, PA_SEEK_RELATIVE);
                written += chunkFrames;
                pa_mainloop_iterate(mainloop, 0, nullptr);
            }

            if (needResample && bufferToRender != local.data())
                delete[] bufferToRender;
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
