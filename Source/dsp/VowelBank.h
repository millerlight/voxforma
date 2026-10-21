#pragma once

#include <array>
#include <cmath>

namespace vox
{

struct VowelSet
{
    static constexpr int numVowels = 10;
    static constexpr int numFormants = 3;

    std::array<float, numVowels> f1 {};
    std::array<float, numVowels> f2 {};
    std::array<float, numVowels> f3 {};
    std::array<const char*, numVowels> names {};

    static const VowelSet& male()
    {
        static const VowelSet s {
            { 700.0f, 530.0f, 300.0f, 400.0f, 350.0f, 375.0f, 620.0f, 610.0f, 500.0f, 400.0f },
            { 1200.0f, 1500.0f, 1950.0f, 720.0f, 640.0f, 1810.0f, 1490.0f, 1000.0f, 1200.0f, 1150.0f },
            { 2600.0f, 2500.0f, 2750.0f, 2500.0f, 2550.0f, 2500.0f, 2250.0f, 2600.0f, 2675.0f, 2500.0f },
            { "ah", "e", "ee", "u", "o", "i", "ae", "aw", "a", "er" }
        };
        return s;
    }

    static const VowelSet& female()
    {
        static const VowelSet s {
            { 850.0f, 600.0f, 400.0f, 480.0f, 400.0f, 430.0f, 760.0f, 730.0f, 610.0f, 490.0f },
            { 1220.0f, 1900.0f, 2300.0f, 850.0f, 750.0f, 2000.0f, 1800.0f, 1200.0f, 1400.0f, 1350.0f },
            { 2800.0f, 2700.0f, 3000.0f, 2800.0f, 2800.0f, 2800.0f, 2500.0f, 2800.0f, 2900.0f, 2800.0f },
            { "ah", "e", "ee", "u", "o", "i", "ae", "aw", "a", "er" }
        };
        return s;
    }

    static const VowelSet& child()
    {
        static const VowelSet s {
            { 1000.0f, 700.0f, 500.0f, 560.0f, 470.0f, 500.0f, 880.0f, 850.0f, 710.0f, 570.0f },
            { 1400.0f, 2200.0f, 2600.0f, 1000.0f, 880.0f, 2300.0f, 2100.0f, 1400.0f, 1630.0f, 1570.0f },
            { 3200.0f, 3100.0f, 3400.0f, 3200.0f, 3200.0f, 3200.0f, 2900.0f, 3200.0f, 3300.0f, 3200.0f },
            { "ah", "e", "ee", "u", "o", "i", "ae", "aw", "a", "er" }
        };
        return s;
    }

    static const VowelSet& setForIndex (int index)
    {
        switch (index)
        {
            case 1:  return female();
            case 2:  return child();
            default: return male();
        }
    }
};

struct VowelMorph
{
    float f[VowelSet::numFormants] {};

    void compute (const VowelSet& set, float position01)
    {
        const float wrapped = position01 - std::floor (position01);
        const float scaled  = wrapped * VowelSet::numVowels;
        const int   i0      = static_cast<int> (scaled) % VowelSet::numVowels;
        const int   i1      = (i0 + 1) % VowelSet::numVowels;
        const float frac    = scaled - std::floor (scaled);

        const float* src[VowelSet::numFormants] = { set.f1.data(), set.f2.data(), set.f3.data() };

        for (int formant = 0; formant < VowelSet::numFormants; ++formant)
        {
            const auto a = src[formant][i0];
            const auto b = src[formant][i1];
            f[formant] = a + (b - a) * frac;
        }
    }
};

}
