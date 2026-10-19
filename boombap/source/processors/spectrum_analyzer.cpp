//
// Created by Javier Andoni Aizpuru Rodríguez on 17/09/26.
//

#include "spectrum_analyzer.h"

void spectrum_analyzer::prepare(const juce::dsp::ProcessSpec& specs)
{
    _specs = specs;
    _fft = std::make_unique<juce::dsp::FFT>(fft_order);
    _windowing_function = std::make_unique<juce::dsp::WindowingFunction<float>>(fft_size,
                                                                                juce::dsp::WindowingFunction<float>::hann);
}

void spectrum_analyzer::process(const juce::dsp::ProcessContextReplacing<float>& context)
{
    const auto block = context.getInputBlock();
    const auto num_of_samples = block.getNumSamples();
    const auto num_of_channels = block.getNumChannels();

    auto mono_data = std::vector<float>(num_of_samples, 0.f);
    const auto sum_ratio = 1.f / static_cast<float>(num_of_channels);

    for (std::size_t ch = 0; ch < num_of_channels; ch++)
    {
        const auto ptr = block.getChannelPointer(ch);

        for (std::size_t s = 0; s < num_of_samples; ++s)
        {
            mono_data[s] += ptr[s] * sum_ratio;
        }
    }

    auto fft_data = std::vector<float>(fft_data_size, 0.f);

    std::size_t fft_position = 0;
    constexpr std::size_t num_bins = fft_size / 2 + 1;
    auto spectrum_sum = std::vector<double>(num_bins, 0.0);
    std::size_t frame_count = 0;

    while (fft_position + fft_size <= num_of_samples)
    {
        std::ranges::fill(fft_data, 0.f);

        std::ranges::copy(mono_data.begin() + fft_position,
                          mono_data.begin() + fft_position + fft_size,
                          fft_data.begin());

        auto frame_power = 0.0;

        for (std::size_t i = 0; i < fft_size; ++i)
        {
            frame_power += static_cast<double>(fft_data[i]) * fft_data[i];
        }

        frame_power /= fft_size;

        if (frame_power < 1e-6)
        {
            fft_position += fft_hop_size;
            continue;
        }

        _windowing_function->multiplyWithWindowingTable(fft_data.data(), fft_size);
        _fft->performFrequencyOnlyForwardTransform(fft_data.data());

        for (std::size_t bin = 0; bin < num_bins; ++bin)
        {
            const double mag = fft_data[bin] / (fft_size / 2.0);
            spectrum_sum[bin] += mag * mag;
        }

        ++frame_count;
        fft_position += fft_hop_size;
    }

    if (frame_count == 0)
    {
        return;
    }

    const auto bin_hz = _specs.sampleRate / fft_size;
    std::vector<double> centers;

    for (double f = 25.0; f <= 16000.0; f *= std::pow(2.0, 1.0 / 3.0))
    {
        centers.push_back(f);
    }

    auto band_db = std::vector<float>(centers.size(), -120.f);

    for (std::size_t b = 0; b < centers.size(); ++b)
    {
        const double lo = centers[b] / std::pow(2.0, 1.0 / 6.0);
        const double hi = centers[b] * std::pow(2.0, 1.0 / 6.0);

        const auto first = static_cast<std::size_t>(std::ceil(lo / bin_hz));
        const auto last  = std::min(static_cast<std::size_t>(std::floor(hi / bin_hz)),
                                    num_bins - 1);

        if (first > last)
        {
            continue;
        }

        double power = 0.0;
        for (std::size_t bin = first; bin <= last; ++bin)
        {
            power += spectrum_sum[bin] / static_cast<double>(frame_count);
        }

        band_db[b] = static_cast<float>(10.0 * std::log10(power + 1e-12));
    }

    const auto sum_db = std::accumulate(band_db.begin(), band_db.end(), 0.0);
    const auto mean_db = static_cast<float>(sum_db / static_cast<double>(band_db.size()));

    for (auto& db : band_db)
    {
        db -= mean_db;
    }

    _result = { std::move(centers), std::move(band_db) };
}

void spectrum_analyzer::reset()
{

}

std::optional<spectrum_analyzer::result> spectrum_analyzer::get_result()
{
    return _result;
}
