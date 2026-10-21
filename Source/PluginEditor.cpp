#include "PluginEditor.h"

namespace
{
juce::String joinFloatList (const std::array<float, 10>& values)
{
    juce::StringArray parts;
    for (auto v : values)
        parts.add (juce::String (static_cast<int> (std::round (v))));
    return parts.joinIntoString ("|");
}

juce::String setLabelForIndex (int index)
{
    switch (index)
    {
        case 1:  return "Female set";
        case 2:  return "Child set";
        case 3:  return "Custom set";
        default: return "Male set";
    }
}

juce::String keyForSet (const char* base, int setIndex)
{
    juce::StringArray parts;
    parts.add (base);
    parts.add (juce::String (setIndex));
    return parts.joinIntoString ("_");
}
}

void VowelStrip::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f, 1.0f);
    const auto& set = processor.engine().activeVowelSet();

    const auto rawAmount = processor.apvts.getRawParameterValue (vox::ParamIDs::vowelAmount)->load();
    const float pos = rawAmount + processor.engine().lastLfoValue (0);
    const float wrapped = pos - std::floor (pos);

    const float lineY = r.getHeight() * 0.36f;
    g.setColour (voxui::Palette::border());
    g.drawLine (r.getX(), lineY, r.getRight(), lineY, 1.5f);

    g.setFont (juce::Font (juce::FontOptions (10.5f)));
    for (int i = 0; i < vox::VowelSet::numVowels; ++i)
    {
        const float x = r.getX() + (static_cast<float> (i) / vox::VowelSet::numVowels) * r.getWidth();
        g.setColour (voxui::Palette::textDim());
        g.drawVerticalLine (juce::roundToInt (x), lineY - 5.0f, lineY + 5.0f);

        const auto name = juce::String (set.names[static_cast<size_t> (i)]).toUpperCase();
        auto labelArea = juce::Rectangle<float> (x - 22.0f, lineY + 7.0f, 44.0f, 16.0f);
        g.setColour (voxui::Palette::textDim());
        g.drawText (name, labelArea.toNearestIntEdges(), juce::Justification::centred);
    }

    const float cursorX = r.getX() + wrapped * r.getWidth();
    g.setColour (voxui::Palette::magenta().withAlpha (0.35f));
    g.drawLine (cursorX, 2.0f, cursorX, lineY + 2.0f, 1.0f);
    g.setColour (voxui::Palette::magenta());
    g.fillEllipse (cursorX - 5.0f, lineY - 5.0f, 10.0f, 10.0f);
}

CustomVowelsComponent::CustomVowelsComponent (juce::AudioProcessorValueTreeState& state) : apvts (state)
{
    currentSet = static_cast<int> (apvts.state.getProperty ("vowelSet", 0));
    if (currentSet < 0) currentSet = 0;
    if (currentSet > 3) currentSet = 3;

    setBox.addItemList ({ "Male", "Female", "Child", "Custom" }, 1);
    setBox.setSelectedId (currentSet + 1, juce::dontSendNotification);
    setBox.onChange = [this] { switchSet (setBox.getSelectedId() - 1); };
    addAndMakeVisible (setBox);

    const char* headerNames[] = { "NAME", "F1 (Hz)", "F2 (Hz)", "F3 (Hz)" };
    for (auto* h : headerNames)
    {
        auto* label = new juce::Label ({}, h);
        label->setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        label->setColour (juce::Label::textColourId, voxui::Palette::textDim());
        label->setJustificationType (juce::Justification::centred);
        headers.add (label);
        addAndMakeVisible (*label);
    }

    for (int row = 0; row < vox::VowelSet::numVowels; ++row)
    {
        for (int col = 0; col < 4; ++col)
        {
            auto* ed = new juce::TextEditor();
            ed->setJustification (col == 0 ? juce::Justification::centredLeft : juce::Justification::centred);
            ed->setInputRestrictions (col == 0 ? 8 : 6, col == 0 ? juce::String() : "0123456789");
            edits.add (ed);
            addAndMakeVisible (*ed);
        }
    }

    loadForSet (currentSet);

    applyButton.onClick = [this] { apply(); };
    cancelButton.onClick = [this] { closeDialog(); };
    addAndMakeVisible (applyButton);
    addAndMakeVisible (cancelButton);
}

