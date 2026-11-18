#pragma once
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <chrono>
#include <functional>
#include "SilkDsp.h"
inline int g_pass = 0, g_fail = 0;
#define CHECK(x) do { if (x) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); } } while (0)
#define CHECKM(x, ...) do { if (x) ++g_pass; else { ++g_fail; std::printf("FAIL %s:%d: %s | ", __FILE__, __LINE__, #x); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)
namespace tu {
inline float peak(const std::vector<float>& x) { float p = 0; for (float v : x) p = std::max(p, std::fabs(v)); return p; }
inline float peak2(const std::vector<float>& a, const std::vector<float>& b) { return std::max(peak(a), peak(b)); }
inline bool finite(const std::vector<float>& x) { for (float v : x) if (!std::isfinite(v)) return false; return true; }
inline double rms(const std::vector<float>& x, size_t a = 0, size_t b = 0) {
    if (b == 0) b = x.size();
    b = std::min(b, x.size()); a = std::min(a, b); double e = 0; size_t n = b - a;
    for (size_t i = a; i < b; ++i) e += (double)x[i] * x[i]; return n ? std::sqrt(e / n) : 0;
}
inline double rmsDiff(const std::vector<float>& a, const std::vector<float>& b) {
    size_t n = std::min(a.size(), b.size()); if (!n) return 0;
    double e = 0; for (size_t i = 0; i < n; ++i) { double d = (double)a[i] - (double)b[i]; e += d * d; }
    return std::sqrt(e / n);
}
inline int ident(const std::vector<float>& a, const std::vector<float>& b) {
    int n = (int)std::min(a.size(), b.size()), same = 0;
    for (int i = 0; i < n; ++i) if (a[(size_t)i] == b[(size_t)i]) ++same;
    return same;
}
inline int argmaxAbs(const std::vector<float>& x) {
    int k = 0; float p = -1;
    for (int i = 0; i < (int)x.size(); ++i) { float a = std::fabs(x[(size_t)i]); if (a > p) { p = a; k = i; } }
    return k;
}
struct Spec {
    int n = 0, np = 0; double sr = 44100, wsum = 0; std::vector<float> re, im, pw; silk::FFT fft;
    void run(const std::vector<float>& x, size_t start, int len, double fs) {
        sr = fs; n = len; np = 1; while (np < 4 * len) np <<= 1;
        if (fft.n != np) fft.init(np);
        re.assign((size_t)np, 0); im.assign((size_t)np, 0); wsum = 0;
        for (int i = 0; i < len; ++i) {
            double t = silk::kTwoPi * i / len;
            double w = 0.35875 - 0.48829 * std::cos(t) + 0.14128 * std::cos(2 * t) - 0.01168 * std::cos(3 * t);
            wsum += w; size_t idx = start + (size_t)i; re[(size_t)i] = (float)(w * (idx < x.size() ? x[idx] : 0.0f));
        }
        fft.forward(re.data(), im.data());
        pw.assign((size_t)np / 2 + 1, 0);
        for (int k = 0; k <= np / 2; ++k) pw[(size_t)k] = re[(size_t)k] * re[(size_t)k] + im[(size_t)k] * im[(size_t)k];
    }
    double binHz() const { return sr / np; }
    double peak(double f, double hw, double& freq) const {
        int k0 = std::max(1, (int)((f - hw) / binHz())), k1 = std::min(np / 2 - 1, (int)((f + hw) / binHz()) + 1);
        int kb = k0; float m = -1; for (int k = k0; k <= k1; ++k) if (pw[(size_t)k] > m) { m = pw[(size_t)k]; kb = k; }
        double a = 0.5 * std::log(pw[(size_t)kb - 1] + 1e-30), b = 0.5 * std::log(pw[(size_t)kb] + 1e-30), c = 0.5 * std::log(pw[(size_t)kb + 1] + 1e-30);
        double den = a - 2 * b + c; double p = den < 0 ? 0.5 * (a - c) / den : 0; p = std::max(-0.5, std::min(0.5, p));
        freq = (kb + p) * binHz();
        return 2.0 * std::exp(b - 0.25 * (a - c) * p) / wsum;
    }
    double amp(double f, double hw) const { double fr; return peak(f, hw, fr); }
    double bandPower(double f0, double f1) const {
        int k0 = std::max(1, (int)(f0 / binHz())), k1 = std::min(np / 2, (int)(f1 / binHz()));
        double s = 0; for (int k = k0; k <= k1; ++k) s += pw[(size_t)k];
        double w2 = 0; for (int i = 0; i < n; ++i) { double t = silk::kTwoPi * i / n; double w = 0.35875 - 0.48829 * std::cos(t) + 0.14128 * std::cos(2 * t) - 0.01168 * std::cos(3 * t); w2 += w * w; }
        return 2.0 * s / ((double)np * w2);
    }
};
inline double cents(double a, double b) { return 1200.0 * std::log2(a / b); }
inline double db(double x) { return 20.0 * std::log10(std::max(x, 1e-15)); }
inline float truePeak(const std::vector<float>& x) {
    const int os = 8, taps = 32; float pk = 0;
    static std::vector<std::vector<double>> co;
    if (co.empty()) for (int f = 1; f < os; ++f) {
        std::vector<double> c; double sum = 0; double t = (double)f / os;
        for (int k = -taps + 1; k <= taps; ++k) { double d = t - k; double w = 0.42 + 0.5 * std::cos(silk::kPi * d / (taps + 1)) + 0.08 * std::cos(2 * silk::kPi * d / (taps + 1));
            double sinc = std::fabs(d) < 1e-12 ? 1.0 : std::sin(silk::kPi * d) / (silk::kPi * d); c.push_back(sinc * w); sum += sinc * w; }
        for (double& v : c) v /= sum; co.push_back(c); }
    for (size_t i = 0; i < x.size(); ++i) {
        pk = std::max(pk, std::fabs(x[i]));
        for (int f = 1; f < os; ++f) {
            double s = 0; const auto& c = co[(size_t)f - 1];
            for (int k = -taps + 1; k <= taps; ++k) { long j = (long)i + k; if (j < 0 || j >= (long)x.size()) continue; s += x[(size_t)j] * c[(size_t)(k + taps - 1)]; }
            pk = std::max(pk, (float)std::fabs(s));
        }
    }
    return pk;
}
inline bool hasSubnormal(const std::vector<float>& x) { for (float v : x) if (v != 0 && std::fpclassify(v) == FP_SUBNORMAL) return true; return false; }
}
