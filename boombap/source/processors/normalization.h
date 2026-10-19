//
// Created by Javier Andoni Aizpuru Rodríguez on 16/09/26.
//

#ifndef MATERINGSERVICE_NORMALIZATION_PROCESSOR_H
#define MATERINGSERVICE_NORMALIZATION_PROCESSOR_H

#include <juce_dsp/juce_dsp.h>

class normalization final : public juce::dsp::ProcessorBase
{
public:
    void prepare(const juce::dsp::ProcessSpec&) override;
    void process(const juce::dsp::ProcessContextReplacing<float>&) override;
    void reset() override;
    static constexpr int normalization_target_db = -16;
};


#endif //MATERINGSERVICE_NORMALIZATION_PROCESSOR_H
