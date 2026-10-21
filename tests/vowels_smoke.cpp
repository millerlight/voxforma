#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>

namespace
{
juce::TextEditor* findEditor (juce::Component& parent, int index)
{
    int seen = 0;
    for (int i = 0; i < parent.getNumChildComponents(); ++i)
    {
        if (auto* ed = dynamic_cast<juce::TextEditor*> (parent.getChildComponent (i)))
        {
            if (seen++ == index)
                return ed;
        }
    }
    return nullptr;
}

juce::ComboBox* findCombo (juce::Component& parent)
{
    for (int i = 0; i < parent.getNumChildComponents(); ++i)
        if (auto* cb = dynamic_cast<juce::ComboBox*> (parent.getChildComponent (i)))
            return cb;
    return nullptr;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    VoxformaAudioProcessor processor;
    bool pass = true;

    {
        CustomVowelsComponent dialog (processor.apvts);
        dialog.setSize (560, 430);
        dialog.resized();

        auto* combo = findCombo (dialog);
        auto* name0 = findEditor (dialog, 0);
        auto* f1_0 = findEditor (dialog, 1);

        if (combo == nullptr || name0 == nullptr || f1_0 == nullptr)
        {
            std::printf ("dialog widgets not found\n");
            return 1;
        }

        const auto maleName = name0->getText();
        const auto maleF1 = f1_0->getText();

        combo->setSelectedId (2, juce::sendNotificationSync);
        const auto femaleName = name0->getText();
        const auto femaleF1 = f1_0->getText();

        combo->setSelectedId (3, juce::sendNotificationSync);
        const auto childName = name0->getText();
        const auto childF1 = f1_0->getText();

        if (femaleName == maleName && femaleF1 == maleF1)
        {
            std::printf ("female set identical to male set (%s / %s)\n",
                         maleName.toRawUTF8(), maleF1.toRawUTF8());
            pass = false;
        }

        if (childName == femaleName && childF1 == femaleF1)
        {
            std::printf ("child set identical to female set\n");
            pass = false;
        }

        name0->setText ("ZZTest");
        combo->setSelectedId (1, juce::sendNotificationSync);
        if (name0->getText() != maleName)
        {
            std::printf ("edits leaked across sets: got '%s' expected '%s'\n",
                         name0->getText().toRawUTF8(), maleName.toRawUTF8());
            pass = false;
        }

        std::printf ("male='%s' female='%s' child='%s'\n",
                     maleName.toRawUTF8(), femaleName.toRawUTF8(), childName.toRawUTF8());
    }

    for (int i = 0; i < 50; ++i)
    {
        CustomVowelsComponent dialog (processor.apvts);
        dialog.setSize (560, 430);
        dialog.resized();
        if (auto* combo = findCombo (dialog))
            combo->setSelectedId (2, juce::sendNotificationSync);
    }

    std::printf (pass ? "VOWELS PASS\n" : "VOWELS FAIL\n");
    return pass ? 0 : 1;
}
