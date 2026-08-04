// =====================================================================
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
// Build (macOS AU/VST3):  faust2juce -midi -nvoices 1 SergeAnimalTKB.dsp
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
midiFreq = hslider("h:[00]KEY/[0]freq[unit:Hz]", 261.63, 20, 8000, 0.01);
midiGate = button("h:[00]KEY/[1]gate");
midiGain = hslider("h:[00]KEY/[2]gain", 0.8, 0, 1, 0.01) : si.smoo;
tkbMode  = checkbox("h:[00]KEY/[3]TKB mode");
glide    = hslider("h:[00]KEY/[4]glide[unit:s]", 0.0, 0, 0.5, 0.001);

ntoCoarse = hslider("h:[01]NTO/[0]coarse[unit:semi]", 0, -24, 24, 0.01);
ntoFine   = hslider("h:[01]NTO/[1]fine[unit:cent]", 0, -100, 100, 0.1);
ntoShape  = hslider("h:[01]NTO/[2]shape", 0.15, 0, 1, 0.001) : si.smoo;
ntoKtrk   = checkbox("h:[01]NTO/[3]keytrack") + 0*checkbox("h:[01]NTO/[4]init_on");

d1Rise = hslider("h:[02]DUSG1/[0]rise[unit:s][scale:log]", 0.001, 0.00005, 10, 0.00001);
d1Fall = hslider("h:[02]DUSG1/[1]fall[unit:s][scale:log]", 0.25, 0.00005, 10, 0.00001);
d1Cyc  = checkbox("h:[02]DUSG1/[2]cycle");

d2Rise = hslider("h:[03]DUSG2/[0]rise[unit:s][scale:log]", 0.05, 0.00005, 10, 0.00001);
d2Fall = hslider("h:[03]DUSG2/[1]fall[unit:s][scale:log]", 0.05, 0.00005, 10, 0.00001);
d2Cyc  = checkbox("h:[03]DUSG2/[2]cycle");

smRate = hslider("h:[04]SSG/[0]smooth rate[unit:s][scale:log]", 0.05, 0.00005, 10, 0.00001);
stRate = hslider("h:[04]SSG/[1]stepped rate[unit:s][scale:log]", 0.001, 0.00005, 10, 0.00001);

vcfFreq = hslider("h:[05]VCFQ/[0]freq[unit:Hz][scale:log]", 800, 20, 12000, 0.01) : si.smoo;
vcfQ    = hslider("h:[05]VCFQ/[1]Q[scale:log]", 2, 0.5, 40, 0.01) : si.smoo;
vcfGain = hslider("h:[05]VCFQ/[2]input gain", 1.0, 0, 2, 0.001) : si.smoo;
vcfPing = hslider("h:[05]VCFQ/[3]ping level", 0.6, 0, 1, 0.001);

vcmTop  = hslider("h:[06]VCM/[0]top drive", 2, 1, 10, 0.01) : si.smoo;
vcmFold = hslider("h:[06]VCM/[1]fold", 0.3, 0, 1, 0.001) : si.smoo;
vcmBias = hslider("h:[06]VCM/[2]bias", 0, -1, 1, 0.001) : si.smoo;

ringGain = hslider("h:[07]RING-NOISE/[0]ring gain", 1, 0.5, 4, 0.01) : si.smoo;
rvgRate  = hslider("h:[07]RING-NOISE/[1]RVG rate[unit:Hz][scale:log]", 4, 0.05, 50, 0.01);

