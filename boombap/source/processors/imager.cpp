//
// Created by Javier Andoni Aizpuru Rodríguez on 20/09/26.
//

#include "imager.h"

void imager::process_bands(const std::array<juce::AudioBuffer<float>, 4>& bands, const std::array<float, 4>& bandWidths,
    juce::AudioBuffer<float>& output)
{
    jassert (output.getNumChannels() == 2);

    const int numSamples = output.getNumSamples();

    for (int b = 0; b < 4; ++b)
    {
        jassert (bands[b].getNumSamples() == numSamples);
    }

    output.clear();

    auto* outL = output.getWritePointer (0);
    auto* outR = output.getWritePointer (1);

    for (int b = 0; b < 4; ++b)
    {
        const auto* L = bands[b].getReadPointer (0);
        const auto* R = bands[b].getReadPointer (1);

        const float w = bandWidths[b];

        for (int i = 0; i < numSamples; ++i)
        {
            const float mid  = 0.5f * (L[i] + R[i]);
            const float side = 0.5f * (R[i] - L[i]);

            const float sideW = side * w;

            outL[i] += mid - sideW;
            outR[i] += mid + sideW;
        }
    }
}
