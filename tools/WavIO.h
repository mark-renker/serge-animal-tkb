// Serge Animal TKB v2 — sound-bar evidence tools: 16-bit PCM WAV I/O.
// Shared by satk_render (tools/RenderGauntlet.cpp) and satk_measure
// (tools/MeasureSound.cpp). Pure C++17, no JUCE.
//
// Sample convention matches legacy/render_gauntlet.cpp exactly (v1 reference
// renders): float in [-1, 1] is clip-guarded and written as round(x * 32000),
// so a v1 and a v2 sample of the same float value produce the same integer.
// The reader inverts that (int / 32000).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace wavio {

constexpr double kScale = 32000.0; // v1 writer convention (not 32767)

struct Stereo
{
    int sr = 0;
    std::vector<float> L, R;
};

inline bool write16 (const std::string& fn, const std::vector<float>& L,
                     const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (fn.c_str(), "wb");
    if (! f) return false;
    const uint32_t n = (uint32_t) L.size(), dataSz = n * 4, riff = 36 + dataSz;
    const uint16_t ch = 2, bits = 16, blk = 4, pcm = 1;
    const uint32_t byps = (uint32_t) sr * blk, fmtSz = 16, usr = (uint32_t) sr;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f); std::fwrite ("WAVEfmt ", 1, 8, f);
    std::fwrite (&fmtSz, 4, 1, f); std::fwrite (&pcm, 2, 1, f); std::fwrite (&ch, 2, 1, f);
    std::fwrite (&usr, 4, 1, f); std::fwrite (&byps, 4, 1, f); std::fwrite (&blk, 2, 1, f);
    std::fwrite (&bits, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&dataSz, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i)
    {
        const int16_t s[2] = {
            (int16_t) std::lround (std::clamp (L[i], -1.0f, 1.0f) * kScale),
            (int16_t) std::lround (std::clamp (R[i], -1.0f, 1.0f) * kScale) };
        std::fwrite (s, 2, 2, f);
    }
    std::fclose (f);
    return true;
}

// Reads 16-bit PCM mono/stereo (mono is duplicated to R). Returns false on
// anything else.
inline bool read16 (const std::string& fn, Stereo& out)
{
    FILE* f = std::fopen (fn.c_str(), "rb");
    if (! f) return false;
    std::vector<unsigned char> b;
    unsigned char buf[65536];
    size_t got;
    while ((got = std::fread (buf, 1, sizeof (buf), f)) > 0) b.insert (b.end(), buf, buf + got);
    std::fclose (f);
    auto u32 = [&b] (size_t o) { uint32_t v; std::memcpy (&v, &b[o], 4); return v; };
    auto u16 = [&b] (size_t o) { uint16_t v; std::memcpy (&v, &b[o], 2); return v; };
    if (b.size() < 12 || std::memcmp (&b[0], "RIFF", 4) != 0 || std::memcmp (&b[8], "WAVE", 4) != 0)
        return false;
    int channels = 0, bits = 0;
    size_t pos = 12;
    while (pos + 8 <= b.size())
    {
        const uint32_t sz = u32 (pos + 4);
        if (std::memcmp (&b[pos], "fmt ", 4) == 0)
        {
            if (u16 (pos + 8) != 1) return false; // PCM only
            channels = u16 (pos + 10);
            out.sr = (int) u32 (pos + 12);
            bits = u16 (pos + 22);
        }
        else if (std::memcmp (&b[pos], "data", 4) == 0)
        {
            if (bits != 16 || (channels != 1 && channels != 2)) return false;
            const size_t frames = std::min<size_t> (sz, b.size() - pos - 8) / (size_t) (2 * channels);
            out.L.resize (frames); out.R.resize (frames);
            for (size_t i = 0; i < frames; ++i)
            {
                int16_t l, r;
                std::memcpy (&l, &b[pos + 8 + i * (size_t) (2 * channels)], 2);
                r = l;
                if (channels == 2) std::memcpy (&r, &b[pos + 8 + i * 4 + 2], 2);
                out.L[i] = (float) (l / kScale);
                out.R[i] = (float) (r / kScale);
            }
            return true;
        }
        pos += 8 + sz + (sz & 1u);
    }
    return false;
}

inline double rms (const std::vector<float>& x)
{
    double s = 0.0;
    for (float v : x) s += (double) v * v;
    return std::sqrt (s / (double) std::max<size_t> (1, x.size()));
}
inline double peak (const std::vector<float>& x)
{
    double p = 0.0;
    for (float v : x) p = std::max (p, (double) std::fabs (v));
    return p;
}

} // namespace wavio
