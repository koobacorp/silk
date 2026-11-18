# Testing SILK v1.0

Automated tests run on the engine (no window needed) and again in the plugin.

## Engine

`silk_engine_tests` covers:

- Reported latency equals the measured impulse delay at 44.1 / 48 / 96 kHz for Live, Eco, Normal, High and Ultra
- Mix 0 % and Bypass are a bit-exact delayed dry; Depth −18 at Mix 100 % is a near-null (FFT reconstruction tolerance)
- A protruding tone is reduced more than a flatter neighbour
- Vocal modes lean on the intended range; Learn parks general-purpose nodes on planted peaks
- Flat-ish noise stays close in 1/3-octave bands at factory depth
- Soft / Hard, Sharpness, Selectivity and Attack change the result
- Sidechain follows the key; mid-only balance leaves a side-only tone alone; Delta emphasises the reduced region; Auto trim raises a quiet wet
- Wet true peak stays under −0.1 dBFS when pushed
- Every Params field is observable
- CPU budget on the machine that runs the test (Normal 1× is the plan’s everyday quality)

## Plugin

`silk_plugin_tests` covers the stereo insert layout with an optional sidechain bus, all 67 automatable parameters, latency vs a Mix-0 impulse (Live / Normal / Ultra at 44.1 / 48 / 96 kHz), Mix 0 and Bypass through `processBlock` (mono and stereo), Depth −18, the clip-safe ceiling, factory presets, state round-trip, an automation smoke test on every control, and UI screenshots:

- `SILK-factory.png`
- `SILK-soft-default.png`
- `SILK-hard.png`
- `SILK-delta.png`
- `SILK-vocal-sibilance.png`
- `SILK-vocal-harsh.png`
- `SILK-midside.png`
- `SILK-band-card.png`
- `SILK-small.png`
- `SILK-large.png`

pluginval at strictness 10 runs on the Mac VST3 and AU. auval (`aufx Silk Koob`) runs on the AU.

Listening in a host is still the real test. Automated tests cannot tell you how it feels on a real voice in FL Studio.
