# SILK — dynamic resonance suppressor for vocals

Status: **plan only in this folder.** This is the build spec. Do not treat anything here as already built, pushed, or installed.

**SILK** is a VST3 audio effect (AU optional) you put on a mixer track in FL Studio, primarily a vocal. It listens for harsh, ringing, or boomy spots as the voice plays, and turns those spots down only while they stick out. The rest of the vocal keeps its brightness and life.

It is inspired by the *job* and the *workflow* of oeksound soothe2 (and, in the same family, Mercurial Tones Dagon 2). A soothe2 tutorial should teach someone how to use SILK, because the core controls, names, ranges, defaults, and on-screen placement follow soothe2 as closely as public information allows. It is **not a copy**. The code, the product name, the logo, the fonts, the knob artwork, the icons, and all text are original. No oeksound graphics, names, or preset wording are reused.

Primary focus: **vocals** (lead, doubles, spoken word, stacked ad-libs). It still works on other tracks, but the factory presets, vocal modes, and Learn feature are built for voices first.

No machine learning and no paid compute. The owner's name must not appear in any repo file, comment, preset, credit, or install note.

---

## 1. What you do with it

This is the same six-step loop soothe2's own manual teaches, under our name:

1. Start from the factory default preset. Turn **Depth** up until it is clearly taking too much, then back off a little.
2. Adjust **Sharpness** to the material — lower for a general smooth, higher to target tight rings.
3. Press **Delta** and listen to what is being removed.
4. Drag the coloured nodes on the frequency graph to focus the processing. Boost a region (raise **Sens**) until you hear the resonances you want gone. On a vocal, you can also pick a **Vocal mode** or press **Learn** (see section 5) — these sit in the preset toolbar and do not move any soothe2-mapped control.
5. Turn Delta off and settle on a Depth that sounds right.
6. Use the in-plugin **Bypass** to compare, and add **Trim** if the processed vocal got quieter.

Put SILK on the vocal mixer track, not on an instrument slot. It has stereo in and stereo out. For sidechain ducking of a reverb or guitar bus, route the vocal into the **Sidechain** input and turn Sidechain on.

It should already sound useful with only Depth and the default curve. Everything else is there when you need it.

---

## 2. How it works (plain language)

SILK does not use a static EQ cut. It keeps listening.

1. A short-time spectral analysis (an STFT) splits the incoming audio into many narrow frequency slices, many times a second.
2. It builds a **smoothed spectral envelope** — a blurry picture of the voice's overall shape, without the sharp spikes.
3. Each slice is compared to that envelope. If a slice is **prominent** (it sticks out over its neighbours), that slice is a candidate for reduction. This is how a ringing room note, a sharp "sss", or a nasal honk gets found without cutting the whole region all the time.
4. **Selectivity** is the threshold on that prominence: high means only the most obvious spikes are touched; low means almost every bump is eased down.
5. **Sharpness** controls how narrow and deep those cuts are (higher = more surgical).
6. **Depth** scales how hard the cuts are allowed to go. The number is not "dB of reduction"; the graph shows the actual reduction.
7. The white **sensitivity curve**, shaped by the band nodes, is a focus mask — like a sidechain EQ. Boosting a band makes SILK more sensitive there (more reduction). Cutting a band tells it to leave that area alone. The process does not copy the curve as a static EQ; it only weights where the detector is allowed to work.
8. **Attack** and **Release** smooth those cuts in time. The displayed values are constants, not milliseconds, and the real times are faster at high frequencies than at low ones, the same way soothe2 describes them.
9. The per-slice gains are applied in the spectrum, then the audio is rebuilt (overlap-add). **Mix** blends dry and wet. **Trim** boosts only the wet path, so you can level-match an A/B. **Delta** is the difference (what was removed).

**Soft** vs **Hard** changes the character of that detector. Soft (the default) is less level-dependent, more transparent, kinder to transients, and usually the right starting point on a sung vocal. Hard is more level-dependent and more aggressive at the same Depth number — useful when Soft is topping out on a very ringy recording. A Depth of 3.0 in Soft is not the same as a Depth of 3.0 in Hard.

When **Sidechain** is on, detection (and the reduction graph) follows the external key, and the reduction is applied to the main input. Main and key must arrive in time with each other; FL's delay compensation has to be correct, and the UI should warn if the sidechain input is silent or obviously late.

This is our own implementation of a well-known spectral idea. It is not soothe2's private algorithm, which is unpublished.

---

## 3. Core controls (soothe2-mapped)

