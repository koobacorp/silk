#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace silk;

static const char* bandField[7] = { "on", "freq", "sens", "q", "bal", "type", "slope" };
static const char* bandName[6] = { "lc", "y", "o", "r", "p", "hc" };

static std::array<std::string, kNumParams> idStore;
static std::array<const char*, kNumParams> idPtr;
static bool idsReady = false;
static void ensureIds() {
    if (idsReady) return;
    auto set = [](int i, const char* s) { idStore[(size_t)i] = s; idPtr[(size_t)i] = idStore[(size_t)i].c_str(); };
    set(P_HARD, "hard"); set(P_DEPTH, "depth"); set(P_SHARPNESS, "sharpness"); set(P_SELECTIVITY, "selectivity");
    set(P_STEREO_MODE, "stereoMode"); set(P_LINK, "link"); set(P_BALANCE, "balance");
    set(P_ATTACK, "attack"); set(P_RELEASE, "release");
    set(P_OVERSAMPLE, "oversample"); set(P_RESOLUTION, "resolution"); set(P_QUALITY_LOCK, "qualityLock");
    set(P_OFFLINE_SAME, "offlineSame"); set(P_OFFLINE_OS, "offlineOs"); set(P_OFFLINE_RES, "offlineRes");
    set(P_SIDECHAIN, "sidechain"); set(P_SC_LISTEN, "scListen");
    set(P_MIX, "mix"); set(P_TRIM, "trim"); set(P_AUTO_TRIM, "autoTrim");
    set(P_DELTA, "delta"); set(P_DELTA_LOCK, "deltaLock"); set(P_BYPASS, "bypass"); set(P_INPUT_TRIM, "inputTrim");
    set(P_VOCAL_MODE, "vocalMode");
    for (int b = 0; b < kNumBands; ++b)
        for (int f = 0; f < kBandStride; ++f) {
            idStore[(size_t)P_BAND(b, f)] = std::string("b") + bandName[b] + "_" + bandField[f];
            idPtr[(size_t)P_BAND(b, f)] = idStore[(size_t)P_BAND(b, f)].c_str();
        }
    idsReady = true;
}
const char* silkParamId(int i) {
    ensureIds();
    if (i < 0 || i >= kNumParams) return "";
    return idPtr[(size_t)i];
}

static juce::String freqText(float f) {
    if (f >= 10000) return juce::String(f / 1000.0, 1) + "k";
    if (f >= 1000) {
        int hundreds = (int)std::lround(f / 100.0);
        int k = hundreds / 10, h = hundreds % 10;
        return h ? juce::String(k) + "k" + juce::String(h) : juce::String(k) + "k0";
    }
    if (f >= 100) return juce::String((int)std::lround(f));
    return juce::String(f, 1);
}

static juce::AudioParameterFloat* fl(int id, const juce::String& name, juce::NormalisableRange<float> r, float def,
                                    std::function<juce::String(float, int)> text = {}) {
    ensureIds();
    auto p = std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ idPtr[(size_t)id], 1 }, name, r, def, juce::AudioParameterFloatAttributes()
            .withLabel({}).withStringFromValueFunction(text ? text : [](float v, int){ return juce::String(v, 2); }));
    return p.release();
}
static juce::AudioParameterBool* bl(int id, const juce::String& name, bool def) {
    ensureIds();
    return new juce::AudioParameterBool(juce::ParameterID{ idPtr[(size_t)id], 1 }, name, def);
}
static juce::AudioParameterChoice* ch(int id, const juce::String& name, const juce::StringArray& items, int def) {
    ensureIds();
    return new juce::AudioParameterChoice(juce::ParameterID{ idPtr[(size_t)id], 1 }, name, items, def);
}
static juce::AudioParameterInt* iv(int id, const juce::String& name, int mn, int mx, int def) {
    ensureIds();
    return new juce::AudioParameterInt(juce::ParameterID{ idPtr[(size_t)id], 1 }, name, mn, mx, def);
}

