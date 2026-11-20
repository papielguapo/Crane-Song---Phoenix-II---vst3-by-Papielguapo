// ============================================================================
// PluginProcessor.h - JUCE AudioProcessor for Crane Song Phoenix II 1:1 Clone
// Plugin Creator / Developer: papielguapo
// ============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "../PhoenixDSP.h"

class PhoenixIIAudioProcessor : public juce::AudioProcessor {
public:
    PhoenixIIAudioProcessor();
    ~PhoenixIIAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Phoenix II (papielguapo)"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 7; }
    int getCurrentProgram() override { return currentPresetIndex; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;
    PhoenixIIDSP dsp;

    std::atomic<float> meterInPeak  { 0.0f };
    std::atomic<float> meterOutPeak { 0.0f };
    std::atomic<float> meterDrive   { 0.0f };

private:
    int currentPresetIndex = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhoenixIIAudioProcessor)
};
