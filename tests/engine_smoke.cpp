#include "../Source/dsp/FormantEngine.h"
#include <cstdio>

namespace
{
juce::AudioBuffer<float> makeSineBuffer (int numChannels, int numSamples, int blockIndex)
{
    juce::AudioBuffer<float> buf (numChannels, numSamples);
    const double phase0 = static_cast<double> (blockIndex * numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
            buf.setSample (ch, i, 0.5f * std::sin (static_cast<float> (phase0 + i) * 0.0471f));
    return buf;
}

juce::AudioBuffer<float> makeToneBuffer (int numChannels, int numSamples, int blockIndex)
{
    juce::AudioBuffer<float> buf (numChannels, numSamples);
    const double phase0 = static_cast<double> (blockIndex * numSamples);
    for (int ch = 0; ch < numChannels; ++ch)
        for (int i = 0; i < numSamples; ++i)
        {
            const float t = static_cast<float> (phase0 + i);
            const float v = 0.16f * std::sin (t * 0.0262f)   // 200 Hz
                          + 0.16f * std::sin (t * 0.1440f)   // 1100 Hz
                          + 0.12f * std::sin (t * 0.3400f);  // 2600 Hz
            buf.setSample (ch, i, v);
        }
    return buf;
}

struct MasterReading
{
    float rms = 0.0f;
    float peak = 0.0f;
};

MasterReading runMaster (vox::FormantEngine& engine, float gainDb, float wetMix01, int blocks = 30)
{
    engine.setBypass (false);

    float formGain[3] = { 0.0f, -3.0f, -6.0f };
    engine.setTargets (0.5f, 0.6f, 800.0f, 0.5f, 1, formGain, gainDb, wetMix01);

    juce::MidiBuffer midi;

    for (int block = 0; block < 20; ++block)
    {
        auto warm = makeToneBuffer (2, 512, 500 + block);
        engine.process (warm, midi);
    }

    double sumSq = 0.0;
    float peak = 0.0f;
    long count = 0;

    for (int block = 0; block < blocks; ++block)
    {
        auto buf = makeToneBuffer (2, 512, 100 + block);
        engine.process (buf, midi);
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                const float v = buf.getSample (ch, i);
                sumSq += static_cast<double> (v) * v;
                peak = juce::jmax (peak, std::abs (v));
                ++count;
            }
    }

    MasterReading r;
    r.rms = static_cast<float> (std::sqrt (sumSq / static_cast<double> (std::max (1L, count))));
    r.peak = peak;
    return r;
}
}

