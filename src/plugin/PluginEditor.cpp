#include "PluginEditor.h"
using namespace silk;

void SilkLook::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) {
    auto r = juce::Rectangle<float>((float)x, (float)y, (float)w, (float)h).reduced(3);
    auto c = r.getCentre();
    float rad = std::min(r.getWidth(), r.getHeight()) * 0.48f;
    g.setColour(knob);
    g.fillEllipse(c.x - rad, c.y - rad, rad * 2, rad * 2);
    // tick ring
    g.setColour(tick);
    int ticks = 32;
    for (int i = 0; i < ticks; ++i) {
        float t = (float)i / (float)ticks;
        float a = a0 + t * (a1 - a0);
        float r0 = rad * 0.82f, r1 = rad * 0.96f;
        g.drawLine(c.x + r0 * std::cos(a), c.y + r0 * std::sin(a), c.x + r1 * std::cos(a), c.y + r1 * std::sin(a), 1.0f);
    }
    float a = a0 + pos * (a1 - a0);
    g.setColour(tick);
    g.drawLine(c.x, c.y, c.x + rad * 0.72f * std::cos(a), c.y + rad * 0.72f * std::sin(a), 2.0f);
    g.fillEllipse(c.x - 3, c.y - 3, 6, 6);
}

void SilkEditor::K::setup(SilkEditor& e, int param, const juce::String& nm, juce::Slider::SliderStyle st) {
    s.setSliderStyle(st);
    s.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    s.setLookAndFeel(&e.look);
    s.setColour(juce::Slider::rotarySliderFillColourId, e.look.knob);
    s.setColour(juce::Slider::rotarySliderOutlineColourId, e.look.tick);
    name.setText(nm, juce::dontSendNotification);
    name.setJustificationType(juce::Justification::centred);
    name.setColour(juce::Label::textColourId, e.look.inkMuted);
    name.setFont(e.small);
    value.setJustificationType(juce::Justification::centred);
    value.setColour(juce::Label::textColourId, e.look.ink);
    value.setFont(e.small);
    e.addAndMakeVisible(s); e.addAndMakeVisible(name); e.addAndMakeVisible(value);
    at = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(e.proc.apvts, silkParamId(param), s);
}

// Attachment needs the actual param id string. We'll set up knobs in the editor ctor with a helper that uses raw IDs from the processor.

