#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include "Lfo.h"
#include "VowelBank.h"

namespace vox
{

class BiquadBandpass
{
public:
    struct Coeffs
    {
        float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    };

    void reset() { z1 = z2 = 0.0f; }

    Coeffs currentCoeffs() const { return coeffs; }

    void setCoefficients (float freqHz, float q, double sampleRate)
    {
        const double w0 = juce::MathConstants<double>::twoPi * juce::jlimit (10.0f, 18000.0f, freqHz) / sampleRate;
        const double cw = std::cos (w0);
        const double sw = std::sin (w0);
        const double alpha = sw / (2.0 * juce::jmax (0.1f, q));

        const double b0 = alpha;
        const double b1 = 0.0;
        const double b2 = -alpha;
        const double a0 = 1.0 + alpha;
        const double a1 = -2.0 * cw;
        const double a2 = 1.0 - alpha;

        coeffs.b0 = static_cast<float> (b0 / a0);
        coeffs.b1 = static_cast<float> (b1 / a0);
        coeffs.b2 = static_cast<float> (b2 / a0);
        coeffs.a1 = static_cast<float> (a1 / a0);
        coeffs.a2 = static_cast<float> (a2 / a0);
        b0f = coeffs.b0;
        b1f = coeffs.b1;
        b2f = coeffs.b2;
        a1f = coeffs.a1;
        a2f = coeffs.a2;
    }

    float process (float x) noexcept
    {
        const float y = b0f * x + z1;
        z1 = b1f * x - a1f * y + z2;
        z2 = b2f * x - a2f * y;
        return y;
    }

private:
    Coeffs coeffs;
    float b0f = 0.0f, b1f = 0.0f, b2f = 0.0f, a1f = 0.0f, a2f = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;
};

class FormantEngine
{
public:
    static constexpr int numFormants = 3;
    static constexpr int updateInterval = 8;

    void prepare (double sampleRate, int maxBlockSize, int numChannels)
    {
        sr = sampleRate;
        channels = juce::jmax (1, numChannels);

        for (auto& ch : filters)
            for (auto& f : ch)
                f.reset();

        vowelPos.reset (sampleRate, 0.02);
        vowelPos.setCurrentAndTargetValue (0.0f);
        emphasisSm.reset (sampleRate, 0.02);
        emphasisSm.setCurrentAndTargetValue (0.4f);
        driftSm.reset (sampleRate, 0.02);
        driftSm.setCurrentAndTargetValue (0.0f);
        for (auto& g : formantGainSm)
            g.reset (sampleRate, 0.02);
        for (auto& g : formantGainSm)
            g.setCurrentAndTargetValue (1.0f);
        outGainSm.reset (sampleRate, 0.02);
        outGainSm.setCurrentAndTargetValue (1.0f);
        wetSm.reset (sampleRate, 0.02);
        wetSm.setCurrentAndTargetValue (1.0f);
        bypassSm.reset (sampleRate, 0.02);
        bypassSm.setCurrentAndTargetValue (1.0f);

        for (auto& l : lfos)
            l.prepare (sampleRate);

        counter = 0;
        setSmoothingTime (0.08f);
        updateCoefficients();
    }

    void setSmoothingTime (float seconds)
    {
        const auto t = juce::jmax (0.001f, seconds);
        vowelPos.reset (sr, t);
        emphasisSm.reset (sr, t);
        driftSm.reset (sr, t);
        for (auto& g : formantGainSm)
            g.reset (sr, t);
    }

    void setTargets (float vowelPos01, float emphasis01, float driftRangeHz, float driftAmount01,
                     int driftPolarityIndex, float fGainDb[numFormants], float outGainDb, float wetMix01)
    {
        vowelPos.setTargetValue (vowelPos01);
        emphasisSm.setTargetValue (emphasis01);
        driftSm.setTargetValue (driftRangeHz * driftAmount01);
        driftRangeForLfo = driftRangeHz;
        polarity = driftPolarityIndex;
        for (int i = 0; i < numFormants; ++i)
            formantGainSm[i].setTargetValue (juce::Decibels::decibelsToGain (fGainDb[i]));
        outGainSm.setTargetValue (outGainDb <= muteThresholdDb ? 0.0f
                                                                : juce::Decibels::decibelsToGain (outGainDb));
        wetSm.setTargetValue (juce::jlimit (0.0f, 1.0f, wetMix01));
    }

    void setBypass (bool shouldBypass)
    {
        bypassSm.setTargetValue (shouldBypass ? 0.0f : 1.0f);
    }

    void reset()
    {
        for (auto& ch : filters)
            for (auto& f : ch)
                f.reset();
        vowelPos.setCurrentAndTargetValue (vowelPos.getCurrentValue());
        counter = 0;
    }

    void setFormantPower (int index, bool on)
    {
        if (juce::isPositiveAndBelow (index, numFormants))
            powers[index] = on ? 1.0f : 0.0f;
    }

    void setLfo (int slot, float rate, float amount, float phaseDeg, int waveIdx, bool sync, bool power)
    {
        if (! juce::isPositiveAndBelow (slot, 3))
            return;
        lfoTargets[slot] = { rate, amount, phaseDeg, waveIdx, sync, power };
        lfos[slot].setPhaseOffset (phaseDeg / 360.0f);
    }

    void setTempo (double bpm) { tempoBpm = bpm; }

    void process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer&)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh      = juce::jmin (buffer.getNumChannels(), channels);