juce::AudioProcessorValueTreeState::ParameterLayout SilkProcessor::layout() {
    ensureIds();
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto add = [&](juce::RangedAudioParameter* p) { l.add(std::unique_ptr<juce::RangedAudioParameter>(p)); };
    add(bl(P_HARD, "Hard", false));
    add(fl(P_DEPTH, "Depth", { kDepthMin, kDepthMax, 0.01f }, kDepthDef, [](float v, int){ return juce::String(v, 1); }));
    add(fl(P_SHARPNESS, "Sharpness", { 0, 10, 0.01f }, kSharpDef, [](float v, int){ return juce::String(v, 1); }));
    add(fl(P_SELECTIVITY, "Selectivity", { 0, 10, 0.01f }, kSelDef, [](float v, int){ return juce::String(v, 1); }));
    add(ch(P_STEREO_MODE, "Mode", { "left | right", "mid | side" }, 0));
    add(fl(P_LINK, "Link", { 0, 100, 0.1f }, 100, [](float v, int){ return juce::String((int)std::lround(v)) + " %"; }));
    add(fl(P_BALANCE, "Balance", { -1, 1, 0.001f }, 0, [](float v, int){
        if (std::fabs(v) < 0.02f) return juce::String("even");
        return juce::String(v, 2);
    }));
    add(fl(P_ATTACK, "Attack", { 0, 10, 0.01f }, kAttackDef, [](float v, int){ return juce::String(v, 1); }));
    add(fl(P_RELEASE, "Release", { 0, 10, 0.01f }, kReleaseDef, [](float v, int){
        if (std::fabs(v - 2.0f) < 0.2f) return juce::String("fast");
        if (v < 1.0f) return juce::String("fastest");
        if (v < 3.5f) return juce::String("fast");
        if (v < 6.5f) return juce::String("mid");
        if (v < 8.5f) return juce::String("slow");
        return juce::String("slowest");
    }));
    add(ch(P_OVERSAMPLE, "Oversample", { "1x", "2x", "4x" }, 0));
    add(ch(P_RESOLUTION, "Resolution", { "Live", "Eco", "Normal", "High", "Ultra" }, 2));
    add(bl(P_QUALITY_LOCK, "Lock quality", false));
    add(bl(P_OFFLINE_SAME, "Offline same as real-time", true));
    add(ch(P_OFFLINE_OS, "Offline oversample", { "1x", "2x", "4x" }, 0));
    add(ch(P_OFFLINE_RES, "Offline resolution", { "Live", "Eco", "Normal", "High", "Ultra" }, 2));
    add(bl(P_SIDECHAIN, "Sidechain", false));
    add(bl(P_SC_LISTEN, "Sidechain listen", false));
    add(fl(P_MIX, "Mix", { 0, 100, 0.1f }, 100, [](float v, int){ return juce::String((int)std::lround(v)) + " %"; }));
    add(fl(P_TRIM, "Trim", { -12, 12, 0.01f }, 0, [](float v, int){ return (std::fabs(v) < 0.05f ? juce::String("0.0") : juce::String(v, 1)) + " dB"; }));
    add(bl(P_AUTO_TRIM, "Auto trim", false));
    add(bl(P_DELTA, "Delta", false));
    add(bl(P_DELTA_LOCK, "Delta lock", false));
    add(bl(P_BYPASS, "Bypass", false));
    add(fl(P_INPUT_TRIM, "Input trim", { -24, 24, 0.01f }, 0, [](float v, int){ return juce::String(v, 1) + " dB"; }));
    add(ch(P_VOCAL_MODE, "Vocal", { "Off", "Harsh", "Sibilance", "Mud / Boom", "Nasal", "Ring", "Breath / Air" }, 0));

    juce::NormalisableRange<float> freqR(20.0f, 20000.0f, 0.001f);
    freqR.setSkewForCentre(1000.0f);
    const float defF[6] = { 80, 350, 2000, 4500, 10000, 18000 };
    const float defS[6] = { 0, 0, 0, 0.5f, 1.0f, 0 };
    const int defT[6] = { 6, 0, 0, 0, 0, 7 }; // LowCut, peaking x4, HighCut
    const int defSlope[6] = { 24, 12, 12, 12, 12, 12 };
    for (int b = 0; b < kNumBands; ++b) {
        juce::String bn = juce::String(bandName[b]);
        add(bl(P_BAND(b, 0), "Band " + bn + " on", true));
        add(fl(P_BAND(b, 1), "Band " + bn + " freq", freqR, defF[b], [](float v, int){ return freqText(v); }));
        add(fl(P_BAND(b, 2), "Band " + bn + " sens", { -24, 24, 0.01f }, defS[b], [](float v, int){ return juce::String(v, 1) + " dB"; }));
        add(fl(P_BAND(b, 3), "Band " + bn + " Q", { 0.1f, 8.0f, 0.001f }, 0.5f, [](float v, int){ return juce::String(v, 2); }));
        add(fl(P_BAND(b, 4), "Band " + bn + " balance", { -1, 1, 0.001f }, 0, [](float v, int){ return std::fabs(v)<0.02f ? juce::String("even") : juce::String(v, 2); }));
        add(ch(P_BAND(b, 5), "Band " + bn + " type",
               { "peaking", "low shelf", "high shelf", "band shelf", "band reject", "tilt", "low cut", "high cut" }, defT[b]));
        add(ch(P_BAND(b, 6), "Band " + bn + " slope", { "6", "12", "18", "24", "36", "48" },
               defSlope[b] == 6 ? 0 : defSlope[b] == 12 ? 1 : defSlope[b] == 18 ? 2 : defSlope[b] == 24 ? 3 : defSlope[b] == 36 ? 4 : 5));
    }
    return l;
}