SilkEditor::SilkEditor(SilkProcessor& p) : AudioProcessorEditor(p), proc(p) {
    setLookAndFeel(&look);
    setResizable(true, true);
    setResizeLimits(860, 500, 2400, 1400);
    setSize(1120, 640);

    auto setupK = [this](K& k, int id, const juce::String& nm, bool large = false) {
        k.s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.s.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        k.s.setLookAndFeel(&look);
        k.name.setText(nm, juce::dontSendNotification);
        k.name.setJustificationType(juce::Justification::centred);
        k.name.setColour(juce::Label::textColourId, look.inkMuted);
        k.name.setFont(small);
        k.value.setJustificationType(juce::Justification::centred);
        k.value.setColour(juce::Label::textColourId, look.ink);
        k.value.setFont(large ? body : small);
        addAndMakeVisible(k.s); addAndMakeVisible(k.name); addAndMakeVisible(k.value);
        if (auto* prm = proc.param(id)) {
            // Use the parameter's ID from APVTS: we stored them in the processor's apvts
            auto* rp = dynamic_cast<juce::RangedAudioParameter*>(prm);
            juce::String pid;
            // Find id by scanning apvts state or by kParamIds which we can access if non-null
            // Fall back: attachment via param pointer isn't built in, so use known names.
        }
    };

    // Attachments via apvts IDs — reconstruct the same IDs used in the processor.
    auto idFor = [](int i) -> juce::String { return juce::String(silkParamId(i)); };

    auto bind = [&](K& k, int id, const juce::String& nm) {
        k.s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.s.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        k.s.setLookAndFeel(&look);
        k.name.setText(nm, juce::dontSendNotification);
        k.name.setJustificationType(juce::Justification::centred);
        k.name.setColour(juce::Label::textColourId, look.inkMuted);
        k.name.setFont(small);
        k.value.setJustificationType(juce::Justification::centred);
        k.value.setColour(juce::Label::textColourId, look.ink);
        k.value.setFont(small);
        addAndMakeVisible(k.s); addAndMakeVisible(k.name); addAndMakeVisible(k.value);
        k.at = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, idFor(id), k.s);
        k.s.onValueChange = [this]{ refreshTexts(); };
    };
    bind(depth, P_DEPTH, "depth");
    bind(sharp, P_SHARPNESS, "sharpness");
    bind(sel, P_SELECTIVITY, "selectivity");
    bind(attack, P_ATTACK, "attack");
    bind(release, P_RELEASE, "release");
    bind(mix, P_MIX, "mix");
    bind(trim, P_TRIM, "trim");
    bind(link, P_LINK, "link");
    bind(bal, P_BALANCE, "balance");
    bind(bandFreq, P_BAND(3, 1), "freq");
    bind(bandSens, P_BAND(3, 2), "sens");
    bind(bandQ, P_BAND(3, 3), "q");
    bind(bandBal, P_BAND(3, 4), "balance");

    auto styleBtn = [this](juce::TextButton& b, bool toggle = true) {
        b.setClickingTogglesState(toggle);
        b.setColour(juce::TextButton::buttonColourId, look.well);
        b.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffe8eef2));
        b.setColour(juce::TextButton::textColourOffId, look.inkMuted);
        b.setColour(juce::TextButton::textColourOnId, look.ink);
        addAndMakeVisible(b);
        b.addListener(this);
    };
    styleBtn(softBtn, true); styleBtn(hardBtn, true);
    softBtn.setRadioGroupId(1); hardBtn.setRadioGroupId(1);
    softBtn.setToggleState(proc.value(P_HARD) < 0.5f, juce::dontSendNotification);
    hardBtn.setToggleState(proc.value(P_HARD) >= 0.5f, juce::dontSendNotification);

    styleBtn(modeBtn, false);
    styleBtn(scOn, true); styleBtn(scListen, true);
    styleBtn(deltaBtn, true); styleBtn(bypassBtn, true); styleBtn(autoBtn, true); styleBtn(lockBtn, true);
    scOn.setButtonText(juce::String::fromUTF8("\xE2\x8F\xBB")); // might mojibake — use "on"
    scOn.setButtonText("on");
    scListen.setButtonText("listen");
    deltaBtn.setButtonText("delta");
    bypassBtn.setButtonText("bypass");
    autoBtn.setButtonText("auto");
    lockBtn.setButtonText("lock");

    scAt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, idFor(P_SIDECHAIN), scOn);
    scLAt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, idFor(P_SC_LISTEN), scListen);
    dAt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, idFor(P_DELTA), deltaBtn);
    byAt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, idFor(P_BYPASS), bypassBtn);
    autoAt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, idFor(P_AUTO_TRIM), autoBtn);
    lockAt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, idFor(P_QUALITY_LOCK), lockBtn);

    auto tool = [this](juce::TextButton& b, const juce::String& t) {
        b.setButtonText(t);
        b.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        b.setColour(juce::TextButton::textColourOffId, look.tick);
        b.setColour(juce::TextButton::textColourOnId, look.white);
        addAndMakeVisible(b); b.addListener(this);
    };
    tool(prevPreset, "<"); tool(nextPreset, ">"); tool(presetBtn, "Factory Default");
    tool(undoBtn, "undo"); tool(redoBtn, "redo");
    tool(aBtn, "a"); tool(bBtn, "b"); tool(copyAB, "a->b"); tool(helpBtn, "?"); tool(cfgBtn, "cfg");
    tool(vocalBtn, "Vocal"); tool(learnBtn, "Learn");
    aBtn.setClickingTogglesState(false);

    hideCard.setButtonText(" ");
    addAndMakeVisible(hideCard); hideCard.addListener(this);
    hideCard.setColour(juce::TextButton::buttonColourId, look.well);

    bandOn.setButtonText("on"); bandListen.setButtonText("listen");
    styleBtn(bandOn, true); styleBtn(bandListen, false);
    const char* typelab[8] = { "peak", "lo sh", "hi sh", "bd sh", "rej", "tilt", "lo cut", "hi cut" };
    for (int i = 0; i < 8; ++i) { typeBtn[i].setButtonText(typelab[i]); styleBtn(typeBtn[i], true); typeBtn[i].setRadioGroupId(2); }

    qualityLabel.setColour(juce::Label::textColourId, look.ink);
    qualityLabel.setFont(small);
    qualityLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(qualityLabel);
    qualityLabel.addMouseListener(this, false);

    latencyLabel.setColour(juce::Label::textColourId, look.inkMuted);
    latencyLabel.setFont(small);
    addAndMakeVisible(latencyLabel);
    freqReadout.setColour(juce::Label::textColourId, look.ink);
    freqReadout.setFont(small);
    addAndMakeVisible(freqReadout);
    tooltip.setColour(juce::Label::textColourId, look.inkMuted);
    tooltip.setFont(small);
    addAndMakeVisible(tooltip);

    um = proc.apvts.undoManager;
    refreshTexts();
    syncSoftHard();
    syncModeButton();
    syncQualityLabel();
    startTimerHz(30);
}

SilkEditor::~SilkEditor() { setLookAndFeel(nullptr); }

