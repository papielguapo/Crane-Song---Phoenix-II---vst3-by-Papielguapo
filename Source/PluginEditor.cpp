// ============================================================================
// PluginEditor.cpp - 1:1 Pixel-Perfect 515x161 Crane Song Phoenix II Editor
// Image 1: Exact 515x161 Hardware Faceplate with 5 black value boxes
// Image 2: Double-click on CRANE SONG logo toggles 515x161 Skinned View + Meters & Visor
// Plugin Creator / Developer: papielguapo
// ============================================================================
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>

static const char* kSkinCodes[7]   = { "CS", "FL", "PT", "AB", "LP", "98", "AT" };
static const char* kBrightNames[3] = { "Opal", "Gold", "Sapphire" };
static const char* kTypeNames[5]   = { "Luminescent", "Iridescent", "Radiant", "Luster", "Dark Essence" };
static const char* kTypeShort[5]   = { "Lumin", "Irid", "Radiant", "Luster", "DrkEs" };
static const char* kParamTitles[5] = { "INPUT TRIM", "PROCESS", "OUTPUT TRIM", "BRIGHTNESS", "TYPE (COLOR)" };
static const char* kParamIds[7]    = { "inputTrim", "process", "outputTrim", "brightness", "type", "bypass", "skin" };

PhoenixIIAudioProcessorEditor::PhoenixIIAudioProcessorEditor(PhoenixIIAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    imgFaceplate = juce::ImageCache::getFromMemory(
        BinaryData::res_PNG_31011_515x161_png, BinaryData::res_PNG_31011_515x161_pngSize);
    imgKnobCont = juce::ImageCache::getFromMemory(
        BinaryData::res_PNG_31010_60x8100_png, BinaryData::res_PNG_31010_60x8100_pngSize);
    imgKnobBright = juce::ImageCache::getFromMemory(
        BinaryData::res_PNG_31013_60x180_png, BinaryData::res_PNG_31013_60x180_pngSize);
    imgKnobType = juce::ImageCache::getFromMemory(
        BinaryData::res_PNG_31015_60x300_png, BinaryData::res_PNG_31015_60x300_pngSize);

    // Exact 1:1 dimensions from Image 1 (515 x 161 pixels)
    setSize(515, 161);
    startTimerHz(30);
}

PhoenixIIAudioProcessorEditor::~PhoenixIIAudioProcessorEditor() {
    stopTimer();
}

void PhoenixIIAudioProcessorEditor::timerCallback() {
    repaint();
}

float PhoenixIIAudioProcessorEditor::getParamNorm(int paramIdx) const {
    if (paramIdx < 0 || paramIdx >= 7) return 0.0f;
    if (auto* prm = processorRef.apvts.getParameter(kParamIds[paramIdx]))
        return prm->getValue();
    return 0.0f;
}

void PhoenixIIAudioProcessorEditor::setParamNorm(int paramIdx, float norm) {
    if (paramIdx < 0 || paramIdx >= 7) return;
    float clamped = juce::jlimit(0.0f, 1.0f, norm);
    if (auto* prm = processorRef.apvts.getParameter(kParamIds[paramIdx]))
        prm->setValueNotifyingHost(clamped);
    repaint();
}

void PhoenixIIAudioProcessorEditor::resetParam(int paramIdx) {
    if (paramIdx < 0 || paramIdx >= 7) return;
    if (auto* prm = processorRef.apvts.getParameter(kParamIds[paramIdx]))
        prm->setValueNotifyingHost(prm->getDefaultValue());
    repaint();
}

