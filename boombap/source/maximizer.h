
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <limits>
#include <vector>

namespace mx
{
inline double dbToLin (double dB) { return std::pow (10.0, dB / 20.0); }
inline double linToDb (double x)  { return 20.0 * std::log10 (std::max (x, 1.0e-12)); }

struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

inline void makeKWeighting (double fs, Biquad& shelf, Biquad& highpass)
{
    const double pi = juce::MathConstants<double>::pi;
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K  = std::tan (pi * f0 / fs);
        const double Vh = std::pow (10.0, G / 20.0);
        const double Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        shelf.b0 = (Vh + Vb * K / Q + K * K) / a0;
        shelf.b1 = 2.0 * (K * K - Vh) / a0;
        shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
        shelf.a1 = 2.0 * (K * K - 1.0) / a0;
        shelf.a2 = (1.0 - K / Q + K * K) / a0;
    }
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K  = std::tan (pi * f0 / fs);
        const double a0 = 1.0 + K / Q + K * K;
        highpass.b0 = 1.0; highpass.b1 = -2.0; highpass.b2 = 1.0;
        highpass.a1 = 2.0 * (K * K - 1.0) / a0;
        highpass.a2 = (1.0 - K / Q + K * K) / a0;
    }
}

inline double integratedLUFS (const juce::AudioBuffer<float>& buf, double fs)
{
    const int nCh   = std::min (buf.getNumChannels(), 2);
    const int hop   = (int) std::round (0.1 * fs);            // 100 ms
    const int nHops = hop > 0 ? buf.getNumSamples() / hop : 0;
    const double neginf = -std::numeric_limits<double>::infinity();

    if (nHops < 4 || nCh == 0)
        return neginf;

    std::vector<double> hopEnergy ((size_t) nHops, 0.0);

    for (int ch = 0; ch < nCh; ++ch)
    {
        Biquad shelf, hp;
        makeKWeighting (fs, shelf, hp);
        const float* x = buf.getReadPointer (ch);

        for (int h = 0; h < nHops; ++h)
        {
            double sum = 0.0;
            for (int i = h * hop; i < (h + 1) * hop; ++i)
            {
                const double y = hp.process (shelf.process ((double) x[i]));
                sum += y * y;
            }
            hopEnergy[(size_t) h] += sum / hop;
        }
    }

    std::vector<double> blocks;
    for (int h = 0; h + 4 <= nHops; ++h)
        blocks.push_back (0.25 * (hopEnergy[(size_t) h] + hopEnergy[(size_t) h + 1]
                                + hopEnergy[(size_t) h + 2] + hopEnergy[(size_t) h + 3]));

    auto lufs = [] (double z) { return -0.691 + 10.0 * std::log10 (std::max (z, 1.0e-20)); };

    auto gatedMean = [&] (double thresholdLUFS, double& mean) -> bool
    {
        double s = 0.0; int c = 0;
        for (double z : blocks)
            if (lufs (z) > thresholdLUFS) { s += z; ++c; }
        if (c == 0) return false;
        mean = s / c;
        return true;
    };

    double mean = 0.0;
    if (! gatedMean (-70.0, mean))                    return neginf;
    if (! gatedMean (lufs (mean) - 10.0, mean))       return neginf;
    return lufs (mean);
}

class TruePeakDetector
{
public:
    static constexpr int K = 8;

    TruePeakDetector()
    {
        const double pi = juce::MathConstants<double>::pi;
        for (int p = 0; p < 3; ++p)
        {
            const double f = 0.25 * (p + 1);
            for (int j = 0; j < 2 * K; ++j)
            {
                const double t    = f - (j - (K - 1));
                const double sinc = std::abs (t) < 1e-12 ? 1.0 : std::sin (pi * t) / (pi * t);
                const double w    = 0.5 * (1.0 + std::cos (pi * t / K));   // Hann
                h[(size_t) p][(size_t) j] = (float) (sinc * w);
            }
        }
    }

    void envelope (const float* x, int n, float* env) const
    {
        std::vector<float> pad ((size_t) (n + 2 * K), 0.0f);
        std::copy (x, x + n, pad.begin() + K);

        for (int i = 0; i < n; ++i)
        {
            float pk = std::abs (x[i]);
            const float* base = pad.data() + i + 1;

            for (int p = 0; p < 3; ++p)
            {
                float acc = 0.0f;
                for (int j = 0; j < 2 * K; ++j)
                    acc += base[j] * h[(size_t) p][(size_t) j];
                pk = std::max (pk, std::abs (acc));
            }
            env[i] = pk;
        }
    }

private:
    std::array<std::array<float, 2 * K>, 3> h {};
};

