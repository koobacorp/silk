// SILK — spectral resonance suppressor engine (no JUCE). Original code.
// STFT analysis, per-bin reduction from prominence over a smoothed envelope,
// sensitivity curve as a focus mask, Mix/Trim/Delta, latency-matched dry, safety ceiling.
#pragma once
#include "SilkDsp.h"
#include <array>
#include <cstring>

namespace silk {

enum class Resolution { Live = 0, Eco, Normal, High, Ultra };
enum class Oversample { X1 = 0, X2, X4 };
enum class StereoMode { LeftRight = 0, MidSide };
enum class BandType { Peaking = 0, LowShelf, HighShelf, BandShelf, BandReject, Tilt, LowCut, HighCut };

constexpr int kNumBands = 6;          // 0 = low cut, 1-4 = Y/O/R/P, 5 = high cut
constexpr int kMaxFft = 16384;
constexpr int kDispBins = 256;
constexpr float kDepthMin = -18.0f, kDepthMax = 18.0f, kDepthDef = 3.0f;
constexpr float kSharpDef = 4.8f, kSelDef = 3.6f, kAttackDef = 1.0f, kReleaseDef = 2.0f;
constexpr float kCeilingDb = -0.1f;

enum class VocalMode { Off = 0, Harsh, Sibilance, MudBoom, Nasal, Ring, BreathAir };

struct Band {
    bool on = true;
    float freq = 1000.0f;     // Hz
    float sens = 0.0f;        // estimated dB (boost = more reduction)
    float q = 0.5f;
    float balance = 0.0f;     // -1 .. +1 (left/mid .. right/side)
    BandType type = BandType::Peaking;
    int slope = 24;           // dB/oct for cuts: 6,12,18,24,36,48
};

struct Params {
    bool hard = false;
    float depth = kDepthDef;
    float sharpness = kSharpDef;
    float selectivity = kSelDef;
    StereoMode stereoMode = StereoMode::LeftRight;
    float link = 100.0f;
    float balance = 0.0f;
    float attack = kAttackDef;
    float release = kReleaseDef;
    Oversample oversample = Oversample::X1;
    Resolution resolution = Resolution::Normal;
    bool qualityLock = false;
    bool sidechain = false;
    bool sidechainListen = false;
    float mix = 100.0f;
    float trim = 0.0f;          // dB, wet only
    bool autoTrim = false;
    bool delta = false;
    bool deltaLock = false;
    bool bypass = false;
    float inputTrim = 0.0f;     // hidden, dB
    std::array<Band, kNumBands> band {};
    Params() { setFactoryDefault(); }
    void setFactoryDefault();
    void setFlatStart();
};


struct Engine;

// Apply a vocal-mode starting curve onto an existing Params (keeps Depth etc.).
void applyVocalMode(Params& p, VocalMode m);

// Quality → FFT / hop at a given sample rate.
inline void qualitySizes(Resolution r, double sr, int& N, int& H) {
    double scale = std::max(0.5, sr / 48000.0);
    auto p2 = [](int x) { int n = 64; while (n < x) n <<= 1; return std::min(n, kMaxFft); };
    switch (r) {
        case Resolution::Live:   N = p2((int)std::lround(256 * scale));  H = std::max(32, N / 4); break;
        case Resolution::Eco:    N = p2((int)std::lround(2048 * scale)); H = std::max(64, N / 2); break;
        case Resolution::Normal: N = p2((int)std::lround(2048 * scale)); H = std::max(64, N / 4); break;
        case Resolution::High:   N = p2((int)std::lround(2048 * scale)); H = std::max(32, N / 8); break;
        case Resolution::Ultra:  N = p2((int)std::lround(4096 * scale)); H = std::max(32, N / 8); break;
    }
}
inline int oversampleFactor(Oversample o) { return o == Oversample::X4 ? 4 : (o == Oversample::X2 ? 2 : 1); }

// -------- sensitivity curve --------
inline double hpLpDb(double f, double fc, int order, bool highpass) {
    if (fc < 1.0) fc = 1.0;
    if (f < 1e-6) f = 1e-6;
    double n = std::max(1, order);
    double r = highpass ? (f / fc) : (fc / f);
    double r2n = std::pow(std::max(r, 0.0), 2.0 * n);
    return 10.0 * std::log10(r2n / (1.0 + r2n) + 1e-30);
}
inline double logGauss(double f, double f0, double q) {
    if (f <= 0 || f0 <= 0) return 0;
    double oct = std::log2(f / f0);
    double w = 0.5 / std::max(0.05, (double)q); // higher Q = narrower
    return std::exp(-0.5 * (oct / w) * (oct / w));
}
inline double bandResponseDb(const Band& b, double f) {
    if (!b.on) return 0;
    switch (b.type) {
        case BandType::LowCut:  return hpLpDb(f, b.freq, std::max(1, b.slope / 6), true);
        case BandType::HighCut: return hpLpDb(f, b.freq, std::max(1, b.slope / 6), false);
        case BandType::Peaking: return (double)b.sens * logGauss(f, b.freq, b.q);
        case BandType::LowShelf: {
            // shelf below f0 tends toward sens, above toward 0
            double x = 1.0 / (1.0 + std::pow(f / std::max(1.0, (double)b.freq), 2.0 * std::max(0.3, (double)b.q * 2)));
            return (double)b.sens * x;
        }
        case BandType::HighShelf: {
            double x = 1.0 / (1.0 + std::pow(std::max(1.0, (double)b.freq) / std::max(f, 1e-6), 2.0 * std::max(0.3, (double)b.q * 2)));
            return (double)b.sens * x;
        }
        case BandType::BandShelf: {
            // flatter top than peaking
            double g = logGauss(f, b.freq, std::max(0.15f, b.q * 0.45f));
            return (double)b.sens * std::pow(g, 0.4);
        }
        case BandType::BandReject: return -(double)std::fabs(b.sens) * logGauss(f, b.freq, b.q);
        case BandType::Tilt: {
            double oct = std::log2(std::max(f, 1.0) / std::max((double)b.freq, 1.0));
            return (double)b.sens * std::clamp(oct / 4.0, -1.0, 1.0);
        }
    }
    return 0;
}
inline double sensitivityDbAt(const Params& p, double f, int channel /*0=L/M, 1=R/S*/) {
    double s = 0;
    for (int i = 0; i < kNumBands; ++i) {
        const Band& b = p.band[(size_t)i];
        double resp = bandResponseDb(b, f);
        // per-band balance: reduce this band's contribution on the opposite channel
        float bal = b.balance;
        double w = 1.0;
        if (channel == 0) w = 1.0 - std::max(0.0, (double)bal);
        else              w = 1.0 - std::max(0.0, (double)-bal);
        s += resp * w;
    }
    return s;
}

// -------- Params factory (avoid recursive assignment) --------
inline void fillFactoryBands(std::array<Band, kNumBands>& b) {
    b[0] = Band{ true, 80.0f,    0.0f, 0.7f, 0, BandType::LowCut,  24 };
    b[1] = Band{ true, 350.0f,   0.0f, 0.50f, 0, BandType::Peaking, 12 };
    b[2] = Band{ true, 2000.0f,  0.0f, 0.50f, 0, BandType::Peaking, 12 };
    b[3] = Band{ true, 4500.0f,  0.5f, 0.60f, 0, BandType::Peaking, 12 };
    b[4] = Band{ true, 10000.0f, 1.0f, 0.50f, 0, BandType::Peaking, 12 };
    b[5] = Band{ true, 18000.0f, 0.0f, 0.7f, 0, BandType::HighCut, 12 };
}
inline void fillFlatBands(std::array<Band, kNumBands>& b) {
    b[0] = Band{ false, 80.0f,    0, 0.7f, 0, BandType::LowCut,  24 };
    b[1] = Band{ false, 350.0f,   0, 0.5f, 0, BandType::Peaking, 12 };
    b[2] = Band{ false, 2000.0f,  0, 0.5f, 0, BandType::Peaking, 12 };
    b[3] = Band{ false, 4500.0f,  0, 0.5f, 0, BandType::Peaking, 12 };
    b[4] = Band{ false, 10000.0f, 0, 0.5f, 0, BandType::Peaking, 12 };
    b[5] = Band{ false, 18000.0f, 0, 0.7f, 0, BandType::HighCut, 12 };
}

// Redefine factory methods here (the ones on the struct will be replaced by these after we fix the header).
// We'll implement setFactoryDefault in a way that doesn't recurse: do it as free functions called from ctor.

inline Params makeFactory() {
    Params p; // this will call setFactoryDefault if we're not careful.
    return p;
}

struct Engine {
    void prepare(double sr, int maxBlock);
    void setParams(const Params& p);
    const Params& params() const { return p; }
    void process(float* L, float* R, int n,
                 const float* scL = nullptr, const float* scR = nullptr);
    int latencySamples() const { return latency; }
    int frameSize() const { return N; }
    int hopSize() const { return H; }
    int fftSize() const { return M; }
    int limiterLookahead() const { return lim.la; }
    float latencyMs() const { return sr > 0 ? (float)(latency / sr * 1000.0) : 0; }
    const SafetyLimiter& limiter() const { return lim; }
    SafetyLimiter& limiter() { return lim; }

