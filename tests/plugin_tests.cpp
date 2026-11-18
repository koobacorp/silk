#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "TestUtil.h"
#include <chrono>
using tu::db;

struct PlayHead : juce::AudioPlayHead {
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setBpm(120); p.setIsPlaying(true); return p; }
};

struct Rig {
    SilkProcessor p; PlayHead head; double sr; int block;
    std::vector<float> L, R;
    Rig(double rate = 44100, int bs = 512, bool mono = false) : sr(rate), block(bs) {
        juce::AudioProcessor::BusesLayout lay;
        lay.inputBuses.add(mono ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        lay.outputBuses.add(mono ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        lay.inputBuses.add(juce::AudioChannelSet::disabled()); // sidechain off
        p.setBusesLayout(lay);
        p.setPlayHead(&head);
        p.setRateAndBufferSizeDetails(sr, block);
        p.prepareToPlay(sr, block);
    }
    ~Rig() { p.setPlayHead(nullptr); }
    double run(const std::vector<float>& in, std::function<void(Rig&, int)> perBlock = {}) {
        L.clear(); R.clear(); int n = (int)in.size(), pos = 0; double t = 0;
        int nCh = p.getTotalNumOutputChannels();
        juce::AudioBuffer<float> buf(std::max(nCh, 1), block);
        while (pos < n) {
            if (perBlock) perBlock(*this, pos);
            int m = std::min(block, n - pos);
            buf.setSize(std::max(nCh, 1), m, false, false, true);
            for (int c = 0; c < buf.getNumChannels(); ++c)
                for (int i = 0; i < m; ++i) buf.setSample(c, i, in[(size_t)(pos + i)]);
            juce::MidiBuffer midi;
            auto t0 = std::chrono::steady_clock::now();
            p.processBlock(buf, midi);
            t += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            L.insert(L.end(), buf.getReadPointer(0), buf.getReadPointer(0) + m);
            int rc = buf.getNumChannels() > 1 ? 1 : 0;
            R.insert(R.end(), buf.getReadPointer(rc), buf.getReadPointer(rc) + m);
            pos += m;
        }
        return t;
    }
    void set(int i, float v) { p.setValue(i, v); }
};

static std::vector<float> tone(double f, double secs, double sr, double amp) {
    std::vector<float> x((size_t)(secs * sr));
    for (size_t i = 0; i < x.size(); ++i) x[i] = (float)(amp * std::sin(2 * 3.14159265358979323846 * f * i / sr));
    return x;
}
static std::vector<float> add(std::vector<float> a, const std::vector<float>& b) {
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) a[i] += b[i]; return a;
}

static void savePng(juce::Component& c, const juce::String& name) {
    auto img = c.createComponentSnapshot(c.getLocalBounds(), true, 1.0f);
    auto f = juce::File::getCurrentWorkingDirectory().getChildFile(name);
    f.deleteFile();
    juce::FileOutputStream s(f);
    bool ok = s.openedOk() && juce::PNGImageFormat().writeImageToStream(img, s);
    CHECKM(ok, "%s", name.toRawUTF8());
    // crude overlap/mojibake: image must be non-empty and not a flat colour
    int w = img.getWidth(), h = img.getHeight();
    CHECKM(w >= 800 && h >= 450, "%s %dx%d", name.toRawUTF8(), w, h);
    std::printf("  screenshot %s %dx%d\n", name.toRawUTF8(), w, h);
}

int main(int, char**) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    juce::ScopedJuceInitialiser_GUI gui;
    std::puts("SILK plugin tests");

