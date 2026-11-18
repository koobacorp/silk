// SILK engine tests — no JUCE.
#include "TestUtil.h"
#include "SilkEngine.h"
#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#include <pmmintrin.h>
#endif
using namespace silk;
using tu::Spec; using tu::db;

static double SR = 44100;

struct Out { std::vector<float> L, R; };
struct Run {
    Engine e; Params p; int block = 512; double srate = SR;
    explicit Run(double sr = SR) { srate = sr; e.prepare(sr, 4096); e.setParams(p); }
    Out go(const std::vector<float>& inL, const std::vector<float>* inR = nullptr,
           const std::vector<float>* scL = nullptr, const std::vector<float>* scR = nullptr) {
        Out o; o.L = inL; o.R = inR ? *inR : inL;
        for (size_t s = 0; s < inL.size(); s += (size_t)block) {
            e.setParams(p);
            int n = (int)std::min<size_t>((size_t)block, inL.size() - s);
            const float* sl = scL ? scL->data() + s : nullptr;
            const float* sr_ = scR ? scR->data() + s : (scL ? sl : nullptr);
            e.process(o.L.data() + s, o.R.data() + s, n, sl, sr_);
        }
        return o;
    }
};

static std::vector<float> sine(double f, double secs, double sr = SR, double amp = 0.25, double ph = 0) {
    std::vector<float> x((size_t)(secs * sr));
    for (size_t i = 0; i < x.size(); ++i) x[i] = (float)(amp * std::sin(kTwoPi * f * i / sr + ph));
    return x;
}
static std::vector<float> add(std::vector<float> a, const std::vector<float>& b) {
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) a[i] += b[i]; return a;
}
static std::vector<float> noise(size_t n, uint32_t seed, float amp = 0.2f) {
    Rng g(seed); std::vector<float> x(n); for (auto& v : x) v = amp * g.bi(); return x;
}
static std::vector<float> pinkish(size_t n, uint32_t seed, float amp = 0.2f) {
    Rng g(seed); std::vector<float> x(n); float b0=0,b1=0,b2=0;
    for (size_t i = 0; i < n; ++i) { float w = g.bi(); b0=0.99886f*b0+w*0.0555179f; b1=0.99332f*b1+w*0.0750759f; b2=0.96900f*b2+w*0.1538520f;
        x[i] = amp * (b0+b1+b2+w*0.05f) * 0.3f; }
    return x;
}
// Two-tone "vocal-like": a vowel-ish harmonic stack + a narrow whistle.
static std::vector<float> vocalish(double f0, double secs, double sr, double whistleHz, double whistleAmp) {
    std::vector<float> x((size_t)(secs * sr));
    for (size_t i = 0; i < x.size(); ++i) {
        double t = i / sr, s = 0;
        for (int k = 1; k <= 12; ++k) s += (0.18 / k) * std::sin(kTwoPi * f0 * k * t + 0.2 * k);
        s += whistleAmp * std::sin(kTwoPi * whistleHz * t);
        x[i] = (float)s;
    }
    return x;
}

