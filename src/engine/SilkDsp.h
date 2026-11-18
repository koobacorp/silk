// SILK — DSP building blocks (no JUCE). Original code.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace silk {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

inline float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }
inline float gainToDb(float g) { return 20.0f * std::log10(std::max(g, 1e-12f)); }
inline double dbToGainD(double db) { return std::pow(10.0, db / 20.0); }
inline double gainToDbD(double g) { return 20.0 * std::log10(std::max(g, 1e-15)); }

struct Rng {
    uint32_t s = 0x9E3779B9u;
    Rng() = default;
    explicit Rng(uint32_t seed) : s(seed ? seed : 1u) {}
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float uni() { return (float)(next() >> 8) * (1.0f / 16777216.0f); }
    inline float bi() { return uni() * 2.0f - 1.0f; }
};

// Radix-2 complex FFT. Inverse is a sign flip on the twiddles plus 1/n.
struct FFT {
    int n = 0, logn = 0;
    std::vector<float> cs, sn;
    std::vector<int> rev;
    void init(int size) {
        n = size; logn = 0; while ((1 << logn) < n) ++logn;
        cs.resize((size_t)n / 2); sn.resize((size_t)n / 2); rev.resize((size_t)n);
        for (int i = 0; i < n / 2; ++i) {
            cs[(size_t)i] = (float)std::cos(kTwoPi * i / n);
            sn[(size_t)i] = (float)-std::sin(kTwoPi * i / n);
        }
        for (int i = 0; i < n; ++i) {
            int r = 0; for (int b = 0; b < logn; ++b) if (i & (1 << b)) r |= 1 << (logn - 1 - b);
            rev[(size_t)i] = r;
        }
    }
    void transform(float* re, float* im, bool inverse) const {
        for (int i = 0; i < n; ++i) {
            int r = rev[(size_t)i];
            if (r > i) { std::swap(re[i], re[r]); std::swap(im[i], im[r]); }
        }
        for (int len = 2; len <= n; len <<= 1) {
            int half = len >> 1, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j) {
                    float wr = cs[(size_t)(j * step)];
                    float wi = inverse ? -sn[(size_t)(j * step)] : sn[(size_t)(j * step)];
                    float xr = re[i + j + half], xi = im[i + j + half];
                    float tr = xr * wr - xi * wi, ti = xr * wi + xi * wr;
                    re[i + j + half] = re[i + j] - tr; im[i + j + half] = im[i + j] - ti;
                    re[i + j] += tr; im[i + j] += ti;
                }
        }
        if (inverse) {
            float s = 1.0f / (float)n;
            for (int i = 0; i < n; ++i) { re[i] *= s; im[i] *= s; }
        }
    }
    void forward(float* re, float* im) const { transform(re, im, false); }
    void inverse(float* re, float* im) const { transform(re, im, true); }
};

struct Delay {
    std::vector<float> buf; int w = 0, d = 0;
    void init(int maxDelay) { buf.assign((size_t)std::max(1, maxDelay + 1), 0.0f); w = 0; d = 0; }
    void setDelay(int samples) { d = std::clamp(samples, 0, (int)buf.size() - 1); }
    inline float tick(float x) {
        buf[(size_t)w] = x; int r = w - d; if (r < 0) r += (int)buf.size();
        float y = buf[(size_t)r]; if (++w >= (int)buf.size()) w = 0; return y;
    }
    void clear() { std::fill(buf.begin(), buf.end(), 0.0f); }
};

