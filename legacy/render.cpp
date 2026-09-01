// Offline renderer for SergeAnimalTKB.dsp -> demo WAV files.
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
        // suffix match so demo code can use short unambiguous paths
        FAUSTFLOAT* z = nullptr; int hits = 0;
        for (auto& kv : zones)
            if (kv.first.size() >= p.size() &&
                kv.first.compare(kv.first.size() - p.size(), p.size(), p) == 0) { z = kv.second; hits++; }
        if (hits != 1) { fprintf(stderr, "PARAM '%s' matched %d zones\n", p.c_str(), hits); exit(1); }
        *z = v;
    }
};

// ---- WAV writer (16-bit stereo) ----
static void writeWav(const char* fn, const std::vector<float>& L, const std::vector<float>& R, int sr) {
    float peak = 1e-9f;
    for (size_t i = 0; i < L.size(); i++) peak = std::max(peak, std::max(std::fabs(L[i]), std::fabs(R[i])));
    float g = std::min(8.0f, 0.89f / peak);
    FILE* f = fopen(fn, "wb");
    uint32_t n = (uint32_t)L.size(), dataSz = n * 4, riff = 36 + dataSz;
    uint16_t ch = 2, bits = 16, blk = 4; uint32_t byps = sr * blk;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    uint32_t fmtSz = 16; uint16_t pcm = 1;
    fwrite(&fmtSz, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f);
    fwrite(&sr, 4, 1, f); fwrite(&byps, 4, 1, f); fwrite(&blk, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dataSz, 4, 1, f);
    for (uint32_t i = 0; i < n; i++) {
        int16_t s[2] = { (int16_t)std::lround(std::max(-1.f, std::min(1.f, L[i]*g)) * 32000),
                         (int16_t)std::lround(std::max(-1.f, std::min(1.f, R[i]*g)) * 32000) };
        fwrite(s, 2, 2, f);
    }
    fclose(f);
    double rms = 0; for (auto v : L) rms += double(v)*v;
    printf("  %-34s  %5.1fs  peak %.3f  rmsL %.4f\n", fn, n/(double)sr, peak, std::sqrt(rms/std::max<size_t>(1,n)));
}

struct Ev { double t; std::string p; float v; };            // param event
struct Note { double t, dur; float hz; };                    // MIDI-style note