void SilkEditor::resized() {
    auto r = getLocalBounds();
    leftCol = r.removeFromLeft(210);
    midCol = r.removeFromLeft(110);
    graphR = r;
    toolR = graphR.removeFromTop(44);
    hideR = juce::Rectangle<int>(graphR.getX() + 8, graphR.getBottom() - 28, 22, 22);
    resizeR = juce::Rectangle<int>(getWidth() - 22, getHeight() - 22, 18, 18);
    int cardH = showBandCard ? 88 : 0;
    cardR = juce::Rectangle<int>(graphR.getX() + 40, graphR.getBottom() - cardH - 8, graphR.getWidth() - 80, cardH);
    layoutKnobs();
}

void SilkEditor::layoutKnobs() {
    auto placeK = [](K& k, juce::Rectangle<int> box, bool) {
        auto n = box.removeFromTop(14);
        auto v = box.removeFromBottom(14);
        k.name.setBounds(n); k.value.setBounds(v); k.s.setBounds(box);
        k.name.setVisible(true); k.value.setVisible(true);
    };
    auto L = leftCol.reduced(10, 8);
    L.removeFromTop(68); // wordmark
    auto sh = L.removeFromTop(26);
    softBtn.setBounds(sh.removeFromLeft(sh.getWidth() / 2).reduced(2, 2));
    hardBtn.setBounds(sh.reduced(2, 2));
    L.removeFromTop(6);
    int rest = std::max(1, L.getHeight());
    int gap = 6;
    int body = std::max(1, rest - gap * 2);
    int depthH = body * 44 / 100;
    int pairH = body * 28 / 100;
    int stereoBlock = body - depthH - pairH;
    if (depthH < 120 || pairH < 88 || stereoBlock < 88) {
        depthH = std::max(110, body * 42 / 100);
        pairH = std::max(80, body * 28 / 100);
        stereoBlock = std::max(80, body - depthH - pairH);
    }
    placeK(depth, L.removeFromTop(depthH), true);
    L.removeFromTop(gap);
    auto row = L.removeFromTop(pairH);
    placeK(sharp, row.removeFromLeft(row.getWidth() / 2).reduced(2, 0), false);
    placeK(sel, row.reduced(2, 0), false);
    L.removeFromTop(gap);
    stereoHead = L.removeFromTop(16);
    modeBtn.setBounds(L.removeFromTop(22).removeFromLeft(std::min(110, L.getWidth())));
    auto brow = L;
    placeK(link, brow.removeFromLeft(brow.getWidth() / 2).reduced(2, 0), false);
    placeK(bal, brow.reduced(2, 0), false);

    auto M = midCol.reduced(6, 8);
    speedHead = M.removeFromTop(16);
    int mRest = std::max(1, M.getHeight());
    const int qBlock = 52, scBlock = 38, outHdr = 16, outBtns = 44, gaps = 20;
    int rem = mRest - qBlock - scBlock - outHdr - outBtns - gaps;
    if (rem < 200) rem = std::max(180, mRest * 55 / 100);
    int attH = std::max(64, rem * 28 / 100);
    int relH = std::max(64, rem * 28 / 100);
    int mixH = std::max(64, rem * 22 / 100);
    int trimH = std::max(64, rem - attH - relH - mixH);
    placeK(attack, M.removeFromTop(attH), false);
    placeK(release, M.removeFromTop(relH), false);
    M.removeFromTop(4);
    qualityHead = M.removeFromTop(16);
    qualityLabel.setBounds(M.removeFromTop(18));
    lockBtn.setBounds(M.removeFromTop(18).removeFromLeft(50));
    M.removeFromTop(4);
    scHead = M.removeFromTop(16);
    auto sc = M.removeFromTop(22);
    scOn.setBounds(sc.removeFromLeft(sc.getWidth() / 2).reduced(1));
    scListen.setBounds(sc.reduced(1));
    M.removeFromTop(4);
    outHead = M.removeFromTop(outHdr);
    placeK(mix, M.removeFromTop(mixH), false);
    placeK(trim, M.removeFromTop(trimH), false);
    auto tb = M.removeFromTop(22);
    autoBtn.setBounds(tb.removeFromLeft(tb.getWidth() / 2).reduced(1));
    auto db = M.removeFromTop(22);
    deltaBtn.setBounds(db.removeFromLeft(db.getWidth() / 2).reduced(1));
    bypassBtn.setBounds(db.reduced(1));

    auto T = toolR.reduced(4, 6);
    const int tw = T.getWidth();
    const bool tight = tw < 700;
    undoBtn.setButtonText(tight ? "un" : "undo");
    redoBtn.setButtonText(tight ? "re" : "redo");
    copyAB.setButtonText(tight ? "a>b" : "a->b");
    cfgBtn.setButtonText("cfg");
    prevPreset.setBounds(T.removeFromLeft(22));
    nextPreset.setBounds(T.removeFromLeft(22));
    cfgBtn.setBounds(T.removeFromRight(30));
    helpBtn.setBounds(T.removeFromRight(24));
    copyAB.setBounds(T.removeFromRight(tight ? 32 : 44));
    bBtn.setBounds(T.removeFromRight(22));
    aBtn.setBounds(T.removeFromRight(22));
    redoBtn.setBounds(T.removeFromRight(tight ? 28 : 44));
    undoBtn.setBounds(T.removeFromRight(tight ? 28 : 44));
    presetBtn.setBounds(T.removeFromLeft(juce::jlimit(90, 200, tight ? 100 : 200)));
    vocalBtn.setBounds(T.removeFromLeft(tight ? 54 : 70));
    learnBtn.setBounds(T.removeFromLeft(tight ? 54 : 70));
    freqReadout.setBounds(T);
    freqReadout.setVisible(T.getWidth() >= 40);
    latencyLabel.setBounds(getWidth() - 130, getHeight() - 20, 100, 16);

    if (showBandCard && cardR.getHeight() > 20) {
        auto c = cardR.reduced(8, 6);
        auto top = c.removeFromTop(24);
        bandOn.setBounds(top.removeFromLeft(40));
        bandListen.setBounds(top.removeFromLeft(54));
        auto types = c.removeFromRight(std::max(120, c.getWidth() / 3));
        auto kn = c;
        auto k1 = kn.removeFromLeft(kn.getWidth() / 4);
        auto k2 = kn.removeFromLeft(kn.getWidth() / 3);
        auto k3 = kn.removeFromLeft(kn.getWidth() / 2);
        placeK(bandFreq, k1, false); placeK(bandSens, k2, false); placeK(bandQ, k3, false); placeK(bandBal, kn, false);
        auto r1 = types.removeFromTop(types.getHeight() / 2);
        auto r2 = types;
        for (int i = 0; i < 4; ++i) typeBtn[i].setBounds(r1.removeFromLeft(r1.getWidth() / (4 - i)).reduced(1, 2));
        for (int i = 4; i < 8; ++i) typeBtn[i].setBounds(r2.removeFromLeft(r2.getWidth() / (8 - i)).reduced(1, 2));
        hideCard.setBounds(hideR);
        bandOn.setVisible(true); bandListen.setVisible(true);
        bandFreq.s.setVisible(true); bandSens.s.setVisible(true); bandQ.s.setVisible(true); bandBal.s.setVisible(true);
        for (int i = 0; i < 8; ++i) typeBtn[i].setVisible(true);
    } else {
        hideCard.setBounds(hideR);
        bandOn.setVisible(false); bandListen.setVisible(false);
        bandFreq.s.setVisible(false); bandSens.s.setVisible(false); bandQ.s.setVisible(false); bandBal.s.setVisible(false);
        bandFreq.name.setVisible(false); bandSens.name.setVisible(false); bandQ.name.setVisible(false); bandBal.name.setVisible(false);
        bandFreq.value.setVisible(false); bandSens.value.setVisible(false); bandQ.value.setVisible(false); bandBal.value.setVisible(false);
        for (int i = 0; i < 8; ++i) typeBtn[i].setVisible(false);
    }
}