void PhoenixIIAudioProcessorEditor::resized() {
    // 1. Exact 1:1 Hardware Coordinates on 515x161 canvas (from binary VA 0x1803ABDD8..0x1803ABE18)
    const int knobX[5] = { 19, 109, 199, 289, 379 };
    for (int i = 0; i < 5; ++i) {
        hwKnobRects[i]    = juce::Rectangle<int>(knobX[i], 41, 60, 60);
        hwReadoutRects[i] = juce::Rectangle<int>(knobX[i], 132, 60, 19);
    }

    hwBrightHotspots[0] = juce::Rectangle<int>(268, 25, 34, 17);
    hwBrightHotspots[1] = juce::Rectangle<int>(301, 19, 34, 17);
    hwBrightHotspots[2] = juce::Rectangle<int>(335, 25, 55, 17);

    for (int i = 0; i < 5; ++i) {
        hwTypeHotspots[i] = juce::Rectangle<int>(438, 34 + i * 16, 74, 15);
    }

    craneSongLogoRect = juce::Rectangle<int>(442, 126, 71, 33);

    // 2. Second View (Image 2 adapted to 515x161) coordinates
    for (int i = 0; i < 7; ++i) {
        skinTabRects[i] = juce::Rectangle<int>(310 + i * 21, 2, 19, 13);
    }
    closeTo1to1Rect = juce::Rectangle<int>(460, 2, 52, 13);

    const int colW = 101;
    for (int i = 0; i < 5; ++i) {
        int modX = 4 + i * colW;
        int knobSz = (i < 3) ? 44 : 32;
        int knobY  = (i < 3) ? 31 : 29;
        deckKnobRects[i] = juce::Rectangle<int>(modX + (colW - knobSz) / 2, knobY, knobSz, knobSz);
    }

    int brX = 4 + 3 * colW + 6;
    for (int i = 0; i < 3; ++i) {
        deckBrightPills[i] = juce::Rectangle<int>(brX, 74 + i * 12, colW - 14, 11);
    }

    int tyX = 4 + 4 * colW + 6;
    for (int i = 0; i < 5; ++i) {
        deckTypePills[i] = juce::Rectangle<int>(tyX, 72 + i * 8, colW - 14, 7);
    }

    visorRect = juce::Rectangle<int>(322, 121, 187, 36);
}

int PhoenixIIAudioProcessorEditor::hitTestKnob(juce::Point<int> pt) const {
    for (int i = 0; i < 5; ++i) {
        if (!showSkinnedView && hwKnobRects[i].contains(pt)) return i;
        if (showSkinnedView && deckKnobRects[i].contains(pt)) return i;
    }
    return -1;
}

void PhoenixIIAudioProcessorEditor::mouseDown(const juce::MouseEvent& e) {
    auto pt = e.getPosition();

    if (!showSkinnedView) {
        for (int i = 0; i < 3; ++i) {
            if (hwBrightHotspots[i].contains(pt)) {
                setParamNorm(3, static_cast<float>(i) / 2.0f);
                return;
            }
        }
        for (int i = 0; i < 5; ++i) {
            if (hwTypeHotspots[i].contains(pt)) {
                setParamNorm(4, static_cast<float>(i) / 4.0f);
                return;
            }
        }
        if (hwReadoutRects[3].contains(pt)) {
            int cur = juce::jlimit(0, 2, static_cast<int>(std::round(getParamNorm(3) * 2.0f)));
            setParamNorm(3, static_cast<float>((cur + 1) % 3) / 2.0f);
            return;
        }
        if (hwReadoutRects[4].contains(pt)) {
            int cur = juce::jlimit(0, 4, static_cast<int>(std::round(getParamNorm(4) * 4.0f)));
            setParamNorm(4, static_cast<float>((cur + 1) % 5) / 4.0f);
            return;
        }
    } else {
        for (int i = 0; i < 7; ++i) {
            if (skinTabRects[i].contains(pt)) {
                setParamNorm(6, static_cast<float>(i) / 6.0f);
                return;
            }
        }
        if (closeTo1to1Rect.contains(pt)) {
            showSkinnedView = false;
            repaint();
            return;
        }
        for (int i = 0; i < 3; ++i) {
            if (deckBrightPills[i].contains(pt)) {
                setParamNorm(3, static_cast<float>(i) / 2.0f);
                return;
            }
        }
        for (int i = 0; i < 5; ++i) {
            if (deckTypePills[i].contains(pt)) {
                setParamNorm(4, static_cast<float>(i) / 4.0f);
                return;
            }
        }
    }

    int k = hitTestKnob(pt);
    if (k >= 0) {
        activeDragParam = k;
        dragStartY = pt.y;
        dragStartNorm = getParamNorm(k);
    }
}

void PhoenixIIAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e) {
    if (activeDragParam >= 0) {
        float sens = e.mods.isShiftDown() ? 320.0f : 110.0f;
        float delta = static_cast<float>(dragStartY - e.getPosition().y) / sens;
        setParamNorm(activeDragParam, dragStartNorm + delta);
    }
}

void PhoenixIIAudioProcessorEditor::mouseUp(const juce::MouseEvent&) {
    activeDragParam = -1;
}

void PhoenixIIAudioProcessorEditor::mouseDoubleClick(const juce::MouseEvent& e) {
    auto pt = e.getPosition();

    // Double-click on CRANE SONG logo (or bottom-right visor / header in second view) toggles between Image 1 and Image 2
    if (craneSongLogoRect.contains(pt) || (showSkinnedView && (visorRect.contains(pt) || pt.y < 17))) {
        showSkinnedView = !showSkinnedView;
        repaint();
        return;
    }

    int k = hitTestKnob(pt);
    if (k >= 0) {
        resetParam(k);
    }
}

void PhoenixIIAudioProcessorEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    int k = hitTestKnob(e.getPosition());
    if (k >= 0) {
        float step = (k == 3) ? 0.5f : ((k == 4) ? 0.25f : 0.025f);
        float dir = (wheel.deltaY > 0) ? 1.0f : -1.0f;
        setParamNorm(k, getParamNorm(k) + dir * step);
    }
}

void PhoenixIIAudioProcessorEditor::paint(juce::Graphics& g) {
    if (!showSkinnedView) {
        drawNative1to1View(g);
    } else {
        const int skinId = juce::jlimit(0, 6, static_cast<int>(processorRef.apvts.getRawParameterValue("skin")->load()));
        drawCompactSkinnedView(g, skinId);
    }
}

void PhoenixIIAudioProcessorEditor::drawNative1to1View(juce::Graphics& g) {
    if (imgFaceplate.isValid()) {
        g.drawImageAt(imgFaceplate, 0, 0);
    } else {
        g.fillAll(juce::Colour(0xff5b706b));
    }

    const int inFrame = juce::jlimit(0, 134, static_cast<int>(std::round(getParamNorm(0) * 134.0f)));
    const int prFrame = juce::jlimit(0, 134, static_cast<int>(std::round(getParamNorm(1) * 134.0f)));
    const int ouFrame = juce::jlimit(0, 134, static_cast<int>(std::round(getParamNorm(2) * 134.0f)));
    const int brFrame = juce::jlimit(0, 2,   static_cast<int>(std::round(getParamNorm(3) * 2.0f)));
    const int tyFrame = juce::jlimit(0, 4,   static_cast<int>(std::round(getParamNorm(4) * 4.0f)));

    if (imgKnobCont.isValid()) {
        g.drawImage(imgKnobCont, 19,  41, 60, 60, 0, inFrame * 60, 60, 60);
        g.drawImage(imgKnobCont, 109, 41, 60, 60, 0, prFrame * 60, 60, 60);
        g.drawImage(imgKnobCont, 199, 41, 60, 60, 0, ouFrame * 60, 60, 60);
    }
    if (imgKnobBright.isValid()) {
        g.drawImage(imgKnobBright, 289, 41, 60, 60, 0, brFrame * 60, 60, 60);
    }
    if (imgKnobType.isValid()) {
        g.drawImage(imgKnobType, 379, 41, 60, 60, 0, tyFrame * 60, 60, 60);
    }

    const float inDB  = processorRef.apvts.getRawParameterValue("inputTrim")->load();
    const float prPct = processorRef.apvts.getRawParameterValue("process")->load();
    const float ouDB  = processorRef.apvts.getRawParameterValue("outputTrim")->load();

    juce::String boxTexts[5] = {
        juce::String(inDB, 2),
        juce::String(prPct, 2),
        juce::String(ouDB, 2),
        kBrightNames[brFrame],
        kTypeShort[tyFrame]
    };

    g.setFont(juce::Font(13.0f, juce::Font::plain));
    for (int i = 0; i < 5; ++i) {
        g.setColour(juce::Colours::black);
        g.fillRect(hwReadoutRects[i]);
        g.setColour(juce::Colours::white);
        g.drawText(boxTexts[i], hwReadoutRects[i], juce::Justification::centred, false);
    }
}