SilkProcessor::SilkProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
        .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts(*this, &undo, "SILK", layout())
{
    ensureIds();
    for (int i = 0; i < kNumParams; ++i) {
        params[(size_t)i] = apvts.getParameter(idPtr[(size_t)i]);
        jassert(params[(size_t)i] != nullptr);
        raw[(size_t)i] = apvts.getRawParameterValue(idPtr[(size_t)i]);
    }
}

void SilkProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    sr = sampleRate; blockSize = samplesPerBlock;
    auto p = readParams();
    if (isNonRealtime() && !p.qualityLock) {
        if (!value(P_OFFLINE_SAME)) {
            p.oversample = (Oversample)(int)value(P_OFFLINE_OS);
            p.resolution = (Resolution)(int)value(P_OFFLINE_RES);
        }
    }
    eng.prepare(sr, samplesPerBlock);
    eng.setParams(p);
    lastRes = (int)p.resolution; lastOs = (int)p.oversample;
    setLatencySamples(eng.latencySamples());
    latencyMs.store(eng.latencyMs());
    scScratch.setSize(2, samplesPerBlock);
}

void SilkProcessor::reset() { eng.prepare(sr, blockSize); setLatencySamples(eng.latencySamples()); }

bool SilkProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (in != out) return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    auto sc = l.getChannelSet(true, 1);
    if (!sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo()) return false;
    return true;
}

silk::Params SilkProcessor::readParams() const {
    Params p;
    p.hard = value(P_HARD) >= 0.5f;
    p.depth = value(P_DEPTH);
    p.sharpness = value(P_SHARPNESS);
    p.selectivity = value(P_SELECTIVITY);
    p.stereoMode = value(P_STEREO_MODE) >= 0.5f ? StereoMode::MidSide : StereoMode::LeftRight;
    p.link = value(P_LINK);
    p.balance = value(P_BALANCE);
    p.attack = value(P_ATTACK);
    p.release = value(P_RELEASE);
    p.oversample = (Oversample)juce::jlimit(0, 2, (int)std::lround(value(P_OVERSAMPLE)));
    p.resolution = (Resolution)juce::jlimit(0, 4, (int)std::lround(value(P_RESOLUTION)));
    p.qualityLock = value(P_QUALITY_LOCK) >= 0.5f;
    p.sidechain = value(P_SIDECHAIN) >= 0.5f;
    p.sidechainListen = value(P_SC_LISTEN) >= 0.5f;
    p.mix = value(P_MIX);
    p.trim = value(P_TRIM);
    p.autoTrim = value(P_AUTO_TRIM) >= 0.5f;
    p.delta = value(P_DELTA) >= 0.5f;
    p.deltaLock = value(P_DELTA_LOCK) >= 0.5f;
    p.bypass = value(P_BYPASS) >= 0.5f;
    p.inputTrim = value(P_INPUT_TRIM);
    static const int slopes[] = { 6, 12, 18, 24, 36, 48 };
    for (int b = 0; b < kNumBands; ++b) {
        auto& bd = p.band[(size_t)b];
        bd.on = value(P_BAND(b, 0)) >= 0.5f;
        bd.freq = value(P_BAND(b, 1));
        bd.sens = value(P_BAND(b, 2));
        bd.q = value(P_BAND(b, 3));
        bd.balance = value(P_BAND(b, 4));
        bd.type = (BandType)juce::jlimit(0, 7, (int)std::lround(value(P_BAND(b, 5))));
        int si = juce::jlimit(0, 5, (int)std::lround(value(P_BAND(b, 6))));
        bd.slope = slopes[si];
    }
    return p;
}

