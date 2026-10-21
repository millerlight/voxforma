#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

namespace vox
{

class Lfo
{
public:
    enum Wave { sine = 0, triangle, rampUp, rampDown, square, random };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        phase = 0.0;
        rnd = juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
        rndTarget = nextRandom();
    }

    void reset (double startPhase01 = 0.0)
    {
        phase = startPhase01 - std::floor (startPhase01);
        rnd = nextRandom();
        rndTarget = nextRandom();
    }

    void setPhaseOffset (float offset01)
    {
        phaseOffset = offset01 - std::floor (offset01);
    }

    float process (float freqHz, Wave wave, double tempoBpm, bool sync, int divisionIndex)
    {
        double f = freqHz;

        if (sync && tempoBpm > 0.0)
        {
            static constexpr double divisors[] = { 0.0625, 0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };
            const int idx = juce::jlimit (0, 8, divisionIndex);
            const double beatsPerCycle = divisors[idx] * 4.0;
            f = static_cast<double> (tempoBpm) / 60.0 * (1.0 / beatsPerCycle);
        }

        const double delta = f / sr;
        const double prev  = phase;
        phase += delta;
        const bool wrapped = phase >= 1.0;
        if (wrapped)
            phase -= std::floor (phase);

        if (wave == random && wrapped)
        {
            rnd = rndTarget;
            rndTarget = nextRandom();
        }

        const auto wrapd = [] (double x) { return x - std::floor (x); };
        return valueAt (wave, wrapd (prev + phaseOffset), wrapd (phase + phaseOffset), delta);
    }

    float currentPhase01() const { return static_cast<float> (phase); }

private:
    float nextRandom() const
    {
        return juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f;
    }

    float valueAt (Wave wave, double p0, double p1, double delta) const
    {
        const auto lerpRnd = [this, p0, p1, delta]
        {
            juce::ignoreUnused (p0, p1, delta);
            return rnd;
        };

        switch (wave)
        {
            case sine:
                return static_cast<float> (std::sin (p0 * juce::MathConstants<double>::twoPi));
            case triangle:
            {
                const double t = p0 * 2.0;
                return static_cast<float> (t < 1.0 ? (t * 2.0 - 1.0) : (3.0 - t * 2.0));
            }
            case rampUp:
                return static_cast<float> (p0 * 2.0 - 1.0);
            case rampDown:
                return static_cast<float> (1.0 - p0 * 2.0);
            case square:
                return p0 < 0.5 ? 1.0f : -1.0f;
            case random:
                return lerpRnd();
            default:
                return 0.0f;
        }
    }

    double sr = 44100.0;
    double phase = 0.0;
    double phaseOffset = 0.0;
    float rnd = 0.0f;
    float rndTarget = 0.0f;
};

inline juce::NormalisableRange<float> makeLfoRateRange()
{
    return {
        0.03125f,
        8.0f,
        [] (float start, float end, float proportion)
        {
            return end + (start - end) * juce::jlimit (0.0f, 1.0f, proportion);
        },
        [] (float start, float end, float value)
        {
            return juce::jlimit (0.0f, 1.0f, (end - value) / (end - start));
        },
        [] (float start, float end, float value)
        {
            return juce::jlimit (start, end, value);
        }
    };
}

}
