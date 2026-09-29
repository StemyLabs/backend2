// One-shot client-review export: float WAV → 24-bit PCM with TPDF dither.
// Does not master, normalize, or change sample rate.
#include "stemy/audio/wav_io.h"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <input_float.wav> <output_pcm24.wav>\n";
    return 2;
  }
  const std::string in = argv[1];
  const std::string out = argv[2];

  stemy::audio::WavReadResult wav;
  if (auto st = stemy::audio::read_wav(in, wav); !st) {
    std::cerr << "Read failed: " << st.message() << "\n";
    return 1;
  }
  if (auto st = stemy::audio::validate_input_audio(wav.buffer, wav.format); !st) {
    std::cerr << "Validate failed: " << st.message() << "\n";
    return 1;
  }
  if (auto st = stemy::audio::write_wav_pcm24_tpdf(out, wav.buffer, wav.format); !st) {
    std::cerr << "Write PCM24 failed: " << st.message() << "\n";
    return 1;
  }
  std::cout << "Wrote 24-bit PCM (TPDF dither): " << out
            << " sr=" << wav.format.sample_rate
            << " frames=" << wav.buffer.frame_count() << "\n";
  return 0;
}
