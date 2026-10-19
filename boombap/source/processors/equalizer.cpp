//
// Created by Javier Andoni Aizpuru Rodríguez on 17/09/26.
//

#include "equalizer.h"

juce::AudioBuffer<float> equalizer::apply_equaliser(const juce::AudioBuffer<float>& input_song, double sample_rate,
    const spectrum_analyzer::result& input_result, const spectrum_analyzer::result& reference_result, float max_gain_db)
{
      const auto num_bands = input_result.centers.size();

    if (num_bands < 2
        || input_result.band_db.size() != num_bands
        || reference_result.band_db.size() != num_bands
        || reference_result.centers.size() != num_bands)
    {
        return input_song;
    }

    const auto& centers = input_result.centers;

    std::vector<float> diff(num_bands);
    for (std::size_t b = 0; b < num_bands; ++b)
    {
        diff[b] = reference_result.band_db[b] - input_result.band_db[b];
    }

    auto gains = diff;
    for (std::size_t b = 1; b + 1 < num_bands; ++b)
        gains[b] = 0.25f * diff[b - 1] + 0.5f * diff[b] + 0.25f * diff[b + 1];

    for (auto& g : gains)
        g = std::clamp(g, -max_gain_db, max_gain_db);

    const auto gain_at = [&](double freq) -> float
    {
        if (freq <= centers.front()) return gains.front();
        if (freq >= centers.back())  return gains.back();

        const auto it = std::upper_bound(centers.begin(), centers.end(), freq);
        const auto i = static_cast<std::size_t>(it - centers.begin()); // i >= 1

        const auto t = (std::log(freq) - std::log(centers[i - 1]))
                     / (std::log(centers[i]) - std::log(centers[i - 1]));

        return gains[i - 1] + static_cast<float>(t) * (gains[i] - gains[i - 1]);
    };

    constexpr int fir_order = 13;
    constexpr std::size_t fir_size = std::size_t{1} << fir_order;

    juce::dsp::FFT design_fft(fir_order);
    std::vector<std::complex<float>> spectrum(fir_size), impulse(fir_size);


    for (std::size_t k = 0; k <= fir_size / 2; ++k)
    {
        const auto freq = static_cast<double>(k) * sample_rate / static_cast<double>(fir_size);
        const auto mag = std::pow(10.f, gain_at(freq) / 20.f);

        spectrum[k] = mag;
        if (k > 0 && k < fir_size / 2)
            spectrum[fir_size - k] = mag;
    }

    design_fft.perform(spectrum.data(), impulse.data(), true); // inverse

    std::vector<float> fir(fir_size);
    for (std::size_t n = 0; n < fir_size; ++n)
    {
        const auto window = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi
                                                   * static_cast<float>(n)
                                                   / static_cast<float>(fir_size));
        fir[n] = impulse[(n + fir_size / 2) % fir_size].real() * window;
    }

    const std::size_t delay = fir_size / 2; // latency of the linear-phase filter

    constexpr int conv_order = 14;
    constexpr std::size_t conv_size = std::size_t{1} << conv_order;
    constexpr std::size_t block_len = conv_size - fir_size + 1;

    juce::dsp::FFT conv_fft(conv_order);
    std::vector<std::complex<float>> fir_freq(conv_size), buf_in(conv_size),
                                     buf_freq(conv_size), buf_out(conv_size);

    std::fill(buf_in.begin(), buf_in.end(), std::complex<float>{});
    for (std::size_t i = 0; i < fir_size; ++i)
        buf_in[i] = fir[i];
    conv_fft.perform(buf_in.data(), fir_freq.data(), false);

    const auto num_channels = input_song.getNumChannels();
    const auto num_samples = static_cast<std::size_t>(input_song.getNumSamples());

    juce::AudioBuffer<float> output(num_channels, static_cast<int>(num_samples));
    std::vector<float> full(num_samples + fir_size - 1);

    for (int ch = 0; ch < num_channels; ++ch)
    {
        const auto* in = input_song.getReadPointer(ch);
        std::fill(full.begin(), full.end(), 0.f);

        for (std::size_t pos = 0; pos < num_samples; pos += block_len)
        {
            const auto len = std::min(block_len, num_samples - pos);

            std::fill(buf_in.begin(), buf_in.end(), std::complex<float>{});
            for (std::size_t i = 0; i < len; ++i)
                buf_in[i] = in[pos + i];

            conv_fft.perform(buf_in.data(), buf_freq.data(), false);

            for (std::size_t k = 0; k < conv_size; ++k)
                buf_freq[k] *= fir_freq[k];

            conv_fft.perform(buf_freq.data(), buf_out.data(), true);

            for (std::size_t i = 0; i < len + fir_size - 1; ++i)
                full[pos + i] += buf_out[i].real();
        }

        auto* out = output.getWritePointer(ch);
        for (std::size_t s = 0; s < num_samples; ++s)
            out[s] = full[s + delay];
    }

    return output;
}