juce::String CustomVowelsComponent::propertyOrFallback (const char* prop, const juce::String& fallbackList) const
{
    const auto v = apvts.state.getProperty (prop, juce::var()).toString();
    return v.isEmpty() ? fallbackList : v;
}

void CustomVowelsComponent::captureCurrent()
{
    if (edits.size() != (size_t) (vox::VowelSet::numVowels * 4))
        return;

    juce::StringArray names, f1, f2, f3;
    for (int row = 0; row < vox::VowelSet::numVowels; ++row)
    {
        auto get = [this, row] (int col) { return edits[row * 4 + col]->getText().trim(); };
        auto sanitize = [] (juce::String s) { return s.replaceCharacter ('|', '/'); };
        auto numeric = [] (const juce::String& s)
        {
            const auto v = s.getFloatValue();
            return juce::String (static_cast<int> (juce::jlimit (0.0f, 12000.0f, v)));
        };
        names.add (sanitize (get (0)));
        f1.add (numeric (get (1)));
        f2.add (numeric (get (2)));
        f3.add (numeric (get (3)));
    }

    auto& p = pending[static_cast<size_t> (currentSet)];
    p.names = names.joinIntoString ("|");
    p.f1 = f1.joinIntoString ("|");
    p.f2 = f2.joinIntoString ("|");
    p.f3 = f3.joinIntoString ("|");
}

void CustomVowelsComponent::writeToState()
{
    for (int i = 0; i < 4; ++i)
    {
        const auto& p = pending[static_cast<size_t> (i)];
        if (p.names.isEmpty())
            continue;
        apvts.state.setProperty (keyForSet ("vowelNames", i).toRawUTF8(), p.names, nullptr);
        apvts.state.setProperty (keyForSet ("vowelF1", i).toRawUTF8(), p.f1, nullptr);
        apvts.state.setProperty (keyForSet ("vowelF2", i).toRawUTF8(), p.f2, nullptr);
        apvts.state.setProperty (keyForSet ("vowelF3", i).toRawUTF8(), p.f3, nullptr);
    }
    apvts.state.setProperty ("vowelSet", currentSet, nullptr);
}

void CustomVowelsComponent::loadForSet (int setIndex)
{
    const auto& base = vox::VowelSet::setForIndex (setIndex == 3 ? 0 : setIndex);
    const auto fallbackNames = [&base]
    {
        juce::StringArray parts;
        for (int i = 0; i < vox::VowelSet::numVowels; ++i)
            parts.add (base.names[static_cast<size_t> (i)]);
        return parts.joinIntoString ("|");
    }();

    auto& p = pending[static_cast<size_t> (setIndex)];
    if (p.names.isEmpty())
    {
        p.names = propertyOrFallback (keyForSet ("vowelNames", setIndex).toRawUTF8(), fallbackNames);
        p.f1 = propertyOrFallback (keyForSet ("vowelF1", setIndex).toRawUTF8(), joinFloatList (base.f1));
        p.f2 = propertyOrFallback (keyForSet ("vowelF2", setIndex).toRawUTF8(), joinFloatList (base.f2));
        p.f3 = propertyOrFallback (keyForSet ("vowelF3", setIndex).toRawUTF8(), joinFloatList (base.f3));
    }

    auto nameParts = juce::StringArray::fromTokens (p.names, "|", {});
    auto f1Parts = juce::StringArray::fromTokens (p.f1, "|", {});
    auto f2Parts = juce::StringArray::fromTokens (p.f2, "|", {});
    auto f3Parts = juce::StringArray::fromTokens (p.f3, "|", {});

    if (edits.size() != (size_t) (vox::VowelSet::numVowels * 4))
        return;

    for (int row = 0; row < vox::VowelSet::numVowels; ++row)
    {
        const juce::String vals[4] = {
            row < nameParts.size() ? nameParts[row] : base.names[static_cast<size_t> (row)],
            row < f1Parts.size() ? f1Parts[row] : juce::String (static_cast<int> (base.f1[static_cast<size_t> (row)])),
            row < f2Parts.size() ? f2Parts[row] : juce::String (static_cast<int> (base.f2[static_cast<size_t> (row)])),
            row < f3Parts.size() ? f3Parts[row] : juce::String (static_cast<int> (base.f3[static_cast<size_t> (row)]))
        };
        for (int col = 0; col < 4; ++col)
            edits[row * 4 + col]->setText (vals[col], juce::dontSendNotification);
    }
    resized();
}

