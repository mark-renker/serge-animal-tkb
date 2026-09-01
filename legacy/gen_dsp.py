#!/usr/bin/env python3
# Generates SergeAnimalTKB.dsp - a self-contained Serge-style voice ("Animal" + TKB)
# 30 patch sources x 31 destinations, 2 slots per destination (source menu + attenuverter).

MENU = ("[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;"
        "'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;"
        "'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;"
        "'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;"
        "'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;"
        "'KEY gate':28;'VCA1':29;'VCA2':30}]")

# (varname, patch-tab group, widget label, defaultSelA, defaultAmtA, defaultSelB, defaultAmtB)
DESTS = [
    ("dNtoPitch", "NTO",   "pitch FM",   0, 0.0, 0, 0.0),
    ("dNtoLin",   "NTO",   "lin FM",     0, 0.0, 0, 0.0),
    ("dNtoShape", "NTO",   "shape",      0, 0.0, 0, 0.0),
    ("dNtoSync",  "NTO",   "sync",       0, 0.0, 0, 0.0),
    ("dTrig1",    "DUSG1", "trig",      28, 1.0, 0, 0.0),
    ("dIn1",      "DUSG1", "signal in",  0, 0.0, 0, 0.0),
    ("dRise1",    "DUSG1", "rise CV",    0, 0.0, 0, 0.0),
    ("dFall1",    "DUSG1", "fall CV",    0, 0.0, 0, 0.0),
    ("dTrig2",    "DUSG2", "trig",       0, 0.0, 0, 0.0),
    ("dIn2",      "DUSG2", "signal in",  0, 0.0, 0, 0.0),
    ("dRise2",    "DUSG2", "rise CV",    0, 0.0, 0, 0.0),
    ("dFall2",    "DUSG2", "fall CV",    0, 0.0, 0, 0.0),
    ("dSmIn",     "SSG",   "smooth in",  0, 0.0, 0, 0.0),
    ("dSRate",    "SSG",   "smooth rate CV", 0, 0.0, 0, 0.0),
    ("dHold",     "SSG",   "hold",       0, 0.0, 0, 0.0),
    ("dStIn",     "SSG",   "stepped in", 0, 0.0, 0, 0.0),
    ("dStClk",    "SSG",   "stepped clk",0, 0.0, 0, 0.0),
    ("dVcfIn",    "VCFQ",  "audio in",  15, 0.8, 0, 0.0),
    ("dFcv",      "VCFQ",  "freq CV",    0, 0.0, 0, 0.0),
    ("dQcv",      "VCFQ",  "Q CV",       0, 0.0, 0, 0.0),
    ("dPing",     "VCFQ",  "ping trig",  0, 0.0, 0, 0.0),
    ("dVcmIn",    "VCM",   "audio in",   3, 0.8, 0, 0.0),
    ("dFold",     "VCM",   "fold CV",    0, 0.0, 0, 0.0),
    ("dRingX",    "RING",  "X in",       0, 0.0, 0, 0.0),
    ("dRingY",    "RING",  "Y in",       0, 0.0, 0, 0.0),
    ("dV1in",     "VCA1",  "audio in",  11, 1.0, 0, 0.0),
    ("dV1cv",     "VCA1",  "CV",         4, 1.0, 0, 0.0),
    ("dV2in",     "VCA2",  "audio in",   0, 0.0, 0, 0.0),
    ("dV2cv",     "VCA2",  "CV",         0, 0.0, 0, 0.0),
    ("dTkbClk",   "TKB",   "ext clock",  0, 0.0, 0, 0.0),
    ("dTkbRst",   "TKB",   "reset",      0, 0.0, 0, 0.0),
]

GROUPS = ["NTO","DUSG1","DUSG2","SSG","VCFQ","VCM","RING","VCA1","VCA2","TKB"]
GIDX = {g: i for i, g in enumerate(GROUPS)}