inline const TruePeakDetector& tpDetector()
{
    static const TruePeakDetector d;
    return d;
}

inline double truePeakdBTP (const juce::AudioBuffer<float>& buf)
{
    float peak = 0.0f;
    std::vector<float> env ((size_t) buf.getNumSamples());
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        tpDetector().envelope (buf.getReadPointer (ch), buf.getNumSamples(), env.data());
        for (float e : env) peak = std::max (peak, e);
    }
    return linToDb (peak);
}

inline void softClip (juce::AudioBuffer<float>& buf, double ceilLin, float knee)
{
    if (knee >= 0.999f) return;

    const int nCh = buf.getNumChannels(), n = buf.getNumSamples();
    constexpr int blockSize = 4096;

    juce::dsp::Oversampling<float> os ((size_t) nCh, 2,   // 2^2 = 4x
                                       juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
                                       true, true);
    os.initProcessing (blockSize);
    os.reset();

    const int latency = (int) std::ceil (os.getLatencyInSamples());
    const int total   = n + latency;

    juce::AudioBuffer<float> work (nCh, total);
    work.clear();
    for (int ch = 0; ch < nCh; ++ch)
        work.copyFrom (ch, 0, buf, ch, 0, n);

    const float c = (float) ceilLin;
    auto shape = [c, knee] (float x)
    {
        const float a = std::abs (x) / c;
        if (a <= knee) return x;
        const float y = knee + (1.0f - knee) * std::tanh ((a - knee) / (1.0f - knee));
        return std::copysign (y * c, x);
    };

    juce::dsp::AudioBlock<float> full (work);
    for (int pos = 0; pos < total; pos += blockSize)
    {
        auto sub = full.getSubBlock ((size_t) pos, (size_t) std::min (blockSize, total - pos));
        auto up  = os.processSamplesUp (sub);

        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
        {
            float* p = up.getChannelPointer (ch);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
                p[i] = shape (p[i]);
        }
        os.processSamplesDown (sub);
    }

    for (int ch = 0; ch < nCh; ++ch)
        buf.copyFrom (ch, 0, work, ch, latency, n);   // remove filter latency
}

struct LimiterSettings
{
    double lookaheadMs   = 2.0;
    double releaseFastMs = 40.0;
    double releaseSlowMs = 400.0;
    double grForSlowDb   = 6.0;
};

struct LimiterStats { double maxGRdB = 0.0, meanGRdB = 0.0; };

inline LimiterStats truePeakLimit (juce::AudioBuffer<float>& buf, double fs,
                                   double ceilLin, const LimiterSettings& s)
{
    const int n = buf.getNumSamples(), nCh = buf.getNumChannels();
    LimiterStats stats;
    if (n == 0) return stats;

    std::vector<float> peak ((size_t) n, 0.0f), env ((size_t) n);
    for (int ch = 0; ch < nCh; ++ch)
    {
        tpDetector().envelope (buf.getReadPointer (ch), n, env.data());
        for (int i = 0; i < n; ++i) peak[(size_t) i] = std::max (peak[(size_t) i], env[(size_t) i]);
    }
    for (int i = n - 1; i > 0; --i)
        peak[(size_t) i] = std::max (peak[(size_t) i], peak[(size_t) i - 1]);


    const int N   = std::max (1, (int) std::round (s.lookaheadMs * 0.001 * fs));
    const int len = n + N;
    std::vector<float> gr ((size_t) len, 1.0f);
    for (int i = 0; i < n; ++i)
        if (peak[(size_t) i] > (float) ceilLin)
            gr[(size_t) i] = (float) ceilLin / peak[(size_t) i];


    std::vector<float> m ((size_t) len);
    std::deque<int> dq;
    for (int j = 0; j < len; ++j)
    {
        while (! dq.empty() && gr[(size_t) dq.back()] >= gr[(size_t) j]) dq.pop_back();
        dq.push_back (j);
        if (dq.front() <= j - N) dq.pop_front();
        m[(size_t) j] = gr[(size_t) dq.front()];
    }
    std::vector<double> cs ((size_t) len + 1, 0.0);
    for (int j = 0; j < len; ++j) cs[(size_t) j + 1] = cs[(size_t) j] + m[(size_t) j];

    const double aFast = std::exp (-1.0 / (fs * s.releaseFastMs * 0.001));
    const double aSlow = std::exp (-1.0 / (fs * s.releaseSlowMs * 0.001));
    const double aAvg  = std::exp (-1.0 / (fs * 0.150));

    std::vector<float> gain ((size_t) n);
    double rf = 1.0, rs = 1.0, avgDb = 0.0, sumDb = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const double g = (cs[(size_t) (i + N)] - cs[(size_t) i]) / N;

        rf = g < rf ? g : aFast * rf + (1.0 - aFast) * g;
        rs = g < rs ? g : aSlow * rs + (1.0 - aSlow) * g;

        avgDb = aAvg * avgDb + (1.0 - aAvg) * (-linToDb (g));
        const double w = juce::jlimit (0.0, 1.0, avgDb / s.grForSlowDb);
        const double out = (1.0 - w) * rf + w * rs;

        gain[(size_t) i] = (float) out;

        const double grDb = -linToDb (out);
        sumDb += grDb;
        stats.maxGRdB = std::max (stats.maxGRdB, grDb);
    }
    stats.meanGRdB = sumDb / n;

    for (int ch = 0; ch < nCh; ++ch)
    {
        float* x = buf.getWritePointer (ch);
        for (int i = 0; i < n; ++i) x[i] *= gain[(size_t) i];
    }
    return stats;
}

