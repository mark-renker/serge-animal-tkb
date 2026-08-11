// End-to-end check of the INSTALLED AU: instantiate it, send a MIDI note,
// render, and report level.
//
// Note 60 (C4 = 261.63 Hz) is deliberate -- at C4 the DSP's KEY pitch CV is
// exactly 0, so if DUSG1's trigger source were wrongly set to "KEY pitch"
// instead of "KEY gate" the envelope would never fire and the output would be
// silent. Non-zero RMS therefore proves the patch-matrix defaults survived
// into the running plugin.
//
//   au_render_test                      render with defaults
//   au_render_test --list               dump the AU parameter list
//   au_render_test --set <idx> <val>    set a parameter first, then render
//
// cc -O2 -o au_render_test au_render_test.c -framework AudioToolbox \
//    -framework AudioUnit -framework CoreFoundation
#include <AudioToolbox/AudioToolbox.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SR      48000.0
#define BLOCK   512
#define NBLOCKS 94          // ~1 second

int main(int argc, char **argv) {
    int   doList = (argc > 1 && strcmp(argv[1], "--list") == 0);
    int   setIdx = -1;
    float setVal = 0.0f;
    if (argc > 3 && strcmp(argv[1], "--set") == 0) {
        setIdx = atoi(argv[2]);
        setVal = (float)atof(argv[3]);
    }

    AudioComponentDescription desc = {
        .componentType         = kAudioUnitType_MusicDevice,
        .componentSubType      = 'Satk',
        .componentManufacturer = 'Rnky',
    };

    AudioComponent comp = AudioComponentFindNext(NULL, &desc);
    if (!comp) { fprintf(stderr, "AU not found\n"); return 2; }

    AudioUnit au;
    if (AudioComponentInstanceNew(comp, &au)) { fprintf(stderr, "InstanceNew failed\n"); return 2; }

    AudioStreamBasicDescription fmt = {
        .mSampleRate       = SR,
        .mFormatID         = kAudioFormatLinearPCM,
        .mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked
                             | kAudioFormatFlagIsNonInterleaved,
        .mFramesPerPacket  = 1,
        .mChannelsPerFrame = 2,
        .mBitsPerChannel   = 32,
        .mBytesPerFrame    = 4,
        .mBytesPerPacket   = 4,
    };
    if (AudioUnitSetProperty(au, kAudioUnitProperty_StreamFormat,
                             kAudioUnitScope_Output, 0, &fmt, sizeof(fmt))) {
        fprintf(stderr, "SetFormat failed\n"); return 2;
    }

    UInt32 maxFrames = BLOCK;
    AudioUnitSetProperty(au, kAudioUnitProperty_MaximumFramesPerSlice,
                         kAudioUnitScope_Global, 0, &maxFrames, sizeof(maxFrames));

    if (AudioUnitInitialize(au)) { fprintf(stderr, "Initialize failed\n"); return 2; }

    if (doList) {
        UInt32 sz = 0;
        AudioUnitGetPropertyInfo(au, kAudioUnitProperty_ParameterList,
                                 kAudioUnitScope_Global, 0, &sz, NULL);
        int n = (int)(sz / sizeof(AudioUnitParameterID));
        AudioUnitParameterID *ids = malloc(sz);
        AudioUnitGetProperty(au, kAudioUnitProperty_ParameterList,
                             kAudioUnitScope_Global, 0, ids, &sz);
        printf("%d parameters\n", n);
        for (int i = 0; i < n; i++) {
            AudioUnitParameterInfo info;
            UInt32 isz = sizeof(info);
            if (AudioUnitGetProperty(au, kAudioUnitProperty_ParameterInfo,
                                     kAudioUnitScope_Global, ids[i], &info, &isz)) continue;
            AudioUnitParameterValue v = 0;
            AudioUnitGetParameter(au, ids[i], kAudioUnitScope_Global, 0, &v);
            char nm[64] = "";
            if (info.cfNameString)
                CFStringGetCString(info.cfNameString, nm, sizeof(nm), kCFStringEncodingUTF8);
            printf("  id=%u  %-24s min=%-8g max=%-8g cur=%g\n",
                   (unsigned)ids[i], nm, info.minValue, info.maxValue, v);
        }
        free(ids);
        AudioUnitUninitialize(au); AudioComponentInstanceDispose(au);
        return 0;
    }

    if (setIdx >= 0) {
        OSStatus e = AudioUnitSetParameter(au, (AudioUnitParameterID)setIdx,
                                           kAudioUnitScope_Global, 0, setVal, 0);
        AudioUnitParameterValue back = -1;
        AudioUnitGetParameter(au, (AudioUnitParameterID)setIdx, kAudioUnitScope_Global, 0, &back);
        printf("set param %d = %g (err=%d, reads back %g)\n", setIdx, setVal, (int)e, back);
    }

    float bufL[BLOCK], bufR[BLOCK];
    struct { UInt32 n; AudioBuffer b[2]; } abl = {
        .n = 2,
        .b = {{ .mNumberChannels = 1, .mDataByteSize = BLOCK * 4, .mData = bufL },
              { .mNumberChannels = 1, .mDataByteSize = BLOCK * 4, .mData = bufR }},
    };

    AudioUnitRenderActionFlags flags = 0;
    AudioTimeStamp ts = { .mFlags = kAudioTimeStampSampleTimeValid, .mSampleTime = 0 };

    double sumsq = 0.0, peak = 0.0;
    long   n = 0, bad = 0;

    for (int b = 0; b < NBLOCKS; b++) {
        if (b == 2)  MusicDeviceMIDIEvent(au, 0x90, 60, 100, 0);   // note on, C4
        if (b == 80) MusicDeviceMIDIEvent(au, 0x80, 60, 0, 0);     // note off

        abl.b[0].mDataByteSize = abl.b[1].mDataByteSize = BLOCK * 4;
        if (AudioUnitRender(au, &flags, &ts, 0, BLOCK, (AudioBufferList *)&abl)) {
            fprintf(stderr, "Render failed at block %d\n", b); return 2;
        }
        ts.mSampleTime += BLOCK;

        if (b < 2) continue;   // skip pre-note silence
        for (int c = 0; c < 2; c++) {
            const float *p = (c == 0) ? bufL : bufR;
            for (int i = 0; i < BLOCK; i++) {
                float v = p[i];
                if (isnan(v) || isinf(v)) { bad++; continue; }
                sumsq += (double)v * v;
                if (fabs(v) > peak) peak = fabs(v);
                n++;
            }
        }
    }

    double rms = sqrt(sumsq / (n ? n : 1));
    int audible = (bad == 0 && rms > 1e-5);
    printf("rendered=%ld nan/inf=%ld rms=%.6f peak=%.6f -> %s\n",
           n, bad, rms, peak, audible ? "AUDIBLE, clean" : "silent/!!");

    AudioUnitUninitialize(au);
    AudioComponentInstanceDispose(au);
    return audible ? 0 : 1;
}