tkbOn    = checkbox("h:[08]TKB/h:[0]clock/[0]run");
tkbRate  = hslider("h:[08]TKB/h:[0]clock/[1]rate[unit:Hz][scale:log]", 2, 0.1, 30, 0.01);
tkbLen   = hslider("h:[08]TKB/h:[0]clock/[2]length", 8, 1, 16, 1);
tkbRnd   = checkbox("h:[08]TKB/h:[0]clock/[3]random");
tkbGateL = hslider("h:[08]TKB/h:[0]clock/[4]gate len[unit:s][scale:log]", 0.1, 0.005, 1, 0.001);
tkbOct   = hslider("h:[08]TKB/h:[0]clock/[5]range[unit:oct]", 2, 0.5, 5, 0.1);
rowA(j)  = vslider("h:[08]TKB/h:[1]Row A/[%2j]A%j", 0.5, 0, 1, 0.001) : si.smoo;
rowB(j)  = vslider("h:[08]TKB/h:[2]Row B/[%2j]B%j", 0.5, 0, 1, 0.001) : si.smoo;
rowC(j)  = vslider("h:[08]TKB/h:[3]Row C/[%2j]C%j", 0.5, 0, 1, 0.001) : si.smoo;
rowD(j)  = vslider("h:[08]TKB/h:[4]Row D/[%2j]D%j", 0.5, 0, 1, 0.001) : si.smoo;

