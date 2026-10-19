//
// Created by Javier Andoni Aizpuru Rodríguez on 20/09/26.
//

#include "limiter.h"
#include "band_splitter.h"
// ===========================================================================
// K-weighting (ITU-R BS.1770-4)
// ===========================================================================
void limiter::k_weighting_coeffs (double sample_rate,
                                  float& b0_s1, float& b1_s1, float& b2_s1,
                                  float& a1_s1, float& a2_s1,
                                  float& b0_s2, float& b1_s2, float& b2_s2,
                                  float& a1_s2, float& a2_s2)
{
    // Stage 1: high-shelf
    const double f0_s1 = 1681.974450955533;
    const double G_s1  = 3.999843853973347;
    const double Q_s1  = 0.7071752369554196;

    const double K_s1  = std::tan (M_PI * f0_s1 / sample_rate);
    const double Vh_s1 = std::pow (10.0, G_s1 / 20.0);
    const double Vb_s1 = std::pow (Vh_s1, 0.4996667741545416);

    const double a0_s1 = 1.0 + K_s1 / Q_s1 + K_s1 * K_s1;
    b0_s1 = (Vh_s1 + Vb_s1 * K_s1 / Q_s1 + K_s1 * K_s1) / a0_s1;
    b1_s1 = 2.0 * (K_s1 * K_s1 - Vh_s1) / a0_s1;
    b2_s1 = (Vh_s1 - Vb_s1 * K_s1 / Q_s1 + K_s1 * K_s1) / a0_s1;
    a1_s1 = 2.0 * (K_s1 * K_s1 - 1.0) / a0_s1;
    a2_s1 = (1.0 - K_s1 / Q_s1 + K_s1 * K_s1) / a0_s1;

    // Stage 2: high-pass
    const double f0_s2 = 38.13547087602444;
    const double Q_s2  = 0.5003270373238773;
    const double K_s2  = std::tan (M_PI * f0_s2 / sample_rate);

    const double a0_s2 = 1.0 + K_s2 / Q_s2 + K_s2 * K_s2;
    b0_s2 = 1.0;
    b1_s2 = -2.0;
    b2_s2 = 1.0;
    a1_s2 = 2.0 * (K_s2 * K_s2 - 1.0) / a0_s2;
    a2_s2 = (1.0 - K_s2 / Q_s2 + K_s2 * K_s2) / a0_s2;
}

void limiter::apply_k_weighting (const juce::AudioBuffer<float>& input,
                                 juce::AudioBuffer<float>& output,
                                 double sample_rate)
{
    float b0_s1, b1_s1, b2_s1, a1_s1, a2_s1;
    float b0_s2, b1_s2, b2_s2, a1_s2, a2_s2;
    k_weighting_coeffs (sample_rate,
                        b0_s1, b1_s1, b2_s1, a1_s1, a2_s1,
                        b0_s2, b1_s2, b2_s2, a1_s2, a2_s2);

    const int num_channels = input.getNumChannels();
    const int num_samples  = input.getNumSamples();

    for (int ch = 0; ch < num_channels; ++ch)
    {
        const auto* in  = input.getReadPointer (ch);
        auto* out       = output.getWritePointer (ch);

        float x1_s1 = 0, x2_s1 = 0, y1_s1 = 0, y2_s1 = 0;
        float x1_s2 = 0, x2_s2 = 0, y1_s2 = 0, y2_s2 = 0;

        for (int i = 0; i < num_samples; ++i)
        {
            const float x = in[i];

            const float y_s1 = b0_s1 * x + b1_s1 * x1_s1 + b2_s1 * x2_s1
                             - a1_s1 * y1_s1 - a2_s1 * y2_s1;
            x2_s1 = x1_s1; x1_s1 = x;
            y2_s1 = y1_s1; y1_s1 = y_s1;

            const float y_s2 = b0_s2 * y_s1 + b1_s2 * x1_s2 + b2_s2 * x2_s2
                             - a1_s2 * y1_s2 - a2_s2 * y2_s2;
            x2_s2 = x1_s2; x1_s2 = y_s1;
            y2_s2 = y1_s2; y1_s2 = y_s2;

            out[i] = y_s2;
        }
    }
}