void SilkEditor::refreshTexts() {
    auto txt = [this](int id){ return proc.param(id) ? proc.param(id)->getCurrentValueAsText() : juce::String(); };
    depth.value.setText(txt(P_DEPTH), juce::dontSendNotification);
    sharp.value.setText(txt(P_SHARPNESS), juce::dontSendNotification);
    sel.value.setText(txt(P_SELECTIVITY), juce::dontSendNotification);
    attack.value.setText(txt(P_ATTACK), juce::dontSendNotification);
    release.value.setText(txt(P_RELEASE), juce::dontSendNotification);
    mix.value.setText(txt(P_MIX), juce::dontSendNotification);
    trim.value.setText(txt(P_TRIM), juce::dontSendNotification);
    link.value.setText(txt(P_LINK), juce::dontSendNotification);
    bal.value.setText(txt(P_BALANCE), juce::dontSendNotification);
    bandFreq.value.setText(txt(P_BAND(selectedBand, 1)), juce::dontSendNotification);
    bandSens.value.setText(txt(P_BAND(selectedBand, 2)), juce::dontSendNotification);
    bandQ.value.setText(txt(P_BAND(selectedBand, 3)), juce::dontSendNotification);
    bandBal.value.setText(txt(P_BAND(selectedBand, 4)), juce::dontSendNotification);
    presetBtn.setButtonText(proc.getProgramName(proc.getCurrentProgram()));
    aBtn.setToggleState(proc.isSideA(), juce::dontSendNotification);
    bBtn.setToggleState(!proc.isSideA(), juce::dontSendNotification);
    latencyLabel.setText(juce::String(proc.latencyMs.load(), 1) + " ms", juce::dontSendNotification);
    syncQualityLabel();
    if (proc.engine().isLearning())
        learnBtn.setButtonText("Learn " + juce::String((int)(proc.engine().learnProgress() * 100)) + "%");
    else
        learnBtn.setButtonText("Learn");
}