struct Settings
{
    double targetLUFS    = -8.0;
    double ceilingdBTP   = -1.0;
    double toleranceLU   = 0.05;
    double minGainDb     = -12.0;
    double maxGainDb     = 24.0;
    float  clipKnee      = 0.75f;
    int    maxIterations = 10;
    LimiterSettings limiter;
};

struct Result
{
    double inputLUFS = 0, outputLUFS = 0, truePeakdBTP = 0, gainDb = 0;
    double maxGRdB = 0, meanGRdB = 0;
    int    iterations = 0;
    bool   reachedTarget = false;
};


inline Result maximize (juce::AudioBuffer<float>& buffer, double fs, const Settings& s)
{
    Result r;
    r.inputLUFS = integratedLUFS (buffer, fs);

    if (! std::isfinite (r.inputLUFS))
    {
        r.outputLUFS = r.inputLUFS;
        return r;
    }

    const juce::AudioBuffer<float> original (buffer);
    const int nCh = buffer.getNumChannels(), n = buffer.getNumSamples();

    constexpr double safetyMarginDb = 0.15;
    const double ceilLin = dbToLin (s.ceilingdBTP - safetyMarginDb);

    auto render = [&] (double gDb)
    {
        for (int ch = 0; ch < nCh; ++ch)
            buffer.copyFrom (ch, 0, original, ch, 0, n);

        buffer.applyGain ((float) dbToLin (gDb));
        softClip (buffer, ceilLin, s.clipKnee);
        return truePeakLimit (buffer, fs, ceilLin, s.limiter);
    };

    double gDb = std::clamp (s.targetLUFS - r.inputLUFS, s.minGainDb, s.maxGainDb);
    double lastG = gDb, prevG = 0.0, prevL = 0.0;
    double bestErr = std::numeric_limits<double>::infinity();
    bool havePrev = false;

    for (int it = 1; it <= s.maxIterations; ++it)
    {
        const auto st = render (gDb);
        lastG = gDb;

        const double measured = integratedLUFS (buffer, fs);
        const double L   = std::isfinite (measured) ? measured : -70.0;
        const double err = s.targetLUFS - L;

        if (std::abs (err) < bestErr)
        {
            bestErr = std::abs (err);
            r.outputLUFS = L;  r.gainDb = gDb;  r.iterations = it;
            r.maxGRdB = st.maxGRdB;  r.meanGRdB = st.meanGRdB;
        }
        if (std::abs (err) <= s.toleranceLU) break;

        double slope = 1.0;
        if (havePrev && std::abs (gDb - prevG) > 1e-6)
            slope = std::clamp ((L - prevL) / (gDb - prevG), 0.1, 1.5);
        prevG = gDb;  prevL = L;  havePrev = true;

        const double next = std::clamp (gDb + err / slope, s.minGainDb, s.maxGainDb);
        if (std::abs (next - gDb) < 1e-4) break;   // pinned at a gain limit
        gDb = next;
    }

    if (std::abs (lastG - r.gainDb) > 1e-9)
        render (r.gainDb);

    double tp = truePeakdBTP (buffer);
    if (tp > s.ceilingdBTP)
    {
        const double trim = s.ceilingdBTP - tp;
        buffer.applyGain ((float) dbToLin (trim));
        r.gainDb += trim;
        r.outputLUFS = integratedLUFS (buffer, fs);
        tp = truePeakdBTP (buffer);
    }

    r.truePeakdBTP  = tp;
    r.reachedTarget = std::abs (s.targetLUFS - r.outputLUFS) <= 2.0 * s.toleranceLU;
    return r;
}

} // namespace mx