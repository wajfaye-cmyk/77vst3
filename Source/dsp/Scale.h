#pragma once

#include <cmath>
#include <cstdint>

namespace s77
{

enum class ScaleType
{
    Chromatic = 0, Major, NaturalMinor, HarmonicMinor, MelodicMinor,
    MajorPentatonic, MinorPentatonic, Blues, Dorian, Phrygian, Lydian, Mixolydian, Custom
};

inline const char* const* scaleNames()
{
    static const char* names[] = {
        "Chromatic", "Major", "Minor", "Harmonic Minor", "Melodic Minor",
        "Major Pentatonic", "Minor Pentatonic", "Blues", "Dorian", "Phrygian",
        "Lydian", "Mixolydian", "Custom", nullptr
    };
    return names;
}

inline const char* const* keyNames()
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", nullptr };
    return names;
}

/** 12-bit mask relative to C (bit 0 = C) for a scale rooted on C. */
inline uint32_t scaleIntervals (ScaleType t)
{
    auto bits = [] (std::initializer_list<int> iv) { uint32_t m = 0; for (int i : iv) m |= 1u << i; return m; };
    switch (t)
    {
        case ScaleType::Chromatic:       return 0xFFFu;
        case ScaleType::Major:           return bits ({ 0, 2, 4, 5, 7, 9, 11 });
        case ScaleType::NaturalMinor:    return bits ({ 0, 2, 3, 5, 7, 8, 10 });
        case ScaleType::HarmonicMinor:   return bits ({ 0, 2, 3, 5, 7, 8, 11 });
        case ScaleType::MelodicMinor:    return bits ({ 0, 2, 3, 5, 7, 9, 11 });
        case ScaleType::MajorPentatonic: return bits ({ 0, 2, 4, 7, 9 });
        case ScaleType::MinorPentatonic: return bits ({ 0, 3, 5, 7, 10 });
        case ScaleType::Blues:           return bits ({ 0, 3, 5, 6, 7, 10 });
        case ScaleType::Dorian:          return bits ({ 0, 2, 3, 5, 7, 9, 10 });
        case ScaleType::Phrygian:        return bits ({ 0, 1, 3, 5, 7, 8, 10 });
        case ScaleType::Lydian:          return bits ({ 0, 2, 4, 6, 7, 9, 11 });
        case ScaleType::Mixolydian:      return bits ({ 0, 2, 4, 5, 7, 9, 10 });
        case ScaleType::Custom:          return 0xFFFu;
    }
    return 0xFFFu;
}

/** Absolute pitch-class mask (bit 0 = C) for a key + scale. */
inline uint32_t scaleMask (int key, ScaleType t)
{
    const uint32_t iv = scaleIntervals (t);
    uint32_t m = 0;
    for (int i = 0; i < 12; ++i)
        if (iv & (1u << i))
            m |= 1u << ((i + key) % 12);
    return m;
}

inline int pitchClass (int midi) { return ((midi % 12) + 12) % 12; }

/** Nearest MIDI note whose pitch class is allowed by mask. */
inline int nearestAllowed (float midi, uint32_t mask)
{
    const int base = (int) std::lround (midi);
    if ((mask & 0xFFFu) == 0)
        return base;

    int best = base;
    float bestDist = 1.0e9f;
    for (int k = base - 6; k <= base + 6; ++k)
    {
        if (! (mask & (1u << pitchClass (k))))
            continue;
        const float d = std::abs ((float) k - midi);
        if (d < bestDist) { bestDist = d; best = k; }
    }
    return best;
}

inline float hzToMidi (float hz) { return 69.0f + 12.0f * std::log2 (hz / 440.0f); }

} // namespace s77
