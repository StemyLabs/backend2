//
// Created by Javier Andoni Aizpuru Rodríguez on 20/09/26.
//

#ifndef MATERINGSERVICE_COMPRESSOR_H
#define MATERINGSERVICE_COMPRESSOR_H

#include <juce_dsp/juce_dsp.h>

class compressor
{
public:

    struct dynamics_params
    {
        // compressor
        float comp_threshold  = -18.0f;  // dB
        float comp_ratio      = 4.0f;
        float comp_attack     = 10.0f;   // ms
        float comp_release    = 100.0f;  // ms
        float comp_makeup     = 0.0f;    // dB

        float lim_threshold   = -1.0f;   // dB
        float lim_ratio       = 30.0f;   // high ratio = limiting behavior
        float lim_attack      = 1.0f;    // ms
        float lim_release     = 50.0f;   // ms

        float parallel        = 0.0f;
    };

    static void process_comp_and_limit (juce::AudioBuffer<float>& buffer,
                                 double sample_rate,
                                 const dynamics_params& p);
};


#endif //MATERINGSERVICE_COMPRESSOR_H
