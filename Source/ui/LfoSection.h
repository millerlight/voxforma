#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "../Params.h"
#include "VoxLookAndFeel.h"

namespace voxui
{

class LfoMiniScope : public juce::Component,
                     private juce::Timer
{
public:
    LfoMiniScope (juce::Colour accentColour, std::function<int()> waveFn,
                  std::function<bool()> powerFn, std::function<float()> phaseFn)
        : accent (accentColour), getWave (std::move (waveFn)),
          isOn (std::move (powerFn)), getPhase (std::move (phaseFn))
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff10131a));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Palette::border());
        g.drawRoundedRectangle (r, 6.0f, 1.0f);

        const auto wave = static_cast<int> (juce::jlimit (0, 5, getWave()));
        const bool on = isOn();
        const float phase = juce::jlimit (0.0f, 1.0f, getPhase());

        juce::Path path;
        const int steps = juce::jmax (8, getWidth() - 8);
        for (int i = 0; i <= steps; ++i)
        {
            const float t = static_cast<float> (i) / static_cast<float> (steps);
            const float v = on ? waveValue (wave, t) : 0.0f;
            const float x = 4.0f + t * (r.getWidth() - 8.0f);
            const float y = r.getCentreY() - v * (r.getHeight() * 0.34f);
            if (i == 0) path.startNewSubPath (x, y);
            else        path.lineTo (x, y);
        }

        g.setColour (on ? accent.withAlpha (0.85f) : Palette::textDim().withAlpha (0.4f));
        g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        const float v = on ? waveValue (wave, phase) : 0.0f;
        const float px = 4.0f + phase * (r.getWidth() - 8.0f);
        const float py = r.getCentreY() - v * (r.getHeight() * 0.34f);
        g.setColour (on ? Palette::text() : Palette::textDim());
        g.fillEllipse (px - 3.5f, py - 3.5f, 7.0f, 7.0f);
    }

    static float waveValue (int wave, float t)
    {
        switch (wave)
        {
            case 1:  return t < 0.5f ? (t * 4.0f - 1.0f) : (3.0f - t * 4.0f);
            case 2:  return t * 2.0f - 1.0f;
            case 3:  return 1.0f - t * 2.0f;
            case 4:  return t < 0.5f ? 1.0f : -1.0f;
            default: return std::sin (t * juce::MathConstants<float>::twoPi);
        }
    }

private:
    void timerCallback() override { repaint(); }

    juce::Colour accent;
    std::function<int()> getWave;
    std::function<bool()> isOn;
    std::function<float()> getPhase;
};

class LfoSection : public juce::Component
{
public:
    LfoSection (juce::AudioProcessorValueTreeState& state, vox::FormantEngine& engine,
                int slotIn, const juce::String& title, juce::Colour accentColour)
        : sectionTitle (title),
          accent (accentColour),
          scope (accent,
                 [this] { return juce::jlimit (0, 5, waveBox.getSelectedId() - 1); },
                 [this] { return powerButton.getToggleState(); },
                 [this] { return engineRef.lfoPhase01 (slot); }),
          engineRef (engine),
          slot (slotIn),
          rateSlider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
          amountSlider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
          phaseSlider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow)
    {
        static constexpr const char* ids[3][6] = {
            { vox::ParamIDs::vowelLfoRate,    vox::ParamIDs::vowelLfoAmount,    vox::ParamIDs::vowelLfoPhase,    vox::ParamIDs::vowelLfoWave,    vox::ParamIDs::vowelLfoSync,    vox::ParamIDs::vowelLfoPower },
            { vox::ParamIDs::emphasisLfoRate, vox::ParamIDs::emphasisLfoAmount, vox::ParamIDs::emphasisLfoPhase, vox::ParamIDs::emphasisLfoWave, vox::ParamIDs::emphasisLfoSync, vox::ParamIDs::emphasisLfoPower },
            { vox::ParamIDs::driftLfoRate,    vox::ParamIDs::driftLfoAmount,    vox::ParamIDs::driftLfoPhase,    vox::ParamIDs::driftLfoWave,    vox::ParamIDs::driftLfoSync,    vox::ParamIDs::driftLfoPower }
        };

        const auto* p = ids[juce::jlimit (0, 2, slot)];

        auto setupKnob = [this, accentColour] (juce::Slider& s, const juce::String& name)
        {
            s.setName (name);
            s.setTextValueSuffix (" " + name);
            s.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
            s.setPopupDisplayEnabled (true, true, this);
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 68, 17);
        };

        setupKnob (rateSlider, "Rate");
        setupKnob (amountSlider, "Amount");
        setupKnob (phaseSlider, "Phase");

        addAndMakeVisible (rateSlider);
        addAndMakeVisible (amountSlider);
        addAndMakeVisible (phaseSlider);
        addAndMakeVisible (scope);

        waveBox.addItemList (vox::waveChoices(), 1);
        waveBox.setTextWhenNothingSelected ("Wave");
        addAndMakeVisible (waveBox);

        addAndMakeVisible (syncButton);
        syncButton.setClickingTogglesState (true);
        addAndMakeVisible (powerButton);
        powerButton.setClickingTogglesState (true);

        rateAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, p[0], rateSlider);
        amountAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, p[1], amountSlider);
        phaseAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, p[2], phaseSlider);
        waveAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, p[3], waveBox);
        syncAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, p[4], syncButton);
        powerAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, p[5], powerButton);

        powerButton.onStateChange = [this]
        {
            powerButton.setButtonText (powerButton.getToggleState() ? "ON" : "OFF");
        };
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (Palette::panel());
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (Palette::border());
        g.drawRoundedRectangle (r, 12.0f, 1.0f);

        g.setColour (accent);
        g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
        g.drawText (sectionTitle.toUpperCase(), getLocalBounds().removeFromTop (26).reduced (12, 6),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        auto top = r.removeFromTop (20);
        powerButton.setBounds (top.removeFromRight (50));
        r.removeFromTop (2);

        scope.setBounds (r.removeFromTop (50));
        r.removeFromTop (6);

        auto knobs = r.removeFromTop (88);
        const int kw = knobs.getWidth() / 3;
        rateSlider.setBounds (knobs.getX(), knobs.getY(), kw - 4, knobs.getHeight());
        amountSlider.setBounds (knobs.getX() + kw, knobs.getY(), kw - 4, knobs.getHeight());
        phaseSlider.setBounds (knobs.getX() + 2 * kw, knobs.getY(), kw - 4, knobs.getHeight());
        r.removeFromTop (6);

        auto bottom = r.removeFromTop (26);
        syncButton.setBounds (bottom.removeFromRight (58));
        bottom.removeFromRight (6);
        waveBox.setBounds (bottom);
    }

private:
    juce::String sectionTitle;
    juce::Colour accent;
    LfoMiniScope scope;
    vox::FormantEngine& engineRef;
    int slot = 0;
    juce::Slider rateSlider, amountSlider, phaseSlider;
    juce::ComboBox waveBox;
    juce::ToggleButton syncButton { "SYNC" }, powerButton { "OFF" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAtt, amountAtt, phaseAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAtt, powerAtt;
};

}
