# SILK

A dynamic resonance suppressor. Put it on a mixer track, the same way you would put on an EQ or a de-esser. It finds notes and narrow bands that stick out of the sound and turns them down as they appear, leaving the rest of the tone alone.

It is aimed first at vocals, and it also works on other sources. It is an **effect**, not a generator.

The main controls use the same names, ranges, defaults and on-screen placement as oeksound soothe2, so a soothe2 tutorial maps onto SILK. The code, knobs, colours and typography are original. This is not an oeksound product and it does not include soothe2 or soothe3 features that sit outside that control set.

## Controls

- **Soft / Hard** — Soft (default) is less level-dependent and usually more transparent. Hard is more aggressive and more level-dependent.
- **Depth** (−18 to +18, factory 3.0) — how hard the suppressor works. This is a unitless depth, not dB. −18 is off (a near-null of the wet path).
- **Sharpness** (factory 4.8) and **Selectivity** (factory 3.6).
- **Attack** (factory 1.0) and **Release** (factory labelled “fast” at 2.0). Attack is faster at high frequencies.
- **Stereo** — left | right (factory) or mid | side, plus Link (factory 100 %) and Balance (even).
- **Quality** — Oversample 1× / 2× / 4× (default 1×) and Resolution Live / Eco / Normal (default) / High / Ultra, with a quality lock and optional offline settings.
- **Sidechain** — detect from a key input, with listen.
- **Mix** (factory 100 %), **Trim** (wet only, ±12 dB, factory 0.0 dB), optional Auto trim (off by default), **Delta**, and in-plugin **Bypass** (the process still runs; host bypass saves CPU).
- **Graph** — a white sensitivity curve (a focus mask: boosting a node means more reduction, not a boost), a steel-blue floor, and cream reduction. Six nodes: low cut, four general-purpose bands (yellow / orange / red / purple), and high cut. Types include peaking, low / high / band shelf, band reject, tilt, and cut slopes 6–48 dB/oct.
- **Vocal** modes (graph toolbar only): Harsh, Sibilance, Mud / Boom, Nasal, Ring, Breath / Air. **Learn** listens for a few seconds and parks / boosts general-purpose nodes on the average peaks. These do not move the soothe-mapped knobs.

Factory presets cover gentle and close-mic lead vocal, de-ess, harsh vowels, mud, nasal mid, room ring, breath, doubles, spoken verse, vocal into a dense mix, air only, plus drum overheads, acoustic guitar and a gentle mix-bus starting point.

## Latency and loudness

SILK has to look at a short slice of audio, so it reports a fixed latency to the host. FL Studio compensates for it automatically. The dry path is delayed by exactly the same amount, so Mix at 0 % and Bypass are a bit-exact delayed dry even if the input is hot.

At 48 kHz, factory Normal quality is 2120 samples (44.17 ms). Live is 328 samples (6.83 ms). Eco and High share Normal’s window; Ultra is 4168 samples (86.83 ms). Figures scale with sample rate; Live stays around 7 ms.

A true-peak safety limiter sits at the very end, inside the reported latency, with a working ceiling of −0.2 dBTP and a last-resort clamp at −0.1 dBFS. At normal levels it does not engage.

## Building

```
git clone --depth 1 --branch 8.0.4 https://github.com/juce-framework/JUCE.git JUCE
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

macOS universal: add `-G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0`. See `.github/workflows/macos.yml`. Tests are described in [TESTING.md](TESTING.md); the design is in [PLAN.md](PLAN.md).

The plugin is JUCE 8 / CMake, VST3 and AU (VST3 only on Linux). v1.0 is a macOS universal build, ad-hoc signed.