static void render(const char* fn, double dur, std::vector<Ev> evs, std::vector<Note> notes, int sr = 48000) {
    sergedsp d; d.init(sr);
    MapU ui; d.buildUserInterface(&ui);
    for (auto& nt : notes) {                                  // expand notes into gate/freq events
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
    writeWav(fn, L, R, sr);
}

// convenience: set a TKB row
static void setRow(std::vector<Ev>& e, const char* row, const float* v, int n = 16) {
    char buf[64];
    for (int i = 0; i < n; i++) { snprintf(buf, 64, "Row %s/%s%d", row, row, i); e.push_back({0.0, buf, v[i]}); }
}

int main(int argc, char** argv) {
    if (argc > 1 && !strcmp(argv[1], "--list")) {
        sergedsp d; d.init(48000); MapU ui; d.buildUserInterface(&ui);
        for (auto& kv : ui.zones) printf("%s\n", kv.first.c_str());
        return 0;
    }
    printf("Rendering demos...\n");

    // ---------------------------------------------------------------
    // DEMO 1 - Default patch: MIDI melody -> NTO var -> VCM mid (fold
    // swept by cycling DUSG2) -> VCFQ LP -> VCA1 with DUSG1 envelope.
    {
        std::vector<Ev> e = {
            {0, "VCM/fold", 0.15f},
            {0, "PATCH/NTO/shape A", 6}, {0, "PATCH/NTO/shape A amt", 0.35f},
            {0, "DUSG2/cycle", 1}, {0, "DUSG2/rise", 0.9f}, {0, "DUSG2/fall", 0.9f},
            {0, "PATCH/VCM/fold CV A", 6}, {0, "PATCH/VCM/fold CV A amt", 0.45f},
            {0, "VCFQ/freq", 2200.f}, {0, "VCFQ/Q", 3.f},
            {0, "DUSG1/fall", 0.5f},
            {5.5, "NTO/shape", 0.6f},
        };
        std::vector<Note> n;
        float seq[8] = {130.81f, 155.56f, 196.f, 233.08f, 261.63f, 196.f, 311.13f, 233.08f};
        for (int i = 0; i < 16; i++) n.push_back({0.2 + i*0.55, 0.28, seq[i % 8]});
        render("/mnt/user-data/outputs/demo1_default_voice.wav", 10.0, e, n);
    }

    // ---------------------------------------------------------------
    // DEMO 2 - VCFQ ring/ping percussion: TKB gate strikes the filter,
    // row B tunes it per step, RVG randomizes Q. No oscillator at all.
    {
        std::vector<Ev> e = {
            {0, "KEY/TKB mode", 1},
            {0, "TKB/clock/run", 1}, {0, "TKB/clock/rate", 7.f}, {0, "TKB/clock/length", 8},
            {0, "PATCH/VCA1/audio in A amt", 0.f},
            {0, "PATCH/VCFQ/ping trig A", 26}, {0, "PATCH/VCFQ/ping trig A amt", 1.f},
            {0, "PATCH/VCFQ/freq CV A", 23}, {0, "PATCH/VCFQ/freq CV A amt", 0.5f},
            {0, "PATCH/VCFQ/Q CV A", 20}, {0, "PATCH/VCFQ/Q CV A amt", 0.25f},
            {0, "VCFQ/freq", 300.f}, {0, "VCFQ/Q", 18.f}, {0, "VCFQ/ping level", 0.9f},
            {0, "PATCH/VCFQ/audio in A amt", 0.f},
            {0, "OUT/monitor src", 11}, {0, "OUT/monitor level", 0.9f},
            {0, "RING-NOISE/RVG rate", 3.f},
            {6.0, "VCFQ/Q", 32.f},
        };
        float rB[16] = {0.05f, 0.5f, 0.25f, 0.8f, 0.12f, 0.6f, 0.35f, 0.95f, 0,0,0,0,0,0,0,0};
        setRow(e, "B", rB);
        render("/mnt/user-data/outputs/demo2_vcfq_pings.wav", 12.0, e, {});
    }

    // ---------------------------------------------------------------
    // DEMO 3 - "Serge bongos": DUSG1 cycles at audio rate (the classic
    // slope-as-oscillator trick), TKB row A shifts its rise/fall times
    // for pitch, DUSG2 transients strike it through VCA2.
    {
        std::vector<Ev> e = {
            {0, "KEY/TKB mode", 1},
            {0, "TKB/clock/run", 1}, {0, "TKB/clock/rate", 6.f}, {0, "TKB/clock/length", 16},
            {0, "TKB/clock/gate len", 0.02f},
            {0, "PATCH/VCA1/audio in A amt", 0.f},
            {0, "PATCH/DUSG1/trig A amt", 0.f},
            {0, "DUSG1/cycle", 1}, {0, "DUSG1/rise", 0.004f}, {0, "DUSG1/fall", 0.004f},
            {0, "PATCH/DUSG1/rise CV A", 22}, {0, "PATCH/DUSG1/rise CV A amt", 0.35f},
            {0, "PATCH/DUSG1/fall CV A", 22}, {0, "PATCH/DUSG1/fall CV A amt", 0.35f},
            {0, "PATCH/DUSG2/trig A", 26}, {0, "PATCH/DUSG2/trig A amt", 1.f},
            {0, "DUSG2/rise", 0.001f}, {0, "DUSG2/fall", 0.09f},
            {0, "PATCH/VCA2/audio in A", 4}, {0, "PATCH/VCA2/audio in A amt", 1.f},
            {0, "PATCH/VCA2/CV A", 6}, {0, "PATCH/VCA2/CV A amt", 1.f},
            {0, "OUT/vca2 exp", 1},
        };
        float rA[16] = {0.2f, 0.2f, 0.55f, 0.2f, 0.75f, 0.35f, 0.2f, 0.9f,
                        0.2f, 0.55f, 0.2f, 0.35f, 0.65f, 0.2f, 0.85f, 0.45f};
        setRow(e, "A", rA);
        render("/mnt/user-data/outputs/demo3_dusg_bongos.wav", 11.0, e, {});
    }

    // ---------------------------------------------------------------
    // DEMO 4 - Ring-mod wave-multiplier drone: NTO x DUSG2 slope-osc
    // through the bottom folder, fold and filter swept by smoothed random.
    {
        std::vector<Ev> e = {
            {0, "KEY/gate", 1}, {0, "KEY/freq", 65.41f},
            {0, "PATCH/VCA1/audio in A amt", 0.f},
            {0, "PATCH/DUSG1/trig A amt", 0.f},
            {0, "DUSG2/cycle", 1}, {0, "DUSG2/rise", 0.0051f}, {0, "DUSG2/fall", 0.0051f},
            {0, "PATCH/RING/X in A", 3}, {0, "PATCH/RING/X in A amt", 1.f},
            {0, "PATCH/RING/Y in A", 6}, {0, "PATCH/RING/Y in A amt", 1.f},
            {0, "PATCH/VCM/audio in A", 17}, {0, "PATCH/VCM/audio in A amt", 0.8f},
            {0, "PATCH/VCM/fold CV A", 21}, {0, "PATCH/VCM/fold CV A amt", 0.6f},
            {0, "RING-NOISE/RVG rate", 0.3f},
            {0, "VCM/fold", 0.35f},
            {0, "PATCH/NTO/pitch FM A", 21}, {0, "PATCH/NTO/pitch FM A amt", 0.04f},
            {0, "PATCH/VCA1/audio in B", 16}, {0, "PATCH/VCA1/audio in B amt", 0.7f},
            {0, "OUT/vca1 level", 1.f}, {0, "OUT/vca1 exp", 0},
            {0, "PATCH/VCFQ/audio in A", 16}, {0, "PATCH/VCFQ/audio in A amt", 0.6f},
            {0, "VCFQ/freq", 500.f}, {0, "VCFQ/Q", 4.f},
            {0, "PATCH/VCA2/audio in A", 11}, {0, "PATCH/VCA2/audio in A amt", 0.8f},
            {0, "OUT/vca2 level", 0.8f},
            {0, "PATCH/VCFQ/freq CV A", 21}, {0, "PATCH/VCFQ/freq CV A amt", 0.35f},
            {10.0, "VCM/fold", 0.6f},
        };
        render("/mnt/user-data/outputs/demo4_ring_fold_drone.wav", 16.0, e, {});
    }

    // ---------------------------------------------------------------
    // DEMO 5 - SSG random melody: noise -> stepped (clocked by TKB) ->
    // NTO pitch; smooth section slews a copy into the filter cutoff.
    {
        std::vector<Ev> e = {
            {0, "TKB/clock/run", 1}, {0, "TKB/clock/rate", 4.f},
            {0, "KEY/gate", 0}, {0, "KEY/freq", 130.81f},
            {0, "PATCH/SSG/stepped in A", 18}, {0, "PATCH/SSG/stepped in A amt", 1.f},
            {0, "PATCH/SSG/stepped clk A", 26}, {0, "PATCH/SSG/stepped clk A amt", 1.f},
            {0, "PATCH/SSG/smooth in A", 9}, {0, "PATCH/SSG/smooth in A amt", 1.f},
            {0, "SSG/smooth rate", 0.12f},
            {0, "PATCH/NTO/pitch FM A", 9}, {0, "PATCH/NTO/pitch FM A amt", 0.45f},
            {0, "PATCH/DUSG1/trig A", 26},
            {0, "DUSG1/fall", 0.35f},
            {0, "PATCH/VCFQ/freq CV A", 8}, {0, "PATCH/VCFQ/freq CV A amt", 0.4f},
            {0, "VCFQ/freq", 900.f}, {0, "VCFQ/Q", 6.f},
            {0, "NTO/shape", 0.55f}, {0, "VCM/fold", 0.2f},
        };
        render("/mnt/user-data/outputs/demo5_ssg_random_melody.wav", 14.0, e, {});
    }

    // ---------------------------------------------------------------
    // DEMO 6 - "Wild animal" feedback: VCFQ BP -> folder -> ring mod
    // (against the NTO) -> back into the filter; SSG-slewed random
    // voltages steer the scream. Legal feedback through the matrix.
    {
        std::vector<Ev> e = {
            {0, "KEY/gate", 1}, {0, "KEY/freq", 98.f},
            {0, "PATCH/VCA1/audio in A amt", 0.f},
            {0, "PATCH/DUSG1/trig A amt", 0.f},
            {0, "PATCH/VCM/audio in A", 12}, {0, "PATCH/VCM/audio in A amt", 0.9f},
            {0, "PATCH/VCM/audio in B", 3}, {0, "PATCH/VCM/audio in B amt", 0.5f},
            {0, "PATCH/RING/X in A", 15}, {0, "PATCH/RING/X in A amt", 1.f},
            {0, "PATCH/RING/Y in A", 1}, {0, "PATCH/RING/Y in A amt", 1.f},
            {0, "PATCH/VCFQ/audio in A", 17}, {0, "PATCH/VCFQ/audio in A amt", 0.85f},
            {0, "VCFQ/freq", 400.f}, {0, "VCFQ/Q", 9.f},
            {0, "PATCH/VCFQ/freq CV A", 8}, {0, "PATCH/VCFQ/freq CV A amt", 0.55f},
            {0, "PATCH/SSG/stepped in A", 18}, {0, "PATCH/SSG/stepped in A amt", 1.f},
            {0, "PATCH/SSG/stepped clk A", 5}, {0, "PATCH/SSG/stepped clk A amt", 1.f},
            {0, "PATCH/SSG/smooth in A", 9}, {0, "PATCH/SSG/smooth in A amt", 1.f},
            {0, "SSG/smooth rate", 0.3f},
            {0, "DUSG1/cycle", 1}, {0, "DUSG1/rise", 0.4f}, {0, "DUSG1/fall", 0.4f},
            {0, "VCM/fold", 0.5f},
            {0, "PATCH/VCA1/audio in B", 12}, {0, "PATCH/VCA1/audio in B amt", 0.8f},
            {0, "OUT/vca1 level", 0.9f},
            {0, "PATCH/VCA2/audio in A", 17}, {0, "PATCH/VCA2/audio in A amt", 0.5f},
            {0, "OUT/vca2 level", 0.7f},
            {8.0, "VCM/fold", 0.8f},
            {12.0, "VCFQ/Q", 20.f},
        };
        render("/mnt/user-data/outputs/demo6_wild_animal_feedback.wav", 16.0, e, {});
    }

    printf("done.\n");
    return 0;
}
