// Serge Animal TKB v2 — sound-bar renderer (satk_render). Pure C++17, no JUCE.
//
// Renders the v2 equivalent of each of the ten v1 reference patches in
// gauntlet/renders/v1/MANIFEST.md into gauntlet/renders/v2/<same filename>,
// 48 kHz 16-bit stereo, fixed gain (writer identical to the v1 one in
// legacy/render_gauntlet.cpp: clip-guard, x * 32000, no normalisation).
// Drives satk::System exactly like the gauntlet tests do (ctls poked
// directly, cables via sys.patch, KEY fields written per sample, the WAV is
// OUT's outL/outR — i.e. everything goes through the real v2 OUT mixer and
// monitor bus).
//
// Also renders
//   * v2-only supplementary patches the v1 file set cannot express
//     (gauntlet/renders/v2/supplementary/), and
//   * the blind-critic NEGATIVE CONTROL (gauntlet/renders/control/): a
//     deliberately degraded NTO saw, routed through the same v2 OUT bus.
//
// Determinism: fixed instance seed (kDefaultInstanceSeed) per file; ANALOG
// DRIFT is set explicitly per file and printed. Re-running reproduces
// byte-identical WAVs.
//
// Level convention (v1 monitor tap = src x monLvl x 0.7 x master 0.8 x vel
// 0.8 -> DC block -> soft clip). v2's OUT mixer has the same law (SPEC
// §4.12) with kAudio = 2.5 V == 1.0 FS. v1's +-1 unit audio == v2's +-2.5 V
// audio, so monitor renders use MASTER 0.8 (v1's master; v2's default is
// 0.53) and v1's monitor level; 0..5 V CV sources (2x the v1 swing in FS
// terms) use half v1's monitor level. Exceptions are listed per file.
//
// Usage: satk_render <repo>/gauntlet/renders
#include "../tests/t_nto.h"      // freqKnob, zcFreq
#include "../tests/t_vcfq.h"     // qKnob
#include "../tests/t_dusg_ssg.h" // dial (DUSG expo time map)
#include "WavIO.h"
#include <filesystem>
#include <functional>
#include <utility>

namespace {

using namespace satk;
constexpr int kSR = 48000;
constexpr uint64_t kSeed = kDefaultInstanceSeed;
constexpr float kBotAbsMonLvl = 0.59f;  // r4 control: single naive abs, level-matched to the vcm_bot pair
constexpr float kRingMonLvl = 0.35f;    // ring_sines, level-matched to v1 RMS (r2: 0.4 -> 0.35, linear core)
constexpr float kRingBipMonLvl = 0.31f; // ring_bipolar/morph, level-matched to v1's bipolar render (r4 full-level modulator; r2/r3 1.0)

struct Ev { long at; std::function<void()> fn; };

struct Job
{
    System sys;
    std::vector<Ev> ev;
    std::function<void (long)> perSample;   // external inputs (KEY fields, injected sources)
    const float* probe = nullptr;           // jack to report achieved f0 from
    double probeFrom = 0.5, probeTo = 4.5;  // probe window (s)

