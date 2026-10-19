//
// Created by Javier Andoni Aizpuru Rodríguez on 20/09/26.
//

#ifndef MATERINGSERVICE_LIMITER_H
#define MATERINGSERVICE_LIMITER_H

#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>
#include <deque>
#include <algorithm>
#include <limits>

class limiter
{
public:
    struct loudness_params
    {
        float target_true_peak_db = -1.0f;
        float target_lufs         = -8.5f;
        float lookahead_ms        = 5.0f;
        float release_ms          = 150.0f;
        int   oversample_factor   = 4;
        float analysis_window_s   = 5.0f;

        bool  clipper_enabled     = false;
        float clipper_drive_db    = 3.0f;
    };

    static void normalize_true_peak_and_lufs (juce::AudioBuffer<float>& buffer,
                                              double sample_rate,
                                              const loudness_params& params);

private:

    static void k_weighting_coeffs (double sample_rate,
                                    float& b0_s1, float& b1_s1, float& b2_s1,
                                    float& a1_s1, float& a2_s1,
                                    float& b0_s2, float& b1_s2, float& b2_s2,
                                    float& a1_s2, float& a2_s2);

    static void apply_k_weighting (const juce::AudioBuffer<float>& input,
                                   juce::AudioBuffer<float>& output,
                                   double sample_rate);

    static float measure_loudest_window_lufs (const juce::AudioBuffer<float>& buffer,
                                              double sample_rate,
                                              float window_seconds);

    static float measure_true_peak_db (const juce::AudioBuffer<float>& buffer,
                                       juce::dsp::Oversampling<float>& oversampler);

    static void apply_soft_clip (juce::AudioBuffer<float>& buffer,
                                 float drive_db);

    static void apply_true_peak_limiter (juce::AudioBuffer<float>& buffer,
                                         juce::dsp::Oversampling<float>& oversampler,
                                         const loudness_params& params,
                                         double sample_rate);
};

#endif //MATERINGSERVICE_LIMITER_H