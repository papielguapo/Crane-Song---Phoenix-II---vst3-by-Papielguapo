// ============================================================================
// PluginProcessor.cpp - 1:1 Crane Song Phoenix II AudioProcessor
// Plugin Creator / Developer: papielguapo
// ============================================================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

struct PresetDef {
    const char* name;
    float inputTrim;
    float process;
    float outputTrim;
    int brightness; // 0=Opal, 1=Gold, 2=Sapphire
    int type;       // 0=Luminescent, 1=Iridescent, 2=Radiant, 3=Luster, 4=Dark Essence
};

static const PresetDef kFactoryPresets[7] = {
    { "00 - Default (Gold / Luminescent)",       0.0f,  35.0f,  0.0f,  1, 0 },
    { "01 - Vocal Silk (Opal / Iridescent)",     1.2f,  48.0f, -0.8f,  0, 1 },
    { "02 - Drum Bus Punch (Sapphire / Radiant)",3.5f,  75.0f, -2.2f,  2, 2 },
    { "03 - Master Tape Glue (Gold / Luster)",   0.5f,  42.0f, -0.4f,  1, 3 },
    { "04 - Vintage 15 IPS (Opal / Dark Essence)",2.0f, 68.0f, -1.5f,  0, 4 },
    { "05 - 30 IPS Air & Sheen (Sapphire / Lumin)",0.8f,52.0f, -0.6f,  2, 0 },
    { "06 - Hot Transformer Drive (Gold / Radiant)",5.5f,88.0f,-3.5f,  1, 2 }
};

PhoenixIIAudioProcessor::PhoenixIIAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PhoenixIIParameters", createParameterLayout())
{
}

PhoenixIIAudioProcessor::~PhoenixIIAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout PhoenixIIAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "inputTrim", 1 }, "Input Trim",
        juce::NormalisableRange<float>(-10.0f, 10.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "process", 1 }, "Process",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "outputTrim", 1 }, "Output Trim",
        juce::NormalisableRange<float>(-6.0f, 6.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "brightness", 1 }, "Brightness",
        juce::StringArray { "Opal", "Gold", "Sapphire" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "type", 1 }, "Type",
        juce::StringArray { "Luminescent", "Iridescent", "Radiant", "Luster", "Dark Essence" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "bypass", 1 }, "Master Bypass", false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "skin", 1 }, "Skin",
        juce::StringArray { "Crane Song Phoenix", "FL Studio", "Pro Tools", "Ableton", "Logic Pro", "Win 98", "Atari PC" }, 5));

    return { params.begin(), params.end() };
}

void PhoenixIIAudioProcessor::prepareToPlay(double sampleRate, int) {
    dsp.setSampleRate(sampleRate);
    dsp.reset();
}

void PhoenixIIAudioProcessor::releaseResources() {}

bool PhoenixIIAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto outSet = layouts.getMainOutputChannelSet();
    const auto inSet  = layouts.getMainInputChannelSet();
    if (outSet != juce::AudioChannelSet::mono() && outSet != juce::AudioChannelSet::stereo())
        return false;
    return outSet == inSet;
}

void PhoenixIIAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples             = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    if (numSamples <= 0 || totalNumInputChannels <= 0) return;

    const float inTrimDB  = apvts.getRawParameterValue("inputTrim")->load();
    const float procPct   = apvts.getRawParameterValue("process")->load();
    const float outTrimDB = apvts.getRawParameterValue("outputTrim")->load();
    const int brightIdx   = static_cast<int>(apvts.getRawParameterValue("brightness")->load()) + 1; // 1..3
    const int typeIdx     = static_cast<int>(apvts.getRawParameterValue("type")->load()) + 1;       // 1..5
    const bool bypassed   = apvts.getRawParameterValue("bypass")->load() >= 0.5f;

    dsp.inputTrimDB  = inTrimDB;
    dsp.processPct   = procPct;
    dsp.outputTrimDB = outTrimDB;
    dsp.brightness   = brightIdx;
    dsp.colorType    = typeIdx;
    dsp.masterBypass = bypassed;

    float inPk = 0.0f;
    const float inGain = std::pow(10.0f, inTrimDB / 20.0f);
    for (int ch = 0; ch < totalNumInputChannels; ++ch) {
        const float* r = buffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            float a = std::fabs(r[i] * inGain);
            if (a > inPk) inPk = a;
        }
    }

    float* ch0 = buffer.getWritePointer(0);
    float* ch1 = (totalNumInputChannels > 1) ? buffer.getWritePointer(1) : nullptr;
    dsp.processStereo(ch0, ch1, ch0, ch1, numSamples);

    float outPk = 0.0f;
    for (int ch = 0; ch < totalNumInputChannels; ++ch) {
        const float* r = buffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            float a = std::fabs(r[i]);
            if (a > outPk) outPk = a;
        }
    }

    meterInPeak.store(std::max(inPk, meterInPeak.load() * 0.88f));
    meterOutPeak.store(std::max(outPk, meterOutPeak.load() * 0.88f));
    const float drv = bypassed ? 0.0f : std::min(100.0f, procPct * 0.72f + inPk * 38.0f);
    meterDrive.store(std::max(drv, meterDrive.load() * 0.88f));
}

const juce::String PhoenixIIAudioProcessor::getProgramName(int index) {
    if (index >= 0 && index < 7) return kFactoryPresets[index].name;
    return "Default";
}

void PhoenixIIAudioProcessor::setCurrentProgram(int index) {
    if (index < 0 || index >= 7) return;
    currentPresetIndex = index;
    const auto& p = kFactoryPresets[index];
    if (auto* prm = apvts.getParameter("inputTrim"))
        prm->setValueNotifyingHost(prm->convertTo0to1(p.inputTrim));
    if (auto* prm = apvts.getParameter("process"))
        prm->setValueNotifyingHost(prm->convertTo0to1(p.process));
    if (auto* prm = apvts.getParameter("outputTrim"))
        prm->setValueNotifyingHost(prm->convertTo0to1(p.outputTrim));
    if (auto* prm = apvts.getParameter("brightness"))
        prm->setValueNotifyingHost(prm->convertTo0to1(static_cast<float>(p.brightness)));
    if (auto* prm = apvts.getParameter("type"))
        prm->setValueNotifyingHost(prm->convertTo0to1(static_cast<float>(p.type)));
}

void PhoenixIIAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PhoenixIIAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor* PhoenixIIAudioProcessor::createEditor() {
    return new PhoenixIIAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new PhoenixIIAudioProcessor();
}
