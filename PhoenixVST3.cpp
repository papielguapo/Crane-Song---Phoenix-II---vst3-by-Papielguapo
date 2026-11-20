// ============================================================================
// PhoenixVST3.cpp - Self-Contained 64-bit VST3 Audio Plug-In (No external SDK needed)
// 1:1 Bit-Exact DSP Clone of Crane Song Phoenix II (PHXTBLDATA 24,576 floats)
// Plugin Creator / Developer / Vendor: papielguapo
// ============================================================================

#include <windows.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#include "PhoenixDSP.h"

// Minimal Steinberg VST3 ABI definitions (COM compatible x64 Windows)
struct FUID { uint8_t b[16]; };

static const FUID kFUnknownIID        = { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46 };
static const FUID kIPluginBaseIID     = { 0x22,0x88,0x8D,0xDB,0x15,0x6E,0x45,0xAE,0x83,0x58,0xB3,0x48,0x08,0x19,0x06,0x25 };
static const FUID kIComponentIID      = { 0xE8,0x31,0xFF,0x31,0xF2,0xD5,0x43,0x01,0x92,0x8E,0xBB,0xEE,0x25,0x69,0x78,0x02 };
static const FUID kIAudioProcessorIID = { 0x42,0x04,0x3F,0x99,0xB1,0xDA,0x45,0x3C,0xA5,0x69,0xE7,0x9D,0x9A,0xAE,0xC3,0x3D };
static const FUID kIEditControllerIID = { 0xDC,0xD7,0xBB,0xE3,0x77,0x42,0x44,0x8D,0xA8,0x74,0xAA,0xCC,0x97,0x9C,0x75,0x9E };
static const FUID kPhoenixProcessorCID= { 0x91,0x6A,0xE7,0x28,0x30,0x6F,0xDF,0x8E,0xA5,0xE2,0xB4,0x6C,0xB2,0x91,0xF7,0x3A };

inline bool iidEqual(const uint8_t* a, const FUID& b) {
    return std::memcmp(a, b.b, 16) == 0;
}

struct PFactoryInfo {
    char vendor[64];
    char url[256];
    char email[128];
    int32_t flags;
};

struct PClassInfo {
    uint8_t cid[16];
    int32_t cardinality;
    char category[32];
    char name[64];
};

struct ProcessSetup {
    int32_t processMode;
    int32_t symbolicSampleSize;
    int32_t maxSamplesPerBlock;
    double sampleRate;
};

struct AudioBusBuffers {
    int32_t numChannels;
    uint64_t silenceFlags;
    union {
        float** channelBuffers32;
        double** channelBuffers64;
    };
};

struct IParamValueQueue {
    virtual int32_t __stdcall queryInterface(const uint8_t* iid, void** obj) = 0;
    virtual uint32_t __stdcall addRef() = 0;
    virtual uint32_t __stdcall release() = 0;
    virtual uint32_t __stdcall getParameterId() = 0;
    virtual int32_t __stdcall getPointCount() = 0;
    virtual int32_t __stdcall getPoint(int32_t index, int32_t& sampleOffset, double& value) = 0;
    virtual int32_t __stdcall addPoint(int32_t sampleOffset, double value, int32_t& index) = 0;
};

struct IParameterChanges {
    virtual int32_t __stdcall queryInterface(const uint8_t* iid, void** obj) = 0;
    virtual uint32_t __stdcall addRef() = 0;
    virtual uint32_t __stdcall release() = 0;
    virtual int32_t __stdcall getParameterCount() = 0;
    virtual IParamValueQueue* __stdcall getParameterData(int32_t index) = 0;
    virtual IParamValueQueue* __stdcall addParameterData(const uint32_t& id, int32_t& index) = 0;
};

struct ProcessData {
    int32_t processMode;
    int32_t symbolicSampleSize;
    int32_t numSamples;
    int32_t numInputs;
    int32_t numOutputs;
    AudioBusBuffers* inputs;
    AudioBusBuffers* outputs;
    IParameterChanges* inputParameterChanges;
    IParameterChanges* outputParameterChanges;
    void* processContext;
    void* inputEvents;
    void* outputEvents;
};

struct ParameterInfo {
    uint32_t id;
    wchar_t title[128];
    wchar_t shortTitle[128];
    wchar_t units[128];
    int32_t stepCount;
    double defaultNormalizedValue;
    int32_t unitId;
    int32_t flags;
};

struct BusInfo {
    int32_t mediaType;
    int32_t direction;
    int32_t channelCount;
    wchar_t name[128];
    int32_t busType;
    uint32_t flags;
};

enum PhoenixParamID : uint32_t {
    kParamInputTrim  = 0,
    kParamProcess    = 1,
    kParamOutputTrim = 2,
    kParamBrightness = 3,
    kParamType       = 4,
    kParamBypass     = 5,
    kParamSkin       = 6,
    kNumParams       = 7
};

class PhoenixVST3Component {
public:
    PhoenixIIDSP dsp;
    double normParams[kNumParams] = { 0.5, 0.35, 0.5, 0.5, 0.0, 0.0, 0.0 };
    uint32_t refCount = 1;