void SilkProcessor::applyParams(const silk::Params& p, bool alsoQuality) {
    auto set = [this](int i, float v) {
        if (auto* prm = params[(size_t)i]) {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost(prm->convertTo0to1(v));
            prm->endChangeGesture();
        }
    };
    set(P_HARD, p.hard ? 1.0f : 0.0f);
    set(P_DEPTH, p.depth); set(P_SHARPNESS, p.sharpness); set(P_SELECTIVITY, p.selectivity);
    set(P_STEREO_MODE, p.stereoMode == StereoMode::MidSide ? 1.0f : 0.0f);
    set(P_LINK, p.link); set(P_BALANCE, p.balance);
    set(P_ATTACK, p.attack); set(P_RELEASE, p.release);
    if (alsoQuality || !value(P_QUALITY_LOCK)) {
        set(P_OVERSAMPLE, (float)(int)p.oversample);
        set(P_RESOLUTION, (float)(int)p.resolution);
    }
    set(P_SIDECHAIN, p.sidechain ? 1.f : 0.f);
    set(P_SC_LISTEN, p.sidechainListen ? 1.f : 0.f);
    set(P_MIX, p.mix); set(P_TRIM, p.trim); set(P_AUTO_TRIM, p.autoTrim ? 1.f : 0.f);
    if (!value(P_DELTA_LOCK)) set(P_DELTA, p.delta ? 1.f : 0.f);
    set(P_BYPASS, p.bypass ? 1.f : 0.f); set(P_INPUT_TRIM, p.inputTrim);
    auto slopeIdx = [](int s){ return s<=6?0:s<=12?1:s<=18?2:s<=24?3:s<=36?4:5; };
    for (int b = 0; b < kNumBands; ++b) {
        const auto& bd = p.band[(size_t)b];
        set(P_BAND(b, 0), bd.on ? 1.f : 0.f);
        set(P_BAND(b, 1), bd.freq); set(P_BAND(b, 2), bd.sens); set(P_BAND(b, 3), bd.q);
        set(P_BAND(b, 4), bd.balance); set(P_BAND(b, 5), (float)(int)bd.type);
        set(P_BAND(b, 6), (float)slopeIdx(bd.slope));
    }
}

void SilkProcessor::setValue(int i, float realValue) {
    if (auto* prm = params[(size_t)i]) {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost(prm->convertTo0to1(realValue));
        prm->endChangeGesture();
    }
}

void SilkProcessor::maybeReconfig(const Params& p) {
    int res = (int)p.resolution, os = (int)p.oversample;
    if (isNonRealtime() && value(P_OFFLINE_SAME) < 0.5f) {
        res = (int)value(P_OFFLINE_RES); os = (int)value(P_OFFLINE_OS);
    }
    if (res != lastRes || os != lastOs) {
        Params q = p; q.resolution = (Resolution)res; q.oversample = (Oversample)os;
        eng.prepare(sr, blockSize);
        eng.setParams(q);
        lastRes = res; lastOs = os;
        setLatencySamples(eng.latencySamples());
        latencyMs.store(eng.latencyMs());
    } else {
        eng.setParams(p);
    }
}