Wherever a public source gives a name, a default, or a range, we match it. Where the official manual does not publish a number, the plan uses values confirmed from official or widely published soothe2 screenshots, and says so. Layout is in section 7.

### 3.1 Main

| Control | Behaviour | Default | Range / notes |
|---|---|---|---|
| **Soft / Hard** | Two processing characters. Soft is default, more transparent, less level-dependent. Hard is stronger and easier to overuse. Parameters are relative to the mode. | Soft | Two-state |
| **Depth** | Overall amount of reduction. Not a dB value. At the top of the range, notches can reach about 60 dB and will mangle the source — that headroom is for creative use, not everyday vocals. Always set Depth to the take, even when a preset is loaded. | **3.0** (Factory Default) | **−18 to +18**. −18 is the Flat Start / “off” end (no reduction). Confirmed min and Factory Default from public preset tutorials; max inferred as the symmetric top of the same scale, consistent with published values such as 14.2. |
| **Sharpness** | Higher = deeper, narrower cuts. Lower = broader smoothing of energy build-up. Very high Sharpness can leave a grainy, “all the tone is gone” residue. | **4.8** (from the official soothe2 1.3.x GUI screenshot; 1.1.x factory shots showed 4.6) | Same numeric style as soothe2 (factory sits near the middle-low). Implement the same end-stops visible in public GUIs (treat as a 0–10-class scale unless a screenshot shows otherwise). |
| **Selectivity** | Threshold. High = only the most prominent spikes. Low = almost all resonances. For de-essing: lower for noisy broadband “sss”, higher for whistling or lisping “s”. | **3.6** (official and review screenshots) | Same numeric style as soothe2. |

Soft/Hard, Depth, Sharpness, and Selectivity are interdependent. Changing one may mean touching the others.

### 3.2 Stereo

Greyed out in a mono instance (label the section **Mono** then, as soothe2 does). Enabled for stereo.

| Control | Behaviour | Default |
|---|---|---|
| **Mode** | **left \| right** or **mid \| side**. All stereo controls follow this. Left/right with a low Link is for hard-panned sources. Mid/side with Link at 100% is the usual starting point for a complex stereo image. Lowering Link in mid/side typically widens the image. | **left \| right** (factory GUI) |
| **Link** | 100% = analyse a sum and apply the same reduction to both channels. 0% = dual mono, each channel on its own. | **100%** |
| **Balance** | How much processing each channel (or mid vs side) gets. “even” in the centre. Turning left focuses processing on left (or mid). | **even** |

Each band also has its own **Balance**, in the same Mode as the global control, so you can, for example, tame sibilance more in the sides than the mid.

### 3.3 Speed

| Control | Behaviour | Default |
|---|---|---|
| **Attack** | How quickly cuts appear. Higher = slower, which lets transients (consonants, plosives) through. Displayed as a constant; actual time is frequency-dependent and faster at the top. | **1.0** (official GUI) |
| **Release** | How quickly cuts disappear once the spike is gone. Higher / slower can reduce the “phasey” artifacts of fast-moving notches. Same display rules as Attack. | **fast** (official GUI uses this label at the default; keep that label when the value sits at the factory point) |

### 3.4 Quality

Clicking the quality label opens the quality panel, as in soothe2. The collapsed label shows **oversample × resolution**, e.g. `1× normal`. An asterisk (*) means the offline settings differ from real-time.

| Control | Behaviour | Default |
|---|---|---|
| **Oversample** | **1×, 2×, 4×**. Higher = finer frequency resolution, better at low frequencies and at high Sharpness, more CPU. | **1×** |
| **Resolution** | **Live**, **Eco**, **Normal**, **High**, **Ultra**. Eco roughly halves CPU vs Normal, but can artifact when the spectrum is changing fast. High/Ultra update the detector more often — better on busy or transient material, more CPU. **Live** is our extra choice (soothe2 does not have this word): a shorter analysis window for low latency, with coarser low-frequency resolution. It lives in this same menu so the rest of the layout stays still. | **Normal** |
| **Offline rendering** | When the host reports an offline bounce, use the offline quality instead. Options: **same as real-time** (default), any of the real-time combinations, and **copy to all instances**. Offline settings travel with presets. | same as real-time |
| **Lock** | A lock icon keeps quality settings still when you change presets. | unlocked |

Live vs Quality is therefore a Quality-menu choice, not a second mode switch that would confuse a soothe2 tutorial. The exact latency of the current Quality is always reported to FL with `setLatencySamples` and shown in the UI in milliseconds.

### 3.5 Sidechain

