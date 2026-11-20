#pragma once
// Voice line processing: high-pass, silence trim, loudness to -13 LUFS,
// peak limit, fades. All on mono 16-bit PCM.
#include "core/Wwise.h"

namespace lol {

struct VoiceEdit {
    double trimStart = 0;     // seconds into the source
    double trimEnd = -1;      // seconds; <0 = end of source
    double gainDb = 0;        // after loudness normalisation
    double fadeInMs = 15;
    double fadeOutMs = 60;
    bool normalize = true;    // integrated loudness -> -13 LUFS
};

PcmClip resample(const PcmClip &in, int rate);
// Auto trim: [start, end) seconds where the signal is above -45 dBFS, with a little padding.
std::pair<double, double> detectVoiceRange(const PcmClip &c);
double integratedLoudness(const std::vector<float> &x, int rate);    // LUFS (BS.1770), -inf for silence
PcmClip processVoice(const PcmClip &source, const VoiceEdit &e);
std::vector<float> peaks(const PcmClip &c, int buckets);              // 0..1 per bucket, for waveform drawing

} // namespace lol
