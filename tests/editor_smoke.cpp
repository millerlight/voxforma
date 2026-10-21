#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    VoxformaAudioProcessor processor;
    auto* editor = processor.createEditor();

    if (editor == nullptr)
    {
        std::printf ("createEditor returned null\nEDITOR FAIL\n");
        return 1;
    }

    bool pass = true;

    for (auto& size : { juce::Rectangle<int> (1080, 748),
                        juce::Rectangle<int> (940, 680),
                        juce::Rectangle<int> (1720, 1200) })
    {
        editor->setSize (size.getWidth(), size.getHeight());
        juce::Image img (juce::Image::ARGB, size.getWidth(), size.getHeight(), true);
        juce::Graphics g (img);
        editor->paintEntireComponent (g, true);

        for (int i = 0; i < editor->getNumChildComponents(); ++i)
        {
            auto& child = *editor->getChildComponent (i);
            if (child.getLocalBounds().isEmpty())
            {
                std::printf ("child %d (%s) has empty bounds at %dx%d\n",
                             i, child.getName().toRawUTF8(),
                             size.getWidth(), size.getHeight());
                pass = false;
            }
            const auto b = child.getBoundsInParent();
            if (b.getRight() < -2 || b.getBottom() < -2
                || b.getX() > size.getWidth() + 2 || b.getY() > size.getHeight() + 2)
            {
                std::printf ("child %d (%s) out of bounds: %s\n",
                             i, child.getName().toRawUTF8(), b.toString().toRawUTF8());
                pass = false;
            }
        }

        if (! img.isValid())
            pass = false;

        if (size.getWidth() == 1080)
        {
            auto file = juce::File ("/tmp/opencode/voxforma_editor.png");
            file.deleteFile();
            juce::FileOutputStream stream (file);
            if (stream.openedOk())
                juce::PNGImageFormat().writeImageToStream (img, stream);

            std::printf ("--- child bounds at 1080x748 ---\n");
            for (int i = 0; i < editor->getNumChildComponents(); ++i)
            {
                auto& child = *editor->getChildComponent (i);
                std::printf ("%2d  %-18s %s\n", i, child.getName().toRawUTF8(),
                             child.getBoundsInParent().toString().toRawUTF8());
            }
        }
    }

    processor.editorBeingDeleted (editor);
    delete editor;

    std::printf (pass ? "EDITOR PASS\n" : "EDITOR FAIL\n");
    return pass ? 0 : 1;
}