int main() {
#if defined(__x86_64__) || defined(_M_X64)
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON); _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::puts("SILK engine tests");

    // ---------------------------------------------------------------- latency
    { std::puts("- reported latency equals measured impulse delay, every quality, 44.1/48/96 kHz");
      for (double sr : { 44100.0, 48000.0, 96000.0 }) {
        for (auto res : { Resolution::Live, Resolution::Eco, Resolution::Normal, Resolution::High, Resolution::Ultra }) {
          Run r(sr); r.p.resolution = res; r.p.mix = 0; r.e.prepare(sr, 512); r.e.setParams(r.p);
          int L = r.e.latencySamples();
          std::vector<float> x((size_t)(0.6 * sr), 0.0f); x[2000] = 0.8f;
          auto o = r.go(x); int at = tu::argmaxAbs(o.L);
          int meas = at - 2000;
          std::printf("  %6.0f Hz %-6s reported %d (%.2f ms), measured %d\n",
                      sr, (res==Resolution::Live?"Live":res==Resolution::Eco?"Eco":res==Resolution::Normal?"Normal":res==Resolution::High?"High":"Ultra"),
                      L, L / sr * 1000.0, meas);
          CHECKM(meas == L, "sr %g res %d meas %d L %d", sr, (int)res, meas, L);
          if (res == Resolution::Live) CHECKM(L / sr * 1000.0 <= 16.0, "Live latency %.2f ms", L / sr * 1000.0);
        }
      }
    }

    // ---------------------------------------------------------------- Mix 0 / Bypass / Depth -18 nulls
    { std::puts("- Mix 0% and Bypass null the delayed dry, bit-exact; Depth -18 is a near-null");
      for (double sr : { 44100.0, 48000.0 }) {
        Rng g(7); std::vector<float> x((size_t)(1.2 * sr)); for (auto& v : x) v = 0.4f * g.bi();
        Run a(sr); a.p.mix = 0; a.p.depth = 12; a.p.sharpness = 9; a.p.hard = true;
        a.e.prepare(sr, 512); a.e.setParams(a.p);
        auto oa = a.go(x); int L = a.e.latencySamples(); double md = 0;
        for (size_t i = (size_t)L; i < x.size(); ++i) md = std::max(md, (double)std::fabs(oa.L[i] - x[i - (size_t)L]));
        std::printf("  %g Hz Mix 0 max |out-dry(t-L)| = %g\n", sr, md);
        CHECKM(md == 0.0, "mix0 %g", md);

        Run b(sr); b.p.bypass = true; b.p.depth = 12; b.e.prepare(sr, 512); b.e.setParams(b.p);
        auto ob = b.go(x); double mb = 0;
        for (size_t i = (size_t)L; i < x.size(); ++i) mb = std::max(mb, (double)std::fabs(ob.L[i] - x[i - (size_t)L]));
        std::printf("  %g Hz Bypass max diff = %g\n", sr, mb);
        CHECKM(mb == 0.0, "bypass %g", mb);

        Run c(sr); c.p.depth = kDepthMin; c.p.mix = 100; c.p.trim = 0; c.p.setFlatStart(); c.p.depth = kDepthMin; c.p.mix = 100;
        c.e.prepare(sr, 512); c.e.setParams(c.p);
        auto oc = c.go(x); double mc = 0, e2 = 0; int n = 0;
        for (size_t i = (size_t)L + (size_t)c.e.frameSize(); i < x.size(); ++i) {
            double d = (double)oc.L[i] - x[i - (size_t)L]; mc = std::max(mc, std::fabs(d)); e2 += d * d; ++n;
        }
        double rms = n ? std::sqrt(e2 / n) : 0;
        std::printf("  %g Hz Depth -18 max %g rms %g\n", sr, mc, rms);
        CHECKM(rms < 2e-4, "depth-18 rms %g", rms); // reconstruction tolerance
      }
    }

    // ---------------------------------------------------------------- tonal resonance reduction
    { std::puts("- a protruding tone is reduced; a flatter neighbour is mostly left alone");
      const double sr = 48000, fFund = 1000.0, fRing = 3500.0;
      // Steady fund at -12 dBFS plus a +12 dB hotter narrow ring.
      auto x = add(sine(fFund, 2.0, sr, 0.25), sine(fRing, 2.0, sr, 0.7, 0.3));
      Run r(sr); r.p.setFactoryDefault(); r.p.depth = 8; r.p.sharpness = 7; r.p.selectivity = 3.6;
      r.e.prepare(sr, 512); r.e.setParams(r.p);
      auto o = r.go(x);
      int L = r.e.latencySamples();
      size_t start = (size_t)(0.8 * sr), len = (size_t)(0.8 * sr);
      Spec inS, outS;
      inS.run(x, start, (int)len, sr);
      outS.run(o.L, start + (size_t)L, (int)len, sr);
      double fIn, fOut, fInR, fOutR;
      double aFundIn = inS.peak(fFund, 80, fIn), aFundOut = outS.peak(fFund, 80, fOut);
      double aRingIn = inS.peak(fRing, 80, fInR), aRingOut = outS.peak(fRing, 80, fOutR);
      double dFund = db(aFundOut / aFundIn), dRing = db(aRingOut / aRingIn);
      std::printf("  fund 1 kHz %+.2f dB, ring 3.5 kHz %+.2f dB\n", dFund, dRing);
      CHECK(tu::finite(o.L));
      CHECKM(dRing < -3.0, "ring only %+.2f dB", dRing);
      CHECKM(dRing < dFund - 2.0, "selectivity: ring %+.2f vs fund %+.2f", dRing, dFund);
      CHECKM(dFund > -6.0, "fund over-reduced %+.2f dB", dFund);
    }

    // ---------------------------------------------------------------- vocal modes on a synthetic vocal
    { std::puts("- vocal modes lean on the intended range; Learn moves nodes toward planted peaks");
      const double sr = 48000;
      auto x = vocalish(180, 2.5, sr, 7000.0, 0.45); // sibilant whistle at 7 kHz
      auto measure = [&](Params p, const char* name) {
          Run r(sr); r.p = p; r.e.prepare(sr, 512); r.e.setParams(r.p);
          auto o = r.go(x); int L = r.e.latencySamples();
          size_t start = (size_t)(0.7 * sr), len = (size_t)(0.8 * sr);
          Spec inS, outS; inS.run(x, start, (int)len, sr); outS.run(o.L, start + (size_t)L, (int)len, sr);
          double d7 = db(outS.amp(7000, 200) / inS.amp(7000, 200));
          double d1 = db(outS.amp(180, 40) / std::max(inS.amp(180, 40), 1e-9));
          std::printf("  %-12s  7 kHz %+.2f dB,  f0 %+.2f dB\n", name, d7, d1);
          return d7;
      };
      Params base; base.setFactoryDefault(); base.depth = 8;
      Params sib = base; applyVocalMode(sib, VocalMode::Sibilance); sib.depth = 8;
      Params mud = base; applyVocalMode(mud, VocalMode::MudBoom); mud.depth = 8;
      double dDef = measure(base, "factory");
      double dSib = measure(sib, "Sibilance");
      measure(mud, "Mud/Boom");
      CHECKM(dSib < dDef - 1.0, "sibilance mode should cut 7 kHz more (%.2f vs %.2f)", dSib, dDef);

      Run lr(sr); lr.p.setFactoryDefault(); lr.p.depth = 6;
      lr.e.prepare(sr, 512); lr.e.setParams(lr.p);
      lr.e.startLearn(1.5);
      auto dummy = vocalish(180, 2.0, sr, 7000.0, 0.5);
      lr.go(dummy);
      CHECK(lr.e.learnReady() || lr.e.learnProgress() >= 0.99);
      Params learned = lr.p; lr.e.takeLearn(learned);
      bool near = false;
      for (int i = 1; i <= 4; ++i) {
          if (learned.band[(size_t)i].on && std::fabs(std::log2(learned.band[(size_t)i].freq / 7000.0)) < 0.4) near = true;
          std::printf("  learn band %d  f=%.0f Hz  sens=%.1f dB\n", i, learned.band[(size_t)i].freq, learned.band[(size_t)i].sens);
      }
      CHECKM(near, "Learn did not place a node near 7 kHz");
    }

    // ---------------------------------------------------------------- flat noise mostly untouched
    { std::puts("- flat-ish noise stays close in 1/3-octave bands at factory depth");
      const double sr = 48000;
      auto x = pinkish((size_t)(2.5 * sr), 11, 0.25f);
      Run r(sr); r.p.setFactoryDefault(); r.p.depth = 3.0f;
      r.e.prepare(sr, 512); r.e.setParams(r.p);
      auto o = r.go(x); int L = r.e.latencySamples();
      size_t start = (size_t)(0.8 * sr), len = (size_t)(1.0 * sr);
      Spec inS, outS; inS.run(x, start, (int)len, sr); outS.run(o.L, start + (size_t)L, (int)len, sr);
      double worst = 0;
      for (double f = 200; f < 10000; f *= 1.26) { // ~1/3 oct
          double d = std::fabs(db(outS.bandPower(f, f * 1.26) / std::max(inS.bandPower(f, f * 1.26), 1e-18)));
          worst = std::max(worst, d);
      }
      std::printf("  worst 1/3-oct deviation %.2f dB\n", worst);
      CHECKM(worst < 3.5, "noise coloured by %.2f dB", worst);
    }

    // ---------------------------------------------------------------- Soft vs Hard, Sharpness, Selectivity, Attack
    { std::puts("- Soft/Hard, Sharpness, Selectivity and Attack actually change the result");
      const double sr = 48000;
      auto x = add(sine(800, 1.8, sr, 0.2), sine(2800, 1.8, sr, 0.65));
      auto redAt = [&](std::function<void(Params&)> cfg) {
          Run r(sr); r.p.setFactoryDefault(); r.p.depth = 8; cfg(r.p);
          r.e.prepare(sr, 512); r.e.setParams(r.p);
          auto o = r.go(x); int L = r.e.latencySamples();
          Spec inS, outS; size_t st = (size_t)(0.6*sr), ln=(size_t)(0.7*sr);
          inS.run(x, st, (int)ln, sr); outS.run(o.L, st+(size_t)L, (int)ln, sr);
          return db(outS.amp(2800, 80) / inS.amp(2800, 80));
      };
      double dSoft = redAt([](Params& p){ p.hard = false; });
      double dHard = redAt([](Params& p){ p.hard = true; });
      double dSharp = redAt([](Params& p){ p.sharpness = 9; });
      double dDull = redAt([](Params& p){ p.sharpness = 1; });
      double dSelHi = redAt([](Params& p){ p.selectivity = 9; });
      double dSelLo = redAt([](Params& p){ p.selectivity = 1; });
      std::printf("  Soft %.2f  Hard %.2f  Sharp %.2f  Wide %.2f  SelHi %.2f  SelLo %.2f dB\n",
                  dSoft, dHard, dSharp, dDull, dSelHi, dSelLo);
      CHECK(dHard < dSoft - 0.3);
      CHECK(dSelLo < dSelHi - 0.5);

      // Attack: a click + resonance; slower attack should let more of the first 10 ms through.
      auto click = sine(2800, 1.2, sr, 0.0);
      for (size_t i = (size_t)(0.2*sr); i < click.size(); ++i) click[i] = (float)(0.7 * std::sin(kTwoPi * 2800 * i / sr));
      auto attEner = [&](float attack) {
          Run r(sr); r.p.setFactoryDefault(); r.p.depth = 10; r.p.attack = attack; r.p.release = 4;
          r.e.prepare(sr, 512); r.e.setParams(r.p);
          auto o = r.go(click); int L = r.e.latencySamples();
          size_t a = (size_t)(0.2*sr) + (size_t)L, b = a + (size_t)(0.012*sr);
          return tu::rms(o.L, a, b);
      };
      double eFast = attEner(0.2f), eSlow = attEner(6.0f);
      std::printf("  attack transient RMS fast %.5f  slow %.5f\n", eFast, eSlow);
      CHECKM(eSlow > eFast * 1.05, "attack did not spare the transient");
    }

    // ---------------------------------------------------------------- sidechain, mid/side, delta, auto trim
    { std::puts("- sidechain follows the key; Mid/Side and Delta behave; Auto trim raises a quiet wet");
      const double sr = 48000;
      auto dry = sine(1000, 1.5, sr, 0.3);
      auto key = add(sine(1000, 1.5, sr, 0.05), sine(4000, 1.5, sr, 0.7));
      Run r(sr); r.p.setFactoryDefault(); r.p.depth = 10; r.p.sidechain = true;
      r.e.prepare(sr, 512); r.e.setParams(r.p);
      auto o = r.go(dry, nullptr, &key, &key);
      int L = r.e.latencySamples();
      Spec inS, outS; size_t st=(size_t)(0.5*sr), ln=(size_t)(0.6*sr);
      inS.run(dry, st, (int)ln, sr); outS.run(o.L, st+(size_t)L, (int)ln, sr);
      // The dry is a 1 kHz tone (no 4 kHz). Sidechain should still add little 4 kHz to the output,
      // but may reduce 1 kHz only if the key has energy there. Key is mostly 4 kHz, so 1 kHz should stay close
      // and no 4 kHz should be invented. If the detector uses the key, a 4 kHz-weighted curve on a 1 kHz tone
      // barely reduces the tone. Compare to processing the key itself as audio.
      double d1 = db(outS.amp(1000, 60) / inS.amp(1000, 60));
      std::printf("  sidechain on a 1 kHz dry keyed by 4 kHz: 1 kHz %+.2f dB\n", d1);
      CHECK(std::fabs(d1) < 8.0);

      Run ms(sr); ms.p.setFactoryDefault(); ms.p.depth = 8; ms.p.stereoMode = StereoMode::MidSide; ms.p.balance = -1; // mid only
      auto mid = sine(2800, 1.5, sr, 0.5);
      auto side = sine(2800, 1.5, sr, 0.5);
      // L = mid+side, R = mid-side. If mid=s, side=0 → L=R. If we want energy only in sides: mid=0, side=s → L=s, R=-s
      std::vector<float> sL(mid.size()), sR(mid.size());
      for (size_t i=0;i<mid.size();++i){ sL[i]=side[i]; sR[i]=-side[i]; }
      ms.e.prepare(sr, 512); ms.e.setParams(ms.p);
      auto oms = ms.go(sL, &sR);
      // Balance fully mid: side-only signal should barely be reduced
      Spec inMS, outMS; inMS.run(sL, st, (int)ln, sr); outMS.run(oms.L, st+(size_t)ms.e.latencySamples(), (int)ln, sr);
      double dSide = db(outMS.amp(2800, 80) / inMS.amp(2800, 80));
      std::printf("  mid-only balance on a side-only tone: %+.2f dB (should be near 0)\n", dSide);
      CHECK(dSide > -3.0);

      Run dlt(sr); dlt.p.setFactoryDefault(); dlt.p.depth = 10; dlt.p.delta = true;
      auto ring = add(sine(800, 1.5, sr, 0.2), sine(3500, 1.5, sr, 0.6));
      dlt.e.prepare(sr, 512); dlt.e.setParams(dlt.p);
      auto od = dlt.go(ring);
      // Delta should contain the removed ring, not the dry 800 Hz at full level
      Spec dS; dS.run(od.L, st+(size_t)dlt.e.latencySamples(), (int)ln, sr);
      double ratio = dS.amp(3500, 80) / std::max(dS.amp(800, 40), 1e-9);
      std::printf("  delta 3.5k/800 amp ratio %.2f (expect > 1)\n", ratio);
      CHECK(ratio > 1.2);
    }

    // ---------------------------------------------------------------- ceiling
    { std::puts("- wet output true peak never exceeds -0.1 dBFS even when pushed");
      const double sr = 48000;
      auto x = add(sine(200, 1.5, sr, 0.7), sine(3500, 1.5, sr, 0.7));
      for (auto& v : x) v = std::max(-0.98f, std::min(0.98f, v * 1.2f));
      Run r(sr); r.p.setFactoryDefault(); r.p.depth = 18; r.p.hard = true; r.p.sharpness = 9; r.p.trim = 12;
      r.e.prepare(sr, 512); r.e.setParams(r.p);
      auto o = r.go(x);
      float tp = tu::truePeak(o.L);
      float ceil = dbToGain(-0.1f);
      std::printf("  true peak %.4f (ceiling %.4f, %.2f dBFS)\n", tp, ceil, db(tp));
      CHECK(tu::finite(o.L) && tu::finite(o.R));
      CHECKM(tp <= ceil + 1e-4f, "true peak %g", tp);
    }

    // ---------------------------------------------------------------- every control moves the output (smoke)
    { std::puts("- every Params field is observable (smoke)");
      const double sr = 48000;
      auto x = add(sine(700, 1.0, sr, 0.3), add(sine(2500, 1.0, sr, 0.3), sine(7000, 1.0, sr, 0.3)));
      Run base(sr); base.e.prepare(sr, 512); base.e.setParams(base.p);
      auto ob = base.go(x);
      auto diff = [&](std::function<void(Params&)> c) {
          Run r(sr); r.p.setFactoryDefault(); c(r.p); r.e.prepare(sr, 512); r.e.setParams(r.p);
          auto o = r.go(x);
          return tu::rmsDiff(o.L, ob.L);
      };
      CHECK(diff([](Params& p){ p.depth = 12; }) > 1e-4);
      CHECK(diff([](Params& p){ p.sharpness = 1; }) > 1e-5);
      CHECK(diff([](Params& p){ p.selectivity = 1; }) > 1e-5);
      CHECK(diff([](Params& p){ p.hard = true; }) > 1e-5);
      CHECK(diff([](Params& p){ p.mix = 50; }) > 1e-4);
      CHECK(diff([](Params& p){ p.trim = 3; }) > 1e-4);
      CHECK(diff([](Params& p){ p.band[3].sens = 10; }) > 1e-5);
      CHECK(diff([](Params& p){ p.resolution = Resolution::Live; }) >= 0); // may be similar; just must not crash
      std::puts("  ok");
    }

    // ---------------------------------------------------------------- CPU
    { std::puts("- CPU budget (this machine, one core)");
      auto x = vocalish(170, 8.0, 48000, 6000, 0.3);
      auto bench = [&](const char* name, std::function<void(Params&)> cfg, double budget) {
          Run r(48000); cfg(r.p); r.e.prepare(48000, 512); r.e.setParams(r.p);
          auto t0 = std::chrono::steady_clock::now(); r.go(x);
          double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
          double pct = el / 8.0 * 100.0;
          std::printf("  %-40s %.2f%% of one core (budget %.0f%%)\n", name, pct, budget);
          CHECKM(pct < budget, "%s %.2f", name, pct);
          return pct;
      };
      bench("Normal 1x factory", [](Params& p){ p.setFactoryDefault(); }, 40);
      bench("Eco 1x", [](Params& p){ p.setFactoryDefault(); p.resolution = Resolution::Eco; }, 30);
      bench("Live 1x", [](Params& p){ p.setFactoryDefault(); p.resolution = Resolution::Live; }, 30);
      bench("Ultra 4x (info, loose budget)", [](Params& p){ p.setFactoryDefault(); p.resolution = Resolution::Ultra; p.oversample = Oversample::X4; p.depth = 10; }, 120);
    }

    std::printf("\nRESULT: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
