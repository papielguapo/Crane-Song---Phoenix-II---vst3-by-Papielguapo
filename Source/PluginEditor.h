// ============================================================================
// PluginEditor.h - 1:1 Pixel-Perfect 515x161 Crane Song Phoenix II Editor
// Double-click on CRANE SONG logo toggles the 515x161 Skinned Deck + Meters & Visor
// Plugin Creator / Developer: papielguapo
// ============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class PhoenixIIAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer {
public:
    explicit PhoenixIIAudioProcessorEditor(PhoenixIIAudioProcessor&);
    ~PhoenixIIAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override;

    float getParamNorm(int paramIdx) const;
    void setParamNorm(int paramIdx, float norm);
    void resetParam(int paramIdx);
    int hitTestKnob(juce::Point<int> pt) const;

    void drawNative1to1View(juce::Graphics& g);
    void drawCompactSkinnedView(juce::Graphics& g, int skinId);
    void drawCompactKnob(juce::Graphics& g, juce::Rectangle<float> box, int paramIdx, float norm, int skinId);

    PhoenixIIAudioProcessor& processorRef;

    juce::Image imgFaceplate;
    juce::Image imgKnobCont;
    juce::Image imgKnobBright;
    juce::Image imgKnobType;

    // false = 1:1 Hardware Faceplate (Image 1: 515x161)
    // true  = 515x161 Adapted Skinned Deck with Meters & Visor (Image 2)
    bool showSkinnedView = false;

    // 1:1 Hardware view hitboxes (515x161)
    juce::Rectangle<int> hwKnobRects[5];
    juce::Rectangle<int> hwReadoutRects[5];
    juce::Rectangle<int> hwBrightHotspots[3];
    juce::Rectangle<int> hwTypeHotspots[5];
    juce::Rectangle<int> craneSongLogoRect;

    // Compact 515x161 Skinned view hitboxes
    juce::Rectangle<int> skinTabRects[7];
    juce::Rectangle<int> closeTo1to1Rect;
    juce::Rectangle<int> deckKnobRects[5];
    juce::Rectangle<int> deckBrightPills[3];
    juce::Rectangle<int> deckTypePills[5];
    juce::Rectangle<int> visorRect;

    int activeDragParam = -1;
    int dragStartY = 0;
    float dragStartNorm = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhoenixIIAudioProcessorEditor)
};