// Lookahead true-peak safety limiter. Linked stereo. Ceiling default -0.1 dBTP.
struct SafetyLimiter {
    static constexpr int kTaps = 32, kHalf = kTaps / 2, kPh = 4;
    int la = 64, win = 60, avg = 28;
    float ceiling = 0.977237f;  // -0.2 dBTP working target, keeps true peak under -0.1
    float clampAt = 0.988553f;  // -0.1 dBFS hard safety
    float rel = 0.999f;
    std::vector<float> dl, dr, req, avgBuf;
    float coef[kPh][kTaps] {};
    float hl[kTaps] {}, hr[kTaps] {};
    int w = 0, n = 0, aw = 0, hp = 0; double avgSum = 0; float g = 1.0f; float minGain = 1.0f;
    int engaged = 0;
    double fs = 44100;
    void init(double sr, float ceilingDb = -0.2f) {
        fs = sr;
        ceiling = dbToGain(ceilingDb);
        clampAt = dbToGain(-0.1f);
        la = std::max(48, (int)std::lround(0.0015 * sr));
        win = la - 3; avg = std::max(1, (win - kHalf - 2) / 2);
        n = la + 1;
        dl.assign((size_t)n, 0.0f); dr.assign((size_t)n, 0.0f); req.assign((size_t)n, 1.0f);
        avgBuf.assign((size_t)avg, 1.0f); avgSum = avg; aw = 0; w = 0; g = 1.0f; hp = 0;
        std::fill(std::begin(hl), std::end(hl), 0.0f); std::fill(std::begin(hr), std::end(hr), 0.0f);
        rel = (float)std::exp(-1.0 / (0.08 * sr));
        minGain = 1.0f; engaged = 0;
        for (int ph = 0; ph < kPh; ++ph)
            for (int t = 0; t < kTaps; ++t) {
                double d = (t - (kHalf - 1)) - (double)ph / kPh;
                double x = kPi * d, sinc = std::fabs(d) < 1e-12 ? 1.0 : std::sin(x) / x;
                double wv = 0.5 + 0.5 * std::cos(kPi * d / (kHalf + 0.5));
                coef[ph][t] = (float)(sinc * wv);
            }
        for (int ph = 1; ph < kPh; ++ph) {
            double sum = 0; for (int t = 0; t < kTaps; ++t) sum += coef[ph][t];
            for (int t = 0; t < kTaps; ++t) coef[ph][t] = (float)(coef[ph][t] / sum);
        }
    }
    void reset() { init(fs); }
    inline float interPeak(const float* h) const {
        float m = 0;
        for (int ph = 1; ph < kPh; ++ph) {
            float s = 0;
            for (int t = 0; t < kTaps; ++t) { int idx = hp - kTaps + t; if (idx < 0) idx += kTaps; s += coef[ph][t] * h[idx]; }
            m = std::max(m, std::fabs(s));
        }
        return m;
    }
    inline void tick(float& L, float& R) {
        hl[hp] = L; hr[hp] = R; if (++hp >= kTaps) hp = 0;
        float p = std::max({ std::fabs(L), std::fabs(R), interPeak(hl), interPeak(hr) });
        float rq = p > ceiling ? ceiling / p : 1.0f;
        dl[(size_t)w] = L; dr[(size_t)w] = R; req[(size_t)w] = rq;
        int outIdx = w + 1; if (outIdx >= n) outIdx = 0;
        float mn = 1.0f;
        for (int k = 0; k < win + 3; ++k) { int idx = outIdx + k; if (idx >= n) idx -= n; mn = std::min(mn, req[(size_t)idx]); }
        avgSum += mn - avgBuf[(size_t)aw]; avgBuf[(size_t)aw] = mn; if (++aw >= avg) aw = 0;
        float s = (float)(avgSum / avg);
        float rising = g + (1.0f - g) * (1.0f - rel);
        if (rising > 0.99999f) rising = 1.0f;
        g = std::min(s, rising);
        L = dl[(size_t)outIdx] * g; R = dr[(size_t)outIdx] * g;
        if (g < 0.9999f) ++engaged;
        minGain = std::min(minGain, g);
        L = std::clamp(L, -clampAt, clampAt); R = std::clamp(R, -clampAt, clampAt);
        if (++w >= n) w = 0;
    }
};

} // namespace silk
