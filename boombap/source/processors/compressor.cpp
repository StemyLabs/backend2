//
// Created by Javier Andoni Aizpuru Rodríguez on 20/09/26.
//

#include "compressor.h"

void compressor::process_comp_and_limit(juce::AudioBuffer<float>& buffer, double sample_rate, const dynamics_params& p)
{
    const int num_channels = buffer.getNumChannels();
    const int num_samples  = buffer.getNumSamples();

    juce::AudioBuffer<float> dry;
    dry.makeCopyOf (buffer);

    juce::dsp::ProcessSpec spec {
        sample_rate,
        (juce::uint32) num_samples,
        (juce::uint32) num_channels
    };


    juce::dsp::Compressor<float> compressor;
    compressor.prepare (spec);
    compressor.setThreshold (p.comp_threshold);
    compressor.setRatio     (p.comp_ratio);
    compressor.setAttack    (p.comp_attack);
    compressor.setRelease   (p.comp_release);

    juce::dsp::Compressor<float> limiter;
    limiter.prepare (spec);
    limiter.setThreshold (p.lim_threshold);
    limiter.setRatio     (p.lim_ratio);
    limiter.setAttack    (p.lim_attack);
    limiter.setRelease   (p.lim_release);

    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);

        compressor.process (ctx);
        limiter.process (ctx);
    }

    if (p.comp_makeup != 0.0f)
    {
        buffer.applyGain (juce::Decibels::decibelsToGain (p.comp_makeup));
    }

    // -------- parallel blend --------
    const float wet     = juce::jlimit (0.0f, 1.0f, 1.0f - p.parallel);
    const float dry_amt = juce::jlimit (0.0f, 1.0f, p.parallel);

    for (int ch = 0; ch < num_channels; ++ch)
    {
        auto* out = buffer.getWritePointer (ch);
        const auto* in = dry.getReadPointer (ch);

        for (int i = 0; i < num_samples; ++i)
            out[i] = out[i] * wet + in[i] * dry_amt;
    }
}
