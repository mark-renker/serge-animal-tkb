// Serge Animal TKB v2 — per-widget function descriptions (hover tooltips).
// U03 gate: EVERY interactive widget returns a non-empty tooltip of >= 15
// characters describing its function. Curated texts below follow docs/SPEC.md
// §4; anything uncurated falls back to a generated description built from the
// frozen jack direction/color semantics (always >= 15 chars).
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include "../engine/EngineCore.h"

namespace satkui {

// dusg1/dusg2 and vca1/vca2 share one description set
inline juce::String tipModuleKey (const std::string& id)
{
    juce::String s (id);
    if (s.startsWith ("dusg")) return "dusg";
    if (s.startsWith ("vca"))  return "vca";
    return s;
}

inline const std::map<juce::String, const char*>& curatedJackTips()
{
    static const std::map<juce::String, const char*> m = {
        // ---- NTO --------------------------------------------------------
        { "nto.SINE",     "Sine output: the purest NTO waveform (lowest THD at minimum waveshape), +/-2.5 V audio." },
        { "nto.TRI",      "Triangle output straight from the oscillator core, +/-2.5 V audio." },
        { "nto.SAW",      "Band-limited sawtooth output, +/-2.5 V audio." },
        { "nto.VAR",      "Variable waveshape output: morphs sine to 'M'/pulse-like under the WAVESHAPE knob and VC WAVE input." },
        { "nto.PULSE",    "Pulse output, 0/5 V logic, following the oscillator cycle." },
        { "nto.1V/OCT A", "Calibrated 1 V/octave pitch input A (adds to the FREQ knob)." },
        { "nto.1V/OCT B", "Calibrated 1 V/octave pitch input B (sums with input A)." },
        { "nto.VC FREQ",  "Exponential FM input; depth set by the VC FREQ AMT attenuverter (full = +/-0.4 oct per volt)." },
        { "nto.LIN FM",   "AC-coupled through-zero linear FM input (audio rate); index set by LIN FM DEPTH + VC LIN." },
        { "nto.VC LIN",   "CV over the linear-FM index: adds to the LIN FM DEPTH knob (5 V = full index)." },
        { "nto.VC WAVE",  "CV over the waveshape morph: adds to the WAVESHAPE knob (5 V = full morph)." },
        { "nto.SYNC",     "SOFT sync input: nudges the core toward simple frequency ratios; will not lock far from a ratio." },
        // ---- DUSG (both) ------------------------------------------------
        { "dusg.OUT",      "Slope output, 0-5 V: envelope, slew or LFO depending on how it is patched." },
        { "dusg.END/GATE", "End-of-slope LEVEL: high during the rise and while OUT rests at bottom, low during the fall." },
        { "dusg.TRIG",     "Trigger input: starts a full rise-and-fall transient; ignored while a transient is running." },
        { "dusg.IN",       "Signal input: OUT slews toward this voltage (portamento / envelope follower); a held gate gives ASR sustain." },
        { "dusg.VC RISE",  "Rise-time CV, 1 V/oct: each +1 V halves the rise time." },
        { "dusg.VC FALL",  "Fall-time CV, 1 V/oct: each +1 V halves the fall time." },
        // ---- SSG --------------------------------------------------------
        { "ssg.SMOOTH OUT",  "Smooth-section output: voltage-controlled lag of SMOOTH IN; cycles a 0-5 V triangle when self-patched." },
        { "ssg.CYCLE",       "Pulse out on each smooth-section cycle; patch it to SMOOTH IN to make the section self-oscillate." },
        { "ssg.SMOOTH IN",   "Smooth-section input: the output slews toward this signal at the SMOOTH RATE." },
        { "ssg.VC RATE",     "Smooth-section rate CV, 1 V/oct: positive voltage speeds the lag up." },
        { "ssg.HOLD",        "Hold input: a high gate freezes the smooth output exactly (track-and-hold)." },
        { "ssg.STEPPED OUT", "Stepped-section output: sample-and-hold with step-size limiting; static without SAMPLE pulses." },
        { "ssg.STEPPED IN",  "Stepped-section signal input, sampled on each SAMPLE pulse." },
        { "ssg.SAMPLE",      "Sample-clock input for the stepped section: each pulse takes a new sample." },
        { "ssg.ST VC RATE",  "Stepped step-size CV, 1 V/oct: limits how far each sampled step can move." },
        { "ssg.COUPLER",     "Comparator output: 5 V while smooth > stepped, otherwise 0 V." },
        { "ssg.HOT COUPLER", "Hot comparator output: the same comparison swinging at least +/-10 V." },
        // ---- VCFQ -------------------------------------------------------
        { "vcfq.LP",     "Low-pass output, 12 dB/octave above the cutoff." },
        { "vcfq.BP",     "Band-pass output; patch BP back to IN for stable, amplitude-limited self-oscillation." },
        { "vcfq.HP",     "High-pass output, simultaneous with the other responses." },
        { "vcfq.NOTCH",  "Notch (band-reject) output, simultaneous with LP/BP/HP." },
        { "vcfq.IN",     "Audio input via the GAIN knob - deliberately NOT resonance-compensated, so it overdrives at high Q." },
        { "vcfq.AGC IN", "Resonance-compensated audio input: its level automatically drops as Q rises." },
        { "vcfq.1V/OCT", "Calibrated 1 V/octave cutoff-frequency input." },
        { "vcfq.VC F",   "Cutoff CV via the VC F AMT attenuverter (full = 1 octave per volt)." },
        { "vcfq.VC Q",   "Resonance CV via the VC Q AMT attenuverter (full = 0.6 octave per volt on Q)." },
        { "vcfq.TRIG",   "Ping input: a pulse rings the filter into a damped sine at the cutoff; decay lengthens with Q." },
        // ---- VCM --------------------------------------------------------
        { "vcm.TOP OUT",    "Top-section output: clean linear VCA in LO mode, VC rounded 'square-up' clipping in HI mode." },
        { "vcm.TOP IN",     "Top-section audio input." },
        { "vcm.VC TOP",     "Top-section gain/drive control voltage." },
        { "vcm.MID OUT",    "Middle-section output: the classic odd-harmonic wavefolder (real transfer-curve turnarounds)." },
        { "vcm.MID IN 1",   "Middle-section input 1: mixes with input 2 before the folder." },
        { "vcm.MID IN 2",   "Middle-section input 2: mixes with input 1 before the folder." },
        { "vcm.VC FOLD",    "Fold-depth CV: adds 0.4 octave of fold gain per volt to the FOLD knob." },
        { "vcm.BOT OUT",    "Bottom-section output: cascaded full-wave rectifiers, even harmonics dominant, level-compensated." },
        { "vcm.BOT SQ OUT", "Bottom-section second output: the squared-up, PWM-like version of the rectified signal." },
        { "vcm.BOT IN 1",   "Bottom-section input 1: mixes with input 2 before the rectifier cascade." },
        { "vcm.BOT IN 2",   "Bottom-section input 2: mixes with input 1 before the rectifier cascade." },
        { "vcm.VC BOT",     "Rectifier-depth CV for the bottom section (adds to BOT DEPTH)." },
        // ---- RING -------------------------------------------------------
        { "ring.OUT",      "Output of the four-quadrant multiplier: dry, AM or ring product per the MORPH setting." },
        { "ring.X IN",     "Signal input (X) of the four-quadrant multiplier." },
        { "ring.Y IN",     "Carrier input (Y), DC-coupled: a DC carrier turns the ring into a clean VCA." },
        { "ring.VC MORPH", "CV over the dry-to-AM-to-ring morph (adds to the MORPH knob, 5 V = full)." },
        // ---- NOISE ------------------------------------------------------
        { "noise.WHITE",      "White noise output, +/-2.5 V, flat spectrum." },
        { "noise.PINK",       "Pink noise output, falling 3 dB per octave." },
        { "noise.S/H SOURCE", "Sample-and-hold source: a randomly modulated ~400 Hz sawtooth, 0-5 V (equi-probable S&H input)." },
        // ---- RVG --------------------------------------------------------
        { "rvg.STEPPED", "Stepped random voltage, 0-5 V: changes only when triggered." },
        { "rvg.SMOOTH",  "Smooth random voltage: the linearly slewed version of STEPPED." },
        { "rvg.PULSE",   "Pulse output of the internal random-timing clock." },
        { "rvg.TRIG",    "External trigger input, ORed with the internal random clock." },
        { "rvg.VC RATE", "Internal clock-rate CV, 1 V/octave." },
        // ---- TKB --------------------------------------------------------
        { "tkb.CLOCK",      "Clock input: each pulse advances the sequencer one stage, wrapping at LENGTH." },
        { "tkb.VERT CLOCK", "Vertical clock input: each pulse advances the active row A-B-C-D on the ABCD output (64-step trick)." },
        { "tkb.RESET",      "Reset input: jumps to the LAST-TOUCHED pad's stage (not stage 1)." },
        { "tkb.VERT RESET", "Vertical reset input: returns the active row to row A." },
        { "tkb.RANDOM",     "Random input: a high gate arms random stage selection (ORs with the RANDOM switch)." },
        { "tkb.UP/DOWN",    "Direction input: a high gate makes the sequencer step downward (wraps 1 to LENGTH)." },
        { "tkb.HOLD",       "Hold input: a high gate freezes the sequencer where it is." },
        { "tkb.A",          "Row A CV output: the active stage's row-A knob, scaled by RANGE volts." },
        { "tkb.B",          "Row B CV output: the active stage's row-B knob, scaled by RANGE volts." },
        { "tkb.C",          "Row C CV output: the active stage's row-C knob, scaled by RANGE volts." },
        { "tkb.D",          "Row D CV output: the active stage's row-D knob, scaled by RANGE volts." },
        { "tkb.ABCD",       "Vertical-sequencer output: active row at the active stage (x RANGE), for 64-step sequences." },
        { "tkb.KV",         "Keyboard voltage: the LAST-TOUCHED pad, semitone-calibrated ((pad-1)/12 V), slewed by GLIDE." },
        { "tkb.PRESSURE",   "Pressure output: 0-5 V while a pad is touched (click height or channel aftertouch)." },
        { "tkb.GATE",       "Gate output: emits a GATE LEN gate on each step advance." },
        { "tkb.KP",         "Key pulse output: high while any touch pad is held." },
        // ---- KEY --------------------------------------------------------
        { "key.KV",   "MIDI keyboard voltage, 1 V/octave (note 60 = 0 V), slewed by the GLIDE knob." },
        { "key.GATE", "MIDI gate output: 5 V while any note is held." },
        { "key.VEL",  "Velocity output, 0-5 V: holds the last note-on velocity; also hard-scales the final output mix." },
        { "key.CLK",  "Host-transport clock output: 16th-note pulses while the DAW is playing." },
        // ---- VCA (both) --------------------------------------------------
        { "vca.OUT", "VCA audio output (maximum gain +6 dB at full control voltage)." },
        { "vca.IN",  "VCA audio input, +/-2.5 V nominal." },
        { "vca.VC",  "Gain CV input, 0-5 V: ~12 dB per volt in EXP mode, linear in LIN mode; shut below 0.5 V." },
        // ---- OUT --------------------------------------------------------
        { "out.IN 1", "Output-mixer channel 1 audio input (placed in the stereo field by PAN 1)." },
        { "out.IN 2", "Output-mixer channel 2 audio input (placed in the stereo field by PAN 2)." },
    };
    return m;
}

inline const std::map<juce::String, const char*>& curatedCtlTips()
{
    static const std::map<juce::String, const char*> m = {
        // ---- NTO --------------------------------------------------------
        { "nto.FREQ",         "Coarse frequency, exponential 16 Hz to 16 kHz (default 261.63 Hz = C4)." },
        { "nto.FINE",         "Fine tune, +/-100 cents around the FREQ setting (center = 0)." },
        { "nto.WAVESHAPE",    "Waveshape morph on the VAR output: sine toward 'M'/pulse-like; sums with the VC WAVE input." },
        { "nto.VC FREQ AMT",  "Attenuverter for the VC FREQ input: center = off, full = +/-0.4 octave per volt." },
        { "nto.LIN FM DEPTH", "Linear FM index: at full depth 5 V of LIN FM swings the frequency by three times f0." },
        // ---- DUSG -------------------------------------------------------
        { "dusg.RISE",  "Rise time, exponential 50 microseconds to 10 seconds (halved per volt at VC RISE)." },
        { "dusg.FALL",  "Fall time, exponential 50 microseconds to 10 seconds (halved per volt at VC FALL)." },
        { "dusg.CYCLE", "Cycle switch: ON makes the slope self-retrigger for LFO/oscillator use (reaches audio rate)." },
        // ---- SSG --------------------------------------------------------
        { "ssg.SMOOTH RATE",  "Smooth-section lag rate, exponential 50 microseconds to 10 seconds." },
        { "ssg.STEPPED RATE", "Stepped-section step-size limit rate, exponential 50 microseconds to 10 seconds." },
        // ---- VCFQ -------------------------------------------------------
        { "vcfq.FREQ",       "Filter cutoff frequency, exponential 20 Hz to 12 kHz." },
        { "vcfq.Q",          "Resonance, exponential 0.5 to 40 (CV can extend it to 60)." },
        { "vcfq.GAIN",       "Input level into the uncompensated IN jack, linear 0 to 2 (drive it for grit at high Q)." },
        { "vcfq.VC F AMT",   "Attenuverter for the VC F cutoff input: center = off, full = 1 octave per volt." },
        { "vcfq.VC Q AMT",   "Attenuverter for the VC Q resonance input: center = off." },
        { "vcfq.PING LEVEL", "Level of the ping injected by the TRIG input (struck-filter loudness)." },
        // ---- VCM --------------------------------------------------------
        { "vcm.TOP MODE",  "Top-section mode: LO = clean linear VCA, HI = voltage-controlled rounded square-up clipping." },
        { "vcm.TOP DRIVE", "Top-section drive, exponential 1x to 10x." },
        { "vcm.FOLD",      "Middle-section fold depth: raises the folder gain (0.5x to 8x) for more odd harmonics." },
        { "vcm.BIAS",      "DC bias into the folder: center = 0 V; offsets fold symmetry for even-harmonic content." },
        { "vcm.BOT DEPTH", "Bottom-section rectifier depth (even-harmonic intensity)." },
        // ---- RING -------------------------------------------------------
        { "ring.GAIN",         "Ring-modulator output gain, exponential 0.5x to 4x." },
        { "ring.MORPH",        "Response morph: 0 = dry signal, 0.5 = AM, 1 = full ring modulation." },
        { "ring.X BLEED TRIM", "Signal feedthrough trim: 0 = calibrated minimum (about -80 dB), up = audible signal bleed." },
        { "ring.Y BLEED TRIM", "Carrier feedthrough trim: 0 = calibrated minimum, up = audible carrier bleed." },
        // ---- RVG --------------------------------------------------------
        { "rvg.RATE", "Internal random clock rate, exponential 0.05 to 50 Hz." },
        // ---- TKB --------------------------------------------------------
        { "tkb.RUN",      "Run switch: ON starts the sequencer's internal clock at the RATE setting." },
        { "tkb.RATE",     "Internal sequencer clock rate, exponential 0.1 to 30 Hz." },
        { "tkb.LENGTH",   "Sequence length: number of stages before the sequencer wraps (1 to 16)." },
        { "tkb.RANDOM",   "Random switch: when armed, each clock pulse selects a random stage within LENGTH." },
        { "tkb.GATE LEN", "Length of the gate emitted on each step, exponential 5 ms to 1 s." },
        { "tkb.RANGE",    "Output range: scales the A-D and ABCD row outputs from 0.5 to 5 octaves." },
        { "tkb.GLIDE",    "Glide: slew time applied to the KV keyboard-voltage output, 0 to 1 s." },
        { "tkb.PRESSURE", "Pad pressure, 0-1: written by the touch pads (click height) or channel aftertouch." },
        // ---- KEY --------------------------------------------------------
        { "key.GLIDE", "Portamento time on the MIDI keyboard voltage, 0 to 0.5 s." },
        // ---- VCA --------------------------------------------------------
        { "vca.LEVEL",    "Level offset: adds 0-5 V to the VC input sum (default 0 = the VCA stays closed)." },
        { "vca.RESPONSE", "Response law: LIN = gain linear in CV (v1 law), EXP = exponential 12 dB per volt (hardware)." },
        // ---- OUT --------------------------------------------------------
        { "out.PAN 1",         "Channel 1 stereo position, equal-power pan law." },
        { "out.PAN 2",         "Channel 2 stereo position, equal-power pan law." },
        { "out.MONITOR",       "Monitor source selector: auditions any of 43 internal signals directly at the output." },
        { "out.MONITOR LEVEL", "Monitor bus level (mixed into the output at x0.7)." },
        { "out.MASTER",        "Master output level for the whole instrument." },
        { "out.ANALOG DRIFT",  "Global analog-layer depth: component tolerances and thermal drift; 0 = ideal digital behavior." },
    };
    return m;
}

// generated fallback pieces (always >= 15 chars when assembled)
inline juce::String describeJackKind (const satk::Jack& j)
{
    using JT = satk::JackType; using JC = satk::JackColor;
    const bool out = j.type == JT::Out;
    switch (j.color)
    {
        case JC::White:  return out ? "control-voltage output, 0-5 V"
                                    : "control-voltage input, 0-5 V";
        case JC::Red:    return out ? "pulse/gate output, 0/5 V logic"
                                    : "pulse/gate input (high above 2.5 V)";
        case JC::Black:  return out ? "audio output, +/-2.5 V nominal"
                                    : "audio-rate signal input, +/-2.5 V nominal";
        case JC::Purple: return "edge-sensitive sync input";
        case JC::Grey:   return "hot output, swings beyond +/-5 V";
        default:         return "signal connection";
    }
}

inline juce::String jackTip (const satk::Module& m, int j)
{
    const auto& jk = m.jacks[(size_t) j];
    const juce::String key = tipModuleKey (m.id) + "." + jk.name;
    const auto& map = curatedJackTips();
    if (auto it = map.find (key); it != map.end())
        return it->second;

    const juce::String name (jk.name);
    if (tipModuleKey (m.id) == "tkb" && name.startsWith ("ST "))
        return "Stage " + name.substring (3) + " pulse output: sits at 5 V while stage "
             + name.substring (3) + " is the active step (one-hot level).";

    return name + ": " + describeJackKind (jk) + " of the " + juce::String (m.title).toLowerCase()
         + ". Drag to another jack to patch a banana cable; right-click to remove cables.";
}

inline juce::String ctlTip (const satk::Module& m, int c)
{
    const auto& ct = m.ctls[(size_t) c];
    const juce::String key = tipModuleKey (m.id) + "." + ct.name;
    const auto& map = curatedCtlTips();
    if (auto it = map.find (key); it != map.end())
        return it->second;

    const juce::String name (ct.name);
    if (tipModuleKey (m.id) == "tkb")
    {
        if (name.startsWith ("PAD "))
            return "Touch pad " + name.substring (4) + ": click to select stage " + name.substring (4)
                 + ", fire the KP gate and emit PRESSURE CV; the click height sets the pressure (0-1)."
                   " RESET returns to the last-touched pad.";
        if (name.length() >= 2 && name.length() <= 3
            && juce::String ("ABCD").containsChar (name[0]))
            return "Row " + name.substring (0, 1) + ", stage " + name.substring (1)
                 + " knob: the control voltage this stage sends to the " + name.substring (0, 1)
                 + " output (scaled by the RANGE knob).";
    }
    return juce::String (m.title).toLowerCase() + " " + name
         + " control (see the panel spec for its exact mapping).";
}

} // namespace satkui