void SilkProcessor::run(juce::AudioBuffer<float>& buffer, bool forceBypass) {
    const int n = buffer.getNumSamples();
    const int nOut = buffer.getNumChannels();
    if (nOut <= 0 || n <= 0) return;

    auto p = readParams();
    if (forceBypass) p.bypass = true;
    maybeReconfig(p);

    float* L = buffer.getWritePointer(0);
    float* R = nOut > 1 ? buffer.getWritePointer(1) : L;
    const float* scL = nullptr; const float* scR = nullptr;
    juce::AudioBuffer<float> scBuf;
    if (getBusCount(true) > 1) {
        scBuf = getBusBuffer(buffer, true, 1);
        if (scBuf.getNumSamples() == n && scBuf.getNumChannels() > 0) {
            scL = scBuf.getReadPointer(0);
            scR = scBuf.getNumChannels() > 1 ? scBuf.getReadPointer(1) : scL;
        }
    }
    // Mono: process L=R
    if (nOut == 1) {
        scScratch.setSize(1, n, false, false, true);
        // already L only
    }
    juce::ScopedNoDenormals nd;
    eng.process(L, R, n, scL, scR);
    if (nOut > 1 && buffer.getNumChannels() > 1 && L == R) {}
    pushTelemetry();
}

void SilkProcessor::pushTelemetry() {
    inLevel.store(eng.inPeak());
    outLevel.store(eng.outPeak());
    maxRed.store(eng.maxReductionDb());
    ceilingEngaged.store(eng.ceilingLit() ? 1 : 0);
    latencyMs.store(eng.latencyMs());
    float sL[kDisp], rL[kDisp];
    eng.copyDisplay(sL, nullptr, rL, nullptr, kDisp);
    for (int i = 0; i < kDisp; ++i) { dispSens[(size_t)i].store(sL[i]); dispRed[(size_t)i].store(rL[i]); }
    if (eng.learnReady()) {
        Params p = readParams();
        eng.takeLearn(p);
        applyParams(p, false);
    }
}

void SilkProcessor::processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer&) { run(b, false); }
void SilkProcessor::processBlockBypassed(juce::AudioBuffer<float>& b, juce::MidiBuffer&) { run(b, true); }

void SilkProcessor::startLearn(double seconds) { eng.startLearn(seconds); }
void SilkProcessor::cancelLearn() { eng.cancelLearn(); }
void SilkProcessor::applyVocalMode(VocalMode m) {
    Params p = readParams();
    silk::applyVocalMode(p, m);
    applyParams(p, false);
    setValue(P_VOCAL_MODE, (float)(int)m);
}

void SilkProcessor::captureA() { stateA = apvts.copyState(); }
void SilkProcessor::toggleAB() {
    auto cur = apvts.copyState();
    if (sideA) { stateA = cur; if (stateB.isValid()) apvts.replaceState(stateB); }
    else { stateB = cur; if (stateA.isValid()) apvts.replaceState(stateA); }
    sideA = !sideA;
}
void SilkProcessor::copyAB() {
    auto cur = apvts.copyState();
    if (sideA) stateB = cur; else stateA = cur;
}

int SilkProcessor::getNumPrograms() { return (int)factoryPresets().size(); }
const juce::String SilkProcessor::getProgramName(int i) {
    auto& L = factoryPresets();
    if (i < 0 || i >= (int)L.size()) return {};
    return L[(size_t)i].name;
}
void SilkProcessor::setCurrentProgram(int i) {
    auto& L = factoryPresets();
    if (i < 0 || i >= (int)L.size()) return;
    currentProgram = i;
    bool lockQ = value(P_QUALITY_LOCK) >= 0.5f;
    bool lockD = value(P_DELTA_LOCK) >= 0.5f;
    applyParams(L[(size_t)i].p, !lockQ);
    if (lockD) { /* applyParams already respects delta lock */ }
}

void SilkProcessor::getStateInformation(juce::MemoryBlock& dest) {
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, dest);
}
void SilkProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* SilkProcessor::createEditor() { return new SilkEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SilkProcessor(); }
