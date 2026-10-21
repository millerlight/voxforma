#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdio>

static const char* kBundlePath = "/home/clickibunti2/Dokumente/rea/voxforma/build/voxforma_artefacts/Release/VST3/voxforma.vst3";

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::AudioPluginFormatManager manager;
    manager.addFormat (new juce::VST3PluginFormat());

    juce::OwnedArray<juce::PluginDescription> types;
    manager.getFormat (0)->findAllTypesForFile (types, kBundlePath);
    if (types.isEmpty())
    {
        std::printf ("no types found for %s\n", kBundlePath);
        std::printf ("VST3 LOAD FAIL\n");
        return 1;
    }

    auto* type = types[0];
    std::printf ("type: %s | manufacturer: %s | isInstrument: %d\n",
                 type->name.toRawUTF8(), type->manufacturerName.toRawUTF8(),
                 (int) type->isInstrument);

    juce::String error;
    std::unique_ptr<juce::AudioPluginInstance> instance (
        manager.createPluginInstance (*type, 48000.0, 512, error));

    if (instance == nullptr)
    {
        std::printf ("instantiate failed: %s\n", error.toRawUTF8());
        std::printf ("VST3 LOAD FAIL\n");
        return 1;
    }

    const auto params = instance->getParameters();
    std::printf ("parameters: %d\n", (int) params.size());

    instance->enableAllBuses();
    instance->prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    bool finite = true;
    bool nonSilent = false;
    for (int block = 0; block < 100; ++block)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                buf.setSample (ch, i, 0.4f * std::sin ((block * 512 + i) * 0.05f));
        instance->processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
            {
                const float v = buf.getSample (ch, i);
                if (! std::isfinite (v))
                    finite = false;
                if (std::abs (v) > 1.0e-7f)
                    nonSilent = true;
            }
    }

    bool editorOk = true;
    if (auto* editor = instance->createEditorIfNeeded())
    {
        editor->setSize (1080, 748);
        juce::Image img (juce::Image::ARGB, 1080, 748, true);
        juce::Graphics g (img);
        editor->paintEntireComponent (g, true);
        editorOk = img.isValid();
        instance->editorBeingDeleted (editor);
        delete editor;
    }
    else
    {
        editorOk = false;
    }

    instance->releaseResources();

    const bool pass = finite && nonSilent && editorOk && params.size() >= 30;
    std::printf ("finite=%d nonSilent=%d editorOk=%d params=%d\n",
                 (int) finite, (int) nonSilent, (int) editorOk, (int) params.size());
    std::printf (pass ? "VST3 PASS\n" : "VST3 FAIL\n");
    return pass ? 0 : 1;
}