# One row per destination inside its module page: [A src][A amt][B src][B amt].
#
# Two UI details that matter for the JUCE/plugin build:
#  * the module group is v: (vertical), NOT t:. A tab group holding raw controls
#    makes the Faust JUCE UI give every individual control its own tab, so you
#    can only see one menu or one slider at a time.
#  * the source selectors are hslider, NOT nentry. JuceGUI::addNumEntry ignores
#    [style:menu{...}] (only addSlider honours it), so nentry selectors render as
#    bare 0-30 number spinners instead of named dropdowns. Range/step/default are
#    identical either way, so this is purely a widget choice.
rowidx = {}
slots = []
for (var, grp, lbl, sa, aa, sb, ab) in DESTS:
    k = rowidx.get(grp, 0)
    rowidx[grp] = k + 1
    p = f"t:MAIN/t:[5]PATCH/v:[{GIDX[grp]:02d}]{grp}/h:[{k}]{lbl}"
    slots.append(
        f'  {var}A_s = hslider("{p}/[0]A src{MENU}", {sa}, 0, 30, 1);\n'
        f'  {var}A_a = hslider("{p}/[1]A amt", {aa}, -1, 1, 0.001) : si.smoo;\n'
        f'  {var}B_s = hslider("{p}/[2]B src{MENU}", {sb}, 0, 30, 1);\n'
        f'  {var}B_a = hslider("{p}/[3]B amt", {ab}, -1, 1, 0.001) : si.smoo;\n'
        f'  {var} = pick({var}A_s)*{var}A_a + pick({var}B_s)*{var}B_a;\n'
    )
SLOTS = "\n".join(slots)

