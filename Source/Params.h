#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace vox
{
namespace ParamIDs
{
    inline constexpr auto vowelAmount    = "vowelAmount";
    inline constexpr auto emphasis       = "emphasis";
    inline constexpr auto driftAmount    = "driftAmount";
    inline constexpr auto driftRange     = "driftRange";
    inline constexpr auto driftPolarity  = "driftPolarity";

    inline constexpr auto f1Gain = "f1Gain";
    inline constexpr auto f2Gain = "f2Gain";
    inline constexpr auto f3Gain = "f3Gain";
    inline constexpr auto f1Power = "f1Power";
    inline constexpr auto f2Power = "f2Power";
    inline constexpr auto f3Power = "f3Power";

    inline constexpr auto vowelLfoRate     = "vowelLfoRate";
    inline constexpr auto vowelLfoAmount   = "vowelLfoAmount";
    inline constexpr auto vowelLfoPhase    = "vowelLfoPhase";
    inline constexpr auto vowelLfoWave     = "vowelLfoWave";
    inline constexpr auto vowelLfoSync     = "vowelLfoSync";
    inline constexpr auto vowelLfoPower    = "vowelLfoPower";

    inline constexpr auto emphasisLfoRate   = "emphasisLfoRate";
    inline constexpr auto emphasisLfoAmount = "emphasisLfoAmount";
    inline constexpr auto emphasisLfoPhase  = "emphasisLfoPhase";
    inline constexpr auto emphasisLfoWave   = "emphasisLfoWave";
    inline constexpr auto emphasisLfoSync   = "emphasisLfoSync";
    inline constexpr auto emphasisLfoPower  = "emphasisLfoPower";

    inline constexpr auto driftLfoRate   = "driftLfoRate";
    inline constexpr auto driftLfoAmount = "driftLfoAmount";
    inline constexpr auto driftLfoPhase  = "driftLfoPhase";
    inline constexpr auto driftLfoWave   = "driftLfoWave";
    inline constexpr auto driftLfoSync   = "driftLfoSync";
    inline constexpr auto driftLfoPower  = "driftLfoPower";

    inline constexpr auto gain   = "gain";
    inline constexpr auto smooth = "smooth";
    inline constexpr auto bypass = "bypass";
}

inline juce::StringArray waveChoices()
{
    return { "Sine", "Triangle", "Ramp Up", "Ramp Down", "Square", "Random" };
}

inline juce::StringArray polarityChoices()
{
    return { "Positive", "Negative", "Bipolar" };
}

inline juce::StringArray divisionChoices()
{
    return { "1/64", "1/32", "1/16", "1/8", "1/4", "1/2", "1/1", "2/1", "4/1" };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace ParamIDs;
    using FloatParam  = juce::AudioParameterFloat;
    using BoolParam   = juce::AudioParameterBool;
    using ChoiceParam = juce::AudioParameterChoice;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = juce::NormalisableRange<float> { 0.0f, 1.0f, 0.0f, 1.0f };
    auto lfoRateRange = juce::NormalisableRange<float> { 0.01f, 2000.0f, 0.0f, 0.25f };

    layout.add (std::make_unique<FloatParam> (juce::ParameterID { vowelAmount, 1 }, "Vowel Amount", pct, 0.0f));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { emphasis, 1 }, "Emphasis", pct, 0.4f));

    layout.add (std::make_unique<FloatParam> (juce::ParameterID { driftAmount, 1 }, "Drift Amount", pct, 0.0f));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { driftRange, 1 }, "Drift Range",
                                              juce::NormalisableRange<float> { 10.0f, 2000.0f, 0.0f, 0.4f }, 500.0f, juce::AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.add (std::make_unique<ChoiceParam> (juce::ParameterID { driftPolarity, 1 }, "Drift Polarity", polarityChoices(), 2));

    layout.add (std::make_unique<FloatParam> (juce::ParameterID { f1Gain, 1 }, "Formant 1 Gain",
                                              juce::NormalisableRange<float> { -24.0f, 12.0f, 0.0f, 0.5f }, 0.0f, juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { f2Gain, 1 }, "Formant 2 Gain",
                                              juce::NormalisableRange<float> { -24.0f, 12.0f, 0.0f, 0.5f }, 0.0f, juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { f3Gain, 1 }, "Formant 3 Gain",
                                              juce::NormalisableRange<float> { -24.0f, 12.0f, 0.0f, 0.5f }, 0.0f, juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<BoolParam> (juce::ParameterID { f1Power, 1 }, "Formant 1 Power", true));
    layout.add (std::make_unique<BoolParam> (juce::ParameterID { f2Power, 1 }, "Formant 2 Power", true));
    layout.add (std::make_unique<BoolParam> (juce::ParameterID { f3Power, 1 }, "Formant 3 Power", true));

    for (auto [rate, amount, phase, wave, sync, power, base]
             : { std::tuple { vowelLfoRate, vowelLfoAmount, vowelLfoPhase, vowelLfoWave, vowelLfoSync, vowelLfoPower, "Vowel" },
                 std::tuple { emphasisLfoRate, emphasisLfoAmount, emphasisLfoPhase, emphasisLfoWave, emphasisLfoSync, emphasisLfoPower, "Emphasis" },
                 std::tuple { driftLfoRate, driftLfoAmount, driftLfoPhase, driftLfoWave, driftLfoSync, driftLfoPower, "Drift" } })
    {
        layout.add (std::make_unique<FloatParam> (juce::ParameterID { rate, 1 }, juce::String (base) + " LFO Rate", lfoRateRange, 1.0f, juce::AudioParameterFloatAttributes().withLabel ("Hz")));
        layout.add (std::make_unique<FloatParam> (juce::ParameterID { amount, 1 }, juce::String (base) + " LFO Amount", pct, 0.0f));
        layout.add (std::make_unique<FloatParam> (juce::ParameterID { phase, 1 }, juce::String (base) + " LFO Phase",
                                                  juce::NormalisableRange<float> { 0.0f, 360.0f, 0.0f, 1.0f }, 0.0f, juce::AudioParameterFloatAttributes().withLabel ("deg")));
        layout.add (std::make_unique<ChoiceParam> (juce::ParameterID { wave, 1 }, juce::String (base) + " LFO Wave", waveChoices(), 0));
        layout.add (std::make_unique<BoolParam> (juce::ParameterID { sync, 1 }, juce::String (base) + " LFO Sync", false));
        layout.add (std::make_unique<BoolParam> (juce::ParameterID { power, 1 }, juce::String (base) + " LFO Power", false));
    }

    layout.add (std::make_unique<FloatParam> (juce::ParameterID { gain, 1 }, "Gain",
                                              juce::NormalisableRange<float> { -60.0f, 6.0f, 0.0f, 0.6f }, 0.0f, juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { smooth, 1 }, "Dry / Wet", pct, 1.0f,
                                              juce::AudioParameterFloatAttributes()
                                                  .withLabel ("%")
                                                  .withStringFromValueFunction ([] (float v, int)
                                                                          { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; })
                                                  .withValueFromStringFunction ([] (const juce::String& s)
                                                                            { return juce::jlimit (0.0f, 100.0f, s.getFloatValue()) * 0.01f; })));
    layout.add (std::make_unique<BoolParam> (juce::ParameterID { bypass, 1 }, "Bypass", false));

    return layout;
}
}