    // Telemetry for the editor (log-spaced display).
    void copyDisplay(float* sensL, float* sensR, float* redL, float* redR, int points) const;
    float inPeak() const { return inPk; }
    float outPeak() const { return outPk; }
    float maxReductionDb() const { return maxRed; }
    bool ceilingLit() const { return lim.engaged > 0; }
    float autoTrimDb() const { return autoTrimValue; }

    // Learn: feed audio via process(); call startLearn() then poll.
    void startLearn(double seconds = 3.0);
    void cancelLearn();
    bool isLearning() const { return learning; }
    float learnProgress() const { return learnNeed > 0 ? (float)learnHave / (float)learnNeed : 0; }
    bool learnReady() const { return learnFinished; }
    void takeLearn(Params& dest); // apply suggested nodes into dest (and live params)

    double sampleRate() const { return sr; }

private:
    void reconfig();
    void processFrame(int ch, const float* analysisSrc, float* re, float* im);
    void runHop();
    void finishLearnIfNeeded();

    Params p {};
    double sr = 44100;
    int maxBlock = 512;
    int N = 2048, H = 512, M = 2048, os = 1;
    int stftDelay = 0, latency = 0;
    int hopFill = 0, inW = 0, olaW = 0, histLen = 0;

    FFT fft;
    SafetyLimiter lim;
    Delay dryL, dryR, scDelayL, scDelayR;

