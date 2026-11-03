// Serge Animal TKB v2 — plugin shell (PF50 pattern + REDESIGN_PLAN §3 fixes).
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../engine/System.h"

class SatkProcessor : public juce::AudioProcessor
{
public:
    SatkProcessor();
    ~SatkProcessor() override = default;

    void prepareToPlay (double sr, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Serge Animal TKB"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- patch (cable) operations, callable from the UI thread ----
    bool addCable (int mA, int jA, int mB, int jB);
    void removeCablesAt (int m, int j);
    void removeCable (const satk::Cable& c);
    void clearAllCables();
    std::vector<satk::Cable> cableSnapshot();
    int cableVersion() const noexcept { return cableVer.load(); }

    // momentary panel buttons / TKB pads (UI thread -> audio thread)
    void setMomentary (satk::Module* m, int ctlIdx, bool down);

    // editor scale: single source of truth for zoom AND drag-resize (U01/U02);
    // persisted in plugin state.
    float getEditorScale() const noexcept { return editorScale.load(); }
    void  setEditorScale (float s) noexcept { editorScale.store (s); }   // panel ZOOM

    // engine instance seed (persisted; analog layer derives from it — T14/T17)
    juce::uint64 instanceSeed() const noexcept { return (juce::uint64) sys.engine.seed(); }
    void setInstanceSeed (juce::uint64 s);

    satk::System sys;
    juce::AudioProcessorValueTreeState apvts;
    juce::SpinLock engineLock;

    static juce::String paramID (const satk::Module& m, int ctlIdx);

private:
    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void applyParams();
    juce::ValueTree cablesToTree();
    void cablesFromTree (const juce::ValueTree&);

    std::vector<std::atomic<float>*> paramPtrs; // aligned with module ctl order
    struct MomentaryFlag { satk::Module* mod = nullptr; int ctl = 0; std::atomic<bool> down { false }; };
    std::vector<std::unique_ptr<MomentaryFlag>> momentaries;

    std::atomic<int> cableVer { 0 };
    std::atomic<float> editorScale { 1.0f };
    int heldNote = -1;
    juce::SortedSet<int> notesDown;
    double clockPhase = 0.0; bool wasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SatkProcessor)
};
