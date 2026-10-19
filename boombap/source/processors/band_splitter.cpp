//
// Created by Javier Andoni Aizpuru Rodríguez on 20/09/26.
//

#include "band_splitter.h"

std::array<juce::AudioBuffer<float>, 4> band_splitter::split_into_four_bands(const juce::AudioBuffer<float>& input,
    double sampleRate, const std::array<float, 3>& splitFrequencies)
{
    jassert (input.getNumChannels() == 2);

    const int numChannels = input.getNumChannels();
    const int numSamples  = input.getNumSamples();

    juce::AudioBuffer<float> full (numChannels, numSamples);
    full.makeCopyOf (input);

    juce::AudioBuffer<float> high1 (numChannels, numSamples); // >= f1
    juce::AudioBuffer<float> high2 (numChannels, numSamples); // >= f2
    juce::AudioBuffer<float> high3 (numChannels, numSamples); // >= f3

    // Output bands
    std::array<juce::AudioBuffer<float>, 4> bands {
        juce::AudioBuffer<float> (numChannels, numSamples), // Band 1: 0 - f1
        juce::AudioBuffer<float> (numChannels, numSamples), // Band 2: f1 - f2
        juce::AudioBuffer<float> (numChannels, numSamples), // Band 3: f2 - f3
        juce::AudioBuffer<float> (numChannels, numSamples)  // Band 4: f3 - Nyquist
    };

    using Filter = juce::dsp::LinkwitzRileyFilter<float>;
    Filter lr1, lr2, lr3;

    juce::dsp::ProcessSpec spec { sampleRate,
                                  (juce::uint32) numSamples,
                                  (juce::uint32) numChannels };

    lr1.prepare (spec);
    lr2.prepare (spec);
    lr3.prepare (spec);

    lr1.setCutoffFrequency (splitFrequencies[0]);
    lr2.setCutoffFrequency (splitFrequencies[1]);
    lr3.setCutoffFrequency (splitFrequencies[2]);


    {
        juce::dsp::AudioBlock<float> block (full);
        juce::dsp::ProcessContextReplacing<float> ctx (block);

        juce::AudioBuffer<float> lowCopy;
        lowCopy.makeCopyOf (full);

        juce::dsp::AudioBlock<float> lowBlock (lowCopy);
        juce::dsp::ProcessContextReplacing<float> lowCtx (lowBlock);

        lr1.setType (Filter::Type::lowpass);
        lr1.process (lowCtx);

        lr1.setType (Filter::Type::highpass);
        lr1.process (ctx);

        bands[0].makeCopyOf (lowCopy);
        high1.makeCopyOf (full);
    }


    {
        juce::AudioBuffer<float> lowCopy;
        lowCopy.makeCopyOf (high1);

        juce::dsp::AudioBlock<float> lowBlock (lowCopy);
        juce::dsp::ProcessContextReplacing<float> lowCtx (lowBlock);

        juce::dsp::AudioBlock<float> block (high1);
        juce::dsp::ProcessContextReplacing<float> ctx (block);

        lr2.setType (Filter::Type::lowpass);
        lr2.process (lowCtx);

        lr2.setType (Filter::Type::highpass);
        lr2.process (ctx);

        bands[1].makeCopyOf (lowCopy);
        high2.makeCopyOf (high1);
    }

    {
        juce::AudioBuffer<float> lowCopy;
        lowCopy.makeCopyOf (high2);

        juce::dsp::AudioBlock<float> lowBlock (lowCopy);
        juce::dsp::ProcessContextReplacing<float> lowCtx (lowBlock);

        juce::dsp::AudioBlock<float> block (high2);
        juce::dsp::ProcessContextReplacing<float> ctx (block);

        lr3.setType (Filter::Type::lowpass);
        lr3.process (lowCtx);

        lr3.setType (Filter::Type::highpass);
        lr3.process (ctx);

        bands[2].makeCopyOf (lowCopy);
        bands[3].makeCopyOf (high2);
    }

    return bands;
}

void band_splitter::sum_four_bands (const std::array<juce::AudioBuffer<float>, 4>& bands,
                     juce::AudioBuffer<float>& output)
{
    const int num_channels = output.getNumChannels();
    const int num_samples  = output.getNumSamples();

    output.clear();

    for (int b = 0; b < 4; ++b)
    {
        jassert (bands[b].getNumChannels() == num_channels);
        jassert (bands[b].getNumSamples()  == num_samples);

        for (int ch = 0; ch < num_channels; ++ch)
        {
            const auto* in  = bands[b].getReadPointer (ch);
            auto*       out = output.getWritePointer (ch);

            for (int i = 0; i < num_samples; ++i)
                out[i] += in[i];
        }
    }
}