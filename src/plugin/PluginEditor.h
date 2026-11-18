#pragma once
#include "PluginProcessor.h"

struct SilkLook : juce::LookAndFeel_V4 {
    // Cool blue-grey palette similar to the soothe2 map, original artwork.
    juce::Colour bg { 0xffbecad2 };
    juce::Colour well { 0xffabbac4 };
    juce::Colour header { 0xff7c8e9e };
    juce::Colour sky { 0xffb0bcc2 };
    juce::Colour floor { 0xff92adc1 };
    juce::Colour cream { 0xffd9cbb6 };
    juce::Colour ink { 0xff2a333a };
    juce::Colour inkMuted { 0xff4a5860 };
    juce::Colour knob { 0xff2c3338 };
    juce::Colour tick { 0xfff2f5f7 };
    juce::Colour white { 0xffffffff };
    juce::Colour softFill { 0xffcad6de };
    juce::Colour node[6] { juce::Colour(0xff4a4a4a), juce::Colour(0xfff8e048), juce::Colour(0xffe0a050),
                            juce::Colour(0xffc86860), juce::Colour(0xff9050a8), juce::Colour(0xffd8e0e6) };
    SilkLook() { setColour(juce::Slider::textBoxTextColourId, ink); }
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
};

class SilkEditor : public juce::AudioProcessorEditor, private juce::Timer, private juce::Button::Listener {
public:
    explicit SilkEditor(SilkProcessor&);
    ~SilkEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void buttonClicked(juce::Button*) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;

    int selectedBand = 3;
    bool showBandCard = true;

private:
    SilkProcessor& proc;
    SilkLook look;
    juce::Font body { juce::FontOptions(13.0f) };
    juce::Font small { juce::FontOptions(11.0f) };
    juce::Font title { juce::FontOptions(28.0f).withStyle("Bold") };

    struct K {
        juce::Slider s;
        juce::Label name, value;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> at;
        void setup(SilkEditor& e, int param, const juce::String& nm, juce::Slider::SliderStyle st = juce::Slider::RotaryHorizontalVerticalDrag);
        void place(juce::Rectangle<int> r, bool showName = true);
    };
    K depth, sharp, sel, attack, release, mix, trim, link, bal, bandFreq, bandSens, bandQ, bandBal;
    juce::TextButton softBtn { "soft" }, hardBtn { "hard" };
    juce::TextButton modeBtn, scOn, scListen, deltaBtn, bypassBtn, autoBtn, lockBtn;
    juce::TextButton prevPreset, nextPreset, presetBtn, undoBtn, redoBtn, aBtn, bBtn, copyAB, helpBtn, cfgBtn;
    juce::TextButton vocalBtn, learnBtn, hideCard, bandOn, bandListen;
    juce::TextButton typeBtn[8];
    juce::Label qualityLabel, latencyLabel, freqReadout, tooltip;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hardAt, scAt, scLAt, dAt, byAt, autoAt, lockAt, bandOnAt;
    juce::UndoManager* um = nullptr;

    juce::Rectangle<int> leftCol, midCol, graphR, toolR, cardR, hideR, resizeR;
    juce::Rectangle<int> stereoHead, speedHead, qualityHead, scHead, outHead;
    bool dragging = false, dragQ = false, listenHeld = false;
    int dragBand = -1;
    float dragStartY = 0, dragStartSens = 0, dragStartQ = 0;

    void layoutKnobs();
    void refreshTexts();
    void syncSoftHard();
    void syncModeButton();
    void syncQualityLabel();
    void openPresetMenu();
    void openVocalMenu();
    void openQualityMenu();
    juce::Rectangle<float> nodePos(int b) const;
    int hitNode(juce::Point<float> p) const;
    void paintGraph(juce::Graphics&);
    void paintBandCard(juce::Graphics&);
    juce::Rectangle<int> graphInner() const;
    float yForSens(float sensDb, juce::Rectangle<float> gi) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SilkEditor)
};
