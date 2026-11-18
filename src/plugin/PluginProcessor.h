#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SilkEngine.h"
#include "SilkPresets.h"

enum ParamIndex : int {
    P_HARD = 0, P_DEPTH, P_SHARPNESS, P_SELECTIVITY,
    P_STEREO_MODE, P_LINK, P_BALANCE,
    P_ATTACK, P_RELEASE,
    P_OVERSAMPLE, P_RESOLUTION, P_QUALITY_LOCK,
    P_OFFLINE_SAME, P_OFFLINE_OS, P_OFFLINE_RES,
    P_SIDECHAIN, P_SC_LISTEN,
    P_MIX, P_TRIM, P_AUTO_TRIM, P_DELTA, P_DELTA_LOCK, P_BYPASS, P_INPUT_TRIM,
    P_VOCAL_MODE,
    P_BAND_BASE,
    kBandStride = 7,
    kNumParams = P_BAND_BASE + silk::kNumBands * kBandStride
};

inline int P_BAND(int b, int field) { return P_BAND_BASE + b * kBandStride + field; } // 0 on,1 freq,2 sens,3 q,4 bal,5 type,6 slope
const char* silkParamId(int i);

class SilkProcessor : public juce::AudioProcessor {
public:
    SilkProcessor();
    ~SilkProcessor() override = default;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();

    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return params[P_BYPASS]; }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SILK"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::RangedAudioParameter* param(int i) const { return params[(size_t)i]; }
    float value(int i) const { return raw[(size_t)i]->load(std::memory_order_relaxed); }
    void setValue(int i, float realValue);
    silk::Params readParams() const;
    void applyParams(const silk::Params& p, bool alsoQuality = true);

    int latencyNow() const { return getLatencySamples(); }
    double sampleRateNow() const { return sr; }
    const silk::Engine& engine() const { return eng; }
    silk::Engine& engine() { return eng; }

    void startLearn(double seconds = 3.0);
    void cancelLearn();
    void applyVocalMode(silk::VocalMode m);

    // A/B
    void toggleAB();
    void copyAB();
    bool isSideA() const { return sideA; }
    void captureA();

    std::atomic<float> inLevel { 0 }, outLevel { 0 }, maxRed { 0 }, latencyMs { 0 };
    std::atomic<int> ceilingEngaged { 0 };
    static constexpr int kDisp = silk::kDispBins;
    std::array<std::atomic<float>, kDisp> dispSens {}, dispRed {};

private:
    void run(juce::AudioBuffer<float>&, bool forceBypass);
    void pushTelemetry();
    void maybeReconfig(const silk::Params& p);
    silk::Engine eng;
    std::array<juce::RangedAudioParameter*, kNumParams> params {};
    std::array<std::atomic<float>*, kNumParams> raw {};
    double sr = 44100; int blockSize = 512; int currentProgram = 0;
    int lastRes = -1, lastOs = -1, lastOffline = -1;
    juce::ValueTree stateA, stateB; bool sideA = true;
    juce::UndoManager undo;
    juce::AudioBuffer<float> scScratch;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SilkProcessor)
};
