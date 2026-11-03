// Serge Animal TKB v2 — plugin shell implementation.
// PF50 pattern with the frozen REDESIGN_PLAN §3 upgrades:
//  - APVTS param per ctl, append-only IDs ("{moduleId}_{ctlIndex}") but human-
//    readable names + real choice labels + an AudioProcessorParameterGroup per
//    module.
//  - Block-rate ctl copy; SpinLock cable edits (try-lock: a colliding audio
//    block renders silence rather than glitching).
//  - State = params + cables + editor scale (float) + engine instance seed.
//  - Mono last-note MIDI -> KEY fields; the engine ticks UNCONDITIONALLY
//    (fixes v1's voice-reclamation bug that killed generative patches).
#include "PluginProcessor.h"
#include "../ui/MainView.h"

using namespace satk;

juce::String SatkProcessor::paramID (const Module& m, int c)
{
    return juce::String (m.id) + "_" + juce::String (c);
}

static juce::String paramName (const Module& m, int c)
{
    return juce::String (m.id).toUpperCase() + " " + m.ctls[(size_t) c].name;
}

juce::AudioProcessorValueTreeState::ParameterLayout SatkProcessor::makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (auto& m : sys.engine.mods)
    {
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> (
            juce::String (m->id), juce::String (m->title), " | ");
        for (int c = 0; c < (int) m->ctls.size(); ++c)
        {
            auto& ct = m->ctls[(size_t) c];
            if (ct.steps > 0)
            {
                juce::StringArray labels;
                for (int i = 0; i < ct.steps; ++i)
                    labels.add (ct.labels != nullptr ? juce::String (ct.labels[i])
                                                     : juce::String (i + 1));
                group->addChild (std::make_unique<juce::AudioParameterChoice> (
                    juce::ParameterID (paramID (*m, c), 1), paramName (*m, c),
                    labels, (int) std::lround (ct.def)));
            }
            else
            {
                group->addChild (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID (paramID (*m, c), 1), paramName (*m, c),
                    juce::NormalisableRange<float> (0.0f, 1.0f, 0.0f), ct.def));
            }
        }
        layout.add (std::move (group));
    }
    return layout;
}

SatkProcessor::SatkProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SATKPARAMS", makeLayout())
{
    for (auto& m : sys.engine.mods)
        for (int c = 0; c < (int) m->ctls.size(); ++c)
        {
            paramPtrs.push_back (apvts.getRawParameterValue (paramID (*m, c)));
            if (m->ctls[(size_t) c].steps == 2) // momentary-capable buttons / pads
            {
                auto mf = std::make_unique<MomentaryFlag>();
                mf->mod = m.get(); mf->ctl = c;
                momentaries.push_back (std::move (mf));
            }
        }
    sys.patchDefaults(); // v1's out-of-the-box voice (docs/SPEC.md §6)
}

bool SatkProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SatkProcessor::prepareToPlay (double sr, int)
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    sys.prepare (sr);
}

void SatkProcessor::applyParams()
{
    size_t i = 0;
    for (auto& m : sys.engine.mods)
        for (auto& c : m->ctls)
        {
            // getRawParameterValue returns the denormalized value: for choice
            // params that is already the index (0..steps-1), which is exactly
            // what the engine's ctls expect.
            c.v = paramPtrs[i++]->load();
        }
    // momentary overrides (buttons / TKB pads held with the mouse)
    for (auto& mf : momentaries)
        if (mf->down.load()) mf->mod->ctls[(size_t) mf->ctl].v = 1.0f;
}

void SatkProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buf.getNumSamples();
    auto* key = sys.key;
    auto* out = sys.out;

    const juce::SpinLock::ScopedTryLockType sl (engineLock);
    if (! sl.isLocked()) { buf.clear(); midi.clear(); return; }

    applyParams();

    // host clock -> 16th-note pulses on KEY CLK
    double bpm = 120.0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (pos->getBpm()) bpm = *pos->getBpm();
            playing = pos->getIsPlaying();
        }
    const double sr = getSampleRate();
    const double sixteenthHz = bpm / 60.0 * 4.0;

    float* outL = buf.getWritePointer (0);
    float* outR = buf.getNumChannels() > 1 ? buf.getWritePointer (1) : nullptr;

    auto midiIt = midi.begin();
    if (playing && ! wasPlaying) clockPhase = 0.0;
    wasPlaying = playing;

    for (int s = 0; s < n; ++s)
    {
        while (midiIt != midi.end() && (*midiIt).samplePosition <= s)
        {
            auto msg = (*midiIt).getMessage();
            if (msg.isNoteOn())
            {
                notesDown.add (msg.getNoteNumber());
                heldNote = msg.getNoteNumber();
                key->velV = msg.getFloatVelocity() * kCV; // held until next note-on (v1)
            }
            else if (msg.isNoteOff())
            {
                notesDown.removeValue (msg.getNoteNumber());
                if (notesDown.size() > 0) heldNote = notesDown.getLast();
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                notesDown.clear();
            ++midiIt;
        }
        key->gateV  = notesDown.size() > 0 ? kCV : 0.0f;
        key->pitchV = heldNote >= 0 ? (float) (heldNote - 60) / 12.0f : 0.0f; // 1 V/oct, C4 = 0 V

        if (playing)
        {
            clockPhase += sixteenthHz / sr;
            if (clockPhase >= 1.0) clockPhase -= 1.0;
            key->clkV = clockPhase < 0.15 ? kCV : 0.0f;
        }
        else key->clkV = 0.0f;

        sys.engine.tick(); // unconditional — generative patches keep running

        outL[s] = out->outL;
        if (outR != nullptr) outR[s] = out->outR;
    }
    midi.clear();
}