// ===========================================================================
// Loudness of the loudest N-second window.
//
// 1. K-weight the whole buffer.
// 2. Slide a window of N seconds in 100 ms hops.
// 3. Compute mean-square of each window (summed across channels).
// 4. Pick the window with the highest mean-square.
// 5. Convert to LUFS with the BS.1770 offset.
// ===========================================================================
float limiter::measure_loudest_window_lufs (const juce::AudioBuffer<float>& buffer,
                                            double sample_rate,
                                            float window_seconds)
{
    const int num_channels = buffer.getNumChannels();
    const int num_samples  = buffer.getNumSamples();

    if (num_channels == 0 || num_samples == 0)
        return -std::numeric_limits<float>::infinity();

    juce::AudioBuffer<float> weighted (num_channels, num_samples);
    apply_k_weighting (buffer, weighted, sample_rate);

    const int window_len = static_cast<int> (window_seconds * sample_rate);
    if (window_len <= 0 || window_len > num_samples)
    {
        // Fall back to whole-file mean square if the file is shorter than the window
        double sum_sq = 0.0;
        for (int ch = 0; ch < num_channels; ++ch)
        {
            const auto* d = weighted.getReadPointer (ch);
            for (int i = 0; i < num_samples; ++i)
                sum_sq += static_cast<double> (d[i]) * d[i];
        }
        const double mean_sq = sum_sq / (static_cast<double> (num_samples) * num_channels);
        return static_cast<float> (-0.691 + 10.0 * std::log10 (mean_sq + 1e-30));
    }

    const int hop = std::max (1, static_cast<int> (0.1 * sample_rate));

    // Running sum of squares per channel, slid across the buffer.
    std::vector<double> ch_sum_sq (num_channels, 0.0);

    for (int ch = 0; ch < num_channels; ++ch)
    {
        const auto* d = weighted.getReadPointer (ch);
        double s = 0.0;
        for (int i = 0; i < window_len; ++i)
            s += static_cast<double> (d[i]) * d[i];
        ch_sum_sq[ch] = s;
    }

    auto total_of = [&]() {
        double t = 0.0;
        for (double s : ch_sum_sq) t += s;
        return t;
    };

    double best_sum = total_of();

    for (int start = hop; start + window_len <= num_samples; start += hop)
    {
        for (int ch = 0; ch < num_channels; ++ch)
        {
            const auto* d = weighted.getReadPointer (ch);
            double s = ch_sum_sq[ch];
            for (int i = start - hop; i < start; ++i)
                s -= static_cast<double> (d[i]) * d[i];
            for (int i = start + window_len - hop; i < start + window_len; ++i)
                s += static_cast<double> (d[i]) * d[i];
            ch_sum_sq[ch] = s;
        }

        const double t = total_of();
        if (t > best_sum) best_sum = t;
    }

    const double mean_sq = best_sum / (static_cast<double> (window_len) * num_channels);
    return static_cast<float> (-0.691 + 10.0 * std::log10 (mean_sq + 1e-30));
}

// ===========================================================================
// True-peak measurement (uses the caller-supplied oversampler so that
// measurement and limiting see exactly the same filter).
// ===========================================================================
float limiter::measure_true_peak_db (const juce::AudioBuffer<float>& buffer,
                                     juce::dsp::Oversampling<float>& oversampler)
{
    juce::dsp::AudioBlock<const float> const_block (buffer);
    auto os_block = oversampler.processSamplesUp (const_block);

    float max_peak = 0.0f;
    for (size_t ch = 0; ch < os_block.getNumChannels(); ++ch)
    {
        const auto* d = os_block.getChannelPointer (ch);
        for (size_t i = 0; i < os_block.getNumSamples(); ++i)
            max_peak = juce::jmax (max_peak, std::abs (d[i]));
    }
    return juce::Decibels::gainToDecibels (max_peak);
}

// ===========================================================================
// Soft clipper (tanh). Reduces crest factor so the limiter has less work
// to do, letting us reach very loud LUFS targets at a given dBTP ceiling.
//
// drive_db > 0 pushes the signal into the nonlinear region.
// Output is normalized so that a full-scale input stays at unity gain for
// small drive values; at high drive the output is intentionally compressed.
// ===========================================================================
void limiter::apply_soft_clip (juce::AudioBuffer<float>& buffer,
                               float drive_db)
{
    if (drive_db <= 0.0f)
        return;

    const float drive = juce::Decibels::decibelsToGain (drive_db);
    const float norm  = 1.0f / std::tanh (drive);

    const int num_channels = buffer.getNumChannels();
    const int num_samples  = buffer.getNumSamples();

    for (int ch = 0; ch < num_channels; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < num_samples; ++i)
            d[i] = std::tanh (d[i] * drive) * norm;
    }
}