void CustomVowelsComponent::switchSet (int setIndex)
{
    if (setIndex < 0) setIndex = 0;
    if (setIndex > 3) setIndex = 3;
    captureCurrent();
    currentSet = setIndex;
    setBox.setSelectedId (currentSet + 1, juce::dontSendNotification);
    loadForSet (currentSet);
}

void CustomVowelsComponent::closeDialog()
{
    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
    {
        dw->exitModalState (0);
        dw->setVisible (false);
    }
    else if (auto* parent = getParentComponent())
    {
        parent->setVisible (false);
    }
}

void CustomVowelsComponent::apply()
{
    captureCurrent();
    writeToState();

    if (onApply)
        onApply();

    closeDialog();
}

void CustomVowelsComponent::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (voxui::Palette::panel());
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (voxui::Palette::border());
    g.drawRoundedRectangle (r, 10.0f, 1.0f);

    g.setColour (voxui::Palette::magenta());
    g.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    g.drawText ("CUSTOM VOWELS", getLocalBounds().removeFromTop (30).reduced (14, 8),
                juce::Justification::centredLeft);
}

void CustomVowelsComponent::resized()
{
    auto r = getLocalBounds().reduced (14);
    r.removeFromTop (24);

    auto top = r.removeFromTop (26);
    setBox.setBounds (top.removeFromRight (140));
    top.removeFromRight (10);

    r.removeFromTop (6);
    auto headerRow = r.removeFromTop (18);
    const int col0 = 92;
    const int colW = (r.getWidth() - col0) / 3;

    if (headers.size() >= 4)
    {
        headers[0]->setBounds (headerRow.getX(), headerRow.getY(), col0, 18);
        for (int c = 1; c < 4; ++c)
            headers[c]->setBounds (headerRow.getX() + col0 + (c - 1) * colW, headerRow.getY(), colW, 18);
    }

    r.removeFromTop (2);
    const int rowH = (r.getHeight() - 38) / vox::VowelSet::numVowels;
    for (int row = 0; row < vox::VowelSet::numVowels; ++row)
    {
        const int y = r.getY() + row * rowH;
        edits[row * 4 + 0]->setBounds (r.getX(), y + 1, col0 - 6, rowH - 3);
        for (int c = 1; c < 4; ++c)
            edits[row * 4 + c]->setBounds (r.getX() + col0 + (c - 1) * colW + 3, y + 1, colW - 6, rowH - 3);
    }

    auto bottom = r.removeFromBottom (30);
    cancelButton.setBounds (bottom.removeFromRight (90));
    bottom.removeFromRight (8);
    applyButton.setBounds (bottom.removeFromRight (90));
}

