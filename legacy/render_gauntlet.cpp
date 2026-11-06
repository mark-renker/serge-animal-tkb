// Offline renderer: v1 reference WAVs for the gauntlet sound bar.
// Phase 0 of the v2 rebuild (see gauntlet/GAUNTLET.md). Drives the Faust-
// generated C++ (serge_gen.cpp, faust -cn sergedsp SergeAnimalTKB.dsp)
// exactly like render.cpp does for the demos, but:
//   * writes to gauntlet/renders/v1/ (path given as argv[1])
//   * WAV gain is FIXED at 1.0 (no per-file peak normalization) so absolute
//     levels are comparable across files; hard clip guard only.
//   * every patch is an explicit parameter-event table (documented in
//     gauntlet/renders/v1/MANIFEST.md).
// Determinism: no live randomness. Faust no.noises/no.pink_noise use fixed
// seeds reset by init(); TKB random mode is never enabled here.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif

// ---- minimal Faust glue (stands in for faust/gui/*.h) ----
struct Meta { virtual ~Meta() {} virtual void declare(const char*, const char*) {} };
class UI {
public:
    virtual ~UI() {}
    virtual void openTabBox(const char*) = 0;
    virtual void openHorizontalBox(const char*) = 0;
    virtual void openVerticalBox(const char*) = 0;
    virtual void closeBox() = 0;
    virtual void addButton(const char*, FAUSTFLOAT*) = 0;
    virtual void addCheckButton(const char*, FAUSTFLOAT*) = 0;
    virtual void addVerticalSlider(const char*, FAUSTFLOAT*, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT) = 0;
    virtual void addHorizontalSlider(const char*, FAUSTFLOAT*, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT) = 0;
    virtual void addNumEntry(const char*, FAUSTFLOAT*, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT) = 0;
    virtual void addHorizontalBargraph(const char*, FAUSTFLOAT*, FAUSTFLOAT, FAUSTFLOAT) = 0;
    virtual void addVerticalBargraph(const char*, FAUSTFLOAT*, FAUSTFLOAT, FAUSTFLOAT) = 0;
    virtual void addSoundfile(const char*, const char*, void**) {}
    virtual void declare(FAUSTFLOAT*, const char*, const char*) {}
};
class dsp {
public:
    virtual ~dsp() {}
};

#include "serge_gen.cpp"

// ---- path-collecting UI ----
class MapU : public UI {
public:
    std::vector<std::string> stack;
    std::map<std::string, FAUSTFLOAT*> zones;
    std::string path(const char* l) {
        std::string p;
        for (auto& s : stack) p += "/" + s;
        return p + "/" + l;
    }
    void openTabBox(const char* l) override { stack.push_back(l); }
    void openHorizontalBox(const char* l) override { stack.push_back(l); }
    void openVerticalBox(const char* l) override { stack.push_back(l); }
    void closeBox() override { stack.pop_back(); }
    void addButton(const char* l, FAUSTFLOAT* z) override { zones[path(l)] = z; }
    void addCheckButton(const char* l, FAUSTFLOAT* z) override { zones[path(l)] = z; }
    void addVerticalSlider(const char* l, FAUSTFLOAT* z, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT) override { zones[path(l)] = z; }
    void addHorizontalSlider(const char* l, FAUSTFLOAT* z, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT) override { zones[path(l)] = z; }
    void addNumEntry(const char* l, FAUSTFLOAT* z, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT, FAUSTFLOAT) override { zones[path(l)] = z; }
    void addHorizontalBargraph(const char*, FAUSTFLOAT*, FAUSTFLOAT, FAUSTFLOAT) override {}
    void addVerticalBargraph(const char*, FAUSTFLOAT*, FAUSTFLOAT, FAUSTFLOAT) override {}
    void set(const std::string& p, float v) {
        FAUSTFLOAT* z = nullptr; int hits = 0;   // suffix match (short unambiguous paths)
        for (auto& kv : zones)
            if (kv.first.size() >= p.size() &&
                kv.first.compare(kv.first.size() - p.size(), p.size(), p) == 0) { z = kv.second; hits++; }
        if (hits != 1) { fprintf(stderr, "PARAM '%s' matched %d zones\n", p.c_str(), hits); exit(1); }
        *z = v;
    }
};

