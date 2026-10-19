//
// Created by Javier Andoni Aizpuru Rodríguez on 16/09/26.
//

#include "master_processor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include "normalization.h"
#include "equalizer.h"
#include "band_splitter.h"
#include "imager.h"
#include "compressor.h"
#include "limiter.h"
#include "maximizer.h"
#include "genres/boom_bap.h"

bool master_processor::master_audio_file(const juce::File& file, const juce::File& output_file)
{
    const auto format_manager = std::make_unique<juce::AudioFormatManager>();

    format_manager->registerBasicFormats();

    if (auto format_reader = std::unique_ptr<juce::AudioFormatReader>(format_manager->createReaderFor(file)))
    {
        juce::AudioBuffer<float> audio_buffer;

        const auto num_of_samples = format_reader->lengthInSamples;
        const auto num_of_channels = format_reader->numChannels;

        audio_buffer.setSize(static_cast<int>(num_of_channels), static_cast<int>(num_of_samples));

        const auto success = format_reader->read(&audio_buffer,
                                           0,
                                            static_cast<int>(num_of_samples),
                                           0,
                                           true,
                                           true);

        if (success)
        {
            juce::dsp::ProcessSpec specs
            {
                format_reader->sampleRate,
                maximum_block_size,
                num_of_channels
            };

            master_audio_buffer(audio_buffer, specs);

            if (output_file.existsAsFile())
            {
                std::ignore = output_file.deleteFile();;
            }

            if (auto created = output_file.create();
                  created.failed())
            {
                return false;
            }

            auto output_stream = std::unique_ptr<juce::OutputStream>(new juce::FileOutputStream(output_file));

            if (!dynamic_cast<juce::FileOutputStream*>(output_stream.get())->failedToOpen())
            {
                juce::WavAudioFormat wav_format;

                auto options = juce::AudioFormatWriterOptions()
                               .withSampleRate(specs.sampleRate)
                               .withBitsPerSample(static_cast<int>(format_reader->bitsPerSample))
                               .withNumChannels(static_cast<int>(num_of_channels));

                auto writer = wav_format.createWriterFor(output_stream, options);

                return writer->writeFromAudioSampleBuffer(audio_buffer, 0, audio_buffer.getNumSamples());
            }
        }
    }

    return {};
}

void master_processor::master_audio_buffer(juce::AudioBuffer<float>& audio_buffer, const juce::dsp::ProcessSpec& specs)
{
    auto block = juce::dsp::AudioBlock<float>(audio_buffer);

    {
        const auto context = juce::dsp::ProcessContextReplacing<float>(block);

        normalization normalization;

        normalization.prepare({specs.sampleRate,
            static_cast<juce::uint32>(audio_buffer.getNumSamples()),
            specs.numChannels});

        normalization.process(context);

        spectrum_analyzer analyzer;

        analyzer.prepare(specs);
        analyzer.process(context);
        const auto result = analyzer.get_result();

        jassert(result.has_value());

        audio_buffer = equalizer::equalizer::apply_equaliser(audio_buffer, specs.sampleRate, result.value(), boom_bap::analysis);

        auto ms_four_bands  = band_splitter::split_into_four_bands(audio_buffer,
            specs.sampleRate,
            {132, 956, 5100});

        juce::AudioBuffer<float> ms_buffer;
        auto num_of_samples = audio_buffer.getNumSamples();
        ms_buffer.setSize(audio_buffer.getNumChannels(), num_of_samples);

        imager::process_bands(ms_four_bands, { 0.1f, 1.3f, 1.4f, 2.f }, ms_buffer);

        const float inRms  = audio_buffer.getRMSLevel (0, 0, num_of_samples) +
                             audio_buffer.getRMSLevel (1, 0, num_of_samples);
        const float outRms = ms_buffer.getRMSLevel (0, 0, num_of_samples)
                           + ms_buffer.getRMSLevel (1, 0, num_of_samples);

        if (outRms > 0.0f)
        {
            ms_buffer.applyGain (inRms / outRms);
        }

        audio_buffer = std::move(ms_buffer);

        auto compressor_four_bands  = band_splitter::split_into_four_bands(audio_buffer,
            specs.sampleRate,
    {132, 956, 5100});

        audio_buffer.clear();

        std::array<compressor::dynamics_params, 4> comp_params;

        comp_params[0].comp_attack = 38;
        comp_params[0].comp_release = 80;
        comp_params[0].comp_ratio = 3;
        comp_params[0].comp_threshold = -18.2f;
        comp_params[0].lim_attack = 26.8f;
        comp_params[0].lim_release = 67.0f;
        comp_params[0].lim_threshold = -12.2f;
        comp_params[0].comp_makeup = -1.f;
        comp_params[0].lim_ratio = 30;
        comp_params[0].parallel = 1.f;

        comp_params[1].comp_attack = 10.2f;
        comp_params[1].comp_release = 84.f;
        comp_params[1].comp_ratio = 2;
        comp_params[1].comp_threshold = -22.3f;
        comp_params[1].lim_attack = 18.0f;
        comp_params[1].lim_release = 51.f;
        comp_params[1].lim_threshold = -15.2f;
        comp_params[1].comp_makeup = 0.f;
        comp_params[1].lim_ratio = 10;
        comp_params[1].parallel = 1.f;

        comp_params[2].comp_attack = 20.0f;
        comp_params[2].comp_release = 51.f;
        comp_params[2].comp_ratio = 2;
        comp_params[2].comp_threshold = -21.4f;
        comp_params[2].lim_attack = 12.0f;
        comp_params[2].lim_release = 93.f;
        comp_params[2].lim_threshold = -13.8f;
        comp_params[2].comp_makeup = 0.f;
        comp_params[2].lim_ratio = 10;
        comp_params[2].parallel = 1.f;

        comp_params[3].comp_attack = 6.2f;
        comp_params[3].comp_release = 118.f;
        comp_params[3].comp_ratio = 2;
        comp_params[3].comp_threshold = -22.8f;
        comp_params[3].lim_attack = 10.0f;
        comp_params[3].lim_release = 88.f;
        comp_params[3].lim_threshold = -16.6f;
        comp_params[3].comp_makeup = 0.f;
        comp_params[3].lim_ratio = 30;
        comp_params[3].parallel = 1.f;

        for (auto i = 0; auto& params : comp_params)
        {
            compressor::process_comp_and_limit(compressor_four_bands[i], specs.sampleRate, params);
            ++i;
        }


        band_splitter::sum_four_bands(compressor_four_bands, audio_buffer);

        mx::Settings s;
        s.targetLUFS  = -7.0;
        s.ceilingdBTP = -0.5;

        [[maybe_unused]] mx::Result r = mx::maximize (audio_buffer, specs.sampleRate, s);
    }
}