VoxformaAudioProcessorEditor::VoxformaAudioProcessorEditor (VoxformaAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      visualizer (p),
      vowelStrip (p),
      vowelAmountSlider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
      emphasisSlider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
      driftAmountSlider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
      driftRangeSlider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
      f1GainSlider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
      f2GainSlider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
      f3GainSlider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
      smoothSlider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
      gainSlider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
      lfoVowel (p.apvts, p.engine(), 0, "Vowel LFO", voxui::Palette::magenta()),
      lfoEmphasis (p.apvts, p.engine(), 1, "Emphasis LFO", voxui::Palette::cyan()),
      lfoDrift (p.apvts, p.engine(), 2, "Drift LFO", voxui::Palette::amber())
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (visualizer);
    addAndMakeVisible (vowelStrip);
    addAndMakeVisible (lfoVowel);
    addAndMakeVisible (lfoEmphasis);
    addAndMakeVisible (lfoDrift);

    auto setupKnob = [this] (juce::Slider& s, const juce::String& name, juce::Colour colour)
    {
        s.setColour (juce::Slider::rotarySliderFillColourId, colour);
        s.setName (name);
        s.setPopupDisplayEnabled (true, true, this);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 17);
        addAndMakeVisible (s);
    };

    setupKnob (vowelAmountSlider, "Amount", voxui::Palette::magenta());
    setupKnob (emphasisSlider, "Emphasis", voxui::Palette::cyan());
    setupKnob (driftAmountSlider, "Amount", voxui::Palette::amber());
    setupKnob (driftRangeSlider, "Range", voxui::Palette::amber());
    setupKnob (f1GainSlider, "Gain", voxui::Palette::violet());
    setupKnob (f2GainSlider, "Gain", voxui::Palette::violet());
    setupKnob (f3GainSlider, "Gain", voxui::Palette::violet());
    setupKnob (smoothSlider, "Dry/Wet", voxui::Palette::cyan());
    setupKnob (gainSlider, "Gain", voxui::Palette::cyan());

    auto& st = p.apvts;
    vowelAmountAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, vox::ParamIDs::vowelAmount, vowelAmountSlider);
    emphasisAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, vox::ParamIDs::emphasis, emphasisSlider);
    driftAmountAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, vox::ParamIDs::driftAmount, driftAmountSlider);
    driftRangeAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, vox::ParamIDs::driftRange, driftRangeSlider);
    f1GainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, vox::ParamIDs::f1Gain, f1GainSlider);
    f2GainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, vox::ParamIDs::f2Gain, f2GainSlider);
    f3GainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, vox::ParamIDs::f3Gain, f3GainSlider);
    smoothAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, vox::ParamIDs::smooth, smoothSlider);
    gainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (st, vox::ParamIDs::gain, gainSlider);

    polarityBox.addItemList (vox::polarityChoices(), 1);
    addAndMakeVisible (polarityBox);
    polarityAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (st, vox::ParamIDs::driftPolarity, polarityBox);

    addAndMakeVisible (bypassButton);
    bypassButton.setClickingTogglesState (true);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (st, vox::ParamIDs::bypass, bypassButton);

    addAndMakeVisible (editVowelsButton);
    editVowelsButton.onClick = [this] { openCustomVowels(); };

    f1Power.setClickingTogglesState (true);
    f2Power.setClickingTogglesState (true);
    f3Power.setClickingTogglesState (true);
    addAndMakeVisible (f1Power);
    addAndMakeVisible (f2Power);
    addAndMakeVisible (f3Power);
    f1PowerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (st, vox::ParamIDs::f1Power, f1Power);
    f2PowerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (st, vox::ParamIDs::f2Power, f2Power);
    f3PowerAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (st, vox::ParamIDs::f3Power, f3Power);

    for (auto* label : { &vowelSetLabel, &driftInfoLabel, &emphasisInfoLabel })
    {
        label->setColour (juce::Label::textColourId, voxui::Palette::textDim());
        label->setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
        label->setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (label);
    }

    const juce::Colour infoColours[] = { voxui::Palette::cyan(), voxui::Palette::violet(),
                                         voxui::Palette::magenta() };
    int infoIndex = 0;
    for (auto* label : { &f1Info, &f2Info, &f3Info })
    {
        label->setColour (juce::Label::textColourId, infoColours[infoIndex++]);
        label->setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
        label->setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (label);
    }

    setSize (1080, 748);
    setResizable (true, true);
    setResizeLimits (940, 680, 1720, 1200);

    startTimerHz (10);
}

