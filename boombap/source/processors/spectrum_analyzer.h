//
// Created by Javier Andoni Aizpuru Rodríguez on 17/09/26.
//

#ifndef MATERINGSERVICE_SPECTRUM_ANALIZER_H
#define MATERINGSERVICE_SPECTRUM_ANALIZER_H

#include <juce_dsp/juce_dsp.h>

class spectrum_analyzer : public juce::dsp::ProcessorBase
{
public:
    struct result
    {
        std::vector<double> centers;
        std::vector<float> band_db;
    };

    void prepare(const juce::dsp::ProcessSpec& specs) override;
    void process(const juce::dsp::ProcessContextReplacing<float>& context) override;
    void reset() override;
    std::optional<result> get_result();
private:

    static constexpr auto fft_order = 15;
    static constexpr auto fft_size = 1 << fft_order;
    static constexpr auto fft_data_size = fft_size * 2;
    static constexpr auto fft_hop_size = fft_size / 2;
    juce::dsp::ProcessSpec _specs {};
    std::unique_ptr<juce::dsp::FFT> _fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> _windowing_function;
    std::optional<result> _result {std::nullopt};


};
#endif //MATERINGSERVICE_SPECTRUM_ANALIZER_H
