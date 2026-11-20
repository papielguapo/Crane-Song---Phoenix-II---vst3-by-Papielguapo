// ============================================================================
// PhoenixDSP.h - 1:1 Bit-Exact DSP Engine Cloned from Crane Song Phoenix II
// Extracted from PhoenixII.aaxplugin (0x180010380..0x1800109CF) & PhoenixToo_Stereo.dll
// Plugin Creator / Developer: papielguapo
// ============================================================================
#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>
#include "PhoenixDSPData.h"

class PhoenixIIDSP {
public:
    // Parameters matching PhoenixPages_v4.xml & Phoenix_Parameters
    float inputTrimDB  = 0.0f;   // -10.0 to +10.0 dB
    float processPct   = 35.0f;  // 0.0 to 100.0 %
    float outputTrimDB = 0.0f;   // -6.0 to +6.0 dB
    int   brightness   = 2;      // 1 = Opal, 2 = Gold, 3 = Sapphire
    int   colorType    = 1;      // 1 = Luminescent, 2 = Iridescent, 3 = Radiant, 4 = Luster, 5 = Dark Essence
    bool  masterBypass = false;

    double sampleRate  = 48000.0;

    // Per-channel filter state (matches [rsp+D8h] state_x1 and [rsp+E0h] state_y1)
    float stateX1[2] = { 0.0f, 0.0f };
    float stateY1[2] = { 0.0f, 0.0f };

    void reset() {
        stateX1[0] = stateX1[1] = 0.0f;
        stateY1[0] = stateY1[1] = 0.0f;
    }

    void setSampleRate(double sr) {
        if (sr > 0.0) sampleRate = sr;
    }

    // 1:1 Table Lookup with 19-bit fractional interpolation (VA 0x1800108D0..0x1800109CE)
    static inline float lookupPhxTbl(float val, int32_t tableOffset) {
        if (val > 0.999755859f) val = 0.999755859f; // Prevent int32 overflow at +1.0f (idx max = 4095)
        if (val < -1.0f) val = -1.0f;

        int32_t fixed = static_cast<int32_t>(val * 2147483648.0f);
        int32_t intPart = fixed >> 19; // arithmetic shift right 19 bits -> [-4096 .. +4095]
        int32_t idx = intPart + tableOffset + 4096;
        if (idx < tableOffset) idx = tableOffset;
        if (idx > tableOffset + 8190) idx = tableOffset + 8190;

        float y0 = kPhxTblData[idx];
        float y1 = kPhxTblData[idx + 1];
        float frac = static_cast<float>(fixed & 0x7FFFF) * 1.9073486328125e-06f; // 1.0f / 524288.0f
        return y0 + frac * (y1 - y0);
    }

    // 1:1 Stereo Process Block (VA 0x180010380..0x1800108CE)
    void processStereo(const float* inL, const float* inR, float* outL, float* outR, int numSamples) {
        if (masterBypass) {
            for (int i = 0; i < numSamples; ++i) {
                outL[i] = inL[i];
                outR[i] = inR ? inR[i] : inL[i];
            }
            return;
        }

        // Convert UI parameters to algorithm packet context (VA 0x18001AC40..0x18001ACC0)
        const float inTrimGain  = std::pow(10.0f, inputTrimDB / 20.0f);
        const float processVal  = std::clamp(processPct, 0.0f, 100.0f) / 100.0f;
        const float outTrimGain = std::pow(10.0f, outputTrimDB / 20.0f);

        int srIndex = 0;
        if (sampleRate >= 150000.0) srIndex = 2;
        else if (sampleRate >= 75000.0) srIndex = 1;
        const float srFactor = kSrFactorsTable[srIndex];

        const int tIdx = std::clamp(colorType, 1, 5);
        const int bIdx = std::clamp(brightness, 1, 3);

        // isType4 = (tIdx == 4) ? 1 : 0 (Luster mode special scaling at VA 0x18001045D)
        const int isType4 = (tIdx == 4) ? 1 : 0;
        const float type4Factor = 1.0f - static_cast<float>(isType4) * 0.5f;

        const PhoenixColorType& color = kPhoenixColorTypes[tIdx - 1];
        const float coeffPre  = color.brightCoeffs[bIdx - 1][0];
        const float coeffPost = color.brightCoeffs[bIdx - 1][1];

        const float scaledProcess = processVal * color.driveScale;
        const float effProcess    = scaledProcess * type4Factor;

        const float* inputs[2] = { inL, inR ? inR : inL };
        float* outputs[2]      = { outL, outR ? outR : outL };
        const int numChannels  = (inR && outR) ? 2 : 1;

        for (int ch = 0; ch < numChannels; ++ch) {
            float sx1 = stateX1[ch];
            float sy1 = stateY1[ch];
            const float* src = inputs[ch];
            float* dst       = outputs[ch];

            for (int i = 0; i < numSamples; ++i) {
                const float prevY1   = sy1;
                const float inSample = src[i] * inTrimGain;
                float x              = inSample;

                // High-frequency pre-emphasis filter (VA 0x180010668..0x1800106AD)
                x = (x * (coeffPre * srFactor)) + (x - sx1);
                x = x + (x * color.preEmphasisGain);

                // Base signal selection via color.flagA (VA 0x1800106B8..0x1800106FF)
                const float baseSig = (color.flagA != 0) ? x : inSample;

                // Type 4 pre-lookup scaling (VA 0x180010705..0x180010752)
                if (isType4 != 0) {
                    x = x * scaledProcess;
                }

                // First nonlinear waveshaper table lookup (VA 0x180010766)
                x = lookupPhxTbl(x, color.tableOffset);

                // Harmonic saturation mix + base signal (VA 0x180010777..0x1800107A8)
                x = (x * scaledProcess * color.satMixGain) + baseSig;

                // Second nonlinear waveshaper table lookup (VA 0x1800107BC)
                x = lookupPhxTbl(x, color.tableOffset);

                // De-emphasis / tape head reconstruction filter (VA 0x1800107CD..0x180010821)
                x = ((x - prevY1) * (coeffPost * srFactor)) + prevY1;
                sy1 = x;

                // Dry level dynamic compensation & output trim (VA 0x180010825..0x1800108A1)
                const float dryComp = inSample + (-1.0f * effProcess * inSample * color.dryCompGain);
                const float outSample = ((x * effProcess) + dryComp) * outTrimGain;

                dst[i] = outSample;
                sx1 = inSample;
            }

            stateX1[ch] = sx1;
            stateY1[ch] = sy1;
        }
    }
};