VoxformaAudioProcessorEditor::~VoxformaAudioProcessorEditor()
{
    customDialog.reset();
    stopTimer();
    setLookAndFeel (nullptr);
}

void VoxformaAudioProcessorEditor::timerCallback()
{
    const int setIndex = static_cast<int> (processor.apvts.state.getProperty ("vowelSet", 0));
    vowelSetLabel.setText (setLabelForIndex (setIndex), juce::dontSendNotification);
    emphasisInfoLabel.setText ("Q ~ " + juce::String (processor.engine().lastQForView(), 1),
                               juce::dontSendNotification);

    const auto raw = [this] (const char* id) { return processor.apvts.getRawParameterValue (id)->load(); };
    const float driftHz = raw (vox::ParamIDs::driftRange) * raw (vox::ParamIDs::driftAmount);
    const int pol = static_cast<int> (raw (vox::ParamIDs::driftPolarity));
    const auto sign = pol == 0 ? juce::String ("+") : pol == 1 ? juce::String ("-\u00b1") : juce::String ("\u00b1");
    driftInfoLabel.setText (sign + juce::String (static_cast<int> (std::round (driftHz))) + " Hz",
                            juce::dontSendNotification);

    juce::Label* infos[3] = { &f1Info, &f2Info, &f3Info };
    const juce::String prefix[3] = { "F1  ", "F2  ", "F3  " };
    for (int i = 0; i < 3; ++i)
    {
        const auto freq = processor.engine().formantFrequency (i);
        infos[i]->setText (prefix[i] + juce::String (static_cast<int> (std::round (freq))) + " Hz",
                           juce::dontSendNotification);
    }
}

void VoxformaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (voxui::Palette::bg());

    g.setColour (voxui::Palette::text());
    g.setFont (juce::Font (juce::FontOptions (27.0f, juce::Font::bold)));
    g.drawText ("voxforma", 14, 6, 260, 34, juce::Justification::centredLeft);

    g.setColour (voxui::Palette::textDim());
    g.setFont (juce::Font (juce::FontOptions (10.5f)));
    g.drawText ("clicktone", 150, 14, 100, 18, juce::Justification::centredLeft);

    g.setColour (voxui::Palette::border());
    g.drawHorizontalLine (52, 14.0f, static_cast<float> (getWidth() - 14));

    drawSection (g, secVowels);
    drawSection (g, secEmphasis);
    drawSection (g, secDrift);
    drawSection (g, secFormants);
    drawSection (g, secMaster);
}

void VoxformaAudioProcessorEditor::drawSection (juce::Graphics& g, const Section& s)
{
    if (s.bounds.isEmpty())
        return;
    auto r = s.bounds.toFloat();
    g.setColour (voxui::Palette::panel());
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (voxui::Palette::border());
    g.drawRoundedRectangle (r, 12.0f, 1.0f);
    g.setColour (voxui::Palette::text());
    g.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    g.drawText (s.title, s.bounds.getX() + 12, s.bounds.getY() + 7, s.bounds.getWidth() - 24, 16,
                juce::Justification::centredLeft);
}

void VoxformaAudioProcessorEditor::openCustomVowels()
{
    if (customDialog != nullptr)
    {
        customDialog->setVisible (true);
        customDialog->toFront (true);
        customDialog->centreAroundComponent (this, 560, 430);
        return;
    }

    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Custom Vowels";
    opts.dialogBackgroundColour = voxui::Palette::panel();
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = false;
    opts.resizable = false;

    auto* content = new CustomVowelsComponent (processor.apvts);
    content->onApply = [this] { repaint(); };
    opts.content.setOwned (content);
    opts.content->setSize (560, 430);

    customDialog.reset (opts.create());
    if (customDialog)
    {
        customDialog->setUsingNativeTitleBar (false);
        customDialog->setVisible (true);
        customDialog->toFront (true);
        customDialog->centreAroundComponent (this, 560, 430);
        customDialog->setAlwaysOnTop (true);
    }
}

void VoxformaAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    r.removeFromTop (46);

    bypassButton.setBounds (getWidth() - 14 - 92, 12, 92, 30);

    visualizer.setBounds (r.removeFromTop (216));
    r.removeFromTop (8);

    auto mid = r.removeFromTop (210);
    secVowels.bounds = mid.removeFromLeft (static_cast<int> (mid.getWidth() * 0.26f));
    secEmphasis.bounds = mid.removeFromLeft (static_cast<int> (mid.getWidth() * 0.53f * 0.34f + 40));
    secDrift.bounds = mid.removeFromLeft (static_cast<int> (mid.getWidth() * 0.58f));
    secFormants.bounds = mid;

    r.removeFromTop (8);
    auto lfoRow = r.removeFromTop (222);
    const int masterW = 190;
    secMaster.bounds = lfoRow.removeFromRight (masterW);
    const int each = lfoRow.getWidth() / 3;
    auto place = [&lfoRow, each] (juce::Component& c)
    {
        auto cell = lfoRow.removeFromLeft (each);
        cell.removeFromRight (6);
        c.setBounds (cell);
    };
    place (lfoVowel);
    place (lfoEmphasis);
    place (lfoDrift);

    {
        auto inner = secVowels.bounds.reduced (10);
        inner.removeFromTop (22);
        vowelSetLabel.setBounds (inner.removeFromTop (16));
        vowelStrip.setBounds (inner.removeFromTop (44));
        inner.removeFromTop (6);
        editVowelsButton.setBounds (inner.removeFromBottom (26));
        inner.removeFromBottom (6);
        vowelAmountSlider.setBounds (inner);
    }
    {
        auto inner = secEmphasis.bounds.reduced (10);
        inner.removeFromTop (22);
        emphasisInfoLabel.setBounds (inner.removeFromTop (16));
        emphasisSlider.setBounds (inner);
    }
    {
        auto inner = secDrift.bounds.reduced (10);
        inner.removeFromTop (22);
        driftInfoLabel.setBounds (inner.removeFromTop (16));
        polarityBox.setBounds (inner.removeFromBottom (26));
        inner.removeFromBottom (6);
        auto knobs = inner;
        const int kw = knobs.getWidth() / 2;
        driftAmountSlider.setBounds (knobs.getX(), knobs.getY(), kw - 4, knobs.getHeight());
        driftRangeSlider.setBounds (knobs.getX() + kw, knobs.getY(), kw - 4, knobs.getHeight());
    }
    {
        auto inner = secFormants.bounds.reduced (10);
        inner.removeFromTop (22);
        const int rowH = inner.getHeight() / 3;
        juce::Slider* gains[3] = { &f1GainSlider, &f2GainSlider, &f3GainSlider };
        juce::ToggleButton* powers[3] = { &f1Power, &f2Power, &f3Power };
        juce::Label* infos[3] = { &f1Info, &f2Info, &f3Info };
        for (int i = 0; i < 3; ++i)
        {
            auto row = inner.removeFromTop (rowH);
            powers[i]->setBounds (row.getX(), row.getCentreY() - 11, 46, 22);
            row.removeFromLeft (52);
            auto* gain = gains[i];
            auto knobArea = row.removeFromRight (row.getHeight() + 10);
            auto labelArea = row.reduced (6, 0);
            gain->setBounds (knobArea);
            infos[i]->setBounds (labelArea);
        }
    }
    {
        auto inner = secMaster.bounds.reduced (10);
        inner.removeFromTop (22);
        const int kw = inner.getWidth() / 2;
        smoothSlider.setBounds (inner.getX(), inner.getY(), kw - 4, inner.getHeight());
        gainSlider.setBounds (inner.getX() + kw, inner.getY(), kw - 4, inner.getHeight());
    }
}