    std::vector<float> win, synthWin;
    std::vector<float> histL, histR, histScL, histScR;
    std::vector<float> olaL, olaR, olaReady;
    std::vector<float> fifoL, fifoR; int fifoRpos = 0, fifoWpos = 0, fifoSize = 0, fifoAvail = 0;
    std::vector<float> re, im, mag, env, prom, red, redSm, lastRed[2], lastSens[2], binFreq;
    std::vector<float> frame;
    float colaGain = 1;
    float inPk = 0, outPk = 0, maxRed = 0, autoTrimValue = 0;
    double dryRmsEma = 0, wetRmsEma = 0;
    bool haveP = false;

    // Learn
    bool learning = false, learnFinished = false;
    int learnHave = 0, learnNeed = 0;
    std::vector<float> learnProm, learnEnv;
    int learnFrames = 0;

    // Channel workspace for two-pass stereo
    std::vector<float> tmpRe[2], tmpIm[2];
};

// ---------------- Params methods (non-recursive) ----------------
// Constructor already zero-initializes + we fill in the struct default member values.
// setFactoryDefault / setFlatStart:
inline void Params::setFlatStart() {
    hard = false; depth = kDepthMin; sharpness = kSharpDef; selectivity = kSelDef;
    stereoMode = StereoMode::LeftRight; link = 100; balance = 0;
    attack = kAttackDef; release = kReleaseDef;
    oversample = Oversample::X1; resolution = Resolution::Normal;
    qualityLock = false; sidechain = false; sidechainListen = false;
    mix = 100; trim = 0; autoTrim = false; delta = false; deltaLock = false; bypass = false; inputTrim = 0;
    fillFlatBands(band);
}

// Patch setFactoryDefault properly (the earlier inline was a stub).
// We provide a second function used by the ctor replacement below.

} // namespace silk

// Complete setFactoryDefault after the stub — we'll fix this with a cleaner approach in the cpp-less header
// by defining the body once. The stub above assigned Params() which would recurse. Replace it:

namespace silk {

inline void factoryFill(Params& p) {
    p.hard = false; p.depth = kDepthDef; p.sharpness = kSharpDef; p.selectivity = kSelDef;
    p.stereoMode = StereoMode::LeftRight; p.link = 100; p.balance = 0;
    p.attack = kAttackDef; p.release = kReleaseDef;
    p.oversample = Oversample::X1; p.resolution = Resolution::Normal;
    p.qualityLock = false; p.sidechain = false; p.sidechainListen = false;
    p.mix = 100; p.trim = 0; p.autoTrim = false; p.delta = false; p.deltaLock = false; p.bypass = false; p.inputTrim = 0;
    fillFactoryBands(p.band);
}

inline void Params::setFactoryDefault() { factoryFill(*this); }


inline void applyVocalMode(Params& p, VocalMode m) {
    // Keep global knobs; reshape the four general-purpose bands + cuts.
    switch (m) {
        case VocalMode::Off: factoryFill(p); p.depth = kDepthDef; return;
        case VocalMode::Harsh:
            p.band[0] = Band{ true, 120.0f, 0, 0.7f, 0, BandType::LowCut, 24 };
            p.band[1] = Band{ true, 350.0f, -3.0f, 0.5f, 0, BandType::Peaking, 12 };
            p.band[2] = Band{ true, 1500.0f, 0.0f, 0.5f, 0, BandType::Peaking, 12 };
            p.band[3] = Band{ true, 3200.0f, 8.0f, 0.7f, 0, BandType::Peaking, 12 };
            p.band[4] = Band{ true, 4500.0f, 7.0f, 0.7f, 0, BandType::Peaking, 12 };
            p.band[5] = Band{ true, 12000.0f, 0, 0.7f, 0, BandType::HighCut, 12 };
            break;
        case VocalMode::Sibilance:
            p.band[0] = Band{ true, 200.0f, 0, 0.7f, 0, BandType::LowCut, 24 };
            p.band[1] = Band{ true, 800.0f, -4.0f, 0.5f, 0, BandType::Peaking, 12 };
            p.band[2] = Band{ true, 2500.0f, -1.0f, 0.5f, 0, BandType::Peaking, 12 };
            p.band[3] = Band{ true, 6500.0f, 9.0f, 0.8f, 0, BandType::Peaking, 12 };
            p.band[4] = Band{ true, 8500.0f, 9.0f, 0.8f, 0, BandType::Peaking, 12 };
            p.band[5] = Band{ true, 16000.0f, 0, 0.7f, 0, BandType::HighCut, 18 };
            break;
        case VocalMode::MudBoom:
            p.band[0] = Band{ true, 40.0f, 0, 0.7f, 0, BandType::LowCut, 12 };
            p.band[1] = Band{ true, 180.0f, 8.0f, 0.6f, 0, BandType::Peaking, 12 };
            p.band[2] = Band{ true, 320.0f, 8.0f, 0.6f, 0, BandType::Peaking, 12 };
            p.band[3] = Band{ true, 2000.0f, -2.0f, 0.5f, 0, BandType::Peaking, 12 };
            p.band[4] = Band{ true, 8000.0f, -3.0f, 0.5f, 0, BandType::Peaking, 12 };
            p.band[5] = Band{ true, 18000.0f, 0, 0.7f, 0, BandType::HighCut, 12 };
            break;
        case VocalMode::Nasal:
            p.band[0] = Band{ true, 80.0f, 0, 0.7f, 0, BandType::LowCut, 24 };
            p.band[1] = Band{ true, 400.0f, -1.0f, 0.5f, 0, BandType::Peaking, 12 };
            p.band[2] = Band{ true, 1000.0f, 9.0f, 0.8f, 0, BandType::Peaking, 12 };
            p.band[3] = Band{ true, 1400.0f, 8.0f, 0.8f, 0, BandType::Peaking, 12 };
            p.band[4] = Band{ true, 8000.0f, -2.0f, 0.5f, 0, BandType::Peaking, 12 };
            p.band[5] = Band{ true, 18000.0f, 0, 0.7f, 0, BandType::HighCut, 12 };
            break;
        case VocalMode::Ring:
            p.band[0] = Band{ true, 100.0f, 0, 0.7f, 0, BandType::LowCut, 24 };
            p.band[1] = Band{ true, 600.0f, 5.0f, 1.2f, 0, BandType::Peaking, 12 };
            p.band[2] = Band{ true, 1600.0f, 6.0f, 1.4f, 0, BandType::Peaking, 12 };
            p.band[3] = Band{ true, 2800.0f, 6.0f, 1.4f, 0, BandType::Peaking, 12 };
            p.band[4] = Band{ true, 5000.0f, 5.0f, 1.2f, 0, BandType::Peaking, 12 };
            p.band[5] = Band{ true, 14000.0f, 0, 0.7f, 0, BandType::HighCut, 12 };
            p.sharpness = std::max(p.sharpness, 6.0f);
            break;
        case VocalMode::BreathAir:
            p.band[0] = Band{ true, 200.0f, 0, 0.7f, 0, BandType::LowCut, 24 };
            p.band[1] = Band{ true, 800.0f, -4.0f, 0.5f, 0, BandType::Peaking, 12 };
            p.band[2] = Band{ true, 3000.0f, -2.0f, 0.5f, 0, BandType::Peaking, 12 };
            p.band[3] = Band{ true, 10000.0f, 6.0f, 0.7f, 0, BandType::Peaking, 12 };
            p.band[4] = Band{ true, 13000.0f, 8.0f, 0.8f, 0, BandType::Peaking, 12 };
            p.band[5] = Band{ true, 20000.0f, 0, 0.7f, 0, BandType::HighCut, 12 };
            break;
    }
}

} // namespace silk