void SilkEditor::syncSoftHard() {
    bool h = proc.value(P_HARD) >= 0.5f;
    softBtn.setToggleState(!h, juce::dontSendNotification);
    hardBtn.setToggleState(h, juce::dontSendNotification);
}
void SilkEditor::syncModeButton() {
    modeBtn.setButtonText(proc.value(P_STEREO_MODE) >= 0.5f ? "mid | side" : "left | right");
}
void SilkEditor::syncQualityLabel() {
    const char* os[] = { "1x", "2x", "4x" };
    const char* rs[] = { "Live", "Eco", "normal", "High", "Ultra" };
    int o = juce::jlimit(0, 2, (int)std::lround(proc.value(P_OVERSAMPLE)));
    int r = juce::jlimit(0, 4, (int)std::lround(proc.value(P_RESOLUTION)));
    qualityLabel.setText(juce::String(os[o]) + " " + rs[r], juce::dontSendNotification);
}

void SilkEditor::timerCallback() {
    refreshTexts();
    syncSoftHard();
    syncModeButton();
    repaint(graphR);
}

void SilkEditor::buttonClicked(juce::Button* b) {
    if (b == &softBtn) proc.setValue(P_HARD, 0);
    if (b == &hardBtn) proc.setValue(P_HARD, 1);
    if (b == &modeBtn) proc.setValue(P_STEREO_MODE, proc.value(P_STEREO_MODE) >= 0.5f ? 0.f : 1.f);
    if (b == &prevPreset) proc.setCurrentProgram(std::max(0, proc.getCurrentProgram() - 1));
    if (b == &nextPreset) proc.setCurrentProgram(std::min(proc.getNumPrograms() - 1, proc.getCurrentProgram() + 1));
    if (b == &presetBtn) openPresetMenu();
    if (b == &vocalBtn) openVocalMenu();
    if (b == &learnBtn) {
        if (proc.engine().isLearning()) proc.cancelLearn();
        else proc.startLearn(3.0);
    }
    if (b == &undoBtn && proc.apvts.undoManager) proc.apvts.undoManager->undo();
    if (b == &redoBtn && proc.apvts.undoManager) proc.apvts.undoManager->redo();
    if (b == &aBtn || b == &bBtn) proc.toggleAB();
    if (b == &copyAB) proc.copyAB();
    if (b == &hideCard) { showBandCard = !showBandCard; resized(); }
    if (b == &bandOn) proc.setValue(P_BAND(selectedBand, 0), bandOn.getToggleState() ? 1.f : 0.f);
    if (b == &bandListen) { /* momentary delta of this band approximated by soloing Sens */ }
    if (b == &helpBtn) {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "SILK",
            "SILK is a dynamic resonance suppressor. Raise Depth until it takes a little too much, then back off. "
            "Use Delta to hear what is being removed. Boost a node to process that region more. "
            "Watch a soothe2 tutorial: the same control names sit in the same places.");
    }
    if (b == &cfgBtn) openQualityMenu();
    for (int i = 0; i < 8; ++i) if (b == &typeBtn[i]) proc.setValue(P_BAND(selectedBand, 5), (float)i);
    refreshTexts();
}

void SilkEditor::openPresetMenu() {
    juce::PopupMenu m;
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        m.addItem(i + 1, proc.getProgramName(i), true, i == proc.getCurrentProgram());
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetBtn),
        [this](int r){ if (r > 0) proc.setCurrentProgram(r - 1); });
}
void SilkEditor::openVocalMenu() {
    juce::PopupMenu m;
    const char* names[] = { "Off", "Harsh (2-5 kHz)", "Sibilance (5-10 kHz)", "Mud / Boom (150-400)", "Nasal (800-1.5k)", "Ring", "Breath / Air" };
    int cur = (int)std::lround(proc.value(P_VOCAL_MODE));
    for (int i = 0; i < 7; ++i) m.addItem(i + 1, names[i], true, i == cur);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&vocalBtn), [this](int r){
        if (r > 0) proc.applyVocalMode((VocalMode)(r - 1));
    });
}
void SilkEditor::openQualityMenu() {
    juce::PopupMenu m;
    int curR = (int)std::lround(proc.value(P_RESOLUTION));
    int curO = (int)std::lround(proc.value(P_OVERSAMPLE));
    m.addSectionHeader("Resolution");
    const char* rs[] = { "Live", "Eco", "Normal", "High", "Ultra" };
    for (int i = 0; i < 5; ++i) m.addItem(10 + i, rs[i], true, i == curR);
    m.addSectionHeader("Oversample");
    const char* os[] = { "1x", "2x", "4x" };
    for (int i = 0; i < 3; ++i) m.addItem(20 + i, os[i], true, i == curO);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&qualityLabel), [this](int r){
        if (r >= 10 && r < 15) proc.setValue(P_RESOLUTION, (float)(r - 10));
        if (r >= 20 && r < 23) proc.setValue(P_OVERSAMPLE, (float)(r - 20));
        syncQualityLabel();
    });
}