- On/off (lit when in use).
- Headphones: listen to the external key.
- When on, reduction and the graph follow the key, and that reduction is applied to the main input.
- Typical vocal uses: key a reverb or delay return from the lead vocal so the space ducks only where the voice has energy; or key a dense guitar bus from the vocal to carve a hole for the words, especially in mid/side.

### 3.6 Output

| Control | Behaviour | Default |
|---|---|---|
| **Mix** | Dry/wet. 100% is fully processed; **0% is the dry signal only** and must be a sample-accurate dry path (after the reported delay). The reduction graph shows the processed amount, so lowering Mix makes the notches look shallower. Hovering Mix shows the current maximum reduction in dB. | **100%** |
| **Trim** | Output gain compensation on the **wet** path only, for level-matched A/B. | **0.0 dB** (range ±12 dB unless a public automation list shows a wider official range; then match that) |
| **Delta** | Listen to what is being removed (dry minus wet). Can be left on as an effect. Right-click to **lock** Delta so it stays on while you flip presets. | off, unlocked |
| **Bypass** | Soft, glitch-free in-plugin bypass for A/B. The engine keeps running, so this does not save CPU. To save CPU, bypass or disable the plugin from FL. | off |

**Auto gain** (ours, off by default so tutorials still match): a small **Auto** latch next to Trim. When on, Trim is driven so the wet loudness matches the dry, making Bypass comparisons fair. Double-clicking Trim resets it to 0.0 dB.

**Input trim** is a hidden, automatable parameter (not on the main GUI), saved with presets, for unusually quiet sources that cannot drive the detector. Same idea as soothe2's hidden input trim.

### 3.7 Frequency graph and bands

There are **six nodes**, even though soothe2's manual wording says “five bands: a low cut, a high cut and four general-purpose.” Public screenshots and reviews count six, and that is what we implement:

- **Low cut** (dark grey node) and **High cut** (pale node), each with selectable slope steepness (up to **48 dB/oct**, as described in published soothe2 coverage).
- Four general-purpose bands, coloured **yellow, orange, red, purple**, each switchable between **peaking** (default), **low shelf**, **high shelf**, **band shelf**, **band reject**, and **tilt**.

The graph shows:

- A **white** sensitivity curve.
- A **steel-blue** lower field with **cream** reduction “icicles” hanging down from 0 dB — the live reduction.
- Two scales on the right: a sensitivity scale (about +18 to −9) and a reduction scale (0 to −18). A display-range option (three steps) changes the reduction scale, as in soothe2's configuration menu.
- Frequency labels along the **top** of the graph: 63, 125, 250, 500, 1k, 2k, 4k, 8k, 16k. Processing is full-range, not limited to highs.
- A frequency readout that follows the mouse along the top of the graph, and locks to a band's centre frequency while dragging.
- Tooltips in the top-left of the graph when hovering a control.

Boosting a node (**Sens**) means *more reduction* in that area, not a boost in the output. This is the one thing every soothe2 tutorial spends time on, so the graph and the Sens label must make it obvious.

Band controls appear in a card at the bottom of the graph when a node is selected:

- Band on/off (filled node = on, hollow = off; double-click also toggles).
- **Listen** (headphones) — the delta of that band only. A global preference can pass dry audio instead, and another can auto-engage Listen while dragging a node.
- **Freq** — centre frequency, shown in soothe2's compact style as well (`6k3` = 6300 Hz), and typable as `6300`, `6k3`, or `6,3k`.
- **Sens** — band weight, with an estimated dB display that is a guide, not a guarantee, because Depth and Mix also change the result.
- **Q** — bandwidth.
- **Balance** — per-band stereo / mid-side emphasis.
- Curve-type icons.

Mouse behaviour, so a tutorial's modifiers still work:

- Drag a node to move frequency and Sens.
- **Shift** = fine adjust (nodes and knobs, drag or wheel).
- **Cmd (macOS) / Ctrl (Windows) + drag** = Q; wheel with that modifier = band Balance.
- **Alt-click or Alt-drag** = band Listen (can be released once Listen is on).
- Right-click / Ctrl-click = context menu (type, enable, etc.).
- Shift-click or drag-select for multi-node edits. Enable/disable in the menu applies to the selection.
- Wheel over knobs and nodes adjusts the pointed parameter.
- A square icon at the bottom-left of the graph hides or shows the band-control card.
- Diagonal resize handle at the bottom-right. The window is freely resizable and high-DPI aware.

### 3.8 Toolbar