    Job() { sys.engine.setSeed (kSeed); }
    void at (double t, std::function<void()> fn) { ev.push_back ({ std::lround (t * kSR), std::move (fn) }); }
    // monitor-bus audition: v1 conventions (see header)
    void monitor (int src, float lvl, float master = 0.8f)
    {
        sys.out->ctls[OUT::cMonSrc].v = (float) src;
        sys.out->ctls[OUT::cMonLvl].v = lvl;
        sys.out->ctls[OUT::cMaster].v = master;
    }
    void drift (float d) { sys.out->ctls[OUT::cDrift].v = d; }
    void ntoHz (double hz) { sys.nto->ctls[NTO::cFreq].v = t_nto::freqKnob (hz); }
};

std::string gDir;

void render (Job& j, const std::string& rel, double dur)
{
    j.sys.prepare (kSR); // applies the seed, snaps knob smoothers to the patch
    std::stable_sort (j.ev.begin(), j.ev.end(), [] (const Ev& a, const Ev& b) { return a.at < b.at; });
    const long n = std::lround (dur * kSR);
    std::vector<float> L ((size_t) n), R ((size_t) n), pr;
    size_t ei = 0;
    bool finite = true;
    for (long i = 0; i < n; ++i)
    {
        while (ei < j.ev.size() && j.ev[ei].at <= i) j.ev[ei++].fn();
        if (j.perSample) j.perSample (i);
        j.sys.engine.tick();
        L[(size_t) i] = j.sys.out->outL;
        R[(size_t) i] = j.sys.out->outR;
        if (! std::isfinite (L[(size_t) i]) || ! std::isfinite (R[(size_t) i])) finite = false;
        if (j.probe && i >= std::lround (j.probeFrom * kSR) && i < std::lround (j.probeTo * kSR))
            pr.push_back (*j.probe);
    }
    const std::string fn = gDir + "/" + rel;
    std::filesystem::create_directories (std::filesystem::path (fn).parent_path());
    if (! wavio::write16 (fn, L, R, kSR)) { std::fprintf (stderr, "cannot write %s\n", fn.c_str()); std::exit (1); }
    std::printf ("  %-52s %5.2fs peak %.4f rmsL %.5f rmsR %.5f drift %.2f",
                 rel.c_str(), dur, std::max (wavio::peak (L), wavio::peak (R)), wavio::rms (L),
                 wavio::rms (R), (double) j.sys.out->ctls[OUT::cDrift].v);
    if (! pr.empty()) std::printf ("  f0 %.3f Hz", t_nto::zcFreq (pr));
    std::printf ("%s\n", finite ? "" : "  **NON-FINITE**");
}

// ---- shared patch fragments -------------------------------------------------
// v1's VCM feed was "NTO sin, amt 0.8". v2 banana cables have no attenuator,
// so the 0.8 depth is a VCA in LIN mode with LEVEL 0.8 (gain exactly 0.8, its
// soft-sat contributes a symmetric 3rd harmonic ~ -46 dBc at 2 V).
void vcmFeed (Job& j, int vcmIn)
{
    j.ntoHz (220.0);
    j.sys.vca1->ctls[VCA::cResponse].v = 0.0f; // LIN
    j.sys.vca1->ctls[VCA::cLevel].v = 0.8f;    // gain 0.8
    j.sys.patch (j.sys.nto, NTO::jSine, j.sys.vca1, VCA::jIn);
    j.sys.patch (j.sys.vca1, VCA::jOut, j.sys.vcm, vcmIn);
    j.probe = &j.sys.nto->jacks[NTO::jSine].value;
}

void keyPulses (Job& j, std::initializer_list<double> onsets, double len)
{
    std::vector<std::pair<long, long>> w;
    for (double t : onsets) w.push_back ({ std::lround (t * kSR), std::lround ((t + len) * kSR) });
    KEY* key = j.sys.key;
    j.perSample = [key, w] (long i)
    {
        bool hi = false;
        for (auto& p : w) hi |= i >= p.first && i < p.second;
        key->gateV = hi ? kCV : 0.0f;
    };
}

// =============================================================================
void renderV2()
{
    std::printf ("v2 equivalents of the v1 reference set -> %s/v2\n", gDir.c_str());

    { // 1. NTO SAW -> monitor, 1975 Hz
        Job j; j.drift (0.0f); j.ntoHz (1975.0);
        j.monitor (2, 0.5f);
        j.probe = &j.sys.nto->jacks[NTO::jSine].value;
        render (j, "v2/nto_saw_1975.wav", 5.0);
    }
    { // 2. NTO VAR -> monitor, WAVESHAPE stepped k/7 at t = k s, 220 Hz
        Job j; j.drift (0.0f); j.ntoHz (220.0);
        j.monitor (3, 0.375f); // level-matched (convention 0.5 -> +2.5 dB vs v1 RMS)
        NTO* n = j.sys.nto;
        for (int k = 0; k < 8; ++k) j.at (k, [n, k] { n->ctls[NTO::cWave].v = (float) k / 7.0f; });
        j.probe = &n->jacks[NTO::jSine].value; j.probeTo = 7.5;
        render (j, "v2/nto_shape_sweep.wav", 8.0);
    }
    { // 3. NTO SINE -> (VCA 0.8) -> VCM MID IN 1, BIAS centred, FOLD stepped k/7
        Job j; j.drift (0.0f); vcmFeed (j, VCM::jMidIn1);
        j.monitor (15, 0.48f); // level-matched (r4 peak/RMS makeup; r2/r3 0.41, convention 0.5)
        VCM* v = j.sys.vcm;
        v->ctls[VCM::cBias].v = 0.5f;
        for (int k = 0; k < 8; ++k) j.at (k, [v, k] { v->ctls[VCM::cFold].v = (float) k / 7.0f; });
        j.probeTo = 7.5;
        render (j, "v2/vcm_mid_foldsweep.wav", 8.0);
    }
    { // 4. NTO SINE -> (VCA 0.8) -> VCM BOT IN 1, BOT DEPTH full
        Job j; j.drift (0.0f); vcmFeed (j, VCM::jBotIn1);
        j.sys.vcm->ctls[VCM::cBotDepth].v = 1.0f;
        j.monitor (16, 0.52f); // level-matched (r2 cascade; r1 used 0.6)
        render (j, "v2/vcm_bot_full.wav", 5.0);
    }
    { // 5. TKB GATE (internal clock 2 Hz, GATE LEN 20 ms) -> VCFQ TRIG; fc 800 Hz;
      //    PING 0.9; Q 2 / 6 / 15 / 35 every 2 s; VCFQ BP -> monitor @ 0.9
        Job j; j.drift (0.0f);
        TKB* t = j.sys.tkb; VCFQ* f = j.sys.vcfq;
        t->ctls[TKB::cRun].v = 1.0f;
        t->ctls[TKB::cRate].v = (float) (std::log (2.0 / 0.1) / std::log (300.0));    // 2 Hz
        t->ctls[TKB::cGateLen].v = (float) (std::log (0.02 / 0.005) / std::log (200.0)); // 20 ms
        j.sys.patch (t, TKB::jGate, f, VCFQ::jTrig);
        f->ctls[VCFQ::cFreq].v = (float) (std::log (800.0 / 20.0) / std::log (600.0));
        f->ctls[VCFQ::cPing].v = 0.9f;
        const double qs[4] = { 2.0, 6.0, 15.0, 35.0 };
        for (int k = 0; k < 4; ++k)
        { const float qk = t_vcfq::qKnob (qs[k]); j.at (2.0 * k, [f, qk] { f->ctls[VCFQ::cQ].v = qk; }); }
        j.monitor (12, 0.15f); // level-matched: v2 ping is ~16 dB hotter than v1 at 0.9
        render (j, "v2/vcfq_ping.wav", 8.0);
    }
    { // 6. DUSG1 CYCLE, RISE = FALL = 2.5 ms; DSG1 OUT -> monitor (CV source)
        Job j; j.drift (0.0f);
        DUSG* d = j.sys.dusg1;
        d->ctls[DUSG::cCycle].v = 1.0f;
        d->ctls[DUSG::cRise].v = t_dusg_ssg::dial (0.0025);
        d->ctls[DUSG::cFall].v = t_dusg_ssg::dial (0.0025);
        j.monitor (4, 0.25f);
        j.probe = &j.sys.out->outL;
        render (j, "v2/dusg_cycle.wav", 5.0);
    }
    { // 7. KEY GATE -> DUSG1 TRIG (pulses 0.3/2.4/4.5 s, 0.2 s); RISE 0.5 s
      //    FALL 1.0 s; DSG1 OUT -> VCA2 VC (LIN, LEVEL 0: gain == env/5 V);
      //    NTO SINE 1 kHz -> VCA2 IN; VCA2 -> OUT IN 2, PAN 2 = 0.5.
      //    MASTER 0.4 = v1 master 0.8 x v1's carrier amt 0.5 (no attenuator
      //    on a v2 cable): same audition level as v1's patch.
        Job j; j.drift (0.0f); j.ntoHz (1000.0);
        DUSG* d = j.sys.dusg1;
        d->ctls[DUSG::cRise].v = t_dusg_ssg::dial (0.5);
        d->ctls[DUSG::cFall].v = t_dusg_ssg::dial (1.0);
        j.sys.patch (j.sys.key, KEY::jGate, d, DUSG::jTrig);
        j.sys.patch (d, DUSG::jOut, j.sys.vca2, VCA::jVc);
        j.sys.patch (j.sys.nto, NTO::jSine, j.sys.vca2, VCA::jIn);
        j.sys.patch (j.sys.vca2, VCA::jOut, j.sys.out, OUT::jIn2);
        j.sys.vca2->ctls[VCA::cResponse].v = 0.0f;
        j.sys.vca2->ctls[VCA::cLevel].v = 0.0f;
        j.sys.out->ctls[OUT::cPan2].v = 0.5f;
        j.sys.out->ctls[OUT::cMaster].v = 0.4f;
        keyPulses (j, { 0.3, 2.4, 4.5 }, 0.2);
        j.probe = &j.sys.nto->jacks[NTO::jSine].value; j.probeTo = 6.0;
        render (j, "v2/dusg_env_shape.wav", 6.5);
    }
    { // 7b. same transients, DSG1 OUT straight to the monitor (CV source)
        Job j; j.drift (0.0f);
        DUSG* d = j.sys.dusg1;
        d->ctls[DUSG::cRise].v = t_dusg_ssg::dial (0.5);
        d->ctls[DUSG::cFall].v = t_dusg_ssg::dial (1.0);
        j.sys.patch (j.sys.key, KEY::jGate, d, DUSG::jTrig);
        j.monitor (4, 0.25f);
        keyPulses (j, { 0.3, 2.4, 4.5 }, 0.2);
        render (j, "v2/dusg_env_shape_monitor_direct.wav", 6.5);
    }
    { // 8. NTO SINE 300 Hz -> RING X; DUSG2 CYCLE 6.25/6.25 ms (~80 Hz,
      //    unipolar 0..5 V, exactly the v1 modulator) -> RING Y; RING -> monitor
        Job j; j.drift (0.0f); j.ntoHz (300.0);
        DUSG* d = j.sys.dusg2;
        d->ctls[DUSG::cCycle].v = 1.0f;
        d->ctls[DUSG::cRise].v = t_dusg_ssg::dial (0.00625);
        d->ctls[DUSG::cFall].v = t_dusg_ssg::dial (0.00625);
        j.sys.patch (j.sys.nto, NTO::jSine, j.sys.ring, RING::jX);
        j.sys.patch (d, DUSG::jOut, j.sys.ring, RING::jY);
        j.monitor (17, kRingMonLvl);
        j.probe = &j.sys.nto->jacks[NTO::jSine].value;
        render (j, "v2/ring_sines.wav", 5.0);
    }
    { // 9. untouched default patch (default cables, default ctls incl. ANALOG
      //    DRIFT 0.2 and MASTER 0.53), MIDI note 60 vel 100 at 0.25 s for 2 s.
        Job j;
        j.sys.patchDefaults();
        KEY* key = j.sys.key;
        const long on = std::lround (0.25 * kSR), off = std::lround (2.25 * kSR);
        j.perSample = [key, on, off] (long i)
        {
            if (i == on) { key->pitchV = 0.0f; key->velV = 100.0f / 127.0f * kCV; }
            key->gateV = (i >= on && i < off) ? kCV : 0.0f;
        };
        j.probe = &j.sys.nto->jacks[NTO::jSine].value;
        render (j, "v2/default_note60.wav", 4.5);
    }
}

// Bipolar ~80 Hz modulator (round 4): the DUSG2 0..5 V triangle is shifted
// by an exact -2.5 V DC (KEY KV held at -2.5 V, i.e. a key 2.5 octaves
// below C4: two cables stacked into VCFQ IN) so the coupler sees a
// SYMMETRIC +-2.5 V swing, then VCFQ HP (fc 20 Hz, Q 0.5, GAIN 1x) strips
// whatever DC residue the triangle's own mean leaves -> RING Y.
// Full-level modulator (+-2.5 V, u = +-1). Rounds 2-3 ran the offset 0..5 V
// triangle into the coupler at GAIN 0.3x: an offset swing bends into its
// own 2nd harmonic (-> X +- 2fY products) unless attenuated, and the 0.3x
// left the sidebands 10.5 dB down against RING's fixed X bleed. Centred, the
// input stage's curvature is odd-only (a triangle's own odd series: no
// even products), so unity gain is clean. Same semantics in v1's render
// (KEY CV -0.5 = -2.5 V on the VCFQ audio-in B slot, input gain 1).
constexpr float kBipOffsetV = -2.5f;
void bipolarRing (Job& j)
{
    j.drift (0.0f); j.ntoHz (300.0);
    DUSG* d = j.sys.dusg2;
    d->ctls[DUSG::cCycle].v = 1.0f;
    d->ctls[DUSG::cRise].v = t_dusg_ssg::dial (0.00625);
    d->ctls[DUSG::cFall].v = t_dusg_ssg::dial (0.00625);
    j.sys.key->pitchV = kBipOffsetV;                  // KEY KV = -2.5 V DC
    VCFQ* f = j.sys.vcfq;
    f->ctls[VCFQ::cFreq].v = 0.0f;                    // 20 Hz
    f->ctls[VCFQ::cQ].v = t_vcfq::qKnob (0.5);
    f->ctls[VCFQ::cGain].v = 0.5f;                    // 1x (unity)
    j.sys.patch (d, DUSG::jOut, f, VCFQ::jIn);
    j.sys.patch (j.sys.key, KEY::jKv, f, VCFQ::jIn);  // stacked: triangle - 2.5 V
    j.sys.patch (f, VCFQ::jHp, j.sys.ring, RING::jY);
    j.sys.patch (j.sys.nto, NTO::jSine, j.sys.ring, RING::jX);
    j.monitor (17, kRingBipMonLvl);
}

// Supplementary patches (v1 counterparts of ring_bipolar / dusg_end_duty are
// rendered by legacy/render_gauntlet.cpp into v1/supplementary/ since round 2;
// ring_morph_sweep stays v2-only: v1 has no morph control).
void renderSupplementary()
{
    std::printf ("v2-only supplementary -> %s/v2/supplementary\n", gDir.c_str());
    { // S1. RING with a bipolar modulator, MORPH = 1 (full ring, default)
        Job j; bipolarRing (j);
        render (j, "v2/supplementary/ring_bipolar.wav", 5.0);
    }
    { // S2. same, MORPH stepped k/8 at t = k s (9 steps, dry -> AM -> ring)
        Job j; bipolarRing (j);
        RING* r = j.sys.ring;
        for (int k = 0; k <= 8; ++k) j.at (k, [r, k] { r->ctls[RING::cMorph].v = (float) k / 8.0f; });
        render (j, "v2/supplementary/ring_morph_sweep.wav", 9.0);
    }
    { // S3. DUSG1 CYCLE, period 20 ms, rise/(rise+fall) = 5/25/50/75/95 %
      //     (1.5 s each); DUSG1 END -> monitor (T05e duty law)
        Job j; j.drift (0.0f);
        DUSG* d = j.sys.dusg1;
        d->ctls[DUSG::cCycle].v = 1.0f;
        const double fr[5] = { 0.05, 0.25, 0.50, 0.75, 0.95 };
        for (int k = 0; k < 5; ++k)
        {
            const float r = t_dusg_ssg::dial (fr[k] * 0.020), f = t_dusg_ssg::dial ((1.0 - fr[k]) * 0.020);
            j.at (1.5 * k, [d, r, f] { d->ctls[DUSG::cRise].v = r; d->ctls[DUSG::cFall].v = f; });
        }
        j.monitor (5, 0.25f);
        render (j, "v2/supplementary/dusg_end_duty.wav", 7.5);
    }
}

// Negative control for the blind critic: the v2 patch of file 1 (1975 Hz saw
// through the v2 OUT monitor bus, same level), but the source is a naive,
// non-band-limited phase-accumulator saw run at a 12 kHz rate and
// zero-order-held to 48 kHz. v1's own saw is already a naive 48 kHz
// accumulator, so a plain naive saw would be ~identical to the v1 render;
// the 4x-decimated accumulator is unambiguously worse than either candidate
// (aliases fold straight into the 0-6 kHz band).
void renderControl()
{
    std::printf ("negative control -> %s/control\n", gDir.c_str());
    Job j; j.drift (0.0f); j.ntoHz (1975.0);
    j.monitor (2, 0.5f);
    static float naive = 0.0f;
    j.sys.out->monSrc[2] = &naive; // re-point the NTO SAW monitor tap at the degraded source
    double ph = 0.0;
    j.perSample = [&ph] (long i)
    {
        if (i % 4 == 0)
        {
            ph += 1975.0 * 4.0 / kSR;
            ph -= std::floor (ph);
        }
        naive = (float) (kAudio * (2.0 * ph - 1.0));
    };
    render (j, "control/nto_saw_1975_naive_zoh12k.wav", 5.0);
}

// Round-2 negative control (sound-r2 packet, ring piece): the ring_bipolar
// patch through the v2 OUT monitor bus, but the RING monitor tap re-pointed
// at a degraded "ring": a bare product X·Y with -20 dB of BOTH inputs leaking
// straight through (an untrimmed, badly balanced modulator: ~20 dB carrier
// rejection against the documented 80 dB). Reads the same X (NTO SINE) and Y
// (VCFQ HP) jacks the real RING sees; level-matched to the pair.
void renderControlRing()
{
    Job j; bipolarRing (j);
    j.monitor (17, 0.30f); // level-matched to the pair (r4; r2/r3 0.8)
    static float leaky = 0.0f;
    j.sys.out->monSrc[17] = &leaky;
    const float* x = &j.sys.nto->jacks[NTO::jSine].value;
    const float* y = &j.sys.vcfq->jacks[VCFQ::jHp].value;
    j.perSample = [x, y] (long)
    {
        const float ux = *x * (1.0f / kAudio), uy = *y * (1.0f / kAudio);
        leaky = kAudio * (ux * uy + 0.1f * ux + 0.1f * uy);
    };
    render (j, "control/ring_bipolar_leaky.wav", 5.0);
}

// Round-3 negative control (sound-r3 packet, dusg_env piece): the
// dusg_env_shape patch and level path (NTO SINE 1 kHz -> VCA2 LIN, LEVEL 0
// -> OUT IN 2, PAN 0.5, MASTER 0.4, same three KEY transients), but VCA2's
// VC is a degraded "slope generator": an RC (capacitor-charge) envelope —
// exponential approach toward 5 V for the 0.5 s rise (tau = 0.5 s / ln 100,
// 99 % at the end of the rise), then exponential discharge for the 1.0 s
// fall (tau = 1.0 s / ln 100). An ADSR-style RC curve instead of the Serge's
// linear-by-design slopes: T05d mid-80 % deviation far above 3 % FS.
void renderControlDusgRc()
{
    Job j; j.drift (0.0f); j.ntoHz (1000.0);
    j.sys.patch (j.sys.nto, NTO::jSine, j.sys.vca2, VCA::jIn);
    j.sys.patch (j.sys.vca2, VCA::jOut, j.sys.out, OUT::jIn2);
    j.sys.vca2->ctls[VCA::cResponse].v = 0.0f;
    j.sys.vca2->ctls[VCA::cLevel].v = 0.0f;
    j.sys.out->ctls[OUT::cPan2].v = 0.5f;
    j.sys.out->ctls[OUT::cMaster].v = 0.4f;
    static float rc = 0.0f;
    rc = 0.0f;
    VCA* vca = j.sys.vca2;
    j.at (0.0, [vca] { vca->feeds[(size_t) VCA::jVc].src.push_back (&rc); }); // after prepare()'s rebuild
    const double kr = 1.0 - std::exp (-std::log (100.0) / (0.5 * kSR));
    const double kf = 1.0 - std::exp (-std::log (100.0) / (1.0 * kSR));
    j.perSample = [kr, kf] (long i)
    {
        const long starts[3] = { std::lround (0.3 * kSR), std::lround (2.4 * kSR), std::lround (4.5 * kSR) };
        bool rising = false;
        for (long s0 : starts) rising |= i >= s0 && i < s0 + std::lround (0.5 * kSR);
        rc += (float) ((rising ? (double) kCV - rc : -(double) rc) * (rising ? kr : kf));
    };
    render (j, "control/dusg_env_rc.wav", 6.5);
}

// Round-4 negative control (sound-r4 packet, vcm_bot piece): the
// vcm_bot_full patch and level path (NTO SINE 220 Hz -> VCA1 LIN 0.8 ->
// BOT IN 1, BOT monitor tap, MASTER 0.8), but the BOT monitor pointer
// re-aimed at a degraded "wave multiplier": ONE plain full-wave rectifier,
// 2·|x| minus its sine-referenced mean, computed at 48 kHz with no
// oversampling and no anti-aliasing. One stage instead of three (the octave
// cascade stops at H2) and its cusp series aliases freely.
void renderControlBotAbs()
{
    Job j; j.drift (0.0f); vcmFeed (j, VCM::jBotIn1);
    j.sys.vcm->ctls[VCM::cBotDepth].v = 1.0f;
    j.monitor (16, kBotAbsMonLvl);
    static float rect = 0.0f;
    rect = 0.0f;
    j.sys.out->monSrc[16] = &rect;
    const float* x = &j.sys.vca1->jacks[VCA::jOut].value;
    j.perSample = [x] (long)
    {
        constexpr float kMean = 2.0f * 2.0f * 2.0f / 3.14159265f; // 2·mean|2 V sine|
        rect = 2.0f * std::fabs (*x) - kMean;
    };
    render (j, "control/vcm_bot_single_abs.wav", 5.0);
}

} // namespace

int main (int argc, char** argv)
{
    gDir = argc > 1 ? argv[1] : "gauntlet/renders";
    renderV2();
    renderSupplementary();
    renderControl();
    renderControlRing();
    renderControlDusgRc();
    renderControlBotAbs();
    std::printf ("done.\n");
    return 0;
}