juce::Rectangle<int> SilkEditor::graphInner() const {
    return graphR.reduced(8, 8).withTrimmedLeft(28).withTrimmedRight(30).withTrimmedTop(8).withTrimmedBottom(showBandCard ? 96 : 32);
}

float SilkEditor::yForSens(float sensDb, juce::Rectangle<float> gi) const {
    float y01 = (18.0f - sensDb) / 36.0f; // +18 top, -18 bottom, 0 dB mid
    return gi.getY() + juce::jlimit(0.0f, 1.0f, y01) * gi.getHeight();
}

juce::Rectangle<float> SilkEditor::nodePos(int b) const {
    auto g = graphInner().toFloat();
    float f = proc.value(P_BAND(b, 1));
    float sens = proc.value(P_BAND(b, 2));
    float x = g.getX() + g.getWidth() * (float)(std::log(std::max(20.0, (double)f) / 20.0) / std::log(20000.0 / 20.0));
    Params prm = proc.readParams();
    float s = (b == 0 || b == 5) ? (float)sensitivityDbAt(prm, f, 0) : sens;
    float y = yForSens(s, g);
    return { x - 8, y - 8, 16, 16 };
}

int SilkEditor::hitNode(juce::Point<float> p) const {
    int hit = -1; float best = 16;
    for (int b = 0; b < kNumBands; ++b) {
        auto r = nodePos(b);
        float d = r.getCentre().getDistanceFrom(p);
        if (d < best) { best = d; hit = b; }
    }
    return hit;
}

void SilkEditor::mouseDown(const juce::MouseEvent& e) {
    if (qualityLabel.getBounds().contains(e.getPosition()) && e.eventComponent == this) { openQualityMenu(); return; }
    auto p = e.position;
    // If the event came from a child, skip
    if (e.eventComponent != this) return;
    int b = hitNode(p);
    if (b >= 0) {
        selectedBand = b;
        dragging = true; dragBand = b;
        dragQ = e.mods.isCommandDown() || e.mods.isCtrlDown();
        dragStartY = p.y; dragStartSens = proc.value(P_BAND(b, 2)); dragStartQ = proc.value(P_BAND(b, 3));
        if (e.mods.isAltDown()) listenHeld = true;
        // rebind band knobs to this band — swap attachments
        auto idFor = [](int i){ return juce::String(silkParamId(i)); };
        bandFreq.at.reset(); bandSens.at.reset(); bandQ.at.reset(); bandBal.at.reset();
        bandFreq.at = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, idFor(P_BAND(b, 1)), bandFreq.s);
        bandSens.at = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, idFor(P_BAND(b, 2)), bandSens.s);
        bandQ.at = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, idFor(P_BAND(b, 3)), bandQ.s);
        bandBal.at = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, idFor(P_BAND(b, 4)), bandBal.s);
        bandOn.setToggleState(proc.value(P_BAND(b, 0)) >= 0.5f, juce::dontSendNotification);
        int typ = (int)std::lround(proc.value(P_BAND(b, 5)));
        for (int i = 0; i < 8; ++i) typeBtn[i].setToggleState(i == typ, juce::dontSendNotification);
        if (!showBandCard) { showBandCard = true; resized(); }
        refreshTexts();
        repaint();
    }
}

void SilkEditor::mouseDrag(const juce::MouseEvent& e) {
    if (!dragging || dragBand < 0) return;
    auto g = graphInner().toFloat();
    auto p = e.position;
    if (dragQ) {
        float dy = (dragStartY - p.y) / 80.0f;
        float q = juce::jlimit(0.1f, 8.0f, dragStartQ * std::pow(2.0f, dy));
        proc.setValue(P_BAND(dragBand, 3), q);
    } else {
        float x01 = juce::jlimit(0.0f, 1.0f, (p.x - g.getX()) / g.getWidth());
        float f = (float)(20.0 * std::pow(20000.0 / 20.0, (double)x01));
        float y01 = juce::jlimit(0.0f, 1.0f, (p.y - g.getY()) / g.getHeight());
        float sens = 18.0f - y01 * 27.0f;
        if (!e.mods.isShiftDown()) { /* coarse */ } else { /* already small moves from mouse */ }
        proc.setValue(P_BAND(dragBand, 1), f);
        if (dragBand != 0 && dragBand != 5)
            proc.setValue(P_BAND(dragBand, 2), juce::jlimit(-24.0f, 24.0f, sens));
        freqReadout.setText(proc.param(P_BAND(dragBand, 1))->getCurrentValueAsText(), juce::dontSendNotification);
    }
    refreshTexts();
}

