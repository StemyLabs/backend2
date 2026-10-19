//
// Created by Javier Andoni Aizpuru Rodríguez on 20/09/26.
//

#ifndef MATERINGSERVICE_IMAGER_H
#define MATERINGSERVICE_IMAGER_H

#include <juce_dsp/juce_dsp.h>

class imager
{
public:
    static void process_bands (const std::array<juce::AudioBuffer<float>, 4>& bands,
                     const std::array<float, 4>& bandWidths,
                     juce::AudioBuffer<float>& output);
};


#endif //MATERINGSERVICE_IMAGER_H
