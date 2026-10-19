//
// Created by Javier Andoni Aizpuru Rodríguez on 17/09/26.
//

#ifndef MATERINGSERVICE_EQUALIZER_H
#define MATERINGSERVICE_EQUALIZER_H

#include <juce_dsp/juce_dsp.h>
#include "spectrum_analyzer.h"
class equalizer : public juce::dsp::ProcessorChain<>
{
public:
    static juce::AudioBuffer<float> apply_equaliser(const juce::AudioBuffer<float>& input_song,
                                         double sample_rate,
                                         const spectrum_analyzer::result& input_result,
                                         const spectrum_analyzer::result& reference_result,
                                         float max_gain_db = 2.f);
private:
};


#endif //MATERINGSERVICE_EQUALIZER_H
