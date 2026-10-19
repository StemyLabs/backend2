//
// Created by Javier Andoni Aizpuru Rodríguez on 20/09/26.
//

#ifndef MATERINGSERVICE_BAND_SPLITTER_H
#define MATERINGSERVICE_BAND_SPLITTER_H

#include <juce_dsp/juce_dsp.h>

class band_splitter
{
public:
    static std::array<juce::AudioBuffer<float>, 4>
    split_into_four_bands(const juce::AudioBuffer<float>& input,
                       double sampleRate,
                       const std::array<float, 3>& splitFrequencies);
    static void sum_four_bands (const std::array<juce::AudioBuffer<float>, 4>& bands,
                     juce::AudioBuffer<float>& output);
};
#endif //MATERINGSERVICE_BAND_SPLITTER_H