// The Params ctor currently calls setFactoryDefault which is a stub. Fix by providing the real body
// immediately after, but C++ will use the first definition. I need to fix the first definition.
// I'll do that in a follow-up patch after writing this file — see the footer note.

namespace silk {

inline void Engine::prepare(double sampleRate, int maxBlk) {
    sr = sampleRate; maxBlock = std::max(16, maxBlk);
    reconfig();
}

inline void Engine::setParams(const Params& np) {
    bool qc = (np.resolution != p.resolution) || (np.oversample != p.oversample);
    p = np;
    if (qc && haveP) reconfig();
}

inline void Engine::reconfig() {
    qualitySizes(p.resolution, sr, N, H);
    os = oversampleFactor(p.oversample);
    M = N * os;
    fft.init(M);
    win.resize((size_t)N); synthWin.resize((size_t)N);
    for (int i = 0; i < N; ++i) {
        float w = 0.5f - 0.5f * (float)std::cos(kTwoPi * i / N); // Hann
        win[(size_t)i] = w;
        synthWin[(size_t)i] = w;
    }
    // COLA gain for hann*hann
    std::vector<float> acc((size_t)N, 0);
    for (int s = 0; s < N; s += H)
        for (int i = 0; i < N; ++i) {
            int j = s + i; if (j >= N) break;
            acc[(size_t)j] += win[(size_t)i] * synthWin[(size_t)i];
        }
    // Also wrap-around hops that start negative (steady-state)
    acc.assign((size_t)N, 0);
    for (int k = -N; k <= N; k += H)
        for (int i = 0; i < N; ++i) {
            int t = k + i;
            if (t >= 0 && t < N) acc[(size_t)t] += win[(size_t)i] * synthWin[(size_t)i];
        }
    float mid = acc[(size_t)N / 2];
    colaGain = mid > 1e-8f ? 1.0f / mid : 1.0f;

    histLen = N * 2 + maxBlock + H;
    histL.assign((size_t)histLen, 0); histR.assign((size_t)histLen, 0);
    histScL.assign((size_t)histLen, 0); histScR.assign((size_t)histLen, 0);
    inW = 0; hopFill = 0;

    // OLA buffer and output fifo
    int olaSize = N * 4 + maxBlock;
    olaL.assign((size_t)olaSize, 0); olaR.assign((size_t)olaSize, 0);
    olaW = 0;

    // STFT delay: first complete hop is ready after N samples in, and we emit H samples
    // that correspond to the oldest incomplete region; measured alignment is N - H? We'll
    // set stftDelay = N and verify with an impulse. Output fifo is prefilled with silence.
    stftDelay = N;
    lim.init(sr, -0.2f);
    latency = stftDelay + lim.la;
    dryL.init(latency + 8); dryR.init(latency + 8);
    dryL.setDelay(latency); dryR.setDelay(latency);
    scDelayL.init(latency + 8); scDelayR.init(latency + 8);
    scDelayL.setDelay(latency); scDelayR.setDelay(latency);

    fifoSize = latency + N + maxBlock + 16;
    fifoL.assign((size_t)fifoSize, 0); fifoR.assign((size_t)fifoSize, 0);
    fifoRpos = 0; fifoWpos = stftDelay; // start with stftDelay zeros already "written"
    fifoAvail = stftDelay;

    re.assign((size_t)M, 0); im.assign((size_t)M, 0); mag.assign((size_t)M / 2 + 1, 0);
    env.assign((size_t)M / 2 + 1, 0); prom.assign((size_t)M / 2 + 1, 0);
    red.assign((size_t)M / 2 + 1, 0); redSm.assign((size_t)(M / 2 + 1) * 2, 0);
    lastRed[0].assign((size_t)M / 2 + 1, 0); lastRed[1].assign((size_t)M / 2 + 1, 0);
    lastSens[0].assign((size_t)M / 2 + 1, 0); lastSens[1].assign((size_t)M / 2 + 1, 0);
    binFreq.resize((size_t)M / 2 + 1);
    for (int k = 0; k <= M / 2; ++k) binFreq[(size_t)k] = (float)(k * sr / M);
    frame.assign((size_t)N, 0);
    tmpRe[0].assign((size_t)M, 0); tmpIm[0].assign((size_t)M, 0);
    tmpRe[1].assign((size_t)M, 0); tmpIm[1].assign((size_t)M, 0);
    learnProm.assign((size_t)M / 2 + 1, 0); learnEnv.assign((size_t)M / 2 + 1, 0);
    inPk = outPk = maxRed = 0; autoTrimValue = 0; dryRmsEma = wetRmsEma = 0;
    haveP = true;
}

inline void Engine::startLearn(double seconds) {
    learning = true; learnFinished = false;
    learnNeed = std::max(1, (int)std::lround(seconds * sr));
    learnHave = 0; learnFrames = 0;
    std::fill(learnProm.begin(), learnProm.end(), 0.0f);
    std::fill(learnEnv.begin(), learnEnv.end(), 0.0f);
}
inline void Engine::cancelLearn() { learning = false; learnFinished = false; learnHave = 0; }

inline void Engine::takeLearn(Params& dest) {
    if (learnFrames <= 0) { learnFinished = false; return; }
    int bins = M / 2 + 1;
    std::vector<float> avg((size_t)bins);
    for (int k = 0; k < bins; ++k) avg[(size_t)k] = learnProm[(size_t)k] / (float)learnFrames;
    // Find up to 4 log-spaced peak regions in 150 Hz – 12 kHz.
    std::array<int, 4> peaks { -1, -1, -1, -1 };
    std::array<float, 4> peakV { 0, 0, 0, 0 };
    int kMin = std::max(2, (int)(150.0 / (sr / M)));
    int kMax = std::min(bins - 2, (int)(12000.0 / (sr / M)));
    for (int k = kMin; k <= kMax; ++k) {
        if (avg[(size_t)k] >= avg[(size_t)k - 1] && avg[(size_t)k] >= avg[(size_t)k + 1]) {
            // skip if too close to an existing peak (half octave)
            bool close = false;
            for (int i = 0; i < 4; ++i) if (peaks[(size_t)i] > 0) {
                double oct = std::fabs(std::log2(binFreq[(size_t)k] / binFreq[(size_t)peaks[(size_t)i]]));
                if (oct < 0.4) { close = true; if (avg[(size_t)k] > peakV[(size_t)i]) { peaks[(size_t)i] = k; peakV[(size_t)i] = avg[(size_t)k]; } }
            }
            if (close) continue;
            int slot = 0; for (int i = 1; i < 4; ++i) if (peakV[(size_t)i] < peakV[(size_t)slot]) slot = i;
            if (avg[(size_t)k] > peakV[(size_t)slot]) { peaks[(size_t)slot] = k; peakV[(size_t)slot] = avg[(size_t)k]; }
        }
    }
    // Sort peaks by frequency and write into GP bands 1-4.
    std::array<int, 4> order { 0, 1, 2, 3 };
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        int ka = peaks[(size_t)a], kb = peaks[(size_t)b];
        float fa = ka > 0 ? binFreq[(size_t)ka] : 1e9f;
        float fb = kb > 0 ? binFreq[(size_t)kb] : 1e9f;
        return fa < fb;
    });
    for (int i = 0; i < 4; ++i) {
        int pk = peaks[(size_t)order[(size_t)i]];
        Band& b = dest.band[(size_t)(i + 1)];
        b.on = true; b.type = BandType::Peaking; b.q = 0.8f; b.balance = 0;
        if (pk > 0) {
            b.freq = binFreq[(size_t)pk];
            // Map average prominence (dB-ish) to a modest Sens boost.
            b.sens = std::clamp(peakV[(size_t)order[(size_t)i]] * 0.6f, 2.0f, 10.0f);
        }
    }
    dest.band[0].on = true; dest.band[0].type = BandType::LowCut;
    dest.band[5].on = true; dest.band[5].type = BandType::HighCut;
    learnFinished = false;
    learning = false;
}