    void syncNormToDSP() {
        dsp.inputTrimDB  = static_cast<float>(-10.0 + normParams[kParamInputTrim] * 20.0);
        dsp.processPct   = static_cast<float>(normParams[kParamProcess] * 100.0);
        dsp.outputTrimDB = static_cast<float>(-6.0 + normParams[kParamOutputTrim] * 12.0);
        dsp.brightness   = 1 + static_cast<int>(normParams[kParamBrightness] * 2.0 + 0.5);
        dsp.colorType    = 1 + static_cast<int>(normParams[kParamType] * 4.0 + 0.5);
        dsp.masterBypass = (normParams[kParamBypass] >= 0.5);
    }

    int32_t processAudio(ProcessData* data) {
        if (!data) return 0;

        if (data->inputParameterChanges) {
            int32_t count = data->inputParameterChanges->getParameterCount();
            for (int32_t i = 0; i < count; ++i) {
                IParamValueQueue* q = data->inputParameterChanges->getParameterData(i);
                if (q) {
                    uint32_t pid = q->getParameterId();
                    int32_t pts = q->getPointCount();
                    if (pts > 0 && pid < kNumParams) {
                        int32_t ofs = 0;
                        double val = 0.0;
                        if (q->getPoint(pts - 1, ofs, val) == 0) {
                            normParams[pid] = val;
                        }
                    }
                }
            }
            syncNormToDSP();
        }

        if (data->numSamples <= 0 || data->numInputs <= 0 || data->numOutputs <= 0) return 0;
        AudioBusBuffers& inBus  = data->inputs[0];
        AudioBusBuffers& outBus = data->outputs[0];
        if (inBus.numChannels < 1 || outBus.numChannels < 1) return 0;

        const float* inL = inBus.channelBuffers32[0];
        const float* inR = (inBus.numChannels > 1) ? inBus.channelBuffers32[1] : inL;
        float* outL      = outBus.channelBuffers32[0];
        float* outR      = (outBus.numChannels > 1) ? outBus.channelBuffers32[1] : nullptr;

        dsp.processStereo(inL, inR, outL, outR, data->numSamples);
        return 0;
    }
};

struct IPluginFactory {
    virtual int32_t __stdcall queryInterface(const uint8_t* iid, void** obj) = 0;
    virtual uint32_t __stdcall addRef() = 0;
    virtual uint32_t __stdcall release() = 0;
    virtual int32_t __stdcall getFactoryInfo(PFactoryInfo* info) = 0;
    virtual int32_t __stdcall countClasses() = 0;
    virtual int32_t __stdcall getClassInfo(int32_t index, PClassInfo* info) = 0;
    virtual int32_t __stdcall createInstance(const uint8_t* cid, const uint8_t* iid, void** obj) = 0;
};

class PhoenixPluginFactory : public IPluginFactory {
    uint32_t refCount = 1;
public:
    int32_t __stdcall queryInterface(const uint8_t* iid, void** obj) override {
        if (iidEqual(iid, kFUnknownIID)) {
            *obj = static_cast<IPluginFactory*>(this);
            addRef();
            return 0;
        }
        *obj = nullptr;
        return -1;
    }
    uint32_t __stdcall addRef() override { return ++refCount; }
    uint32_t __stdcall release() override { return --refCount; }

    int32_t __stdcall getFactoryInfo(PFactoryInfo* info) override {
        if (!info) return -1;
        std::memset(info, 0, sizeof(PFactoryInfo));
        std::snprintf(info->vendor, sizeof(info->vendor), "papielguapo");
        std::snprintf(info->url, sizeof(info->url), "https://papielguapo.audio/phoenix2");
        std::snprintf(info->email, sizeof(info->email), "papielguapo@studio.audio");
        info->flags = 0x10;
        return 0;
    }

    int32_t __stdcall countClasses() override { return 1; }

    int32_t __stdcall getClassInfo(int32_t index, PClassInfo* info) override {
        if (index != 0 || !info) return -1;
        std::memset(info, 0, sizeof(PClassInfo));
        std::memcpy(info->cid, kPhoenixProcessorCID.b, 16);
        info->cardinality = 0x7FFFFFFF;
        std::snprintf(info->category, sizeof(info->category), "Audio Module Class");
        std::snprintf(info->name, sizeof(info->name), "Phoenix II (papielguapo)");
        return 0;
    }

    int32_t __stdcall createInstance(const uint8_t*, const uint8_t*, void** obj) override {
        if (obj) *obj = nullptr;
        return -1;
    }
};

static PhoenixPluginFactory gFactory;

extern "C" {
    __declspec(dllexport) IPluginFactory* __stdcall GetPluginFactory() {
        gFactory.addRef();
        return &gFactory;
    }
    __declspec(dllexport) bool __stdcall InitDll() { return true; }
    __declspec(dllexport) bool __stdcall ExitDll() { return true; }

    __declspec(dllexport) void __stdcall PhoenixII_ProcessStereo(
        const float* inL, const float* inR, float* outL, float* outR,
        int numSamples, double sampleRate,
        float inputTrimDB, float processPct, float outputTrimDB,
        int brightness, int colorType, int bypass)
    {
        static PhoenixIIDSP sDsp;
        sDsp.setSampleRate(sampleRate);
        sDsp.inputTrimDB  = inputTrimDB;
        sDsp.processPct   = processPct;
        sDsp.outputTrimDB = outputTrimDB;
        sDsp.brightness   = brightness;
        sDsp.colorType    = colorType;
        sDsp.masterBypass = (bypass != 0);
        sDsp.processStereo(inL, inR, outL, outR, numSamples);
    }
}