void SilkEditor::mouseUp(const juce::MouseEvent&) { dragging = false; dragBand = -1; listenHeld = false; }
void SilkEditor::mouseDoubleClick(const juce::MouseEvent& e) {
    if (e.eventComponent != this) return;
    int b = hitNode(e.position);
    if (b >= 0) proc.setValue(P_BAND(b, 0), proc.value(P_BAND(b, 0)) >= 0.5f ? 0.f : 1.f);
}
void SilkEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
    int b = hitNode(e.position);
    if (b < 0) return;
    if (e.mods.isCommandDown() || e.mods.isCtrlDown()) {
        proc.setValue(P_BAND(b, 4), juce::jlimit(-1.f, 1.f, proc.value(P_BAND(b, 4)) + w.deltaY * 0.15f));
    } else {
        proc.setValue(P_BAND(b, 2), juce::jlimit(-24.f, 24.f, proc.value(P_BAND(b, 2)) + w.deltaY * 3.0f));
    }
}
bool SilkEditor::keyPressed(const juce::KeyPress&) { return false; }

void SilkEditor::paint(juce::Graphics& g) {
    g.fillAll(look.bg);
    // left / mid wells
    g.setColour(look.well.withMultipliedBrightness(0.97f));
    g.fillRect(leftCol);
    g.setColour(look.well);
    g.fillRect(midCol);

    // wordmark
    g.setColour(look.ink);
    g.setFont(title);
    g.drawText("SILK", leftCol.getX() + 16, 10, 180, 36, juce::Justification::centredLeft);
    g.setFont(small);
    g.setColour(look.inkMuted);
    g.drawText("koobacorp   v1.0.0", leftCol.getX() + 16, 44, 180, 16, juce::Justification::centredLeft);

    g.setFont(small);
    g.setColour(look.inkMuted);
    auto head = [&](juce::Rectangle<int> r, const char* s) {
        if (r.getHeight() > 0)
            g.drawText(s, r.getX() + 2, r.getY(), r.getWidth() - 4, r.getHeight(), juce::Justification::centredLeft);
    };
    head(stereoHead, "STEREO");
    head(speedHead, "SPEED");
    head(qualityHead, "QUALITY");
    head(scHead, "SIDECHAIN");
    head(outHead, "OUTPUT");

    paintGraph(g);
    if (showBandCard) paintBandCard(g);

    // resize corner
    g.setColour(look.inkMuted);
    g.drawLine((float)getWidth() - 16, (float)getHeight() - 4, (float)getWidth() - 4, (float)getHeight() - 16, 1.0f);
    g.drawLine((float)getWidth() - 10, (float)getHeight() - 4, (float)getWidth() - 4, (float)getHeight() - 10, 1.0f);
}