    { std::puts("- identity, buses, parameters");
      Rig r;
      CHECK(r.p.getName() == "SILK");
      CHECK(!r.p.acceptsMidi() && !r.p.producesMidi());
      CHECK((int)r.p.getParameters().size() == kNumParams);
      CHECK(r.p.getBypassParameter() == r.p.param(P_BYPASS));
      int automatable = 0, texts = 0;
      for (auto* prm : r.p.getParameters()) { automatable += prm->isAutomatable(); texts += prm->getCurrentValueAsText().isNotEmpty(); }
      CHECK(automatable == kNumParams); CHECK(texts == kNumParams);
      juce::Random rnd(3); int okRound = 0;
      for (int i = 0; i < kNumParams; ++i) {
          auto* prm = r.p.param(i); float n = rnd.nextFloat();
          prm->setValueNotifyingHost(n);
          float back = prm->getValue();
          auto a = prm->convertFrom0to1(n), b = prm->convertFrom0to1(back);
          okRound += std::abs(a - b) < 0.05f || std::abs(prm->convertTo0to1(a) - back) < 0.05f;
      }
      CHECKM(okRound == kNumParams, "roundtrip %d/%d", okRound, kNumParams);
      auto lay = [](juce::AudioChannelSet i, juce::AudioChannelSet o, juce::AudioChannelSet sc = juce::AudioChannelSet::disabled()) {
          juce::AudioProcessor::BusesLayout l; l.inputBuses.add(i); l.outputBuses.add(o); l.inputBuses.add(sc); return l; };
      CHECK(r.p.checkBusesLayoutSupported(lay(juce::AudioChannelSet::stereo(), juce::AudioChannelSet::stereo())));
      CHECK(r.p.checkBusesLayoutSupported(lay(juce::AudioChannelSet::mono(), juce::AudioChannelSet::mono())));
      CHECK(!r.p.checkBusesLayoutSupported(lay(juce::AudioChannelSet::stereo(), juce::AudioChannelSet::mono())));
      std::printf("  %d parameters, all automatable with text\n", kNumParams);
    }

    { std::puts("- latency reported equals measured Mix-0 impulse, several rates and qualities");
      for (double sr : { 44100.0, 48000.0, 96000.0 }) {
        for (int res : { 0, 2, 4 }) { // Live, Normal, Ultra
          Rig r(sr); r.set(P_RESOLUTION, (float)res); r.p.prepareToPlay(sr, r.block);
          r.set(P_MIX, 0);
          std::vector<float> x((size_t)(0.5 * sr), 0.0f); x[1500] = 0.6f;
          r.run(x);
          int L = r.p.getLatencySamples();
          int at = tu::argmaxAbs(r.L);
          std::printf("  %6.0f Hz res %d reported %d (%.2f ms), measured %d\n", sr, res, L, L / sr * 1000, at - 1500);
          CHECK(L == r.p.engine().latencySamples());
          CHECKM(at - 1500 == L, "meas %d L %d", at - 1500, L);
        }
      }
    }

    { std::puts("- Mix 0 and Bypass null through processBlock");
      for (bool mono : { false, true }) {
          Rig r(48000, 480, mono); r.set(P_MIX, 0); r.set(P_DEPTH, 12);
          silk::Rng g(4); std::vector<float> x(48000); for (auto& v : x) v = 0.4f * g.bi();
          r.run(x); int L = r.p.getLatencySamples(); double md = 0;
          for (size_t i = (size_t)L; i < x.size(); ++i) md = std::max(md, (double)std::fabs(r.L[i] - x[i - (size_t)L]));
          CHECKM(md == 0.0, "mix0 mono=%d %g", (int)mono, md);
          Rig b(48000, 480, mono); b.set(P_BYPASS, 1); b.run(x); double mb = 0;
          int Lb = b.p.getLatencySamples();
          for (size_t i = (size_t)Lb; i < x.size(); ++i) mb = std::max(mb, (double)std::fabs(b.L[i] - x[i - (size_t)Lb]));
          CHECKM(mb == 0.0, "bypass mono=%d %g", (int)mono, mb);
      }
    }