inline void Engine::copyDisplay(float* sensL, float* sensR, float* redL, float* redR, int points) const {
    int bins = M / 2 + 1;
    for (int i = 0; i < points; ++i) {
        // log-spaced 20 Hz – Nyquist
        double f0 = 20.0, f1 = sr * 0.5;
        double f = f0 * std::pow(f1 / f0, (double)i / std::max(1, points - 1));
        double bin = f / (sr / M);
        int k = (int)bin; float t = (float)(bin - k);
        k = std::clamp(k, 0, bins - 2);
        auto lerp = [&](const std::vector<float>& a) { return a[(size_t)k] + t * (a[(size_t)k + 1] - a[(size_t)k]); };
        if (sensL) sensL[i] = lastSens[0].empty() ? 0 : lerp(lastSens[0]);
        if (sensR) sensR[i] = lastSens[1].empty() ? 0 : lerp(lastSens[1]);
        if (redL) redL[i] = lastRed[0].empty() ? 0 : lerp(lastRed[0]);
        if (redR) redR[i] = lastRed[1].empty() ? 0 : lerp(lastRed[1]);
    }
}

// Process one analysis frame from `analysisSrc` (N samples) into reduction applied to the
// complex spectrum already in re/im? We'll do window+fft inside.
inline void Engine::processFrame(int ch, const float* analysisSrc, float* reOut, float* imOut) {
    int bins = M / 2 + 1;
    std::fill(reOut, reOut + M, 0.0f);
    std::fill(imOut, imOut + M, 0.0f);
    for (int i = 0; i < N; ++i) reOut[i] = analysisSrc[i] * win[(size_t)i];
    fft.forward(reOut, imOut);

    // magnitude
    for (int k = 0; k < bins; ++k) {
        mag[(size_t)k] = std::sqrt(reOut[k] * reOut[k] + imOut[k] * imOut[k]) + 1e-20f;
    }

    // Frequency-smoothed envelope (wider at high freq: ~1/3 octave, sharpened slightly by selectivity).
    // Log-frequency IIR + a small backward pass.
    env[0] = mag[0];
    float smooth = 0.18f + 0.25f * (1.0f - p.sharpness / 10.0f); // more blur at low sharpness... used later too
    // Use a constant-Q blur: radius in bins grows with k.
    std::vector<float>& e = env;
    e[0] = mag[0];
    double acc = mag[0];
    // forward
    float a = 0.25f;
    for (int k = 1; k < bins; ++k) { acc = a * mag[(size_t)k] + (1.0 - a) * acc; e[(size_t)k] = (float)acc; }
    // backward
    acc = e[(size_t)bins - 1];
    for (int k = bins - 2; k >= 0; --k) { acc = a * e[(size_t)k] + (1.0 - a) * acc; e[(size_t)k] = (float)acc; }
    // Extra wider blur (approx 1/3 octave) by a short box in log index
    std::vector<float> e2 = e;
    for (int k = 1; k < bins; ++k) {
        int w = std::max(1, (int)(0.08 * k)); // ~1/6 of an octave-ish
        int k0 = std::max(0, k - w), k1 = std::min(bins - 1, k + w);
        double s = 0; int n = 0;
        for (int j = k0; j <= k1; ++j) { s += e[(size_t)j]; ++n; }
        e2[(size_t)k] = (float)(s / n);
    }
    e.swap(e2);

    // Prominence in dB
    float thresh = p.selectivity * 1.15f;           // ~4.1 dB at 3.6
    if (p.hard) thresh *= 0.75f;                    // hard engages more easily
    float depth01 = std::clamp((p.depth - kDepthMin) / (kDepthMax - kDepthMin), 0.0f, 1.0f);
    // At depth -18: 0; at 3.0: 21/36=0.583; at 18: 1
    // Amount: up to 60 dB at max, moderate at factory.
    float maxRedDb = 60.0f * depth01 * depth01 * (0.7f + 0.3f * depth01); // ~13 dB at depth 3, 60 at 18
    if (p.hard) maxRedDb *= 1.35f;
    float sharpDepth = 0.65f + 0.55f * (p.sharpness / 10.0f); // higher sharpness → slightly deeper cuts

    for (int k = 0; k < bins; ++k) {
        float f = binFreq[(size_t)k];
        float sensDb = (float)sensitivityDbAt(p, f, ch);
        lastSens[ch][(size_t)k] = sensDb;
        float weight = dbToGain(sensDb); // 0 dB → 1
        if (weight < 0) weight = 0;

        float magDb = gainToDb(mag[(size_t)k]);
        float envDb = gainToDb(std::max(e[(size_t)k], 1e-20f));
        float pr = magDb - envDb; // relative prominence
        if (p.hard) {
            // more level-dependent: add a term when the envelope itself is loud
            float absTerm = std::max(0.0f, envDb - (-24.0f)) * 0.15f;
            pr += absTerm;
        }
        prom[(size_t)k] = pr;
        float excess = std::max(0.0f, pr - thresh);
        float rdb = excess * sharpDepth * weight;
        if (rdb > maxRedDb) rdb = maxRedDb;
        if (depth01 <= 0.0f) rdb = 0;
        red[(size_t)k] = rdb;
    }

    // Sharpness = notch width. Low sharpness blurs reduction across neighbours; high keeps it tight.
    {
        float spread = (10.0f - p.sharpness) / 10.0f; // 0 at max sharpness, 1 at 0
        int maxW = 1 + (int)std::lround(spread * 8.0 * (M / (double)N)); // more bins when oversampled
        if (maxW > 1) {
            std::vector<float> blur = red;
            for (int k = 0; k < bins; ++k) {
                int w = 1 + (int)(maxW * (double)k / bins); // slightly wider at highs? actually narrower in Hz at lows
                w = std::max(1, maxW);
                double s = 0, ww = 0;
                for (int j = -w; j <= w; ++j) {
                    int kk = k + j; if (kk < 0 || kk >= bins) continue;
                    double g = std::exp(-0.5 * (j / (0.35 * w + 0.2)) * (j / (0.35 * w + 0.2)));
                    s += blur[(size_t)kk] * g; ww += g;
                }
                red[(size_t)k] = (float)(s / ww);
            }
        }
    }

    // Attack / Release (frequency-dependent, faster at highs). Stored in redSm, 2 channels packed.
    // time constants: attack 1.0 → ~2–8 ms at highs, slower at lows; higher value = slower.
    int off = ch * bins;
    for (int k = 0; k < bins; ++k) {
        float f = std::max(binFreq[(size_t)k], 20.0f);
        float freqFac = std::clamp(std::sqrt(1000.0f / f), 0.25f, 4.0f); // lows slower
        float attSec = (0.0015f * std::pow(2.0f, p.attack)) * freqFac;
        float relSec = (0.008f * std::pow(2.0f, p.release)) * freqFac;
        float ca = 1.0f - (float)std::exp(-H / (sr * attSec));
        float cr = 1.0f - (float)std::exp(-H / (sr * relSec));
        float prev = redSm[(size_t)(off + k)];
        float tgt = red[(size_t)k];
        float c = (tgt > prev) ? ca : cr; // more reduction = "attack"
        prev += (tgt - prev) * c;
        redSm[(size_t)(off + k)] = prev;
        lastRed[ch][(size_t)k] = prev;
        maxRed = std::max(maxRed, prev);
    }

    if (learning && ch == 0) {
        for (int k = 0; k < bins; ++k) {
            learnProm[(size_t)k] += std::max(0.0f, prom[(size_t)k]);
            learnEnv[(size_t)k] += env[(size_t)k];
        }
        ++learnFrames;
    }

    // Apply gain to bins (keep DC and Nyquist polite)
    for (int k = 0; k < bins; ++k) {
        float g = dbToGain(-redSm[(size_t)(off + k)]);
        reOut[k] *= g; imOut[k] *= g;
        if (k > 0 && k < M - k) {
            // also the negative-freq image: after fft.forward, we have 0..M-1
        }
    }
    // Hermitian: bins M-k should be conjugate of k. We only touched 0..N/2 of padded FFT.
    for (int k = 1; k < bins - 1; ++k) {
        reOut[M - k] = reOut[k];
        imOut[M - k] = -imOut[k];
    }
    fft.inverse(reOut, imOut);
}