int main()
{
    juce::MidiBuffer midi;
    vox::FormantEngine engine;
    engine.prepare (48000.0, 512, 2);

    engine.setLfo (0, 0.5f, 0.35f, 0.0f, 0, false, true);
    engine.setLfo (1, 1.2f, 0.6f, 90.0f, 1, false, true);
    engine.setLfo (2, 0.07f, 1.0f, 180.0f, 4, false, true);
    engine.setFormantPower (0, true);
    engine.setFormantPower (1, true);
    engine.setFormantPower (2, true);

    bool finite = true;
    bool nonSilent = false;
    bool vowelTravel = true;
    float maxAbs = 0.0f;
    float lastEnergy = -1.0f;
    bool energyMoved = false;

    for (int block = 0; block < 400; ++block)
    {
        const float vowelPos = static_cast<float> (block % 100) / 100.0f;
        float gains[3] = { 0.0f, -3.0f, -6.0f };
        engine.setTargets (vowelPos, 0.7f, 900.0f, 0.6f, 1, gains, 0.0f, 0.4f);

        auto buf = makeSineBuffer (2, 512, block);
        engine.process (buf, midi);

        float energy = 0.0f;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const float* data = buf.getReadPointer (ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
            {
                const float v = data[i];
                if (! std::isfinite (v))
                    finite = false;
                maxAbs = juce::jmax (maxAbs, std::abs (v));
                energy += v * v;
                if (std::abs (v) > 1.0e-7f)
                    nonSilent = true;
            }
        }
        if (lastEnergy >= 0.0f && std::abs (energy - lastEnergy) > 1.0e-6f)
            energyMoved = true;
        lastEnergy = energy;
    }

    if (std::abs (engine.formantFrequency (0) - 700.0f) > 400.0f)
        vowelTravel = false;

    juce::AudioBuffer<float> probe (2, 512);
    auto dry = makeSineBuffer (2, 512, 999);
    for (int ch = 0; ch < 2; ++ch)
        probe.copyFrom (ch, 0, dry, ch, 0, 512);
    engine.setBypass (true);
    for (int block = 0; block < 200; ++block)
    {
        engine.process (probe, midi);
        if (block == 199)
        {
            dry = makeSineBuffer (2, 512, 999);
            for (int ch = 0; ch < 2; ++ch)
                probe.copyFrom (ch, 0, dry, ch, 0, 512);
        }
    }
    engine.process (probe, midi);
    float bypassDiff = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 512; ++i)
            bypassDiff = juce::jmax (bypassDiff, std::abs (probe.getSample (ch, i) - dry.getSample (ch, i)));

    engine.setBypass (false);
    const float f1 = engine.formantFrequency (0);
    const float q = engine.lastQForView();

    bool masterOk = true;

    const auto dryRead  = runMaster (engine, 0.0f, 0.0f);
    const auto wetRead  = runMaster (engine, 0.0f, 1.0f);
    const auto midRead  = runMaster (engine, 0.0f, 0.5f);
    const auto muteRead = runMaster (engine, -60.0f, 1.0f);
    const auto loudRead = runMaster (engine, 0.0f, 1.0f);
    const auto lowRead  = runMaster (engine, -12.0f, 1.0f);

    if (dryRead.rms < 0.05f)
    {
        std::printf ("dry level too low: rms=%.4f\n", dryRead.rms);
        masterOk = false;
    }

    if (wetRead.rms < 0.01f)
    {
        std::printf ("wet level too low: rms=%.4f\n", wetRead.rms);
        masterOk = false;
    }

    if (wetRead.rms > dryRead.rms * 8.0f || wetRead.rms < dryRead.rms / 8.0f)
    {
        std::printf ("dry/wet level ratio out of band: dry=%.4f wet=%.4f\n", dryRead.rms, wetRead.rms);
        masterOk = false;
    }

    if (!(midRead.rms > dryRead.rms * 0.4f && midRead.rms < dryRead.rms * 1.6f))
    {
        std::printf ("50/50 mix out of range: rms=%.4f (dry=%.4f)\n", midRead.rms, dryRead.rms);
        masterOk = false;
    }

    if (muteRead.rms > 1.0e-6f || muteRead.peak > 1.0e-6f)
    {
        std::printf ("gain -60dB not silent: rms=%.6f peak=%.6f\n", muteRead.rms, muteRead.peak);
        masterOk = false;
    }

    if (!(loudRead.rms > lowRead.rms * 3.0f))
    {
        std::printf ("gain not monotonic: -12dB rms=%.4f 0dB rms=%.4f\n", lowRead.rms, loudRead.rms);
        masterOk = false;
    }

    if (loudRead.peak > 1.0f)
    {
        std::printf ("limiter exceeded 1.0: peak=%.4f\n", loudRead.peak);
        masterOk = false;
    }

    const bool bypassOk = bypassDiff < 1.0e-5f;
    std::printf ("finite=%d nonSilent=%d energyMoved=%d vowelTravel=%d maxAbs=%.4f bypassDiff=%.2e F1=%.1fHz Q=%.2f\n",
                 (int) finite, (int) nonSilent, (int) energyMoved, (int) vowelTravel,
                 maxAbs, bypassDiff, f1, q);
    std::printf ("master dry=%.4f wet=%.4f mid=%.4f mute=%.6f low=%.4f loud=%.4f ok=%d\n",
                 dryRead.rms, wetRead.rms, midRead.rms, muteRead.rms, lowRead.rms, loudRead.rms,
                 (int) masterOk);

    const bool pass = finite && nonSilent && energyMoved && vowelTravel
                      && maxAbs < 1.5f && bypassOk && masterOk
                      && f1 > 100.0f && f1 < 3000.0f;
    std::printf (pass ? "SMOKE PASS\n" : "SMOKE FAIL\n");
    return pass ? 0 : 1;
}
