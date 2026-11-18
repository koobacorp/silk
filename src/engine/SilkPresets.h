#pragma once
#include "SilkEngine.h"
#include <string>
#include <vector>

namespace silk {

struct Preset {
    const char* name;
    const char* tag;
    Params p;
};

inline Params withMode(VocalMode m, float depth = 3.0f) {
    Params p; factoryFill(p); applyVocalMode(p, m); p.depth = depth; return p;
}

inline const std::vector<Preset>& factoryPresets() {
    static std::vector<Preset> list;
    if (!list.empty()) return list;
    Params def; factoryFill(def);
    Params flat; flat.setFlatStart();
    list.push_back({ "Factory Default", "init", def });
    list.push_back({ "Flat Start", "init", flat });
    list.push_back({ "Lead vocal — gentle", "vocals", withMode(VocalMode::Off, 3.0f) });
    {
        Params p; factoryFill(p); p.depth = 4.5f; p.sharpness = 4.2f;
        p.band[0].freq = 90; p.band[3].sens = 2.0f; p.band[4].sens = 2.5f;
        list.push_back({ "Lead vocal — close mic", "vocals", p });
    }
    list.push_back({ "De-ess — bright vocal", "vocals de-ess", withMode(VocalMode::Sibilance, 4.0f) });
    {
        Params p = withMode(VocalMode::Sibilance, 5.0f); p.selectivity = 6.5f; p.sharpness = 7.0f;
        list.push_back({ "De-ess — whistle", "vocals de-ess", p });
    }
    list.push_back({ "Harsh vowels", "vocals harsh", withMode(VocalMode::Harsh, 5.0f) });
    list.push_back({ "Mud and proximity", "vocals mud", withMode(VocalMode::MudBoom, 4.5f) });
    list.push_back({ "Nasal mid", "vocals", withMode(VocalMode::Nasal, 4.5f) });
    {
        Params p = withMode(VocalMode::Ring, 5.0f); p.sharpness = 7.5f; p.selectivity = 6.0f;
        list.push_back({ "Room ring", "vocals ring", p });
    }
    list.push_back({ "Whisper / breath", "vocals air", withMode(VocalMode::BreathAir, 4.0f) });
    {
        Params p; factoryFill(p); p.depth = 3.5f; p.stereoMode = StereoMode::MidSide; p.balance = 0.35f;
        list.push_back({ "Doubles and stack", "vocals", p });
    }
    {
        Params p; factoryFill(p); p.depth = 3.0f; p.sharpness = 3.5f;
        list.push_back({ "Spoken / dry verse", "vocals spoken", p });
    }
    {
        Params p; factoryFill(p); p.depth = 5.0f; p.stereoMode = StereoMode::MidSide; p.balance = -0.25f; p.link = 80;
        list.push_back({ "Vocal into a dense mix", "vocals mix", p });
    }
    {
        Params p = withMode(VocalMode::BreathAir, 3.5f); p.band[1].sens = -6; p.band[2].sens = -4;
        list.push_back({ "Air only", "vocals air", p });
    }
    {
        Params p; factoryFill(p); p.depth = 4.0f; p.sharpness = 6.5f; p.attack = 3.5f;
        p.band[0].freq = 200; p.band[4].freq = 9000; p.band[4].sens = 4;
        list.push_back({ "Drum overheads", "drums", p });
    }
    {
        Params p; factoryFill(p); p.depth = 3.5f; p.band[1].freq = 200; p.band[1].sens = 4; p.band[3].sens = 3;
        list.push_back({ "Acoustic guitar", "inst", p });
    }
    {
        Params p; factoryFill(p); p.depth = 2.0f; p.sharpness = 3.0f; p.selectivity = 4.5f;
        list.push_back({ "Mix bus — gentle", "bus", p });
    }
    return list;
}

} // namespace silk