inline void Engine::runHop() {
    // Gather last N samples from hist (ending at inW-1).
    auto gather = [&](const std::vector<float>& hist, float* dest) {
        for (int i = 0; i < N; ++i) {
            int idx = inW - N + i; while (idx < 0) idx += histLen;
            dest[i] = hist[(size_t)(idx % histLen)];
        }
    };
    std::vector<float> aL((size_t)N), aR((size_t)N), sL((size_t)N), sR((size_t)N);
    gather(histL, aL.data()); gather(histR, aR.data());
    gather(histScL, sL.data()); gather(histScR, sR.data());

    const float* detL = p.sidechain ? sL.data() : aL.data();
    const float* detR = p.sidechain ? sR.data() : aR.data();

    const bool ms = p.stereoMode == StereoMode::MidSide;
    auto toMS = [](float& x, float& y) { float m = 0.5f * (x + y), s = 0.5f * (x - y); x = m; y = s; };
    auto fromMS = [](float& m, float& s) { float x = m + s, y = m - s; m = x; s = y; };
    std::vector<float> dL((size_t)N), dR((size_t)N), xL = aL, xR = aR;
    for (int i = 0; i < N; ++i) { dL[(size_t)i] = detL[i]; dR[(size_t)i] = detR[i]; }
    if (ms) {
        for (int i = 0; i < N; ++i) { toMS(xL[(size_t)i], xR[(size_t)i]); toMS(dL[(size_t)i], dR[(size_t)i]); }
    }

    float link = std::clamp(p.link, 0.0f, 100.0f) / 100.0f;

    // Linked analysis: blend detectors toward a sum/mid
    std::vector<float> dSum((size_t)N);
    for (int i = 0; i < N; ++i) dSum[(size_t)i] = 0.5f * (dL[(size_t)i] + dR[(size_t)i]);
    std::vector<float> dLuse((size_t)N), dRuse((size_t)N);
    for (int i = 0; i < N; ++i) {
        dLuse[(size_t)i] = link * dSum[(size_t)i] + (1.0f - link) * dL[(size_t)i];
        dRuse[(size_t)i] = link * dSum[(size_t)i] + (1.0f - link) * dR[(size_t)i];
    }

    processFrame(0, dLuse.data(), tmpRe[0].data(), tmpIm[0].data());
    if (link < 0.999f) processFrame(1, dRuse.data(), tmpRe[1].data(), tmpIm[1].data());
    else { lastRed[1] = lastRed[0]; lastSens[1] = lastSens[0]; }

    auto synth = [&](int ch, const float* audio, std::vector<float>& outFrame) {
        std::fill(re.begin(), re.end(), 0.0f); std::fill(im.begin(), im.end(), 0.0f);
        for (int i = 0; i < N; ++i) re[(size_t)i] = audio[i] * win[(size_t)i];
        fft.forward(re.data(), im.data());
        int bins = M / 2 + 1;
        int srcCh = (link >= 0.999f) ? 0 : ch;
        float chAmt = (ch == 0) ? (1.0f - std::max(0.0f, p.balance)) : (1.0f - std::max(0.0f, -p.balance));
        for (int k = 0; k < bins; ++k) {
            float g = dbToGain(-lastRed[srcCh][(size_t)k] * chAmt);
            re[(size_t)k] *= g; im[(size_t)k] *= g;
        }
        for (int k = 1; k < bins - 1; ++k) { re[(size_t)(M - k)] = re[(size_t)k]; im[(size_t)(M - k)] = -im[(size_t)k]; }
        fft.inverse(re.data(), im.data());
        outFrame.resize((size_t)N);
        for (int i = 0; i < N; ++i) outFrame[(size_t)i] = re[(size_t)i] * synthWin[(size_t)i] * colaGain;
    };
    std::vector<float> yL, yR;
    synth(0, xL.data(), yL);
    synth(1, xR.data(), yR);
    if (ms) {
        for (int i = 0; i < N; ++i) fromMS(yL[(size_t)i], yR[(size_t)i]);
    }

    // At time t (>= N), this frame covers input [t-N, t). Adding it at olaW covers that
    // region in the OLA buffer. Samples [t-N, t-N+H) will not receive later overlaps, so they
    // are complete and equal to reconstructed input delayed by N.
    int olaSize = (int)olaL.size();
    for (int i = 0; i < N; ++i) {
        int idx = olaW + i; if (idx >= olaSize) idx -= olaSize;
        olaL[(size_t)idx] += yL[(size_t)i];
        olaR[(size_t)idx] += yR[(size_t)i];
    }
    for (int i = 0; i < H; ++i) {
        int idx = olaW + i; if (idx >= olaSize) idx -= olaSize;
        fifoL[(size_t)fifoWpos] = olaL[(size_t)idx];
        fifoR[(size_t)fifoWpos] = olaR[(size_t)idx];
        olaL[(size_t)idx] = 0;
        olaR[(size_t)idx] = 0;
        if (++fifoWpos >= fifoSize) fifoWpos = 0;
        ++fifoAvail;
    }
    olaW += H; if (olaW >= olaSize) olaW -= olaSize;
}