void PhoenixIIAudioProcessorEditor::drawCompactSkinnedView(juce::Graphics& g, int skinId) {
    juce::Colour outerTeal(0xff008080);
    juce::Colour panelBg(0xffc0c0c0);
    juce::Colour hdrBg(0xff000080);
    juce::Colour borderCol(0xff404040);
    juce::Colour textCol(0xff000000);
    juce::Colour activeCol(0xff00ff00);
    juce::Colour pillIdleBg(0xff222222);
    juce::Colour pillIdleTxt(0xff888888);
    juce::String winTitle = "Phoenix98.exe - Crane Song Phoenix II [Creador: papielguapo]";

    if (skinId == 0) {
        outerTeal = juce::Colour(0xff101c18); panelBg = juce::Colour(0xff23332e); hdrBg = juce::Colour(0xff132820);
        borderCol = juce::Colour(0xff3e5c52); textCol = juce::Colour(0xffe6f5f0); activeCol = juce::Colour(0xff38e5a5);
        winTitle = "Phoenix II - Crane Song Edition [Creador: papielguapo]";
    } else if (skinId == 1) {
        outerTeal = juce::Colour(0xff14171c); panelBg = juce::Colour(0xff252a32); hdrBg = juce::Colour(0xff191c20);
        borderCol = juce::Colour(0xff3c434f); textCol = juce::Colour(0xffd7dae0); activeCol = juce::Colour(0xffff9100);
        winTitle = "Fruity Wrapper - Phoenix II [Creador: papielguapo]";
    } else if (skinId == 2) {
        outerTeal = juce::Colour(0xff1c1c1f); panelBg = juce::Colour(0xff34353a); hdrBg = juce::Colour(0xff52525a);
        borderCol = juce::Colour(0xff62626c); textCol = juce::Colour(0xffefefef); activeCol = juce::Colour(0xff39ff14);
        winTitle = "Pro Tools HDX - Phoenix II [Creador: papielguapo]";
    } else if (skinId == 3) {
        outerTeal = juce::Colour(0xff181818); panelBg = juce::Colour(0xff383838); hdrBg = juce::Colour(0xff242424);
        borderCol = juce::Colour(0xff141414); textCol = juce::Colour(0xffeeeeee); activeCol = juce::Colour(0xffffae00);
        winTitle = "Ableton Live 12 - Phoenix II [Creador: papielguapo]";
    } else if (skinId == 4) {
        outerTeal = juce::Colour(0xff0b0d13); panelBg = juce::Colour(0xff1a1f2c); hdrBg = juce::Colour(0xff252b3d);
        borderCol = juce::Colour(0xff38425e); textCol = juce::Colour(0xffeef2ff); activeCol = juce::Colour(0xff00d2ff);
        winTitle = "Logic Pro 11 - Phoenix II [Creador: papielguapo]";
    } else if (skinId == 6) {
        outerTeal = juce::Colour(0xff020803); panelBg = juce::Colour(0xff061408); hdrBg = juce::Colour(0xff123d1a);
        borderCol = juce::Colour(0xff33ff66); textCol = juce::Colour(0xff33ff66); activeCol = juce::Colour(0xff33ff66);
        winTitle = "PHOENIX2.PRG - ATARI PC CRT [Creador: papielguapo]";
    }

    g.fillAll(outerTeal);

    juce::Rectangle<int> upperWin(2, 1, 511, 113);
    g.setColour(panelBg);
    g.fillRect(upperWin);
    g.setColour(borderCol);
    g.drawRect(upperWin, 1);

    juce::Rectangle<int> titleBar(3, 2, 509, 14);
    g.setColour(hdrBg);
    g.fillRect(titleBar);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText(" " + winTitle, 4, 2, 304, 14, juce::Justification::centredLeft, false);

    g.setFont(juce::Font(8.5f, juce::Font::bold));
    for (int i = 0; i < 7; ++i) {
        bool act = (i == skinId);
        g.setColour(act ? activeCol : hdrBg.brighter(0.25f));
        g.fillRect(skinTabRects[i]);
        g.setColour(act ? juce::Colours::black : juce::Colours::white);
        g.drawText(kSkinCodes[i], skinTabRects[i], juce::Justification::centred, false);
    }

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(9.5f, juce::Font::bold));
    g.drawText("[ _ ] [ # ] [ X ]", closeTo1to1Rect, juce::Justification::centredRight, false);
    const float inDB  = processorRef.apvts.getRawParameterValue("inputTrim")->load();
    const float prPct = processorRef.apvts.getRawParameterValue("process")->load();
    const float ouDB  = processorRef.apvts.getRawParameterValue("outputTrim")->load();
    const int brIdx   = juce::jlimit(0, 2, static_cast<int>(processorRef.apvts.getRawParameterValue("brightness")->load()));
    const int tyIdx   = juce::jlimit(0, 4, static_cast<int>(processorRef.apvts.getRawParameterValue("type")->load()));

    juce::String valStrings[5] = {
        juce::String(inDB >= 0 ? "+" : "") + juce::String(inDB, 1) + " dB",
        juce::String(prPct, 1) + " %",
        juce::String(ouDB >= 0 ? "+" : "") + juce::String(ouDB, 1) + " dB",
        kBrightNames[brIdx],
        kTypeNames[tyIdx]
    };

    const int colW = 101;
    for (int i = 0; i < 5; ++i) {
        juce::Rectangle<int> modBox(5 + i * colW, 18, colW - 3, 94);
        g.setColour(panelBg);
        g.fillRect(modBox);
        g.setColour(borderCol);
        g.drawRect(modBox, 1);

        g.setColour(textCol);
        g.setFont(juce::Font(8.5f, juce::Font::bold));
        g.drawText(kParamTitles[i], modBox.getX(), modBox.getY() + 1, modBox.getWidth(), 10, juce::Justification::centred, false);

        drawCompactKnob(g, deckKnobRects[i].toFloat(), i, getParamNorm(i), skinId);

        int valY = (i < 3) ? 84 : 62;
        int valH = (i < 3) ? 16 : 10;
        juce::Rectangle<int> vBox(modBox.getX() + 8, valY, modBox.getWidth() - 16, valH);
        g.setColour(juce::Colours::black);
        g.fillRect(vBox);
        g.setColour(activeCol);
        g.setFont(juce::Font(i < 3 ? 9.5f : 8.0f, juce::Font::bold));
        g.drawText(valStrings[i], vBox, juce::Justification::centred, false);
    }

    g.setFont(juce::Font(7.8f, juce::Font::bold));
    for (int i = 0; i < 3; ++i) {
        bool act = (i == brIdx);
        g.setColour(act ? activeCol : pillIdleBg);
        g.fillRoundedRectangle(deckBrightPills[i].toFloat(), 1.5f);
        g.setColour(act ? juce::Colours::black : pillIdleTxt);
        g.drawText(kBrightNames[i], deckBrightPills[i], juce::Justification::centred, false);
    }

    g.setFont(juce::Font(6.8f, juce::Font::bold));
    for (int i = 0; i < 5; ++i) {
        bool act = (i == tyIdx);
        g.setColour(act ? activeCol : pillIdleBg);
        g.fillRoundedRectangle(deckTypePills[i].toFloat(), 1.5f);
        g.setColour(act ? juce::Colours::black : pillIdleTxt);
        g.drawText(kTypeNames[i], deckTypePills[i], juce::Justification::centred, false);
    }

    juce::Rectangle<int> lowerWin(2, 117, 511, 42);
    g.setColour(panelBg);
    g.fillRect(lowerWin);
    g.setColour(borderCol);
    g.drawRect(lowerWin, 1);

    const float inP  = processorRef.meterInPeak.load();
    const float outP = processorRef.meterOutPeak.load();
    const float drv  = processorRef.meterDrive.load();

    const float inDb  = (inP > 0.0001f) ? (20.0f * std::log10(inP)) : -90.0f;
    const float outDb = (outP > 0.0001f) ? (20.0f * std::log10(outP)) : -90.0f;
    const float inNorm  = juce::jlimit(0.0f, 1.0f, (inDb + 48.0f) / 54.0f);
    const float outNorm = juce::jlimit(0.0f, 1.0f, (outDb + 48.0f) / 54.0f);

    auto drawCompactMeter = [&](int y, const char* label, float normVal, const juce::String& valTxt) {
        g.setColour(textCol);
        g.setFont(juce::Font(8.2f, juce::Font::bold));
        g.drawText(label, 8, y, 56, 10, juce::Justification::centredLeft, false);

        juce::Rectangle<int> trk(65, y + 1, 200, 8);
        g.setColour(juce::Colours::black);
        g.fillRect(trk);
        g.setColour(borderCol);
        g.drawRect(trk, 1);

        int fillW = static_cast<int>(juce::jlimit(0.0f, 1.0f, normVal) * (trk.getWidth() - 2));
        if (fillW > 0) {
            g.setColour(activeCol);
            g.fillRect(trk.getX() + 1, trk.getY() + 1, fillW, trk.getHeight() - 2);
        }

        g.setColour(textCol);
        g.drawText(valTxt, 270, y, 48, 10, juce::Justification::centredLeft, false);
    };

    drawCompactMeter(121, "INPUT L/R",  inNorm,       inDb <= -60.0f ? "-inf dB" : juce::String(inDb, 1) + " dB");
    drawCompactMeter(133, "TAPE DRIVE", drv / 100.0f, juce::String(drv, 1) + " %");
    drawCompactMeter(145, "OUTPUT L/R", outNorm,      outDb <= -60.0f ? "-inf dB" : juce::String(outDb, 1) + " dB");
    // Right Visor Box (PHXTBL Curve + by papielguapo)
    g.setColour(juce::Colours::black);
    g.fillRect(visorRect);
    g.setColour(borderCol);
    g.drawRect(visorRect, 1);

    const float procVal = prPct / 100.0f;
    const auto& color = kPhoenixColorTypes[tyIdx];
    const int isType4 = (tyIdx == 3) ? 1 : 0;
    const float type4Factor = 1.0f - static_cast<float>(isType4) * 0.5f;
    const float scaledProcess = procVal * color.driveScale;
    const float effProcess = scaledProcess * type4Factor;

    juce::Path curvePath;
    const int cw = visorRect.getWidth() - 4;
    const int ch = visorRect.getHeight() - 4;
    for (int px = 0; px <= cw; ++px) {
        float inSample = (static_cast<float>(px) / static_cast<float>(cw)) * 2.0f - 1.0f;
        float x = inSample * (1.0f + color.preEmphasisGain * 0.5f);
        float baseSig = (color.flagA != 0) ? x : inSample;
        if (isType4 != 0) x *= scaledProcess;
        x = PhoenixIIDSP::lookupPhxTbl(x, color.tableOffset);
        x = (x * scaledProcess * color.satMixGain) + baseSig;
        x = PhoenixIIDSP::lookupPhxTbl(x, color.tableOffset);
        float dryComp = inSample + (-1.0f * effProcess * inSample * color.dryCompGain);
        float outSample = ((x * effProcess) + dryComp);
        float py = visorRect.getY() + 2 + (ch * 0.5f) - juce::jlimit(-1.0f, 1.0f, outSample) * (ch * 0.40f);
        float fx = static_cast<float>(visorRect.getX() + 2 + px);
        if (px == 0) curvePath.startNewSubPath(fx, py);
        else curvePath.lineTo(fx, py);
    }
    g.setColour(activeCol);
    g.strokePath(curvePath, juce::PathStrokeType(1.6f));

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(7.8f, juce::Font::bold));
    g.drawText(juce::String("PHXTBL ") + kTypeNames[tyIdx] + " * by papielguapo",
               visorRect.getX() + 4, visorRect.getY() + 2, visorRect.getWidth() - 8, 10,
               juce::Justification::centredLeft, false);
}