HEADER = r'''// =====================================================================
// SergeAnimalTKB - a self-contained Serge-style modular voice
// Modeled on the Random*Source "Animal" (La Bestia) voice/timbre panel
// concept + TKB sequencer, with a 2-slot-per-destination patch matrix
// standing in for banana cables. MIDI (freq/gate/gain) is the default
// key source; the TKB takes over when "TKB mode" is on.
//
// Signal conventions: audio/CV nominal +/-1.0 (~ +/-5V), gates 0/1.
// All module outputs are matrix SOURCES; all module inputs are matrix
// DESTINATIONS with two slots (source selector + attenuverter).
// Feedback patching is legal everywhere (one-sample loop delay).
//
// Build (macOS AU/VST3):  ./build.sh    (faust2juce -jsynth -midi -nvoices 1,
//                         plus the three architecture patches it documents;
//                         -jsynth is required or MIDI notes never reach the DSP)
// Quick audition:         drag file into https://faustide.grame.fr
// =====================================================================

declare name "SergeAnimalTKB";
declare version "1.0";
declare options "[midi:on][nvoices:1]";

import("stdfaust.lib");

// ------------------------- helpers ----------------------------------
t3(x) = max(-1.0, min(1.0, x*(27.0 + x*x)/(27.0 + 9.0*x*x)));   // cheap tanh-ish
edgeup(x) = (b - b') > 0 with { b = (x > 0.25); };               // rising edge -> 1-sample 1
stretch(dur, trig) = ((f ~ _) > 0.5) with {                      // pulse stretcher
  n = max(1.0, dur*ma.SR);
  f(c) = max(trig*n, c - 1.0);
};
latch(sig, smp) = (f ~ _) with { f(h) = select2(smp > 0.5, h, sig); };

// ---------------- DUSG: Dual Universal Slope Generator --------------
// Trigger fires a full rise/fall transient; signal-in is slew-limited;
// cycle turns it into an LFO / audio oscillator; END pulses at cycle end.
dusgSG(riseT, fallT, cyc, trg, sig) = y, endp
with {
  rs = 1.0/max(riseT*ma.SR, 1.0);
  fs = 1.0/max(fallT*ma.SR, 1.0);
  tedge = edgeup(trg);
  tick(y1, a1) = yn, an
  with {
    start = max(tedge, cyc * ((y1 <= 0.0005) & (a1 < 0.5)));
    a2 = max(a1, start) * (y1 < 0.999);
    tgt = max(sig, a2);
    yn = y1 + max(-fs, min(rs, tgt - y1));
    an = a2;
  };
  y = (tick ~ si.bus(2)) : (_, !);
  endp = stretch(0.004, (y <= 0.0005) & (y' > 0.0005));
};

// ---------------- SSG: Smooth & Stepped Generator --------------------
ssgSmooth(rateT, holdg, x) = (f ~ _)
with {
  st = 1.0/max(rateT*ma.SR, 1.0);
  h = holdg > 0.25;
  f(y1) = select2(h, y1 + max(-st, min(st, x - y1)), y1);
};
ssgStep(rateT, clk, x) = (f ~ _)
with {
  held = latch(x, edgeup(clk));
  st = 1.0/max(rateT*ma.SR, 1.0);
  f(y1) = y1 + max(-st, min(st, held - y1));
};

// ---------------- VCFQ: TPT state-variable filter ---------------------
svf(fc, k, v0) = (tick ~ si.bus(2)) : (!, !, _, _, _)
with {
  fcl = max(5.0, min(fc, 0.45*ma.SR));
  g = tan(ma.PI*fcl/ma.SR);
  a1 = 1.0/(1.0 + g*(g + k));
  a2 = g*a1;
  a3 = g*a2;
  tick(ic1, ic2) = ic1n, ic2n, lp, bp, hp
  with {
    v3 = v0 - ic2;
    v1 = a1*ic1 + a2*v3;
    v2 = ic2 + a2*ic1 + a3*v3;
    ic1n = 2.0*v1 - ic1;
    ic2n = 2.0*v2 - ic2;
    lp = v2; bp = v1; hp = v0 - k*v1 - v2;
  };
};

// ---------------- NTO: triangle-core VCO w/ VC waveshape --------------
// f0: exponential pitch (Hz); flin: through-zero linear FM (Hz);
// shape 0..1 morphs sine -> triangle -> soft square on the VAR output.
nto(f0, shp, syncsig, flin) = sine, saw, var
with {
  fz = f0 + flin;
  inc = fz/ma.SR;
  rst = edgeup(syncsig);
  ph = (pf ~ _) with { pf(p1) = select2(rst, ma.frac(ma.frac(p1 + inc) + 1.0), 0.0); };
  saw = 2.0*ph - 1.0;
  tri = 1.0 - 4.0*abs(ph - 0.5);
  sine = sin(2.0*ma.PI*ph);
  s = max(0.0, min(1.0, shp));
  m1 = min(1.0, s*2.0);
  m2 = max(0.0, s*2.0 - 1.0);
  sqish = t3(3.0*tri);
  base = sine + m1*(tri - sine);
  var = base + m2*(sqish - base);
};

// ---------------------- panel controls -------------------------------
// The whole panel lives inside one top-level TAB group so only one section is
// on screen at a time. Laid out flat, the Faust JUCE UI stacks every group
// vertically at fixed widget sizes (hslider 350x50, vslider 80x250), which
// produced an editor taller than any screen. Tabs plus knob-style TKB steps
// keep it at roughly 1670x648.
midiFreq = hslider("t:MAIN/v:[0]VOICE/h:[0]KEY/[0]freq[unit:Hz]", 261.63, 20, 8000, 0.01);
midiGate = button("t:MAIN/v:[0]VOICE/h:[0]KEY/[1]gate");
midiGain = hslider("t:MAIN/v:[0]VOICE/h:[0]KEY/[2]gain", 0.8, 0, 1, 0.01) : si.smoo;
tkbMode  = checkbox("t:MAIN/v:[0]VOICE/h:[0]KEY/[3]TKB mode");
glide    = hslider("t:MAIN/v:[0]VOICE/h:[0]KEY/[4]glide[unit:s]", 0.0, 0, 0.5, 0.001);

ntoCoarse = hslider("t:MAIN/v:[0]VOICE/h:[1]NTO/[0]coarse[unit:semi]", 0, -24, 24, 0.01);
ntoFine   = hslider("t:MAIN/v:[0]VOICE/h:[1]NTO/[1]fine[unit:cent]", 0, -100, 100, 0.1);
ntoShape  = hslider("t:MAIN/v:[0]VOICE/h:[1]NTO/[2]shape", 0.15, 0, 1, 0.001) : si.smoo;
ntoKtrk   = checkbox("t:MAIN/v:[0]VOICE/h:[1]NTO/[3]keytrack")
            + 0*checkbox("t:MAIN/v:[0]VOICE/h:[1]NTO/[4]init_on");

d1Rise = hslider("t:MAIN/v:[1]SLOPES/h:[0]DUSG1/[0]rise[unit:s][scale:log]", 0.001, 0.00005, 10, 0.00001);
d1Fall = hslider("t:MAIN/v:[1]SLOPES/h:[0]DUSG1/[1]fall[unit:s][scale:log]", 0.25, 0.00005, 10, 0.00001);
d1Cyc  = checkbox("t:MAIN/v:[1]SLOPES/h:[0]DUSG1/[2]cycle");

d2Rise = hslider("t:MAIN/v:[1]SLOPES/h:[1]DUSG2/[0]rise[unit:s][scale:log]", 0.05, 0.00005, 10, 0.00001);
d2Fall = hslider("t:MAIN/v:[1]SLOPES/h:[1]DUSG2/[1]fall[unit:s][scale:log]", 0.05, 0.00005, 10, 0.00001);
d2Cyc  = checkbox("t:MAIN/v:[1]SLOPES/h:[1]DUSG2/[2]cycle");

smRate = hslider("t:MAIN/v:[1]SLOPES/h:[2]SSG/[0]smooth rate[unit:s][scale:log]", 0.05, 0.00005, 10, 0.00001);
stRate = hslider("t:MAIN/v:[1]SLOPES/h:[2]SSG/[1]stepped rate[unit:s][scale:log]", 0.001, 0.00005, 10, 0.00001);

vcfFreq = hslider("t:MAIN/v:[2]TIMBRE/h:[0]VCFQ/[0]freq[unit:Hz][scale:log]", 800, 20, 12000, 0.01) : si.smoo;
vcfQ    = hslider("t:MAIN/v:[2]TIMBRE/h:[0]VCFQ/[1]Q[scale:log]", 2, 0.5, 40, 0.01) : si.smoo;
vcfGain = hslider("t:MAIN/v:[2]TIMBRE/h:[0]VCFQ/[2]input gain", 1.0, 0, 2, 0.001) : si.smoo;
vcfPing = hslider("t:MAIN/v:[2]TIMBRE/h:[0]VCFQ/[3]ping level", 0.6, 0, 1, 0.001);

vcmTop  = hslider("t:MAIN/v:[2]TIMBRE/h:[1]VCM/[0]top drive", 2, 1, 10, 0.01) : si.smoo;
vcmFold = hslider("t:MAIN/v:[2]TIMBRE/h:[1]VCM/[1]fold", 0.3, 0, 1, 0.001) : si.smoo;
vcmBias = hslider("t:MAIN/v:[2]TIMBRE/h:[1]VCM/[2]bias", 0, -1, 1, 0.001) : si.smoo;

ringGain = hslider("t:MAIN/v:[2]TIMBRE/h:[2]RING-NOISE/[0]ring gain", 1, 0.5, 4, 0.01) : si.smoo;
rvgRate  = hslider("t:MAIN/v:[2]TIMBRE/h:[2]RING-NOISE/[1]RVG rate[unit:Hz][scale:log]", 4, 0.05, 50, 0.01);

tkbOn    = checkbox("t:MAIN/v:[3]TKB/h:[0]clock/[0]run");
tkbRate  = hslider("t:MAIN/v:[3]TKB/h:[0]clock/[1]rate[unit:Hz][scale:log]", 2, 0.1, 30, 0.01);
tkbLen   = hslider("t:MAIN/v:[3]TKB/h:[0]clock/[2]length", 8, 1, 16, 1);
tkbRnd   = checkbox("t:MAIN/v:[3]TKB/h:[0]clock/[3]random");
tkbGateL = hslider("t:MAIN/v:[3]TKB/h:[0]clock/[4]gate len[unit:s][scale:log]", 0.1, 0.005, 1, 0.001);
tkbOct   = hslider("t:MAIN/v:[3]TKB/h:[0]clock/[5]range[unit:oct]", 2, 0.5, 5, 0.1);
// knob style: 100x100 per step instead of 80x250, so all 4 rows of 16 fit.
rowA(j)  = vslider("t:MAIN/v:[3]TKB/h:[1]Row A/[%2j]A%j[style:knob]", 0.5, 0, 1, 0.001) : si.smoo;
rowB(j)  = vslider("t:MAIN/v:[3]TKB/h:[2]Row B/[%2j]B%j[style:knob]", 0.5, 0, 1, 0.001) : si.smoo;
rowC(j)  = vslider("t:MAIN/v:[3]TKB/h:[3]Row C/[%2j]C%j[style:knob]", 0.5, 0, 1, 0.001) : si.smoo;
rowD(j)  = vslider("t:MAIN/v:[3]TKB/h:[4]Row D/[%2j]D%j[style:knob]", 0.5, 0, 1, 0.001) : si.smoo;

vca1Lvl = hslider("t:MAIN/v:[4]OUT/h:[0]VCA/[0]vca1 level", 0, 0, 1, 0.001) : si.smoo;
vca1Exp = checkbox("t:MAIN/v:[4]OUT/h:[0]VCA/[1]vca1 exp");
vca2Lvl = hslider("t:MAIN/v:[4]OUT/h:[0]VCA/[2]vca2 level", 0, 0, 1, 0.001) : si.smoo;
vca2Exp = checkbox("t:MAIN/v:[4]OUT/h:[0]VCA/[3]vca2 exp");
pan1    = hslider("t:MAIN/v:[4]OUT/h:[0]VCA/[4]pan1", 0.35, 0, 1, 0.001) : si.smoo;
pan2    = hslider("t:MAIN/v:[4]OUT/h:[0]VCA/[5]pan2", 0.65, 0, 1, 0.001) : si.smoo;
monSel  = hslider("t:MAIN/v:[4]OUT/h:[1]MIX/[0]monitor src__MENU__", 0, 0, 30, 1);
monLvl  = hslider("t:MAIN/v:[4]OUT/h:[1]MIX/[1]monitor level", 0.4, 0, 1, 0.001) : si.smoo;
master  = hslider("t:MAIN/v:[4]OUT/h:[1]MIX/[2]master", 0.8, 0, 1, 0.001) : si.smoo;

vca(x, cv, lvl, expOn) = x * select2(expOn, gl, gl*gl*gl)
with { gl = max(0.0, min(1.0, lvl + cv)); };

// ------------------------- the system --------------------------------
big(nsin, nsaw, nvar, d1o, d1e, d2o, d2e, ssm, sst, scp,
    flp, fbp, fhp, vtp, vmd, vbt, rng, nzw, nzp, rvs,
    rvm, tA, tB, tC, tD, tG, kCV, kG, v1o, v2o) =
  nsinN, nsawN, nvarN, d1oN, d1eN, d2oN, d2eN, ssmN, sstN, scpN,
  flpN, fbpN, fhpN, vtpN, vmdN, vbtN, rngN, nzwN, nzpN, rvsN,
  rvmN, tAN, tBN, tCN, tDN, tGN, kCVN, kGN, v1oN, v2oN, outL, outR
with {
  // matrix source bus (index 0 = OFF)
  srcsig(0)  = 0.0;   srcsig(1)  = nsin;  srcsig(2)  = nsaw;  srcsig(3)  = nvar;
  srcsig(4)  = d1o;   srcsig(5)  = d1e;   srcsig(6)  = d2o;   srcsig(7)  = d2e;
  srcsig(8)  = ssm;   srcsig(9)  = sst;   srcsig(10) = scp;   srcsig(11) = flp;
  srcsig(12) = fbp;   srcsig(13) = fhp;   srcsig(14) = vtp;   srcsig(15) = vmd;
  srcsig(16) = vbt;   srcsig(17) = rng;   srcsig(18) = nzw;   srcsig(19) = nzp;
  srcsig(20) = rvs;   srcsig(21) = rvm;   srcsig(22) = tA;    srcsig(23) = tB;
  srcsig(24) = tC;    srcsig(25) = tD;    srcsig(26) = tG;    srcsig(27) = kCV;
  srcsig(28) = kG;    srcsig(29) = v1o;   srcsig(30) = v2o;
  pick(s) = sum(k, 31, (abs(s - k) < 0.5) * srcsig(k));

  // ----------------- patch matrix destinations -----------------------
__SLOTS__

  // ----------------- TKB / KEY ---------------------------------------
  clkAll = max(os.lf_imptrain(tkbRate)*tkbOn, dTkbClk);
  e = edgeup(clkAll);
  rstE = edgeup(dTkbRst);
  scount = (sf ~ _) with {
    sf(s1) = s2 - tkbLen*floor(s2/tkbLen) with { s2 = select2(rstE, s1 + e, 0.0); };
  };
  rv = min(tkbLen - 1.0, floor(abs(no.noises(4,3))*tkbLen));
  rndst = latch(rv, e);
  stage = select2(tkbRnd, scount, rndst);
  rsel(row) = sum(j, 16, (abs(stage - j) < 0.5) * row(j));
  tAN = rsel(rowA);  tBN = rsel(rowB);  tCN = rsel(rowC);  tDN = rsel(rowD);
  tGN = stretch(tkbGateL, e);
  tkbHz = 130.81*pow(2.0, tAN*tkbOct);

  keyHz = select2(tkbMode, midiFreq, tkbHz) : si.smooth(ba.tau2pole(max(glide, 0.0001)));
  kGN  = select2(tkbMode, midiGate, tGN);
  kCVN = log(keyHz/261.63)/(5.0*log(2.0));

  // ----------------- NTO ---------------------------------------------
  f0 = min(15000.0, select2(ntoKtrk > 0.5, 261.63, keyHz)
       * pow(2.0, ntoCoarse/12.0 + ntoFine/1200.0 + dNtoPitch*2.0));
  flin = dNtoLin * f0 * 3.0;
  ntoO = nto(f0, ntoShape + dNtoShape, dNtoSync, flin);
  nsinN = ntoO : (_,!,!);  nsawN = ntoO : (!,_,!);  nvarN = ntoO : (!,!,_);

  // ----------------- DUSG 1 & 2 ---------------------------------------
  r1 = max(0.00002, min(30.0, d1Rise * pow(2.0, -dRise1*5.0)));
  f1 = max(0.00002, min(30.0, d1Fall * pow(2.0, -dFall1*5.0)));
  du1 = dusgSG(r1, f1, d1Cyc, dTrig1, dIn1);
  d1oN = du1 : (_,!);  d1eN = du1 : (!,_);
  r2 = max(0.00002, min(30.0, d2Rise * pow(2.0, -dRise2*5.0)));
  f2 = max(0.00002, min(30.0, d2Fall * pow(2.0, -dFall2*5.0)));
  du2 = dusgSG(r2, f2, d2Cyc, dTrig2, dIn2);
  d2oN = du2 : (_,!);  d2eN = du2 : (!,_);

  // ----------------- SSG ----------------------------------------------
  smr = max(0.00002, min(30.0, smRate * pow(2.0, -dSRate*5.0)));
  ssmN = ssgSmooth(smr, dHold, dSmIn);
  sstN = ssgStep(stRate, dStClk, dStIn);
  scpN = 2.0*(ssmN > sstN) - 1.0;

  // ----------------- VCFQ ----------------------------------------------
  fc = vcfFreq * pow(2.0, dFcv*5.0);
  qe = max(0.5, min(60.0, vcfQ * pow(2.0, dQcv*3.0)));
  ping = edgeup(dPing) * vcfPing * 3.0;
  fO = svf(fc, 1.0/qe, t3(dVcfIn * vcfGain) + ping);
  flpN = fO : (_,!,!);  fbpN = fO : (!,_,!);  fhpN = fO : (!,!,_);

  // ----------------- VCM (wave multipliers) ---------------------------
  gm = min(16.0, 0.5*pow(2.0, vcmFold*4.0 + dFold*2.0));
  vtpN = t3(dVcmIn * vcmTop);
  vmdN = sin(ma.PI*0.5*(gm*dVcmIn + vcmBias));
  vbtN = 4.0*abs(ma.frac(0.5*gm*dVcmIn + 0.25) - 0.5) - 1.0;

  // ----------------- RING / NOISE / RVG --------------------------------
  rngN = t3(dRingX * dRingY * ringGain * 2.0);
  nzwN = no.noises(4,0);
  nzpN = no.pink_noise * 2.0;
  rimp = os.lf_imptrain(rvgRate);
  rvsN = latch(no.noises(4,1), rimp);
  rvmN = rvsN : si.smooth(ba.tau2pole(0.5/rvgRate));

  // ----------------- VCAs / output mixer --------------------------------
  v1oN = vca(dV1in, dV1cv, vca1Lvl, vca1Exp);
  v2oN = vca(dV2in, dV2cv, vca2Lvl, vca2Exp);
  mon = pick(monSel)*monLvl;
  mL = (v1oN*sqrt(1.0 - pan1) + v2oN*sqrt(1.0 - pan2) + mon*0.7)*master*midiGain;
  mR = (v1oN*sqrt(pan1) + v2oN*sqrt(pan2) + mon*0.7)*master*midiGain;
  outL = mL : fi.dcblocker : t3;
  outR = mR : fi.dcblocker : t3;
};

process = (big ~ si.bus(30)) : (si.block(30), _, _);
'''

import os

src = HEADER.replace("__SLOTS__", SLOTS).replace("__MENU__", MENU)
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "SergeAnimalTKB.dsp")
open(out, "w").write(src)
print("wrote", len(src), "bytes to", out)