inline void Engine::process(float* L, float* R, int n, const float* scL, const float* scR) {
    if (!haveP) prepare(sr, std::max(n, 64));
    maxRed *= 0.995f;
    float inP = inPk * 0.95f, outP = outPk * 0.95f;
    const bool trimIn = std::fabs(p.inputTrim) > 1.0e-6f;
    const float inTrim = trimIn ? dbToGain(p.inputTrim) : 1.0f;

    // Write input into history (after input trim).
    for (int i = 0; i < n; ++i) {
        float xL = trimIn ? L[i] * inTrim : L[i];
        float xR = trimIn ? R[i] * inTrim : R[i];
        inP = std::max(inP, std::max(std::fabs(xL), std::fabs(xR)));
        histL[(size_t)inW] = xL; histR[(size_t)inW] = xR;
        histScL[(size_t)inW] = scL ? scL[i] : 0;
        histScR[(size_t)inW] = scR ? scR[i] : 0;
        if (++inW >= histLen) inW = 0;
        ++hopFill;
        // First complete window is ready after N samples; then every H samples.
        if (hopFill >= N && ((hopFill - N) % H == 0)) runHop();
        if (learning) { ++learnHave; if (learnHave >= learnNeed) { learning = false; learnFinished = true; } }
    }
    inPk = inP;

    // For Mix 0 / Bypass: output delayed dry, bit-exact, no limiter.
    bool dryOnly = p.bypass || p.mix <= 0.0f;
    bool scListen = p.sidechain && p.sidechainListen;

    for (int i = 0; i < n; ++i) {
        float dL = dryL.tick(trimIn ? L[i] * inTrim : L[i]);
        float dR = dryR.tick(trimIn ? R[i] * inTrim : R[i]);
        float kL = 0, kR = 0;
        if (scListen) {
            kL = scDelayL.tick(scL ? scL[i] : 0);
            kR = scDelayR.tick(scR ? scR[i] : 0);
        } else {
            scDelayL.tick(0); scDelayR.tick(0);
        }

        float wL = 0, wR = 0;
        if (fifoAvail > 0) {
            wL = fifoL[(size_t)fifoRpos];
            wR = fifoR[(size_t)fifoRpos];
            if (++fifoRpos >= fifoSize) fifoRpos = 0;
            --fifoAvail;
        }

        if (scListen) { L[i] = kL; R[i] = kR; outP = std::max(outP, std::max(std::fabs(L[i]), std::fabs(R[i]))); continue; }
        if (dryOnly) { L[i] = dL; R[i] = dR; outP = std::max(outP, std::max(std::fabs(L[i]), std::fabs(R[i]))); continue; }

        float trimG = dbToGain(p.trim);
        // Auto trim: slowly match wet RMS to dry RMS
        double dryP = (double)dL * dL + (double)dR * dR;
        double wetP = (double)wL * wL + (double)wR * wR;
        dryRmsEma = dryRmsEma * 0.999 + dryP * 0.001;
        wetRmsEma = wetRmsEma * 0.999 + wetP * 0.001;
        if (p.autoTrim && wetRmsEma > 1e-12) {
            float need = gainToDb((float)std::sqrt(dryRmsEma / std::max(wetRmsEma, 1e-18)));
            autoTrimValue += 0.05f * (std::clamp(need, -12.0f, 12.0f) - autoTrimValue);
            trimG = dbToGain(p.trim + autoTrimValue);
        } else {
            autoTrimValue *= 0.99f;
        }
        wL *= trimG; wR *= trimG;
        float mix = std::clamp(p.mix, 0.0f, 100.0f) / 100.0f;
        float yL = wL * mix + dL * (1.0f - mix);
        float yR = wR * mix + dR * (1.0f - mix);
        if (p.delta) { yL = dL - wL; yR = dR - wR; }

        lim.tick(yL, yR);
        L[i] = yL; R[i] = yR;
        outP = std::max(outP, std::max(std::fabs(yL), std::fabs(yR)));
    }
    outPk = outP;
}

} // namespace silk