void PhoenixIIAudioProcessorEditor::drawCompactKnob(juce::Graphics& g, juce::Rectangle<float> box, int, float norm, int skinId) {
    const float cx = box.getCentreX();
    const float cy = box.getCentreY();
    const float r  = box.getWidth() * 0.40f;
    const float startAng = juce::MathConstants<float>::pi * 1.25f;
    const float endAng   = juce::MathConstants<float>::pi * 2.75f;
    const float valAng   = startAng + norm * (endAng - startAng);

    juce::Colour arcCol(0xff000080);
    juce::Colour bodyCol(0xffc0c0c0);
    juce::Colour ptrCol(0xff000080);

    if (skinId == 0) { arcCol = juce::Colour(0xff38c998); bodyCol = juce::Colour(0xff155742); ptrCol = juce::Colours::white; }
    else if (skinId == 1) { arcCol = juce::Colour(0xffff9100); bodyCol = juce::Colour(0xff282e38); ptrCol = juce::Colours::white; }
    else if (skinId == 2) { arcCol = juce::Colour(0xff39ff14); bodyCol = juce::Colour(0xff5a5c66); ptrCol = juce::Colours::black; }
    else if (skinId == 3) { arcCol = juce::Colour(0xffffae00); bodyCol = juce::Colour(0xff222222); ptrCol = juce::Colours::white; }
    else if (skinId == 4) { arcCol = juce::Colour(0xff00d2ff); bodyCol = juce::Colour(0xff222738); ptrCol = juce::Colours::white; }
    else if (skinId == 6) { arcCol = juce::Colour(0xff33ff66); bodyCol = juce::Colour(0xff041005); ptrCol = juce::Colour(0xff33ff66); }

    juce::Path bgArc;
    bgArc.addCentredArc(cx, cy, r + 3.0f, r + 3.0f, 0.0f, startAng, endAng, true);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.strokePath(bgArc, juce::PathStrokeType(2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valArc;
    valArc.addCentredArc(cx, cy, r + 3.0f, r + 3.0f, 0.0f, startAng, valAng, true);
    g.setColour(arcCol);
    g.strokePath(valArc, juce::PathStrokeType(2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(bodyCol);
    g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour(juce::Colour(0xff333333));
    g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.2f);

    const float sinA = std::sin(valAng);
    const float cosA = -std::cos(valAng);
    g.setColour(ptrCol);
    g.drawLine(cx + sinA * (r * 0.2f), cy + cosA * (r * 0.2f),
               cx + sinA * (r * 0.85f), cy + cosA * (r * 0.85f), 2.0f);
}