        for (int i = 0; i < numSamples; ++i)
        {
            for (int slot = 0; slot < 3; ++slot)
            {
                const auto& t = lfoTargets[slot];
                const auto wave = static_cast<Lfo::Wave> (juce::jlimit (0, 5, t.waveIdx));
                const int divIdx = divisionIndexFor (t.rate);
                lastLfoOut[slot] = t.power ? lfos[slot].process (t.rate, wave, tempoBpm, t.sync, divIdx) * t.amount
                                           : 0.0f;
            }

            if (counter == 0)
                updateCoefficients();
            ++counter;
            if (counter >= updateInterval)
                counter = 0;

            const float wet = wetSm.getNextValue();
            const float gain = outGainSm.getNextValue();
            const float bypass = bypassSm.getNextValue();

            for (int ch = 0; ch < numCh; ++ch)
            {
                const float in = buffer.getReadPointer (ch)[i];

                float sum = 0.0f;
                for (int f = 0; f < numFormants; ++f)
                    sum += filters[ch][f].process (in) * currentFormantGain[f] * powers[f];

                const float clipped = std::tanh (sum * 1.2f) * 0.8333f;
                const float outWet = clipped * wetMakeup;

                float out = in * (1.0f - wet) + outWet * wet;
                out *= gain;
                out = softLimit (out);
                out = in * (1.0f - bypass) + out * bypass;

                buffer.getWritePointer (ch)[i] = out;
            }
        }
    }

    float formantFrequency (int index) const
    {
        return juce::isPositiveAndBelow (index, numFormants) ? lastFreqs[index] : 0.0f;
    }

    float lastLfoValue (int slot) const
    {
        return juce::isPositiveAndBelow (slot, 3) ? lastLfoOut[slot] : 0.0f;
    }

    float lfoPhase01 (int slot) const
    {
        return juce::isPositiveAndBelow (slot, 3) ? lfos[slot].currentPhase01() : 0.0f;
    }

    struct ResponseTap
    {
        BiquadBandpass::Coeffs c;
        float gain = 1.0f;
    };

    std::array<ResponseTap, numFormants> responseTaps() const
    {
        std::array<ResponseTap, numFormants> out {};
        for (int f = 0; f < numFormants; ++f)
        {
            out[static_cast<size_t> (f)].c = filters[0][f].currentCoeffs();
            out[static_cast<size_t> (f)].gain = currentFormantGain[f] * powers[f];
        }
        return out;
    }

    float lastQForView() const { return lastQ; }

private:
    struct LfoTarget
    {
        float rate = 1.0f;
        float amount = 0.0f;
        float phaseDeg = 0.0f;
        int waveIdx = 0;
        bool sync = false;
        bool power = false;
    };

    static int divisionIndexFor (float rate)
    {
        const auto range = juce::NormalisableRange<float> { 0.01f, 2000.0f, 0.0f, 0.25f };
        const float norm = range.convertTo0to1 (rate);
        return juce::jlimit (0, 8, static_cast<int> (std::round (norm * 8.0f)));
    }

    static float softLimit (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.8f)
            return x;
        return std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) * 2.5f), x);
    }

    void updateCoefficients()
    {
        const float vowelLfo = lastLfoOut[0];
        const float emphLfo  = lastLfoOut[1];
        const float driftLfo = lastLfoOut[2];

        const float pos = vowelPos.skip (updateInterval) + vowelLfo;
        const float emph = juce::jlimit (0.0f, 1.0f, emphasisSm.skip (updateInterval) + emphLfo);
        const float drift = driftSm.skip (updateInterval) + driftLfo * driftRangeForLfo;

        morph.compute (activeSet, pos);

        const float q = 0.707f * std::pow (2.0f, emph * 4.0f);
        lastQ = q;

        static constexpr float polSigns[3][3] = {
            { 1.0f, 1.0f, 1.0f },
            { -1.0f, -1.0f, -1.0f },
            { 1.0f, -1.0f, 1.0f }
        };
        const auto& pol = polSigns[juce::jlimit (0, 2, polarity)];

        for (int f = 0; f < numFormants; ++f)
        {
            lastFreqs[f] = morph.f[f] + drift * pol[f];
            for (int ch = 0; ch < channels; ++ch)
                filters[ch][f].setCoefficients (lastFreqs[f], q, sr);
        }

        for (int f = 0; f < numFormants; ++f)
            currentFormantGain[f] = formantGainSm[f].skip (updateInterval);
        lastPos = pos;
    }

public:
    void setActiveVowelSet (const VowelSet& set) { activeSet = set; }
    void setDriftRangeForLfo (float rangeHz) { driftRangeForLfo = rangeHz; }

    const VowelSet& activeVowelSet() const { return activeSet; }

private:
    double sr = 44100.0;
    int channels = 2;
    int counter = 0;

    BiquadBandpass filters[2][numFormants];
    float powers[numFormants] { 1.0f, 1.0f, 1.0f };
    float currentFormantGain[numFormants] { 1.0f, 1.0f, 1.0f };
    float lastFreqs[numFormants] {};
    float lastPos = 0.0f;
    float lastQ = 0.707f;
    static constexpr float muteThresholdDb = -50.0f;
    static constexpr float wetMakeup = 3.2f;
    int polarity = 2;
    float driftRangeForLfo = 500.0f;

    juce::SmoothedValue<float> vowelPos, emphasisSm, driftSm, outGainSm, wetSm, bypassSm;
    juce::SmoothedValue<float> formantGainSm[numFormants];

    Lfo lfos[3];
    LfoTarget lfoTargets[3];
    float lastLfoOut[3] {};
    double tempoBpm = 120.0;

    VowelSet activeSet = VowelSet::male();
    VowelMorph morph;
};

}
