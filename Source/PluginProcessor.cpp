#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
float parseFloatList (const juce::String& tokens, int index, float fallback)
{
    const auto parts = juce::StringArray::fromTokens (tokens, "|", {});
    if (juce::isPositiveAndBelow (index, parts.size()))
        return parts[index].getFloatValue();
    return fallback;
}
}

VoxformaAudioProcessor::VoxformaAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", vox::createParameterLayout())
{
}

void VoxformaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine_.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    engine_.reset();
    fftFifo.fill (0.0f);
    fifoWrite.store (0);
}

void VoxformaAudioProcessor::releaseResources()
{
    engine_.reset();
}

bool VoxformaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

const vox::VowelSet& VoxformaAudioProcessor::resolveVowelSet()
{
    const int setIndex = juce::jlimit (0, 3, static_cast<int> (apvts.state.getProperty ("vowelSet", 0)));

    const auto key = [] (const char* base, int idx)
    {
        juce::String s (base);
        s << "_" << idx;
        return s;
    };

    const auto names = apvts.state.getProperty (key ("vowelNames", setIndex), juce::var()).toString();
    const auto f1    = apvts.state.getProperty (key ("vowelF1", setIndex), juce::var()).toString();
    const auto f2    = apvts.state.getProperty (key ("vowelF2", setIndex), juce::var()).toString();
    const auto f3    = apvts.state.getProperty (key ("vowelF3", setIndex), juce::var()).toString();

    if (names.isEmpty() && f1.isEmpty() && f2.isEmpty() && f3.isEmpty())
        return vox::VowelSet::setForIndex (setIndex == 3 ? 0 : setIndex);

    if (names == cachedNames && f1 == cachedF1 && f2 == cachedF2 && f3 == cachedF3)
        return customSet;

    cachedNames = names;
    cachedF1 = f1;
    cachedF2 = f2;
    cachedF3 = f3;

    const auto& fallback = vox::VowelSet::setForIndex (setIndex == 3 ? 0 : setIndex);
    auto nameParts = juce::StringArray::fromTokens (names, "|", {});

    for (int i = 0; i < vox::VowelSet::numVowels; ++i)
    {
        customSet.f1[i] = parseFloatList (f1, i, fallback.f1[static_cast<size_t> (i)]);
        customSet.f2[i] = parseFloatList (f2, i, fallback.f2[static_cast<size_t> (i)]);
        customSet.f3[i] = parseFloatList (f3, i, fallback.f3[static_cast<size_t> (i)]);

        const juce::String n = i < nameParts.size() ? nameParts[i]
                                                    : juce::String (fallback.names[static_cast<size_t> (i)]);
        auto& dst = customNameStorage[static_cast<size_t> (i)];
        std::memset (dst.data(), 0, dst.size());
        n.copyToUTF8 (dst.data(), dst.size() - 1);
        customSet.names[static_cast<size_t> (i)] = dst.data();
    }

    return customSet;
}

void VoxformaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();

    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
        if (auto pos = playHead->getPosition())
            if (auto hostBpm = pos->getBpm())
                bpm = *hostBpm;
    engine_.setTempo (bpm);

    const auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    engine_.setActiveVowelSet (resolveVowelSet());

    struct LfoIds { const char *rate, *amount, *phase, *wave, *sync, *power; };
    static constexpr LfoIds lfoIds[3] = {
        { vox::ParamIDs::vowelLfoRate,    vox::ParamIDs::vowelLfoAmount,    vox::ParamIDs::vowelLfoPhase,    vox::ParamIDs::vowelLfoWave,    vox::ParamIDs::vowelLfoSync,    vox::ParamIDs::vowelLfoPower },
        { vox::ParamIDs::emphasisLfoRate, vox::ParamIDs::emphasisLfoAmount, vox::ParamIDs::emphasisLfoPhase, vox::ParamIDs::emphasisLfoWave, vox::ParamIDs::emphasisLfoSync, vox::ParamIDs::emphasisLfoPower },
        { vox::ParamIDs::driftLfoRate,    vox::ParamIDs::driftLfoAmount,    vox::ParamIDs::driftLfoPhase,    vox::ParamIDs::driftLfoWave,    vox::ParamIDs::driftLfoSync,    vox::ParamIDs::driftLfoPower }
    };

    for (int slot = 0; slot < 3; ++slot)
    {
        const auto& ids = lfoIds[slot];
        engine_.setLfo (slot,
                        raw (ids.rate),
                        raw (ids.amount),
                        raw (ids.phase),
                        static_cast<int> (raw (ids.wave)),
                        raw (ids.sync) > 0.5f,
                        raw (ids.power) > 0.5f);
    }

    float fGains[vox::FormantEngine::numFormants] {
        raw (vox::ParamIDs::f1Gain), raw (vox::ParamIDs::f2Gain), raw (vox::ParamIDs::f3Gain)
    };

    engine_.setTargets (raw (vox::ParamIDs::vowelAmount),
                        raw (vox::ParamIDs::emphasis),
                        raw (vox::ParamIDs::driftRange),
                        raw (vox::ParamIDs::driftAmount),
                        static_cast<int> (raw (vox::ParamIDs::driftPolarity)),
                        fGains,
                        raw (vox::ParamIDs::gain),
                        raw (vox::ParamIDs::smooth));

    engine_.setFormantPower (0, raw (vox::ParamIDs::f1Power) > 0.5f);
    engine_.setFormantPower (1, raw (vox::ParamIDs::f2Power) > 0.5f);
    engine_.setFormantPower (2, raw (vox::ParamIDs::f3Power) > 0.5f);
    engine_.setBypass (raw (vox::ParamIDs::bypass) > 0.5f);

    engine_.process (buffer, midi);

    pushFifoData (buffer);
}

void VoxformaAudioProcessor::pushFifoData (const juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (numSamples <= 0 || numCh <= 0)
        return;

    int write = fifoWrite.load (std::memory_order_relaxed);
    const float* left = buffer.getReadPointer (0);
    const float* right = numCh > 1 ? buffer.getReadPointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float v = right != nullptr ? 0.5f * (left[i] + right[i]) : left[i];
        fftFifo[static_cast<size_t> (write)] = v;
        if (++write >= fftFifoSize)
            write = 0;
    }
    fifoWrite.store (write, std::memory_order_relaxed);
}

std::array<float, VoxformaAudioProcessor::fftFifoSize> VoxformaAudioProcessor::copyFifo() const
{
    const int write = fifoWrite.load (std::memory_order_relaxed);
    std::array<float, fftFifoSize> out {};
    for (int i = 0; i < fftFifoSize; ++i)
        out[static_cast<size_t> (i)] = fftFifo[static_cast<size_t> ((write + i) % fftFifoSize)];
    return out;
}

void VoxformaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VoxformaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            cachedNames.clear();
        }
}

juce::AudioProcessorEditor* VoxformaAudioProcessor::createEditor()
{
    return new VoxformaAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoxformaAudioProcessor();
}
