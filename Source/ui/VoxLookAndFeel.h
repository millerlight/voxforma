#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace voxui
{

struct Palette
{
    static juce::Colour bg()        { return juce::Colour (0xff0e1014); }
    static juce::Colour panel()     { return juce::Colour (0xff161a21); }
    static juce::Colour panelHi()   { return juce::Colour (0xff1b212c); }
    static juce::Colour border()    { return juce::Colour (0xff262d3a); }
    static juce::Colour text()      { return juce::Colour (0xffe8ecf2); }
    static juce::Colour textDim()   { return juce::Colour (0xff8b94a5); }
    static juce::Colour cyan()      { return juce::Colour (0xff3ee0c8); }
    static juce::Colour magenta()   { return juce::Colour (0xffff5aa8); }
    static juce::Colour amber()     { return juce::Colour (0xffffb454); }
    static juce::Colour violet()    { return juce::Colour (0xff9d7bff); }
    static juce::Colour danger()    { return juce::Colour (0xffff5470); }
};

class VoxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VoxLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, Palette::bg());
        setColour (juce::Label::textColourId, Palette::text());
        setColour (juce::Slider::textBoxTextColourId, Palette::text());
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::trackColourId, Palette::cyan());
        setColour (juce::Slider::rotarySliderFillColourId, Palette::cyan());
        setColour (juce::Slider::thumbColourId, Palette::text());
        setColour (juce::ComboBox::backgroundColourId, Palette::panelHi());
        setColour (juce::ComboBox::textColourId, Palette::text());
        setColour (juce::ComboBox::outlineColourId, Palette::border());
        setColour (juce::ComboBox::arrowColourId, Palette::textDim());
        setColour (juce::PopupMenu::backgroundColourId, Palette::panelHi());
        setColour (juce::PopupMenu::textColourId, Palette::text());
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::cyan().withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, Palette::text());
        setColour (juce::TextButton::buttonColourId, Palette::panelHi());
        setColour (juce::TextButton::buttonOnColourId, Palette::cyan().withAlpha (0.35f));
        setColour (juce::TextButton::textColourOffId, Palette::textDim());
        setColour (juce::TextButton::textColourOnId, Palette::text());
        setColour (juce::ToggleButton::textColourId, Palette::text());
        setColour (juce::ToggleButton::tickColourId, Palette::cyan());
        setColour (juce::TextEditor::backgroundColourId, Palette::panelHi());
        setColour (juce::TextEditor::textColourId, Palette::text());
        setColour (juce::TextEditor::outlineColourId, Palette::border());
        setColour (juce::CaretComponent::caretColourId, Palette::cyan());
        setColour (juce::ScrollBar::thumbColourId, Palette::border());
        setColour (juce::BubbleComponent::backgroundColourId, Palette::panelHi());
        setColour (juce::BubbleComponent::outlineColourId, Palette::border());
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions (juce::jmin (13.0f, buttonHeight * 0.44f)));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return juce::Font (juce::FontOptions (13.0f));
    }

    juce::Font getLabelFont (juce::Label& label) override
    {
        return juce::Font (juce::FontOptions (juce::jmin (13.0f, label.getHeight() * 0.55f)));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                    static_cast<float> (width), static_cast<float> (height))
                                .reduced (2.0f);
        const float size   = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto  centre = bounds.getCentre();
        const float radius = size * 0.5f;
        const float angle  = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        const float arcThick = juce::jmax (2.0f, radius * 0.14f);

        g.setColour (juce::Colour (0xff1d232e));
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));

        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius - arcThick * 0.5f, radius - arcThick * 0.5f,
                           0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour (0xff2a3140));
        g.strokePath (arc, juce::PathStrokeType (arcThick, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        juce::Path fill;
        fill.addCentredArc (centre.x, centre.y, radius - arcThick * 0.5f, radius - arcThick * 0.5f,
                            0.0f, rotaryStartAngle, angle, true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (fill, juce::PathStrokeType (arcThick, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        const float markerLen = radius * 0.62f;
        const float markerW   = juce::jmax (2.0f, radius * 0.10f);
        juce::Path pointer;
        pointer.addRoundedRectangle (-markerW * 0.5f, -markerLen, markerW, markerLen * 0.9f, markerW * 0.5f);
        g.setColour (slider.findColour (juce::Slider::thumbColourId));
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = button.getToggleState();

        auto bg = Palette::panelHi();
        if (on)            bg = Palette::cyan().withAlpha (0.30f);
        else if (shouldDrawButtonAsDown) bg = Palette::border();
        else if (shouldDrawButtonAsHighlighted) bg = Palette::border().brighter (0.15f);

        g.setColour (bg);
        g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);
        g.setColour (on ? Palette::cyan().brighter (0.2f) : Palette::border());
        g.drawRoundedRectangle (bounds, bounds.getHeight() * 0.5f, 1.0f);

        auto textBounds = bounds.toNearestIntEdges();
        g.setColour (on ? Palette::text() : Palette::textDim());
        g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
        g.drawFittedText (button.getButtonText(), textBounds, juce::Justification::centred, 1);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 24, box.getHeight() - 2);
        label.setJustificationType (juce::Justification::centredLeft);
    }
};

}