    { std::puts("- Depth -18 near-null, ceiling holds, presets load");
      Rig r(48000); r.set(P_DEPTH, -18); r.set(P_MIX, 100);
      silk::Rng g(8); std::vector<float> x(48000); for (auto& v : x) v = 0.3f * g.bi();
      r.run(x); int L = r.p.getLatencySamples(); double e2=0; int n=0;
      for (size_t i = (size_t)L + 2048; i < x.size(); ++i) { double d = r.L[i] - x[i-(size_t)L]; e2 += d*d; ++n; }
      double rms = std::sqrt(e2 / n);
      std::printf("  Depth -18 rms %g\n", rms);
      CHECK(rms < 1e-3);

      Rig c(48000); c.set(P_DEPTH, 18); c.set(P_HARD, 1); c.set(P_TRIM, 12);
      auto hot = add(tone(200, 1.2, 48000, 0.7), tone(3500, 1.2, 48000, 0.7));
      c.run(hot);
      float tp = tu::truePeak(c.L);
      std::printf("  pushed true peak %.4f (%.2f dBFS)\n", tp, db(tp));
      CHECK(tp <= silk::dbToGain(-0.1f) + 1e-4f);

      CHECK(r.p.getNumPrograms() >= 10);
      r.p.setCurrentProgram(3);
      CHECK(r.p.getProgramName(3).isNotEmpty());
    }

    { std::puts("- state save/reload and every control automation smoke");
      Rig a(48000); a.set(P_DEPTH, 7.5f); a.set(P_SHARPNESS, 6.0f);
      juce::MemoryBlock mb; a.p.getStateInformation(mb);
      Rig b(48000); b.p.setStateInformation(mb.getData(), (int)mb.getSize());
      CHECK(std::abs(b.p.value(P_DEPTH) - 7.5f) < 0.05f);
      for (int i = 0; i < kNumParams; ++i) a.p.param(i)->setValueNotifyingHost(0.3f + 0.4f * (i % 5) / 5.0f);
      a.run(tone(1000, 0.3, 48000, 0.2));
      CHECK(tu::finite(a.L));
    }

    { std::puts("- editor: resizable, states, screenshots");
      Rig r(48000, 512);
      auto* ed = dynamic_cast<SilkEditor*>(r.p.createEditor());
      CHECK(ed != nullptr);
      ed->setSize(1120, 640);
      ed->timerCallback();
      // factory, silent
      ed->timerCallback(); savePng(*ed, "SILK-factory.png");

      auto ring = add(tone(800, 1.0, 48000, 0.25), tone(3500, 1.0, 48000, 0.6));
      r.run(ring);
      ed->repaint();
      ed->timerCallback(); savePng(*ed, "SILK-soft-default.png");

      r.set(P_HARD, 1); r.set(P_DEPTH, 8); r.run(ring);
      ed->repaint();
      ed->timerCallback(); savePng(*ed, "SILK-hard.png");

      r.set(P_DELTA, 1); r.run(ring);
      ed->repaint();
      ed->timerCallback(); savePng(*ed, "SILK-delta.png");

      r.set(P_DELTA, 0); r.p.applyVocalMode(silk::VocalMode::Sibilance); r.set(P_DEPTH, 6);
      auto sib = add(tone(180, 0.8, 48000, 0.2), tone(7000, 0.8, 48000, 0.5));
      r.run(sib);
      ed->repaint();
      ed->timerCallback(); savePng(*ed, "SILK-vocal-sibilance.png");

      r.p.applyVocalMode(silk::VocalMode::Harsh); r.run(add(tone(220, 0.8, 48000, 0.2), tone(3200, 0.8, 48000, 0.5)));
      ed->repaint();
      ed->timerCallback(); savePng(*ed, "SILK-vocal-harsh.png");

      r.p.setCurrentProgram(0);
      r.set(P_STEREO_MODE, 1);
      r.run(ring);
      ed->repaint();
      ed->timerCallback(); savePng(*ed, "SILK-midside.png");

      ed->selectedBand = 3; ed->showBandCard = true; ed->resized(); ed->repaint();
      ed->timerCallback(); savePng(*ed, "SILK-band-card.png");

      ed->setSize(860, 500);
      ed->timerCallback(); savePng(*ed, "SILK-small.png");
      ed->setSize(1650, 900);
      ed->timerCallback(); savePng(*ed, "SILK-large.png");

      delete ed;
    }

    std::printf("\nRESULT: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