// ------------------------------------------------------------- cables -------
bool SatkProcessor::addCable (int mA, int jA, int mB, int jB)
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    bool ok = sys.engine.connect (mA, jA, mB, jB);
    if (ok) ++cableVer;
    return ok;
}
void SatkProcessor::removeCablesAt (int m, int j)
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    sys.engine.disconnectAt (m, j); ++cableVer;
}
void SatkProcessor::removeCable (const Cable& c)
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    sys.engine.removeCable (c); ++cableVer;
}
void SatkProcessor::clearAllCables()
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    sys.engine.clearCables(); ++cableVer;
}
std::vector<Cable> SatkProcessor::cableSnapshot()
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    return sys.engine.cables;
}
void SatkProcessor::setMomentary (Module* m, int ctlIdx, bool down)
{
    for (auto& mf : momentaries)
        if (mf->mod == m && mf->ctl == ctlIdx) mf->down.store (down);
}
void SatkProcessor::setInstanceSeed (juce::uint64 s)
{
    const juce::SpinLock::ScopedLockType sl (engineLock);
    sys.engine.setSeed ((uint64_t) s);
}

// ------------------------------------------------------------- state --------
juce::ValueTree SatkProcessor::cablesToTree()
{
    juce::ValueTree t ("CABLES");
    for (auto& c : sys.engine.cables)
    {
        juce::ValueTree e ("C");
        e.setProperty ("a",  juce::String (sys.engine.mods[(size_t) c.modA]->id), nullptr);
        e.setProperty ("ja", c.jackA, nullptr);
        e.setProperty ("b",  juce::String (sys.engine.mods[(size_t) c.modB]->id), nullptr);
        e.setProperty ("jb", c.jackB, nullptr);
        t.appendChild (e, nullptr);
    }
    return t;
}
void SatkProcessor::cablesFromTree (const juce::ValueTree& t)
{
    sys.engine.cables.clear();
    for (int i = 0; i < t.getNumChildren(); ++i)
    {
        auto e = t.getChild (i);
        auto* a = sys.engine.byId (e.getProperty ("a").toString().toStdString());
        auto* b = sys.engine.byId (e.getProperty ("b").toString().toStdString());
        if (a != nullptr && b != nullptr)
        {
            Cable c { sys.engine.indexOf (a), (int) e.getProperty ("ja"),
                      sys.engine.indexOf (b), (int) e.getProperty ("jb") };
            if ((size_t) c.jackA < a->jacks.size() && (size_t) c.jackB < b->jacks.size())
                sys.engine.cables.push_back (c);
        }
    }
    sys.engine.rebuild();
    ++cableVer;
}

void SatkProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::ValueTree root ("SATK");
    root.setProperty ("editorScale", (double) editorScale.load(), nullptr);
    root.setProperty ("seed", juce::String::toHexString ((juce::int64) sys.engine.seed()), nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    {
        const juce::SpinLock::ScopedLockType sl (engineLock);
        root.appendChild (cablesToTree(), nullptr);
    }
    juce::MemoryOutputStream mo (dest, true);
    root.writeToStream (mo);
}
void SatkProcessor::setStateInformation (const void* data, int size)
{
    auto root = juce::ValueTree::readFromData (data, (size_t) size);
    if (! root.isValid() || ! root.hasType ("SATK")) return;

    if (root.hasProperty ("editorScale"))
        editorScale.store ((float) (double) root.getProperty ("editorScale"));

    auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid()) apvts.replaceState (params);

    {
        const juce::SpinLock::ScopedLockType sl (engineLock);
        if (root.hasProperty ("seed"))
            sys.engine.setSeed ((uint64_t) root.getProperty ("seed").toString().getHexValue64());
        auto cab = root.getChildWithName ("CABLES");
        if (cab.isValid()) cablesFromTree (cab);
    }
}

// ------------------------------------------------------------- editor -------
// PF50-style panel editor: skeleton module faces today (per-module layouts
// land in the UI fan-out), with the full cable/zoom/tooltip/resize machinery
// live now (U01-U04).
juce::AudioProcessorEditor* SatkProcessor::createEditor()
{
    return new SatkEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SatkProcessor(); }
