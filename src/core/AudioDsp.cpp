#include "core/AudioDsp.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace lol {

namespace {

struct Biquad {
    double b0, b1, b2, a1, a2, z1 = 0, z2 = 0;
    double run(double x) { double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

Biquad highpass(double fc, double fs, double q = 0.7071) {
    const double w = 2 * M_PI * fc / fs, c = std::cos(w), al = std::sin(w) / (2 * q), a0 = 1 + al;
    return {(1 + c) / 2 / a0, -(1 + c) / a0, (1 + c) / 2 / a0, -2 * c / a0, (1 - al) / a0};
}

// BS.1770 K-weighting: high shelf (+4 dB @ ~1.5 kHz) then high-pass (~38 Hz), bilinear for any rate.
std::pair<Biquad, Biquad> kWeighting(double fs) {
    double G = 3.999843853973347, f0 = 1681.974450955533, Q = 0.7071752369554196;
    double K = std::tan(M_PI * f0 / fs), Vh = std::pow(10.0, G / 20.0), Vb = std::pow(Vh, 0.4996667741545416);
    double a0 = 1.0 + K / Q + K * K;
    Biquad s{(Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0};
    f0 = 38.13547087602444; Q = 0.5003270373238773; K = std::tan(M_PI * f0 / fs);
    a0 = 1.0 + K / Q + K * K;
    Biquad h{1.0 / a0, -2.0 / a0, 1.0 / a0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0};
    return {s, h};
}

std::vector<float> toFloat(const PcmClip &c) {
    std::vector<float> x(c.samples.size());
    for (size_t i = 0; i < x.size(); ++i) x[i] = float(c.samples[i]) / 32768.0f;
    return x;
}

} // namespace

PcmClip resample(const PcmClip &in, int rate) {
    if (in.rate == rate || in.samples.empty() || in.rate <= 0) { PcmClip c = in; c.rate = rate; return c; }
    PcmClip out; out.rate = rate;
    const double ratio = double(in.rate) / rate;
    const size_t n = size_t(double(in.samples.size()) / ratio);
    out.samples.resize(n);
    for (size_t i = 0; i < n; ++i) {
        double p = double(i) * ratio; size_t a = size_t(p); double f = p - double(a);
        double s0 = in.samples[std::min(a, in.samples.size() - 1)], s1 = in.samples[std::min(a + 1, in.samples.size() - 1)];
        out.samples[i] = int16_t(std::lround(s0 + (s1 - s0) * f));
    }
    return out;
}

std::pair<double, double> detectVoiceRange(const PcmClip &c) {
    if (c.samples.empty()) return {0, 0};
    const double thr = std::pow(10.0, -45.0 / 20.0) * 32768.0;
    const int win = std::max(1, c.rate / 100);   // 10 ms
    size_t first = c.samples.size(), last = 0;
    for (size_t s = 0; s < c.samples.size(); s += size_t(win)) {
        double e = 0; size_t end = std::min(c.samples.size(), s + size_t(win));
        for (size_t i = s; i < end; ++i) e += double(c.samples[i]) * c.samples[i];
        double rms = std::sqrt(e / double(end - s));
        if (rms > thr) { first = std::min(first, s); last = std::max(last, end); }
    }
    if (first >= last) return {0, c.seconds()};
    const double pad = 0.04;
    return {std::max(0.0, double(first) / c.rate - pad), std::min(c.seconds(), double(last) / c.rate + pad)};
}

double integratedLoudness(const std::vector<float> &x, int rate) {
    auto [s, h] = kWeighting(rate);
    std::vector<double> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) y[i] = h.run(s.run(x[i]));
    const size_t block = size_t(0.4 * rate), step = size_t(0.1 * rate);
    std::vector<double> powers;
    if (y.size() < block) {
        double e = 0; for (double v : y) e += v * v;
        if (!y.empty()) powers.push_back(e / double(y.size()));
    }
    for (size_t st = 0; st + block <= y.size(); st += step) {
        double e = 0; for (size_t i = st; i < st + block; ++i) e += y[i] * y[i];
        powers.push_back(e / double(block));
    }
    auto lufs = [](double p) { return -0.691 + 10.0 * std::log10(std::max(p, 1e-12)); };
    std::vector<double> gated;
    for (double p : powers) if (lufs(p) > -70.0) gated.push_back(p);
    if (gated.empty()) return -std::numeric_limits<double>::infinity();
    double mean = 0; for (double p : gated) mean += p; mean /= double(gated.size());
    const double rel = lufs(mean) - 10.0;
    double sum = 0; int n = 0;
    for (double p : gated) if (lufs(p) > rel) { sum += p; ++n; }
    return n ? lufs(sum / n) : lufs(mean);
}

PcmClip processVoice(const PcmClip &source48, const VoiceEdit &e) {
    PcmClip src = resample(source48, 48000);
    const int fs = 48000;
    std::vector<float> x = toFloat(src);
    // high-pass at 80 Hz removes rumble and DC (process_audio.py does the same)
    Biquad hp = highpass(80, fs);
    for (float &v : x) v = float(hp.run(v));
    // trim
    size_t a = size_t(std::max(0.0, e.trimStart) * fs);
    size_t b = e.trimEnd < 0 ? x.size() : size_t(e.trimEnd * fs);
    a = std::min(a, x.size()); b = std::clamp(b, a, x.size());
    std::vector<float> y(x.begin() + qsizetype(a), x.begin() + qsizetype(b));
    // loudness + gain
    double gain = std::pow(10.0, e.gainDb / 20.0);
    if (e.normalize) {
        double l = integratedLoudness(y, fs);
        if (std::isfinite(l)) gain *= std::pow(10.0, (-13.0 - l) / 20.0);
    }
    for (float &v : y) v = float(v * gain);
    // peak limiter at -1 dBFS: 5 ms look-ahead (instant attack), 80 ms release
    const float ceiling = std::pow(10.0f, -1.0f / 20.0f);
    const size_t look = size_t(0.005 * fs);
    std::vector<float> need(y.size());
    for (size_t i = 0; i < y.size(); ++i) { float m = std::fabs(y[i]); need[i] = m > ceiling ? ceiling / m : 1.0f; }
    std::vector<float> g(y.size(), 1.0f);
    {
        std::deque<size_t> dq;   // sliding minimum of need[] over [i, i+look]
        for (size_t i = 0; i < y.size() + look; ++i) {
            if (i < y.size()) { while (!dq.empty() && need[dq.back()] >= need[i]) dq.pop_back(); dq.push_back(i); }
            if (i >= look) {
                size_t o = i - look;
                while (!dq.empty() && dq.front() < o) dq.pop_front();
                if (o < y.size()) g[o] = dq.empty() ? 1.0f : need[dq.front()];
            }
        }
    }
    const float rel = 1.0f - std::exp(-1.0f / (0.08f * fs));
    float env = 1.0f;
    for (size_t i = 0; i < y.size(); ++i) {
        env = g[i] < env ? g[i] : env + (g[i] - env) * rel;
        y[i] = std::clamp(y[i] * env, -ceiling, ceiling);
    }
    // fades
    const size_t fi = std::min(y.size(), size_t(e.fadeInMs / 1000.0 * fs)), fo = std::min(y.size(), size_t(e.fadeOutMs / 1000.0 * fs));
    for (size_t i = 0; i < fi; ++i) y[i] *= float(i) / float(fi);
    for (size_t i = 0; i < fo; ++i) y[y.size() - 1 - i] *= float(i) / float(fo);
    PcmClip out; out.rate = fs; out.samples.resize(y.size());
    for (size_t i = 0; i < y.size(); ++i) out.samples[i] = int16_t(std::lround(std::clamp(y[i], -1.0f, 1.0f) * 32767.0f));
    if (out.samples.empty()) out.samples.assign(size_t(fs / 10), 0);   // never write an empty sound
    return out;
}

std::vector<float> peaks(const PcmClip &c, int buckets) {
    std::vector<float> out(size_t(std::max(1, buckets)), 0.0f);
    if (c.samples.empty()) return out;
    const double per = double(c.samples.size()) / buckets;
    for (int b = 0; b < buckets; ++b) {
        size_t s = size_t(b * per), e = std::max(s + 1, size_t((b + 1) * per));
        int m = 0;
        for (size_t i = s; i < e && i < c.samples.size(); ++i) m = std::max(m, std::abs(int(c.samples[i])));
        out[size_t(b)] = float(m) / 32768.0f;
    }
    return out;
}

} // namespace lol