// ---- WAV writer: 16-bit stereo, FIXED unity gain (clip-guard only) ----
static void writeWav(const std::string& fn, const std::vector<float>& L, const std::vector<float>& R, int sr) {
    float peak = 0.f;
    for (size_t i = 0; i < L.size(); i++) peak = std::max(peak, std::max(std::fabs(L[i]), std::fabs(R[i])));
    FILE* f = fopen(fn.c_str(), "wb");
    if (!f) { fprintf(stderr, "cannot open %s\n", fn.c_str()); exit(1); }
    uint32_t n = (uint32_t)L.size(), dataSz = n * 4, riff = 36 + dataSz;
    uint16_t ch = 2, bits = 16, blk = 4; uint32_t byps = sr * blk;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    uint32_t fmtSz = 16; uint16_t pcm = 1;
    fwrite(&fmtSz, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f);
    fwrite(&sr, 4, 1, f); fwrite(&byps, 4, 1, f); fwrite(&blk, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dataSz, 4, 1, f);
    for (uint32_t i = 0; i < n; i++) {
        int16_t s[2] = { (int16_t)std::lround(std::max(-1.f, std::min(1.f, L[i])) * 32000),
                         (int16_t)std::lround(std::max(-1.f, std::min(1.f, R[i])) * 32000) };
        fwrite(s, 2, 2, f);
    }
    fclose(f);
    double rms = 0; bool bad = false;
    for (auto v : L) { rms += double(v)*v; if (!std::isfinite(v)) bad = true; }
    printf("  %-60s  %5.2fs  peak %.4f  rmsL %.5f%s\n", fn.c_str(), n/(double)sr, peak,
           std::sqrt(rms/std::max<size_t>(1,n)), bad ? "  **NON-FINITE**" : "");
}

struct Ev { double t; std::string p; float v; };            // param event
struct Note { double t, dur; float hz; };                    // MIDI-style note

static std::string gOutDir;

static void render(const char* name, double dur, std::vector<Ev> evs, std::vector<Note> notes, int sr = 48000) {
    sergedsp d; d.init(sr);
    MapU ui; d.buildUserInterface(&ui);
    for (auto& nt : notes) {
        evs.push_back({nt.t, "KEY/freq", nt.hz});
        evs.push_back({nt.t, "KEY/gate", 1.f});
        evs.push_back({nt.t + nt.dur, "KEY/gate", 0.f});
    }
    std::stable_sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b){ return a.t < b.t; });
    size_t ei = 0;
    const int B = 64;
    float bl[B], br[B]; float* outs[2] = { bl, br };
    long total = (long)(dur * sr);
    std::vector<float> L, R; L.reserve(total); R.reserve(total);
    for (long s = 0; s < total; s += B) {
        double t = s / (double)sr;
        while (ei < evs.size() && evs[ei].t <= t) { ui.set(evs[ei].p, evs[ei].v); ei++; }
        int nb = (int)std::min<long>(B, total - s);
        d.compute(nb, nullptr, outs);
        for (int i = 0; i < nb; i++) { L.push_back(bl[i]); R.push_back(br[i]); }
    }
    writeWav(gOutDir + "/" + name, L, R, sr);
}

