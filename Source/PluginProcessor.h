#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "dsp/FormantEngine.h"

class VoxformaAudioProcessor : public juce::AudioProcessor
{
public:
    VoxformaAudioProcessor();
    ~VoxformaAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.02; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    vox::FormantEngine& engine() noexcept { return engine_; }
    const vox::FormantEngine& engine() const noexcept { return engine_; }

    static constexpr int fftFifoSize = 2048;
    std::array<float, fftFifoSize> copyFifo() const;

private:
    const vox::VowelSet& resolveVowelSet();
    void pushFifoData (const juce::AudioBuffer<float>& buffer);

    vox::FormantEngine engine_;

    vox::VowelSet customSet;
    std::array<std::array<char, 32>, vox::VowelSet::numVowels> customNameStorage {};
    juce::String cachedNames, cachedF1, cachedF2, cachedF3;

    mutable std::array<float, fftFifoSize> fftFifo {};
    std::atomic<int> fifoWrite { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxformaAudioProcessor)
};
