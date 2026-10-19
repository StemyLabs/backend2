//
// Created by Javier Andoni Aizpuru Rodríguez on 16/09/26.
//

#ifndef MATERINGSERVICE_MASTER_PROCESSOR_H
#define MATERINGSERVICE_MASTER_PROCESSOR_H

#include <juce_dsp/juce_dsp.h>
class master_processor
{
public:

    static bool master_audio_file(const juce::File& file, const juce::File& output_file);

    static void master_audio_buffer(juce::AudioBuffer<float>& audio_buffer, const juce::dsp::ProcessSpec& specs);

    static constexpr std::size_t maximum_block_size = 2048;
};


#endif //MATERINGSERVICE_MASTER_PROCESSOR_H
