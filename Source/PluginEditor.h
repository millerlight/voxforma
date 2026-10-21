#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/FormantVisualizer.h"
#include "ui/LfoSection.h"
#include "ui/VoxLookAndFeel.h"

class VowelStrip : public juce::Component,
                   private juce::Timer
{
public:
    VowelStrip (VoxformaAudioProcessor& p) : processor (p) { startTimerHz (30); }
    void paint (juce::Graphics& g) override;
    void resized() override {}

private:
    void timerCallback() override { repaint(); }
    VoxformaAudioProcessor& processor;
};

class CustomVowelsComponent : public juce::Component
{
public:
    explicit CustomVowelsComponent (juce::AudioProcessorValueTreeState& state);
    ~CustomVowelsComponent() override = default;

    void resized() override;
    void paint (juce::Graphics& g) override;

    std::function<void()> onApply;

private:
    struct SetData
    {
        juce::String names, f1, f2, f3;
    };

    juce::String propertyOrFallback (const char* prop, const juce::String& fallbackList) const;
    void apply();
    void closeDialog();
    void loadForSet (int setIndex);
    void switchSet (int setIndex);
    void captureCurrent();
    void writeToState();

    juce::AudioProcessorValueTreeState& apvts;
    int currentSet = 0;
    std::array<SetData, 4> pending;
    juce::ComboBox setBox;
    juce::Array<juce::TextEditor*> edits;
    juce::Array<juce::Label*> headers;
    juce::TextButton applyButton { "Apply" }, cancelButton { "Cancel" };
};

class VoxformaAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit VoxformaAudioProcessorEditor (VoxformaAudioProcessor&);
    ~VoxformaAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    struct Section
    {
        juce::String title;
        juce::Rectangle<int> bounds;
    };

    void drawSection (juce::Graphics& g, const Section& s);
    void openCustomVowels();

    VoxformaAudioProcessor& processor;
    voxui::VoxLookAndFeel lookAndFeel;

    voxui::FormantVisualizer visualizer;
    VowelStrip vowelStrip;

    juce::Slider vowelAmountSlider, emphasisSlider, driftAmountSlider, driftRangeSlider,
        f1GainSlider, f2GainSlider, f3GainSlider, smoothSlider, gainSlider;
    juce::ComboBox polarityBox;
    juce::ToggleButton bypassButton { "BYPASS" };
    juce::TextButton editVowelsButton { "EDIT" };
    juce::ToggleButton f1Power { "1" }, f2Power { "2" }, f3Power { "3" };
    juce::Label vowelSetLabel, driftInfoLabel, emphasisInfoLabel;
    juce::Label f1Info, f2Info, f3Info;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vowelAmountAtt, emphasisAtt,
        driftAmountAtt, driftRangeAtt, f1GainAtt, f2GainAtt, f3GainAtt, smoothAtt, gainAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> polarityAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt,
        f1PowerAtt, f2PowerAtt, f3PowerAtt;

    voxui::LfoSection lfoVowel, lfoEmphasis, lfoDrift;

    std::unique_ptr<juce::DialogWindow> customDialog;

    Section secVowels { "VOWELS", {} }, secEmphasis { "EMPHASIS", {} },
        secDrift { "DRIFT", {} }, secFormants { "FORMANTS", {} },
        secMaster { "MASTER", {} };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxformaAudioProcessorEditor)
};
