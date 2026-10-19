//
// Created by Javier Andoni Aizpuru Rodríguez on 16/09/26.
//

#include "normalization.h"

void normalization::prepare([[maybe_unused]] const juce::dsp::ProcessSpec& specs)
{
}

void normalization::process(const juce::dsp::ProcessContextReplacing<float>& context)
{
    const auto input_block  = context.getInputBlock();
    auto output_block = context.getOutputBlock();

    auto min_and_max = input_block.findMinAndMax();

    auto max = std::max(juce::Decibels::gainToDecibels(std::abs(min_and_max.getStart())),
    juce::Decibels::gainToDecibels(std::abs(min_and_max.getEnd())));

    if (max <= -100.f)
    {
        return;
    }

    const auto db_diff = normalization_target_db - max;
    const auto gain = juce::Decibels::decibelsToGain(db_diff);

    output_block.multiplyBy(gain);
}

void normalization::reset()
{
}