void SilkEditor::paintGraph(juce::Graphics& g) {
    auto outer = graphR.toFloat();
    g.setColour(look.header);
    g.fillRect(toolR);

    auto gi = graphInner().toFloat();
    float y0 = yForSens(0.0f, gi);
    g.setColour(look.sky);
    g.fillRect(gi.getX(), gi.getY(), gi.getWidth(), y0 - gi.getY());
    g.setColour(look.floor);
    g.fillRect(gi.getX(), y0, gi.getWidth(), gi.getBottom() - y0);

    // grid
    g.setColour(look.ink.withAlpha(0.12f));
    const float marks[] = { 63, 125, 250, 500, 1000, 2000, 4000, 8000, 16000 };
    g.setFont(small);
    for (float f : marks) {
        float x = gi.getX() + gi.getWidth() * (float)(std::log(f / 20.0) / std::log(20000.0 / 20.0));
        g.drawLine(x, gi.getY(), x, gi.getBottom(), 1.0f);
        g.setColour(look.inkMuted);
        juce::String lab = f >= 1000 ? juce::String((int)(f / 1000)) + "k" : juce::String((int)f);
        g.drawText(lab, (int)x - 14, toolR.getBottom(), 28, 14, juce::Justification::centred);
        g.setColour(look.ink.withAlpha(0.12f));
    }
    const float dbs[] = { 18, 12, 6, 0, -6, -12, -18 };
    for (float d : dbs) {
        float y = yForSens(d, gi);
        g.setColour(look.ink.withAlpha(d == 0 ? 0.25f : 0.10f));
        g.drawLine(gi.getX(), y, gi.getRight(), y, d == 0 ? 1.5f : 1.0f);
        g.setColour(look.inkMuted);
        g.drawText(juce::String((int)d), graphR.getX() + 2, (int)y - 7, 26, 14, juce::Justification::centredRight);
    }
    // reduction scale 0, -6, -12, -18 on the right of the floor
    for (int d = 0; d <= 18; d += 6) {
        float y = y0 + (d / 18.0f) * (gi.getBottom() - y0);
        g.setColour(look.inkMuted.withAlpha(0.85f));
        g.drawText(juce::String(-d), (int)gi.getRight() + 2, (int)y - 7, 26, 14, juce::Justification::centredLeft);
    }

    // reduction icicles (dispRed is reduction dB, positive = cut)
    juce::Path ice;
    bool started = false;
    for (int i = 0; i < SilkProcessor::kDisp; ++i) {
        double f0 = 20.0, f1 = 20000.0;
        double f = f0 * std::pow(f1 / f0, (double)i / (SilkProcessor::kDisp - 1));
        float x = gi.getX() + gi.getWidth() * (float)(std::log(f / 20.0) / std::log(20000.0 / 20.0));
        float red = proc.dispRed[(size_t)i].load();
        float y = y0 + juce::jlimit(0.0f, 1.0f, red / 18.0f) * (gi.getBottom() - y0);
        if (!started) { ice.startNewSubPath(x, y0); ice.lineTo(x, y); started = true; }
        else ice.lineTo(x, y);
    }
    ice.lineTo(gi.getRight(), y0);
    ice.closeSubPath();
    g.setColour(look.cream.withAlpha(0.85f));
    g.fillPath(ice);

    // sensitivity curve
    juce::Path curve;
    for (int i = 0; i < SilkProcessor::kDisp; ++i) {
        double f0 = 20.0, f1 = 20000.0;
        double f = f0 * std::pow(f1 / f0, (double)i / (SilkProcessor::kDisp - 1));
        float x = gi.getX() + gi.getWidth() * (float)(std::log(f / 20.0) / std::log(20000.0 / 20.0));
        float s = proc.dispSens[(size_t)i].load();
        // If telemetry is empty (no audio yet), evaluate the curve directly
        if (s == 0 && i == 0) {
            // still valid
        }
        Params prm = proc.readParams();
        s = (float)sensitivityDbAt(prm, f, 0);
        float y = yForSens(s, gi);
        if (i == 0) curve.startNewSubPath(x, y); else curve.lineTo(x, y);
    }
    g.setColour(look.white);
    g.strokePath(curve, juce::PathStrokeType(2.0f));

    // 0 dB reference (already drawn) — a faint warm line
    g.setColour(juce::Colour(0xffc88880).withAlpha(0.5f));
    g.drawLine(gi.getX(), y0, gi.getRight(), y0, 1.0f);

    // nodes
    for (int b = 0; b < kNumBands; ++b) {
        auto np = nodePos(b);
        // place node on the actual curve at its frequency
        float f = proc.value(P_BAND(b, 1));
        Params prm = proc.readParams();
        float s = (float)sensitivityDbAt(prm, f, 0);
        auto gi2 = graphInner().toFloat();
        float x = gi2.getX() + gi2.getWidth() * (float)(std::log(std::max(20.0, (double)f) / 20.0) / std::log(20000.0 / 20.0));
        float y = yForSens(s, gi2);
        juce::ignoreUnused(np);
        bool on = proc.value(P_BAND(b, 0)) >= 0.5f;
        auto c = look.node[b];
        if (on) g.setColour(c); else g.setColour(c.withAlpha(0.35f));
        if (on) g.fillEllipse(x - 7, y - 7, 14, 14);
        else { g.drawEllipse(x - 7, y - 7, 14, 14, 2.0f); }
        if (b == selectedBand) {
            g.setColour(look.white);
            g.drawEllipse(x - 10, y - 10, 20, 20, 1.5f);
        }
    }
}

void SilkEditor::paintBandCard(juce::Graphics& g) {
    if (cardR.getHeight() < 20) return;
    g.setColour(look.softFill.withAlpha(0.92f));
    g.fillRoundedRectangle(cardR.toFloat(), 10.0f);
    g.setColour(look.ink.withAlpha(0.15f));
    g.drawRoundedRectangle(cardR.toFloat(), 10.0f, 1.0f);
    g.setColour(look.node[selectedBand]);
    g.fillEllipse((float)cardR.getX() + 14, (float)cardR.getY() + 14, 12, 12);
    juce::ignoreUnused(g);
}