// ===========================================================================
// True-peak limiter (lookahead, operates in the oversampled domain).
//
// Algorithm:
//   1. Upsample.
//   2. Compute per-sample required gain: min(1, ceiling / |x|).
//   3. Sliding-window minimum over the lookahead length -> "target gain".
//      This already encodes the lookahead, so attack is instantaneous.
//   4. Apply a one-pole release smoother to the target gain.
//   5. Multiply the oversampled signal by the smoothed gain.
//   6. Downsample.
// ===========================================================================
void limiter::apply_true_peak_limiter (juce::AudioBuffer<float>& buffer,
                                       juce::dsp::Oversampling<float>& oversampler,
                                       const loudness_params& params,
                                       double sample_rate)
{
    const int num_channels = buffer.getNumChannels();
    if (num_channels == 0 || buffer.getNumSamples() == 0)
        return;

    const float ceiling_lin = juce::Decibels::decibelsToGain (params.target_true_peak_db);

    juce::dsp::AudioBlock<float> block (buffer);
    auto os_block = oversampler.processSamplesUp (block);

    const int os_len      = static_cast<int> (os_block.getNumSamples());
    const int os_channels = static_cast<int> (os_block.getNumChannels());

    if (os_len <= 0)
        return;

    // ---- 2. required gain per sample ----
    std::vector<float> req (os_len, 1.0f);
    for (int i = 0; i < os_len; ++i)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < os_channels; ++ch)
            peak = juce::jmax (peak, std::abs (os_block.getChannelPointer (ch)[i]));

        req[i] = (peak > ceiling_lin) ? (ceiling_lin / peak) : 1.0f;
    }

    // ---- 3. sliding-window minimum over the lookahead ----
    const int lookahead = juce::jmax (1, static_cast<int> (params.lookahead_ms * 0.001
                                                           * sample_rate
                                                           * params.oversample_factor));

    std::vector<float> target (os_len, 1.0f);
    {
        // Monotonic deque of indices, values increasing.
        std::deque<int> dq;

        for (int i = 0; i < os_len; ++i)
        {
            while (! dq.empty() && req[dq.back()] >= req[i])
                dq.pop_back();
            dq.push_back (i);

            const int win_start = i - lookahead + 1;
            while (! dq.empty() && dq.front() < win_start)
                dq.pop_front();

            if (i >= lookahead - 1)
                target[i - lookahead + 1] = req[dq.front()];
        }


        for (int i = os_len - lookahead + 1; i < os_len; ++i)
        {
            if (i < 0) continue;
            float m = 1.0f;
            for (int j = i; j < os_len; ++j) m = juce::jmin (m, req[j]);
            target[i] = m;
        }
    }

    const float release_coeff = std::exp (-1.0f / (params.release_ms * 0.001f
                                                   * sample_rate
                                                   * params.oversample_factor));

    std::vector<float> gain (os_len, 1.0f);
    float env = 1.0f;
    for (int i = 0; i < os_len; ++i)
    {
        const float t = target[i];
        if (t < env)
            env = t;
        else
            env = t + (env - t) * release_coeff;
        gain[i] = env;
    }

    for (int ch = 0; ch < os_channels; ++ch)
    {
        auto* d = os_block.getChannelPointer (ch);
        for (int i = 0; i < os_len; ++i)
            d[i] *= gain[i];
    }

    oversampler.processSamplesDown (block);
}

void limiter::normalize_true_peak_and_lufs (juce::AudioBuffer<float>& buffer,
                                            double sample_rate,
                                            const loudness_params& params)
{
    const int num_channels = buffer.getNumChannels();
    const int num_samples  = buffer.getNumSamples();

    if (num_channels == 0 || num_samples == 0 || sample_rate <= 0.0)
        return;

    if (params.oversample_factor < 1
        || (params.oversample_factor & (params.oversample_factor - 1)) != 0)
        return;

    if (params.analysis_window_s <= 0.0f)
        return;

    if (params.lookahead_ms <= 0.0f || params.release_ms <= 0.0f)
        return;

    juce::dsp::Oversampling<float> oversampler (
        static_cast<size_t> (num_channels),
        static_cast<size_t> (std::log2 (params.oversample_factor)),
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true, false);

    oversampler.initProcessing (static_cast<size_t> (num_samples));
    oversampler.reset();

    const float lufs = measure_loudest_window_lufs (buffer, sample_rate,
                                                    params.analysis_window_s);
    if (! std::isfinite (lufs))
        return;

    const float gain_db = params.target_lufs - lufs;

    if (std::abs (gain_db) > 0.01f)
        buffer.applyGain (juce::Decibels::decibelsToGain (gain_db));


    auto bands = band_splitter::split_into_four_bands (buffer,
                                                       sample_rate,
                                                       { 120.0f, 800.0f, 5000.0f });

    loudness_params band_params = params;
    band_params.target_true_peak_db = params.target_true_peak_db + 2.0f;

    for (auto& band : bands)
    {
        if (params.clipper_enabled)
            apply_soft_clip (band, params.clipper_drive_db);

        oversampler.reset();
        apply_true_peak_limiter (band, oversampler, band_params, sample_rate);
    }

    band_splitter::sum_four_bands (bands, buffer);

    oversampler.reset();
    apply_true_peak_limiter (buffer, oversampler, params, sample_rate);

    oversampler.reset();
    const float final_lufs = measure_loudest_window_lufs (buffer, sample_rate,
                                                          params.analysis_window_s);
    oversampler.reset();
    const float final_tp   = measure_true_peak_db (buffer, oversampler);

    DBG ("[limiter] final: " << final_lufs << " LUFS (target "
         << params.target_lufs << "), "
         << final_tp << " dBTP (ceiling "
         << params.target_true_peak_db << "), "
         << "gain applied " << gain_db << " dB, "
         << "clipper " << (params.clipper_enabled ? "on" : "off")
         << " @" << params.clipper_drive_db << " dB");
}