int main(int argc, char** argv) {
    if (argc > 1 && !strcmp(argv[1], "--list")) {
        sergedsp d; d.init(48000); MapU ui; d.buildUserInterface(&ui);
        for (auto& kv : ui.zones) printf("%s\n", kv.first.c_str());
        return 0;
    }
    gOutDir = (argc > 1) ? argv[1] : ".";
    printf("Rendering v1 gauntlet references -> %s\n", gOutDir.c_str());

    // Shared idea: the OUT monitor bus ("OUT/MIX/monitor src") auditions any
    // matrix source at the output. Monitor path gain = monLvl*0.7*master(0.8)
    // *midiGain(0.8) = 0.448*monLvl, then dcblocker + t3 soft clip (v1 output
    // contract -- there is no way to tap a source before the output shaper).
    // monitor level 0.5 keeps the tap in the near-linear region of t3.

    // 1. NTO saw @ 1975 Hz. keytrack=1 routes KEY freq (exact Hz slider)
    //    straight to f0: achieved f0 = 1975.000 Hz (coarse/fine stay 0).
    {
        std::vector<Ev> e = {
            {0, "NTO/keytrack", 1}, {0, "KEY/freq", 1975.f},
            {0, "OUT/MIX/monitor src", 2}, {0, "OUT/MIX/monitor level", 0.5f},
        };
        render("nto_saw_1975.wav", 5.0, e, {});
    }

    // 2. NTO var out, shape CV stepped over 8 values (0..1), 1 s per step,
    //    fixed 220 Hz (keytrack + KEY freq).
    {
        std::vector<Ev> e = {
            {0, "NTO/keytrack", 1}, {0, "KEY/freq", 220.f},
            {0, "OUT/MIX/monitor src", 3}, {0, "OUT/MIX/monitor level", 0.5f},
        };
        for (int k = 0; k < 8; k++) e.push_back({(double)k, "VOICE/NTO/shape", k / 7.0f});
        render("nto_shape_sweep.wav", 8.0, e, {});
    }

    // 3. NTO sine 220 Hz -> VCM audio in (amt 0.8), bias centered (default 0),
    //    fold knob stepped over 8 values 0..1 (gm = 0.5*2^(4*fold): 0.5 -> 8),
    //    monitor VCM mid.
    {
        std::vector<Ev> e = {
            {0, "NTO/keytrack", 1}, {0, "KEY/freq", 220.f},
            {0, "PATCH/VCM/audio in/A src", 1}, {0, "PATCH/VCM/audio in/A amt", 0.8f},
            {0, "TIMBRE/VCM/bias", 0.f},
            {0, "OUT/MIX/monitor src", 15}, {0, "OUT/MIX/monitor level", 0.5f},
        };
        for (int k = 0; k < 8; k++) e.push_back({(double)k, "TIMBRE/VCM/fold", k / 7.0f});
        render("vcm_mid_foldsweep.wav", 8.0, e, {});
    }

    // 4. Same input, monitor VCM bot at full depth (fold knob max = 1.0).
    {
        std::vector<Ev> e = {
            {0, "NTO/keytrack", 1}, {0, "KEY/freq", 220.f},
            {0, "PATCH/VCM/audio in/A src", 1}, {0, "PATCH/VCM/audio in/A amt", 0.8f},
            {0, "TIMBRE/VCM/bias", 0.f}, {0, "TIMBRE/VCM/fold", 1.0f},
            {0, "OUT/MIX/monitor src", 16}, {0, "OUT/MIX/monitor level", 0.5f},
        };
        render("vcm_bot_full.wav", 5.0, e, {});
    }

    // 5. VCFQ ping: TKB clock 2 Hz -> gate -> ping trig; fc 800 Hz;
    //    Q stepped 2 -> 6 -> 15 -> 35 every 2 s; monitor VCFQ BP.
    //    Default VCFQ audio-in (VCM mid) zeroed so only pings ring.
    {
        std::vector<Ev> e = {
            {0, "TKB/clock/run", 1}, {0, "TKB/clock/rate", 2.f}, {0, "TKB/clock/gate len", 0.02f},
            {0, "PATCH/VCFQ/audio in/A amt", 0.f},
            {0, "PATCH/VCFQ/ping trig/A src", 26}, {0, "PATCH/VCFQ/ping trig/A amt", 1.f},
            {0, "TIMBRE/VCFQ/freq", 800.f}, {0, "TIMBRE/VCFQ/ping level", 0.9f},
            {0, "OUT/MIX/monitor src", 12}, {0, "OUT/MIX/monitor level", 0.9f},
            {0.0, "TIMBRE/VCFQ/Q", 2.f},
            {2.0, "TIMBRE/VCFQ/Q", 6.f},
            {4.0, "TIMBRE/VCFQ/Q", 15.f},
            {6.0, "TIMBRE/VCFQ/Q", 35.f},
        };
        render("vcfq_ping.wav", 8.0, e, {});
    }

    // 6. DUSG1 cycling at audio rate: rise=fall=2.5 ms -> ~200 Hz ramp osc.
    //    VCA1 default feeds zeroed (DSG1 out is its default CV, would leak
    //    the default voice into the mix). Monitor DSG1 out.
    {
        std::vector<Ev> e = {
            {0, "SLOPES/DUSG1/cycle", 1},
            {0, "SLOPES/DUSG1/rise", 0.0025f}, {0, "SLOPES/DUSG1/fall", 0.0025f},
            {0, "PATCH/DUSG1/trig/A amt", 0.f},
            {0, "PATCH/VCA1/audio in/A amt", 0.f},
            {0, "OUT/MIX/monitor src", 4}, {0, "OUT/MIX/monitor level", 0.5f},
        };
        render("dusg_cycle.wav", 5.0, e, {});
    }

    // 7. DUSG1 slow transient: rise 0.5 s / fall 1.0 s, triggered by KEY gate
    //    (its default trig source) at t = 0.3 / 2.4 / 4.5 s.
    //    v1 LIMITATION: the output mixer applies fi.dcblocker, so the monitor
    //    bus cannot pass a sub-audio envelope (direct tap renders at peak
    //    0.002 -- kept below as *_monitor_direct.wav for evidence). The
    //    slope SHAPE is therefore rendered the only way v1 can express it:
    //    DSG1 out -> VCA2 CV (linear mode, level 0 so gain == CV) amplitude-
    //    modulating a 1 kHz NTO sine; the WAV's amplitude envelope IS the
    //    DUSG output shape.
    {
        std::vector<Ev> e = {
            {0, "SLOPES/DUSG1/rise", 0.5f}, {0, "SLOPES/DUSG1/fall", 1.0f},
            {0, "PATCH/VCA1/audio in/A amt", 0.f},
            {0, "NTO/keytrack", 1}, {0, "KEY/freq", 1000.f},
            {0, "PATCH/VCA2/audio in/A src", 1}, {0, "PATCH/VCA2/audio in/A amt", 0.5f},
            {0, "PATCH/VCA2/CV/A src", 4}, {0, "PATCH/VCA2/CV/A amt", 1.f},
            {0, "OUT/VCA/vca2 exp", 0}, {0, "OUT/VCA/vca2 level", 0.f},
            {0, "OUT/VCA/pan2", 0.5f},
            {0.3, "KEY/gate", 1}, {0.5, "KEY/gate", 0},
            {2.4, "KEY/gate", 1}, {2.6, "KEY/gate", 0},
            {4.5, "KEY/gate", 1}, {4.7, "KEY/gate", 0},
        };
        render("dusg_env_shape.wav", 6.5, e, {});
    }
    // 7b. Supplementary: the direct monitor tap of DSG1 out for the same
    //     transients, demonstrating the dc-blocker limitation (near-silent).
    {
        std::vector<Ev> e = {
            {0, "SLOPES/DUSG1/rise", 0.5f}, {0, "SLOPES/DUSG1/fall", 1.0f},
            {0, "PATCH/VCA1/audio in/A amt", 0.f},
            {0, "OUT/MIX/monitor src", 4}, {0, "OUT/MIX/monitor level", 0.5f},
            {0.3, "KEY/gate", 1}, {0.5, "KEY/gate", 0},
            {2.4, "KEY/gate", 1}, {2.6, "KEY/gate", 0},
            {4.5, "KEY/gate", 1}, {4.7, "KEY/gate", 0},
        };
        render("dusg_env_shape_monitor_direct.wav", 6.5, e, {});
    }

    // 8. RING: NTO sin 300 Hz (X) x DSG2 cycling ~80 Hz (Y, unipolar ramp),
    //    monitor RING.
    {
        std::vector<Ev> e = {
            {0, "NTO/keytrack", 1}, {0, "KEY/freq", 300.f},
            {0, "SLOPES/DUSG2/cycle", 1},
            {0, "SLOPES/DUSG2/rise", 0.00625f}, {0, "SLOPES/DUSG2/fall", 0.00625f},
            {0, "PATCH/RING/X in/A src", 1}, {0, "PATCH/RING/X in/A amt", 1.f},
            {0, "PATCH/RING/Y in/A src", 6}, {0, "PATCH/RING/Y in/A amt", 1.f},
            {0, "OUT/MIX/monitor src", 17}, {0, "OUT/MIX/monitor level", 0.5f},
        };
        render("ring_sines.wav", 5.0, e, {});
    }

    // 9. Untouched default patch, MIDI note 60 (261.63 Hz) held 2 s at
    //    t = 0.25, then 2.25 s of release tail.
    {
        render("default_note60.wav", 4.5, {}, { {0.25, 2.0, 261.63f} });
    }

    // ---- Supplementary pairs (added 2026-11-05, sound bar round 2) ----------
    // Same toolchain, written to <dir>/supplementary/. Counterparts of the v2
    // supplementary renders so the judged pieces are real A/B pairs.
    //
    // S1. RING with a BIPOLAR ~80 Hz modulator (T09 band patch, amended
    //     2026-11-05 with Mark's approval). Round 4 (full-level modulator,
    //     same semantics as v2's patch): DSG2 out (A, amt 1) + KEY CV (B,
    //     amt 1; KEY freq 261.63*2^-2.5 Hz -> KEY CV = -0.5 = -2.5 V DC) ->
    //     VCFQ audio in (input gain 1) -> VCFQ HP (fc 20 Hz, Q 0.5; strips
    //     the DC residue) -> RING Y: a centred +-0.5 (+-2.5 V) triangle.
    //     NTO keytrack OFF (KEY now carries the offset): 300 Hz from coarse
    //     = 12*log2(300/261.63) semitones. Rounds 2-3 used the offset 0..1
    //     triangle at input gain 0.3 (sidebands 10.5 dB down).
    {
        const float offHz = (float)(261.63 * std::pow(2.0, -2.5));
        const float coarse = (float)(12.0 * std::log2(300.0 / 261.63));
        std::vector<Ev> e = {
            {0, "NTO/keytrack", 0}, {0, "NTO/coarse", coarse}, {0, "KEY/freq", offHz},
            {0, "SLOPES/DUSG2/cycle", 1},
            {0, "SLOPES/DUSG2/rise", 0.00625f}, {0, "SLOPES/DUSG2/fall", 0.00625f},
            {0, "PATCH/VCFQ/audio in/A src", 6}, {0, "PATCH/VCFQ/audio in/A amt", 1.f},
            {0, "PATCH/VCFQ/audio in/B src", 27}, {0, "PATCH/VCFQ/audio in/B amt", 1.f},
            {0, "TIMBRE/VCFQ/freq", 20.f}, {0, "TIMBRE/VCFQ/Q", 0.5f},
            {0, "TIMBRE/VCFQ/input gain", 1.0f},
            {0, "PATCH/RING/X in/A src", 1}, {0, "PATCH/RING/X in/A amt", 1.f},
            {0, "PATCH/RING/Y in/A src", 13}, {0, "PATCH/RING/Y in/A amt", 1.f},
            {0, "OUT/MIX/monitor src", 17}, {0, "OUT/MIX/monitor level", 0.35f}, // level-matched pair (full-level modulator)
        };
        render("supplementary/ring_bipolar.wav", 5.0, e, {});
    }
    // S2. DUSG1 END/GATE duty law (T05e): DUSG1 cycling, period 20 ms,
    //     rise/(rise+fall) = 5/25/50/75/95 % for 1.5 s each; DSG1 END ->
    //     monitor (src 5). Default KEY trig + VCA1 feed zeroed as in file 6.
    {
        std::vector<Ev> e = {
            {0, "SLOPES/DUSG1/cycle", 1},
            {0, "PATCH/DUSG1/trig/A amt", 0.f},
            {0, "PATCH/VCA1/audio in/A amt", 0.f},
            {0, "OUT/MIX/monitor src", 5}, {0, "OUT/MIX/monitor level", 0.5f},
        };
        const double fr[5] = { 0.05, 0.25, 0.50, 0.75, 0.95 };
        for (int k = 0; k < 5; k++) {
            e.push_back({1.5 * k, "SLOPES/DUSG1/rise", (float)(fr[k] * 0.020)});
            e.push_back({1.5 * k, "SLOPES/DUSG1/fall", (float)((1.0 - fr[k]) * 0.020)});
        }
        render("supplementary/dusg_end_duty.wav", 7.5, e, {});
    }

    printf("done.\n");
    return 0;
}
