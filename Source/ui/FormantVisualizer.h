#pragma once

#include <complex>
#include <juce_dsp/juce_dsp.h>
#include "../PluginProcessor.h"
#include "VoxLookAndFeel.h"

namespace voxui
{

class FormantVisualizer : public juce::Component,
                          private juce::Timer
{
public:
    explicit FormantVisualizer (VoxformaAudioProcessor& p)
        : processor (p), fft (11), windowing (static_cast<size_t> (1u << 11),
                                             juce::dsp::WindowingFunction<float>::hann, true)
    {
        setInterceptsMouseClicks (false, false);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (Palette::bg());
        g.fillRoundedRectangle (bounds, 12.0f);
        g.setColour (Palette::border());
        g.drawRoundedRectangle (bounds, 12.0f, 1.0f);

        plotArea = bounds.reduced (14.0f, 22.0f);

        drawFrequencyGrid (g);
        drawSpectrum (g);
        drawResponse (g);
        drawFormantMarkers (g);

        g.setColour (Palette::textDim());
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.drawText ("FORMANT RESPONSE", plotArea.withHeight (12).translated (0, -16),
                    juce::Justification::centredLeft);

        g.setColour (Palette::cyan());
        g.drawText ("LIVE", getLocalBounds().removeFromRight (46).withHeight (14).reduced (4, 2),
                    juce::Justification::centred);
    }

private:
    static constexpr int fftSize = VoxformaAudioProcessor::fftFifoSize;

    void timerCallback() override { repaint(); }

    float freqToX (float freq) const
    {
        const float minLog = std::log10 (20.0f);
        const float maxLog = std::log10 (20000.0f);
        const float norm = (std::log10 (juce::jmax (20.0f, freq)) - minLog) / (maxLog - minLog);
        return plotArea.getX() + norm * plotArea.getWidth();
    }

    float yForDb (float db) const
    {
        const float norm = juce::jmap (juce::jlimit (-72.0f, 12.0f, db), -72.0f, 12.0f, 1.0f, 0.0f);
        return plotArea.getY() + norm * plotArea.getHeight();
    }

    void drawFrequencyGrid (juce::Graphics& g)
    {
        g.setColour (Palette::textDim().withAlpha (0.35f));
        g.setFont (juce::Font (juce::FontOptions (10.0f)));

        const float marks[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
        for (float f : marks)
        {
            const float x = freqToX (f);
            g.drawVerticalLine (juce::roundToInt (x), plotArea.getY(), plotArea.getBottom());
            juce::String label = f >= 1000.0f
                                     ? juce::String (f / 1000.0f, 1) + "k"
                                     : juce::String (static_cast<int> (f));
            g.drawText (label, juce::Rectangle<float> (x - 18.0f, plotArea.getBottom() - 13.0f, 36.0f, 12.0f),
                        juce::Justification::centred);
        }
    }

    void drawSpectrum (juce::Graphics& g)
    {
        const auto fifo = processor.copyFifo();
        std::array<float, fftSize * 2> work {};
        for (int i = 0; i < fftSize; ++i)
            work[static_cast<size_t> (i)] = fifo[static_cast<size_t> (i)];

        windowing.multiplyWithWindowingTable (work.data(), static_cast<size_t> (fftSize));
        fft.performRealOnlyForwardTransform (work.data());

        const double sampleRate = juce::jmax (8000.0, processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0);

        juce::Path path;
        bool started = false;
        const int numBins = fftSize / 2;
        for (int px = 0; px <= 200; ++px)
        {
            const float norm = static_cast<float> (px) / 200.0f;
            const float freq = std::pow (10.0f, std::log10 (20.0f) + norm * (std::log10 (20000.0f) - std::log10 (20.0f)));
            const int bin = juce::jlimit (1, numBins - 1, static_cast<int> (freq / (static_cast<float> (sampleRate) / fftSize)));

            const float re = work[static_cast<size_t> (2 * bin)];
            const float im = work[static_cast<size_t> (2 * bin + 1)];
            const float mag = std::sqrt (re * re + im * im) / static_cast<float> (fftSize / 4);
            const float db = juce::Decibels::gainToDecibels (juce::jmax (1.0e-6f, mag));

            const float x = plotArea.getX() + norm * plotArea.getWidth();
            const float y = yForDb (db);
            if (! started) { path.startNewSubPath (x, y); started = true; }
            else           { path.lineTo (x, y); }
        }

        auto filled = path;
        filled.lineTo (plotArea.getRight(), plotArea.getBottom());
        filled.lineTo (plotArea.getX(), plotArea.getBottom());
        filled.closeSubPath();
        g.setColour (Palette::cyan().withAlpha (0.10f));
        g.fillPath (filled);

        g.setColour (Palette::cyan().withAlpha (0.45f));
        g.strokePath (path, juce::PathStrokeType (1.2f));
    }

    void drawResponse (juce::Graphics& g)
    {
        const auto taps = processor.engine().responseTaps();
        const double sampleRate = juce::jmax (8000.0, processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 48000.0);

        juce::Path path;
        bool started = false;
        for (int px = 0; px <= 300; ++px)
        {
            const float norm = static_cast<float> (px) / 300.0f;
            const float freq = std::pow (10.0f, std::log10 (20.0f) + norm * (std::log10 (20000.0f) - std::log10 (20.0f)));
            const double w = juce::MathConstants<double>::twoPi * freq / sampleRate;

            std::complex<float> total { 0.0f, 0.0f };
            for (const auto& tap : taps)
            {
                if (tap.gain <= 0.0001f)
                    continue;
                const std::complex<float> z1 { static_cast<float> (std::cos (w)), static_cast<float> (-std::sin (w)) };
                const std::complex<float> z2 = z1 * z1;
                const std::complex<float> num = tap.c.b0 + tap.c.b1 * z1 + tap.c.b2 * z2;
                const std::complex<float> den = 1.0f + tap.c.a1 * z1 + tap.c.a2 * z2;
                total += num / den * tap.gain;
            }

            const float db = juce::Decibels::gainToDecibels (juce::jmax (1.0e-6f, std::abs (total)));
            const float x = plotArea.getX() + norm * plotArea.getWidth();
            const float y = yForDb (db);
            if (! started) { path.startNewSubPath (x, y); started = true; }
            else           { path.lineTo (x, y); }
        }

        g.setColour (Palette::cyan().withAlpha (0.25f));
        g.strokePath (path, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (Palette::cyan().brighter (0.3f));
        g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawFormantMarkers (juce::Graphics& g)
    {
        const auto& engine = processor.engine();
        static const juce::Colour colours[] = { Palette::cyan(), Palette::violet(), Palette::magenta() };

        for (int i = 0; i < 3; ++i)
        {
            const float freq = engine.formantFrequency (i);
            if (freq <= 0.0f)
                continue;
            const float x = freqToX (freq);

            g.setColour (colours[i].withAlpha (0.65f));
            const float dashes[] = { 3.0f, 5.0f };
            g.drawDashedLine (juce::Line<float> (x, plotArea.getY(), x, plotArea.getBottom()),
                              dashes, 2, 1.0f);

            const auto label = juce::String ("F") + juce::String (i + 1) + "  "
                               + juce::String (static_cast<int> (std::round (freq))) + " Hz";
            g.setColour (colours[i].withAlpha (0.92f));
            auto chip = juce::Rectangle<float> (x - 44.0f, plotArea.getY() + 4.0f + i * 18.0f, 88.0f, 16.0f);
            g.fillRoundedRectangle (chip, 8.0f);
            g.setColour (Palette::bg());
            g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
            g.drawText (label, chip.toNearestIntEdges(), juce::Justification::centred);
        }
    }

    VoxformaAudioProcessor& processor;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> windowing;
    juce::Rectangle<float> plotArea { 14.0f, 22.0f, 100.0f, 100.0f };
};

}