vca1Lvl = hslider("h:[09]OUT/[0]vca1 level", 0, 0, 1, 0.001) : si.smoo;
vca1Exp = checkbox("h:[09]OUT/[1]vca1 exp");
vca2Lvl = hslider("h:[09]OUT/[2]vca2 level", 0, 0, 1, 0.001) : si.smoo;
vca2Exp = checkbox("h:[09]OUT/[3]vca2 exp");
pan1    = hslider("h:[09]OUT/[4]pan1", 0.35, 0, 1, 0.001) : si.smoo;
pan2    = hslider("h:[09]OUT/[5]pan2", 0.65, 0, 1, 0.001) : si.smoo;
monSel  = nentry("h:[09]OUT/[6]monitor src", 0, 0, 30, 1);
monLvl  = hslider("h:[09]OUT/[7]monitor level", 0.4, 0, 1, 0.001) : si.smoo;
master  = hslider("h:[09]OUT/[8]master", 0.8, 0, 1, 0.001) : si.smoo;

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
  dNtoPitchA_s = nentry("t:[10]PATCH/t:[00]NTO/[0]pitch FM A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dNtoPitchA_a = hslider("t:[10]PATCH/t:[00]NTO/[1]pitch FM A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dNtoPitchB_s = nentry("t:[10]PATCH/t:[00]NTO/[2]pitch FM B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dNtoPitchB_a = hslider("t:[10]PATCH/t:[00]NTO/[3]pitch FM B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dNtoPitch = pick(dNtoPitchA_s)*dNtoPitchA_a + pick(dNtoPitchB_s)*dNtoPitchB_a;

  dNtoLinA_s = nentry("t:[10]PATCH/t:[00]NTO/[0]lin FM A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dNtoLinA_a = hslider("t:[10]PATCH/t:[00]NTO/[1]lin FM A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dNtoLinB_s = nentry("t:[10]PATCH/t:[00]NTO/[2]lin FM B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dNtoLinB_a = hslider("t:[10]PATCH/t:[00]NTO/[3]lin FM B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dNtoLin = pick(dNtoLinA_s)*dNtoLinA_a + pick(dNtoLinB_s)*dNtoLinB_a;

  dNtoShapeA_s = nentry("t:[10]PATCH/t:[00]NTO/[0]shape A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dNtoShapeA_a = hslider("t:[10]PATCH/t:[00]NTO/[1]shape A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dNtoShapeB_s = nentry("t:[10]PATCH/t:[00]NTO/[2]shape B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dNtoShapeB_a = hslider("t:[10]PATCH/t:[00]NTO/[3]shape B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dNtoShape = pick(dNtoShapeA_s)*dNtoShapeA_a + pick(dNtoShapeB_s)*dNtoShapeB_a;

  dNtoSyncA_s = nentry("t:[10]PATCH/t:[00]NTO/[0]sync A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dNtoSyncA_a = hslider("t:[10]PATCH/t:[00]NTO/[1]sync A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dNtoSyncB_s = nentry("t:[10]PATCH/t:[00]NTO/[2]sync B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dNtoSyncB_a = hslider("t:[10]PATCH/t:[00]NTO/[3]sync B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dNtoSync = pick(dNtoSyncA_s)*dNtoSyncA_a + pick(dNtoSyncB_s)*dNtoSyncB_a;

  dTrig1A_s = nentry("t:[10]PATCH/t:[01]DUSG1/[0]trig A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 28, 0, 30, 1);
  dTrig1A_a = hslider("t:[10]PATCH/t:[01]DUSG1/[1]trig A amt", 1.0, -1, 1, 0.001) : si.smoo;
  dTrig1B_s = nentry("t:[10]PATCH/t:[01]DUSG1/[2]trig B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dTrig1B_a = hslider("t:[10]PATCH/t:[01]DUSG1/[3]trig B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dTrig1 = pick(dTrig1A_s)*dTrig1A_a + pick(dTrig1B_s)*dTrig1B_a;

  dIn1A_s = nentry("t:[10]PATCH/t:[01]DUSG1/[0]signal in A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dIn1A_a = hslider("t:[10]PATCH/t:[01]DUSG1/[1]signal in A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dIn1B_s = nentry("t:[10]PATCH/t:[01]DUSG1/[2]signal in B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dIn1B_a = hslider("t:[10]PATCH/t:[01]DUSG1/[3]signal in B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dIn1 = pick(dIn1A_s)*dIn1A_a + pick(dIn1B_s)*dIn1B_a;

  dRise1A_s = nentry("t:[10]PATCH/t:[01]DUSG1/[0]rise CV A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dRise1A_a = hslider("t:[10]PATCH/t:[01]DUSG1/[1]rise CV A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dRise1B_s = nentry("t:[10]PATCH/t:[01]DUSG1/[2]rise CV B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dRise1B_a = hslider("t:[10]PATCH/t:[01]DUSG1/[3]rise CV B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dRise1 = pick(dRise1A_s)*dRise1A_a + pick(dRise1B_s)*dRise1B_a;

  dFall1A_s = nentry("t:[10]PATCH/t:[01]DUSG1/[0]fall CV A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dFall1A_a = hslider("t:[10]PATCH/t:[01]DUSG1/[1]fall CV A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dFall1B_s = nentry("t:[10]PATCH/t:[01]DUSG1/[2]fall CV B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dFall1B_a = hslider("t:[10]PATCH/t:[01]DUSG1/[3]fall CV B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dFall1 = pick(dFall1A_s)*dFall1A_a + pick(dFall1B_s)*dFall1B_a;

  dTrig2A_s = nentry("t:[10]PATCH/t:[02]DUSG2/[0]trig A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dTrig2A_a = hslider("t:[10]PATCH/t:[02]DUSG2/[1]trig A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dTrig2B_s = nentry("t:[10]PATCH/t:[02]DUSG2/[2]trig B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dTrig2B_a = hslider("t:[10]PATCH/t:[02]DUSG2/[3]trig B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dTrig2 = pick(dTrig2A_s)*dTrig2A_a + pick(dTrig2B_s)*dTrig2B_a;

  dIn2A_s = nentry("t:[10]PATCH/t:[02]DUSG2/[0]signal in A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dIn2A_a = hslider("t:[10]PATCH/t:[02]DUSG2/[1]signal in A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dIn2B_s = nentry("t:[10]PATCH/t:[02]DUSG2/[2]signal in B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dIn2B_a = hslider("t:[10]PATCH/t:[02]DUSG2/[3]signal in B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dIn2 = pick(dIn2A_s)*dIn2A_a + pick(dIn2B_s)*dIn2B_a;

  dRise2A_s = nentry("t:[10]PATCH/t:[02]DUSG2/[0]rise CV A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dRise2A_a = hslider("t:[10]PATCH/t:[02]DUSG2/[1]rise CV A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dRise2B_s = nentry("t:[10]PATCH/t:[02]DUSG2/[2]rise CV B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dRise2B_a = hslider("t:[10]PATCH/t:[02]DUSG2/[3]rise CV B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dRise2 = pick(dRise2A_s)*dRise2A_a + pick(dRise2B_s)*dRise2B_a;

  dFall2A_s = nentry("t:[10]PATCH/t:[02]DUSG2/[0]fall CV A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dFall2A_a = hslider("t:[10]PATCH/t:[02]DUSG2/[1]fall CV A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dFall2B_s = nentry("t:[10]PATCH/t:[02]DUSG2/[2]fall CV B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dFall2B_a = hslider("t:[10]PATCH/t:[02]DUSG2/[3]fall CV B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dFall2 = pick(dFall2A_s)*dFall2A_a + pick(dFall2B_s)*dFall2B_a;

  dSmInA_s = nentry("t:[10]PATCH/t:[03]SSG/[0]smooth in A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dSmInA_a = hslider("t:[10]PATCH/t:[03]SSG/[1]smooth in A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dSmInB_s = nentry("t:[10]PATCH/t:[03]SSG/[2]smooth in B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dSmInB_a = hslider("t:[10]PATCH/t:[03]SSG/[3]smooth in B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dSmIn = pick(dSmInA_s)*dSmInA_a + pick(dSmInB_s)*dSmInB_a;

  dSRateA_s = nentry("t:[10]PATCH/t:[03]SSG/[0]smooth rate CV A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dSRateA_a = hslider("t:[10]PATCH/t:[03]SSG/[1]smooth rate CV A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dSRateB_s = nentry("t:[10]PATCH/t:[03]SSG/[2]smooth rate CV B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dSRateB_a = hslider("t:[10]PATCH/t:[03]SSG/[3]smooth rate CV B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dSRate = pick(dSRateA_s)*dSRateA_a + pick(dSRateB_s)*dSRateB_a;

  dHoldA_s = nentry("t:[10]PATCH/t:[03]SSG/[0]hold A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dHoldA_a = hslider("t:[10]PATCH/t:[03]SSG/[1]hold A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dHoldB_s = nentry("t:[10]PATCH/t:[03]SSG/[2]hold B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dHoldB_a = hslider("t:[10]PATCH/t:[03]SSG/[3]hold B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dHold = pick(dHoldA_s)*dHoldA_a + pick(dHoldB_s)*dHoldB_a;

  dStInA_s = nentry("t:[10]PATCH/t:[03]SSG/[0]stepped in A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dStInA_a = hslider("t:[10]PATCH/t:[03]SSG/[1]stepped in A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dStInB_s = nentry("t:[10]PATCH/t:[03]SSG/[2]stepped in B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dStInB_a = hslider("t:[10]PATCH/t:[03]SSG/[3]stepped in B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dStIn = pick(dStInA_s)*dStInA_a + pick(dStInB_s)*dStInB_a;

  dStClkA_s = nentry("t:[10]PATCH/t:[03]SSG/[0]stepped clk A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dStClkA_a = hslider("t:[10]PATCH/t:[03]SSG/[1]stepped clk A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dStClkB_s = nentry("t:[10]PATCH/t:[03]SSG/[2]stepped clk B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dStClkB_a = hslider("t:[10]PATCH/t:[03]SSG/[3]stepped clk B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dStClk = pick(dStClkA_s)*dStClkA_a + pick(dStClkB_s)*dStClkB_a;

  dVcfInA_s = nentry("t:[10]PATCH/t:[04]VCFQ/[0]audio in A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 15, 0, 30, 1);
  dVcfInA_a = hslider("t:[10]PATCH/t:[04]VCFQ/[1]audio in A amt", 0.8, -1, 1, 0.001) : si.smoo;
  dVcfInB_s = nentry("t:[10]PATCH/t:[04]VCFQ/[2]audio in B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dVcfInB_a = hslider("t:[10]PATCH/t:[04]VCFQ/[3]audio in B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dVcfIn = pick(dVcfInA_s)*dVcfInA_a + pick(dVcfInB_s)*dVcfInB_a;

  dFcvA_s = nentry("t:[10]PATCH/t:[04]VCFQ/[0]freq CV A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dFcvA_a = hslider("t:[10]PATCH/t:[04]VCFQ/[1]freq CV A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dFcvB_s = nentry("t:[10]PATCH/t:[04]VCFQ/[2]freq CV B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dFcvB_a = hslider("t:[10]PATCH/t:[04]VCFQ/[3]freq CV B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dFcv = pick(dFcvA_s)*dFcvA_a + pick(dFcvB_s)*dFcvB_a;

  dQcvA_s = nentry("t:[10]PATCH/t:[04]VCFQ/[0]Q CV A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dQcvA_a = hslider("t:[10]PATCH/t:[04]VCFQ/[1]Q CV A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dQcvB_s = nentry("t:[10]PATCH/t:[04]VCFQ/[2]Q CV B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dQcvB_a = hslider("t:[10]PATCH/t:[04]VCFQ/[3]Q CV B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dQcv = pick(dQcvA_s)*dQcvA_a + pick(dQcvB_s)*dQcvB_a;

  dPingA_s = nentry("t:[10]PATCH/t:[04]VCFQ/[0]ping trig A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dPingA_a = hslider("t:[10]PATCH/t:[04]VCFQ/[1]ping trig A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dPingB_s = nentry("t:[10]PATCH/t:[04]VCFQ/[2]ping trig B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dPingB_a = hslider("t:[10]PATCH/t:[04]VCFQ/[3]ping trig B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dPing = pick(dPingA_s)*dPingA_a + pick(dPingB_s)*dPingB_a;

  dVcmInA_s = nentry("t:[10]PATCH/t:[05]VCM/[0]audio in A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 3, 0, 30, 1);
  dVcmInA_a = hslider("t:[10]PATCH/t:[05]VCM/[1]audio in A amt", 0.8, -1, 1, 0.001) : si.smoo;
  dVcmInB_s = nentry("t:[10]PATCH/t:[05]VCM/[2]audio in B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dVcmInB_a = hslider("t:[10]PATCH/t:[05]VCM/[3]audio in B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dVcmIn = pick(dVcmInA_s)*dVcmInA_a + pick(dVcmInB_s)*dVcmInB_a;

  dFoldA_s = nentry("t:[10]PATCH/t:[05]VCM/[0]fold CV A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dFoldA_a = hslider("t:[10]PATCH/t:[05]VCM/[1]fold CV A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dFoldB_s = nentry("t:[10]PATCH/t:[05]VCM/[2]fold CV B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dFoldB_a = hslider("t:[10]PATCH/t:[05]VCM/[3]fold CV B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dFold = pick(dFoldA_s)*dFoldA_a + pick(dFoldB_s)*dFoldB_a;

  dRingXA_s = nentry("t:[10]PATCH/t:[06]RING/[0]X in A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dRingXA_a = hslider("t:[10]PATCH/t:[06]RING/[1]X in A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dRingXB_s = nentry("t:[10]PATCH/t:[06]RING/[2]X in B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dRingXB_a = hslider("t:[10]PATCH/t:[06]RING/[3]X in B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dRingX = pick(dRingXA_s)*dRingXA_a + pick(dRingXB_s)*dRingXB_a;

  dRingYA_s = nentry("t:[10]PATCH/t:[06]RING/[0]Y in A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dRingYA_a = hslider("t:[10]PATCH/t:[06]RING/[1]Y in A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dRingYB_s = nentry("t:[10]PATCH/t:[06]RING/[2]Y in B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dRingYB_a = hslider("t:[10]PATCH/t:[06]RING/[3]Y in B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dRingY = pick(dRingYA_s)*dRingYA_a + pick(dRingYB_s)*dRingYB_a;

  dV1inA_s = nentry("t:[10]PATCH/t:[07]VCA1/[0]audio in A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 11, 0, 30, 1);
  dV1inA_a = hslider("t:[10]PATCH/t:[07]VCA1/[1]audio in A amt", 1.0, -1, 1, 0.001) : si.smoo;
  dV1inB_s = nentry("t:[10]PATCH/t:[07]VCA1/[2]audio in B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dV1inB_a = hslider("t:[10]PATCH/t:[07]VCA1/[3]audio in B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dV1in = pick(dV1inA_s)*dV1inA_a + pick(dV1inB_s)*dV1inB_a;

  dV1cvA_s = nentry("t:[10]PATCH/t:[07]VCA1/[0]CV A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 4, 0, 30, 1);
  dV1cvA_a = hslider("t:[10]PATCH/t:[07]VCA1/[1]CV A amt", 1.0, -1, 1, 0.001) : si.smoo;
  dV1cvB_s = nentry("t:[10]PATCH/t:[07]VCA1/[2]CV B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dV1cvB_a = hslider("t:[10]PATCH/t:[07]VCA1/[3]CV B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dV1cv = pick(dV1cvA_s)*dV1cvA_a + pick(dV1cvB_s)*dV1cvB_a;

  dV2inA_s = nentry("t:[10]PATCH/t:[08]VCA2/[0]audio in A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dV2inA_a = hslider("t:[10]PATCH/t:[08]VCA2/[1]audio in A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dV2inB_s = nentry("t:[10]PATCH/t:[08]VCA2/[2]audio in B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dV2inB_a = hslider("t:[10]PATCH/t:[08]VCA2/[3]audio in B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dV2in = pick(dV2inA_s)*dV2inA_a + pick(dV2inB_s)*dV2inB_a;

  dV2cvA_s = nentry("t:[10]PATCH/t:[08]VCA2/[0]CV A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dV2cvA_a = hslider("t:[10]PATCH/t:[08]VCA2/[1]CV A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dV2cvB_s = nentry("t:[10]PATCH/t:[08]VCA2/[2]CV B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dV2cvB_a = hslider("t:[10]PATCH/t:[08]VCA2/[3]CV B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dV2cv = pick(dV2cvA_s)*dV2cvA_a + pick(dV2cvB_s)*dV2cvB_a;

  dTkbClkA_s = nentry("t:[10]PATCH/t:[09]TKB/[0]ext clock A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dTkbClkA_a = hslider("t:[10]PATCH/t:[09]TKB/[1]ext clock A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dTkbClkB_s = nentry("t:[10]PATCH/t:[09]TKB/[2]ext clock B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dTkbClkB_a = hslider("t:[10]PATCH/t:[09]TKB/[3]ext clock B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dTkbClk = pick(dTkbClkA_s)*dTkbClkA_a + pick(dTkbClkB_s)*dTkbClkB_a;

  dTkbRstA_s = nentry("t:[10]PATCH/t:[09]TKB/[0]reset A[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dTkbRstA_a = hslider("t:[10]PATCH/t:[09]TKB/[1]reset A amt", 0.0, -1, 1, 0.001) : si.smoo;
  dTkbRstB_s = nentry("t:[10]PATCH/t:[09]TKB/[2]reset B[style:menu{'OFF':0;'NTO sin':1;'NTO saw':2;'NTO var':3;'DSG1 out':4;'DSG1 end':5;'DSG2 out':6;'DSG2 end':7;'SSG smooth':8;'SSG stepped':9;'SSG coupler':10;'VCFQ LP':11;'VCFQ BP':12;'VCFQ HP':13;'VCM top':14;'VCM mid':15;'VCM bot':16;'RING':17;'Noise wht':18;'Noise pnk':19;'RVG stepped':20;'RVG smooth':21;'TKB A':22;'TKB B':23;'TKB C':24;'TKB D':25;'TKB gate':26;'KEY pitch':27;'KEY gate':28;'VCA1':29;'VCA2':30}]", 0, 0, 30, 1);
  dTkbRstB_a = hslider("t:[10]PATCH/t:[09]TKB/[3]reset B amt", 0.0, -1, 1, 0.001) : si.smoo;
  dTkbRst = pick(dTkbRstA_s)*dTkbRstA_a + pick(dTkbRstB_s)*dTkbRstB_a;


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