Sits on the **top of the graph**, not as a bar over the whole plugin (soothe2's logo lives on the left panel; the preset/undo cluster lives on the graph).

- Preset previous / next, a menu button, and the current preset name (opens the preset menu).
- **Undo / Redo** (separate histories for A and B).
- **A/B** switch and **A→B / B→A** copy.
- Help and configuration (our own icons and menus — shortcuts list, about, tooltip toggle, auto band listen, use dry signal with band listen, reduction-graph display range). No “open the oeksound site”, no update checker, no copied tutorial content.

Preset menu: Factory Default, Flat Start (Depth −18, bands flattened/disabled so the curve is neutral), search, tags, user folder, save, set as default for new instances, load from file. Presets are original names and original curves. Factory tags should include vocals, de-ess, harsh, mud, bus, and so on.

**Learn** and the **Vocal** mode menu sit on this same toolbar, to the left of Undo, so no soothe2-mapped knob or graph control has to move. See section 5.

---

## 4. Soothe 2 tutorial mapping

Use this table when watching a soothe2 tutorial or reading the official manual. “Same” means the SILK control should do the same job, under the same name, in the same place, with the same default unless a note says otherwise.

| soothe2 control | SILK control | Same or different |
|---|---|---|
| Soft / Hard | Soft / Hard | Same. Default Soft. |
| Depth | Depth | Same. Default 3.0, range −18 to +18. The number is not dB. |
| Sharpness | Sharpness | Same. Default 4.8. |
| Selectivity | Selectivity | Same. Default 3.6. |
| Stereo Mode (left/right, mid/side) | Mode | Same. Factory default left \| right. |
| Link | Link | Same. Default 100%. |
| Balance (global + per band) | Balance | Same. Default even. |
| Attack | Attack | Same. Default 1.0. Value is a constant, not ms; faster at highs. |
| Release | Release | Same. Default shown as **fast**. |
| Quality: Oversample 1× / 2× / 4× | Oversample | Same. Default 1×. |
| Quality: Resolution Eco / Normal / High / Ultra | Resolution | Same four names, **plus Live** in the same menu. Default Normal. |
| Offline quality, “same as real-time”, “copy to all instances”, lock | Offline quality, copy, lock | Same idea, same place. |
| Sidechain on/off + listen | Sidechain on/off + listen | Same. |
| Mix | Mix | Same. Default 100%. Mix 0% is dry only. |
| Trim (wet only) | Trim | Same. Default 0.0 dB. SILK adds an optional **Auto** latch next to it, off by default. |
| Delta + right-click lock | Delta + right-click lock | Same. |
| In-plugin Bypass (keeps processing) | Bypass | Same. Host bypass is what saves CPU. |
| Hidden input trim | Hidden input trim | Same idea, automatable, saved with presets. |
| Graph: white sensitivity, blue field, reduction notches | Graph | Same layout logic and a similar palette; original drawing, not their artwork. |
| Low cut, 4 coloured bands, high cut | Same six nodes, same colours (yellow / orange / red / purple) | Same jobs and types (peaking, shelves, band shelf, band reject, tilt; cut slopes). |
| Band on, Listen, Freq, Sens, Q, Balance, types | Same names in the same card | Same. Sens up = more reduction, not a boost. |
| Freq typable as 6k3 | Same | Same. |
| Node modifiers (Shift, Cmd/Ctrl, Alt listen, double-click, right-click) | Same | Same, so tutorial gestures work on a Mac. |
| Hide band card, bottom-right resize | Same | Same. |
| Preset < > menu, Undo/Redo, A/B, A→B, help, config | Same cluster on the graph toolbar | Same placement. Help/config contents are ours. |
| Factory Default / Flat Start | Factory Default / Flat Start | Same roles. Our curves and names around them are original. |
| soothe2 wordmark + version on the left | **SILK** wordmark + version | **Different branding**, same corner. |
| — | **Vocal** modes + **Learn** | **SILK only**, in the graph toolbar, no moved knobs. |
| — | Output safety ceiling, Auto gain, latency readout | **SILK only**, see section 8. |
| Interactive in-plugin tutorials with demo audio; iLok; AAX; update checker | Skipped | See section 11. |

### Sources (public)

- Official soothe2 user manual: [https://oeksound.com/manuals/soothe2/](https://oeksound.com/manuals/soothe2/) — control names, Soft/Hard, Depth behaviour and “up to 60 dB” headroom, Sharpness, Selectivity, stereo Mode/Link/Balance, Attack/Release (frequency-dependent constants), Oversample, Resolution (Eco, High, Ultra), offline quality, sidechain, Mix, Trim (wet only), Delta, Bypass, graph (white curve, blue reduction field, inverse-EQ / sidechain-EQ behaviour), band types, Freq/Sens/Q/Balance, modifiers, toolbar, hidden input trim.
- Official soothe2 1.3.x GUI screenshot on that manual page, and the v1.1.2 “factory default preset” shot published in the Everything Recording review — layout, default numbers (Depth 3.0 / older factory 2.4, Sharpness 4.8 / 4.6, Selectivity 3.6, Attack 1.0, Release “fast”, Quality `1× normal`, Mix 100%, Trim 0.0 dB, left/right, Link 100%), node colours, and the light blue-grey palette.
- oeksound soothe2 “What's new?” introduction (Moe Volans): Soft/Hard, mid/side Link and Balance, Attack/Release examples, quality submenu and offline copy-to-all, sidechain examples, “boost a band = more processing”.
- Sound on Sound review of soothe2: latency “roughly halved, to around 45 ms”; quality and sidechain notes.
- MusicRadar review: Attack/Release moved onto the left-side panel; band controls under the graph; Balance and Mode.
- EDMProd “Soothe2 – How To Instantly Clean Up A Muddy Mix”: Depth/Sharpness/Selectivity walkthrough (including Depth values of 5.2 and 14.2), Mix/Trim/Delta/Bypass, Link, Mid-Side, quality and sidechain.
- Production Expert, “Oeksound Soothe2 And Spiff Demystified”: Depth, Sharpness, Selectivity, Attack/Release not in ms.
- Attack Magazine tutorial: Sens vs a normal EQ, Listen, Trim used at +1–2 dB, presets such as vocal and drum starting points.
- Elephorm preset lesson: Factory Default Depth at 3, Flat Start Depth at −18.
- Age of Audio: cut slopes up to 48 dB/oct; Resolution Eco through Ultra with 4× oversampling as the heavy end.
- VI-Control comparison notes: Oversample 1× / 2× / 4× and Resolution Eco / Normal / High / Ultra.
- Everything Recording review: six nodes (high pass, low pass, four parametric bands); Soft/Hard just above Depth.
- Rise Up Mastering notes on soothe2 1.3.3: factory Depth 3.0 raised to 6.0 in their test; Mix 100%.

soothe3 exists and is a different product. This plan follows **soothe2**.

---

## 5. Vocal-focused features (on top of the core)

These do not replace Depth, the nodes, or Delta. They are shortcuts for the same engine, aimed at a sung or spoken vocal in indie, alternative, ambient, Fred again..-style, and electronic folk records.

### 5.1 Vocal modes

A compact **Vocal** menu in the graph toolbar (left of Undo). Picking a mode sets the sensitivity curve — and only gentle starting points for Sharpness/Selectivity when the user is still on Factory Default — so the detector leans on a vocal problem. The user can keep editing every node and knob afterwards. Modes:

| Mode | Where it leans | What it is for |
|---|---|---|
| **Harsh** | 2–5 kHz | The sharp, shouty edge on a loud vowel. |
| **Sibilance** | 5–10 kHz | De-ess. Start with a lower Selectivity for noisy “sss”, higher for a whistle. |
| **Mud / Boom** | 150–400 Hz | Proximity, chest, or a boxy booth. |
| **Nasal** | 800 Hz–1.5 kHz | The honk or “megaphone” mid. |
| **Ring** | Mid resonances that stay put | Room notes, cheap mic rings, headphone spill tones. |
| **Breath / Air** | ~10 kHz and above | Tame harsh air or an over-bright breath, without dulling the whole top unless Depth is pushed. |

A mode is a starting curve, not a locked “vocal only” filter. Full-range processing stays available. A **Clear** item returns the nodes to the current preset's curve.

### 5.2 Learn (no ML)

A **Learn** button next to the Vocal menu. Press it, sing or play a few seconds of the vocal (about 2–4 s, with a visible countdown), press again to stop, or let it finish on its own.

It does **not** train a model and does not need a network or a paid GPU. It averages the prominence profile of that pass, finds the frequency regions where this singer *keeps* poking out (sibilance, a nasal centre, a room ring), and moves or boosts the sensitivity nodes toward those regions. Depth is left in the user's hands unless it is still at the factory 3.0 and the profile is clearly hotter than that — in which case a conservative bump is allowed and shown.

Learn can be cancelled. The result is just ordinary node values, saved with the session and with user presets, so it can be undone or edited. Think “this singer, this mic, this room,” not a general AI.

### 5.3 Vocal factory presets

Original names, same jobs people reach for in a vocal session. Examples (names can be tightened at build time, but must not copy soothe2 preset titles such as “Female vocals full range”):

- Factory Default, Flat Start
- Lead vocal — gentle
- Lead vocal — close mic
- De-ess — bright vocal
- De-ess — whistle
- Harsh vowels
- Mud and proximity
- Nasal mid
- Room ring
- Whisper / breath
- Doubles and stack
- Spoken / dry verse
- Vocal into a dense mix (stronger mid/side lean)
- Air only

A few non-vocal presets (drum overheads, acoustic guitar, mix bus gentle) can exist so the plugin is not helpless on other tracks, but the list should read as vocal-first.

---

## 6. Safety, latency, and loudness

Lessons from a previous plugin that clipped and felt late:

- Report the **real** latency with `setLatencySamples` whenever Quality, Oversample, or the block size changes. Show that number in milliseconds in the Quality area. If Quality changes latency, FL's plugin delay compensation must be told immediately.
- **Mix 0%** and **Bypass** (and Flat Start / Depth −18 with Mix 100%) are dry: the output matches the delayed input, sample-accurate, no extra timbre.
- Processors that take level away should not be “fixed” by a surprise boost. **Trim** and optional **Auto** are the user's level tools.
- A clip-safe **ceiling** at the very end, default **−0.1 dBFS**, so no sample leaves above that ceiling, including after overlap-add and any oversampling is brought back to the host rate. This is a peak controller, not a hard clip used as the only safety. The ceiling must not rewrite a legal dry path: Mix 0% and Bypass stay bit-exact (or denormal-safe with a documented tolerance) even if the incoming audio itself is already hot.
- Input and output peak meters, small and unobtrusive (near Output is fine). A hold light if the ceiling worked. Information, not panic.
- No spin-lock that can mute a block, no unreported look-ahead, no “quality” switch that changes delay in silence.

---

## 7. UI specification (single look)

There is one look, chosen so a soothe2 tutorial maps one-to-one: **the same placement of every soothe2-mapped button, knob, and display**, and a **similar cool blue-grey palette**. The knob caps, logo, fonts, icons, and all typography are drawn fresh. Do not trace, vector-copy, or pixel-match their artwork, and do not use their wordmark, the name “soothe”, or their font files.

### 7.1 Palette (similar, not sampled into a theme file as their IP)

A light, cool, frosted blue-grey plugin — not a dark skin.

- Panel background: light steel / ice grey-blue, about `#B0C0CA` to `#BECAD2`.
- Slightly darker wells behind the left knobs, about `#ABBAC4`.
- Graph header / preset toolbar: deeper slate, about `#7C8E9E`, with light text.
- Graph “sky” above 0 dB: pale grey-blue, about `#B0BCC2`.
- Graph floor: steel blue, about `#92ADC1`.
- Reduction: cream / sand icicles hanging into that blue.
- Sensitivity curve: white, 2 px-class, high contrast against the field.
- Depth / Sharpness / Selectivity knobs: dark charcoal caps with a light tick ring and a clear value under the knob. Draw our own caps.
- Band nodes: **yellow** ~`#F8E048`, **orange** ~`#E0A050`, **red** ~`#C86860`, **purple** ~`#9050A8`, low cut dark grey, high cut pale. Similar roles and hues so a tutorial that says “grab the yellow node” still works; do not copy node artwork.
- Selected Soft (or any selected segment): a clearly lighter pill than the unselected side. Text must stay readable — nudge contrast on labels if the light skin would otherwise fail a high-contrast reading check, without leaving this colour family.
- Fonts: a free, clean grotesque (for example Inter or IBM Plex Sans), not whatever soothe2 ships.

The window is resizable from the bottom-right handle and from the host, with a sensible minimum that keeps the left columns usable.

### 7.2 Layout sketch

Positions below follow official soothe2 1.3.x screenshots. Vocal / Learn are the only additions, and they live in the existing graph toolbar.

```
+------------------+--------+------------------------------------------------------------------+
| SILK     v1.0.0  | SPEED  |  <  >  ···  Factory Default     [Vocal ▾] [Learn]  undo redo    |
|                  | attack |                                  a b   a→b   ?   ⚙              |
|  [ soft | hard ] |  (o)   |  63   125   250   500   1k   2k   4k   8k   16k                 |
|                  |  1.0   |                                                                  |
|      depth       | release|          white sensitivity curve                                 |
|                  |  (o)   |     ●LC   ●Y          ●O        ●R              ●P        ●HC  |
|       ( O )      |  fast  |                                                                  |
|        3.0       | QUALITY|              cream reduction notches                             |
|                  | 1× nrm |  ============================================================    |
| sharpness   sel. |  lock  |  ============ steel-blue graph floor ========================    |
|   (o)       (o)  | SIDECH.|                                                                  |
|   4.8       3.6  |  ⏻  🎧 |                                                         +18     |
|                  | OUTPUT |                                                           0     |
| STEREO           | mix    |                                                         -18     |
| mode    balance  | 100%   |                                                                  |
| left|right  (o)  | trim   |  +-----------------------------------------------------------+  |
| link 100%  even  | 0.0 dB |  | ● band 3   freq    sens     q    balance    [type icons] |  |
|                  | ○ delta|  | 🎧 listen  4k7 Hz  1.4 dB  0.4     (o)                   |  |
|                  | ○ byp. |  +-----------------------------------------------------------+  |
|                  |        |  [■] hide band card                           latency  resize ↗ |
+------------------+--------+------------------------------------------------------------------+
```

Left column, top to bottom: wordmark and version; Soft/Hard; large Depth; Sharpness and Selectivity as a pair; Stereo (Mode, Balance, Link).

Narrow second column, top to bottom: Speed (Attack, Release); Quality; Sidechain; Output (Mix, Trim, Delta, Bypass).

Everything to the right is the graph, with the toolbar on its top edge and the band card over the lower graph when a node is selected.

Mirrors, spacing, and typography should feel like the same “map” as soothe2, but every asset is ours.

---

## 8. CPU and latency targets

All figures are targets for a stereo instance at 48 kHz, measured and written down in the build notes. They are not proven until the tests run.

| Quality (real-time) | Latency target (reported = measured) | CPU target (Apple Silicon, 128-sample buffer) |
|---|---|---|
| **Live**, 1× | As low as the shorter window allows, **ideally ≤ 8–12 ms**, never silent about it | Similar to Eco or better |
| **Eco**, 1× | Same family as Normal unless the window truly changes; report the truth | About **half** of Normal |
| **Normal**, 1× (default) | Typical STFT cost, likely on the order of **20–45 ms** depending on hop/window (soothe2 itself is cited at ~45 ms / around 2048 samples). Prefer the lowest delay that still sounds clean at Normal. | Comfortable: several vocal instances in one song, ballpark **under ~8% of one performance core** at default settings |
| **High / Ultra**, 2× or 4× | May be longer; report it | Can be heavy; Ultra + 4× may be “use for bounce” territory, which is what offline quality is for |

If Live cannot stay under ~12 ms without sounding brittle on low-frequency mud, say so in the build notes and still ship it as the low-latency option, with a plain-language warning in the Quality menu.

GUI drawing should stay off the audio thread. Idle after silence is allowed if it does not click on wake.

---

## 9. Test plan

Automated, every build, at 44.1 / 48 / 96 kHz unless noted. Textbook / analytical checks, not “it looks fine.”

1. **Tonal resonances are reduced by the expected amount.** A steady tone plus a louder narrow resonance (or a tone cluster with one protruding partial) at a known frequency: with a matching Sens boost and a fixed Depth/Sharpness/Selectivity, the protruding bin is reduced by a predicted dB within a tight tolerance, while a neighbouring non-prominent bin stays close to unity. Repeat in Harsh, Sibilance, Mud, Nasal, Ring, and Breath ranges.
2. **Flat noise is left alone.** Pink or white noise with no planted resonances, Factory Default or a flat sensitivity, moderate Depth: the long-term spectrum stays within a small tolerance of the input (no wholesale high-frequency dulling, no cartoon notches).
3. **Nulls.** Mix 0% nulls exactly against the delayed dry input. In-plugin Bypass nulls the same way. Depth at **−18** (Flat Start) with Mix 100% produces no reduction (null within a tiny denormal-safe tolerance). (Note: Depth **0** on this scale is not “off”; 0 is a mid-low setting and may still process. Do not write a test that expects Depth 0 to null.)
4. **Reported latency equals measured latency** at every Quality and Oversample, including changes mid-stream, at 44.1/48/96 kHz. The UI ms readout matches the same number.
5. **No output over the ceiling.** Sweeps, resonances at full Depth, Hard + high Sharpness, Trim up, inter-sample peaks after overlap-add: true peak of the wet output ≤ −0.1 dBFS. Tests must fail on the true peak, not pass because a clipper hid it.
6. **CPU budget.** Measure and fail the build (or at least flag it) if Normal 1× overshoots the documented budget by a wide margin, or if Eco is not materially cheaper than Normal. Record Ultra+4× as information.
7. **pluginval at strictness 10** on the macOS universal VST3 (and AU if it is built).
8. Also: Soft vs Hard actually differ at the same Depth; Selectivity high vs low changes how many bins are reduced on a multi-tone test; Sharpness high vs low changes notch width; Attack slower lets a transient through; Sidechain reduction follows the key, not the main, on a constructed pair; Mid/Side and Left/Right + Link 0/100 and Balance extremes route correctly; Delta is (dry − wet) within tolerance; Auto trim, when on, brings wet loudness near dry; Learn on a planted two-resonance vocal-like signal moves Sens toward those frequencies; every automatable parameter has a test; no NaNs or denormal storms; state save/reload; A/B and undo.

What only a real session can confirm: how it feels on a real voice in FL, whether sibilance stays natural, and whether Live is usable while recording.

---

## 10. Honest limitations versus soothe2

- We do not have oeksound's unpublished detector. SILK will aim at the same *job* and the same *hands* (controls, defaults, layout). It may need different Depth or Sharpness on the same take, and at extreme settings it may show more pumping, more grain, or weaker low-frequency separation than soothe2, especially in Live.
- soothe2 has had years of factory presets, artist presets, and interactive tutorials with demo audio. v1 of SILK will be smaller and vocal-first.
- soothe2's “about 45 ms” latency is a known, constant-feeling figure. Our Normal should be in a similar, honest ballpark; Live is a trade for coarser bass resolution. Neither is magic zero-latency spectral surgery.
- Mid/side and sidechain quality also depend on FL actually compensating delay on those paths.
- Nothing is proven until it is tried on a real vocal in FL.

---

## 11. What we skip, and why

- **Copied branding, assets, fonts, icons, knob art, or a pixel-identical skin.** The layout and palette are a map so tutorials work. The drawing is ours. Legal and the right thing.
- **The names soothe, soothe2, oeksound, and their preset titles.** The product is SILK.
- **soothe3-only features.** The request is soothe2 workflow.
- **Machine learning, “AI Assist”, and paid GPU training.** Learn is a short measurement of the take. Paid compute is out of scope, and the good separation-style models are also a licence problem in other projects.
- **In-plugin video tutorials and demo audio copied from oeksound.** A short built-in help that uses our words is enough.
- **iLok, AAX, Windows-first, update phone-home.** This is a Mac VST3 (AU optional) for FL, signed ad-hoc like the other plugins, installed only when asked.
- **A second, unrelated “dark purple” or beige skin in v1.** One layout, so the tutorial map stays true.
- **A built-in de-esser that is a separate detector fighting the resonance engine.** Sibilance is a Vocal mode and a preset, not a second plugin inside the plugin.
- **DOCTOR-style 24-band manual EQ, or a Soothe-style tamer bolted into DOCTOR.** This stays its own plugin.

---

## 12. Build notes (for when it is built)

- Format: VST3 primary for FL Studio on Mac; **AU optional** (include it if the other recent plugins did). Stereo effect, not a synth. No MIDI required.
- Stack: JUCE + CMake, C++20, original code. Universal macOS (x86_64 + arm64) via GitHub Actions (`macos-15-intel` or the current equivalent that still produces universals). pluginval 10 in CI. Ad-hoc codesign.
- Repo: **koobacorp/silk**, **private** unless a public repo is needed for free Actions minutes. If it must go public, scrub the owner's name from history first, and only after an explicit OK for *this* repo. Approved public repos so far do not include silk.
- Do not install on the Mac until asked (“install for me”). Zip also belongs on the Desktop REP PLUG folder at install time.
- Suggested plugin code / manufacturer names should be original (e.g. manufacturer Kooba, product SILK) and must not include the owner's name.

### Name

The product name is **SILK**. Other products already use that word (among them Sonicbits Silk, a synth; Spectraforge Silk, a vocal-adjacent air enhancer; UJAM Virtual Guitarist SILK; EastWest SILK). That is worth knowing, but the name is already chosen. In FL's plugin list, “SILK” plus our manufacturer should still be easy to find.

---

## 13. What “done” means for v1

Engine tests above pass, plugin tests and pluginval 10 pass, every control has a test, latency and CPU are measured and written down, Mix 0% and Depth −18 null, the ceiling holds, the UI matches this layout and palette without overlapping text, and Vocal modes plus Learn behave as specified.

What only you can confirm: whether it actually tames your vocals in FL, and whether a soothe2 tutorial is enough to drive it.
