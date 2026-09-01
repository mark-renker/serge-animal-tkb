// Serge Animal TKB v2 — main panel view (two Animal rows + full-width TKB
// strip), banana-cable interaction layer, zoom control, shared TooltipWindow,
// and the plugin editor with aspect-locked resize (0.5x-2.4x) whose scale is
// the SINGLE persisted source of truth (U01/U02).
//
// FOUNDATION SKELETON: module faces use a generic auto-layout (all jacks and
// ctls placed on an inch grid with labels + tooltips) plus a hand-blocked TKB
// strip (4x16 knob field, 16 touch pads, one-hot stage jacks). The UI fan-out
// replaces buildSkeleton* with per-module layouts transcribed from the
// hardware photos; everything else here (cables, drag-patching, zoom, resize,
// tooltips, editor) is the shipping implementation.
#pragma once
#include "Widgets.h"
#include "../plugin/PluginProcessor.h"

namespace satkui {

constexpr float RAIL_IN  = 0.24f;   // wooden side rails, inches
constexpr int   HEADER_H = 30;
constexpr int   ROW_GAP  = 20;
constexpr float TKB_H_IN = 5.5f;    // TKB strip is wider-than-tall

inline juce::RangedAudioParameter& ModuleComp::ctlParam (int ctlIdx)
{
    auto* p = proc.apvts.getParameter (SatkProcessor::paramID (mod, ctlIdx));
    jassert (p != nullptr);
    return *p;
}

inline ButtonW* ModuleComp::addButton (int ctlIdx, float x, float y, const juce::String& tip, float r)
{
    auto* b = new ButtonW (tip);
    auto* modPtr = &mod;
    auto* procPtr = &proc;
    b->onPress = [procPtr, modPtr, ctlIdx] (bool down) { procPtr->setMomentary (modPtr, ctlIdx, down); };
    buttons.add (b);
    addAndMakeVisible (b);
    b->setBounds (pxr (sq (x, y, r)));
    return b;
}

inline TouchPadW* ModuleComp::addPad (int padCtlIdx, int stageIdx, float x, float y,
                                      float wInches, float hInches, int pressureCtlIdx,
                                      const juce::String& tip)
{
    auto* modPtr = &mod;
    auto* procPtr = &proc;
    auto* p = new TouchPadW (mod, stageIdx, tip,
        [procPtr, modPtr, padCtlIdx] (bool down) { procPtr->setMomentary (modPtr, padCtlIdx, down); },
        ctlParam (pressureCtlIdx));
    pads.add (p);
    addAndMakeVisible (p);
    p->setBounds (pxr ({ x - wInches / 2, y - hInches / 2, wInches, hInches }));
    return p;
}

// --------------------------------------------------- skeleton layouts -------
// Generic auto-layout: ctls on a knob/toggle grid under the title (choice
// ctls get a full-width menu row), then all jacks on a labeled grid below.
inline void buildSkeletonGeneric (ModuleComp& c)
{
    auto& m = c.mod;
    const float w = c.wIn;

    if (! m.leds.empty())
        c.addLed (0, w - 0.26f, 0.28f, "Activity LED: shows this module's output level or state.", 0.1f);

    // ---- ctls ----
    const float cPitchX = 0.72f, cPitchY = 0.88f;
    const int cCols = juce::jmax (1, (int) ((w - 0.24f) / cPitchX));
    auto colX = [&] (int col, int cols) { return (w - (float) cols * cPitchX) / 2.0f + cPitchX * ((float) col + 0.5f); };
    float y = 0.95f;
    int col = 0;
    for (int i = 0; i < (int) m.ctls.size(); ++i)
    {
        const auto& ct = m.ctls[(size_t) i];
        const juce::String tip = ctlTip (m, i);
        if (ct.steps >= 3) // menu selector on its own full-width row
        {
            if (col != 0) { col = 0; y += cPitchY; }
            c.addChoice (i, w * 0.5f, y, w - 0.5f, tip, ct.name);
            y += cPitchY;
            continue;
        }
        const float x = colX (col, cCols);
        if (ct.steps == 2) c.addToggle (i, x, y, 2, tip, ct.name);
        else               c.addKnob (i, x, y, false, false, ct.name, tip);
        if (++col >= cCols) { col = 0; y += cPitchY; }
    }
    if (col != 0) y += cPitchY;
    y += 0.18f;

    // ---- jacks ----
    const float jPitchX = 0.74f, jPitchY = 0.74f;
    const int jCols = juce::jmax (1, (int) ((w - 0.2f) / jPitchX));
    col = 0;
    for (int j = 0; j < (int) m.jacks.size(); ++j)
    {
        const float x = (w - (float) jCols * jPitchX) / 2.0f + jPitchX * ((float) col + 0.5f);
        c.addJack (j, x, y, m.jacks[(size_t) j].name, true, jackTip (m, j));
        if (++col >= jCols) { col = 0; y += jPitchY; }
    }
}

// TKB strip: transport jacks + clock ctls at left, 4x16 knob field with row
// LEDs, 16 touch pads (active-stage lit), 16 one-hot stage jacks, outputs at
// right. Coordinates in inches; refined in the UI fan-out.
inline void buildSkeletonTKB (ModuleComp& c)
{
    using T = satk::TKB;
    auto& m = c.mod;
    const float w = c.wIn;

    // ---- left transport block ----
    for (int i = 0; i <= T::jHold; ++i)
    {
        const float x = 0.55f + (float) (i % 2) * 0.80f;
        const float y = 0.95f + (float) (i / 2) * 0.72f;
        c.addJack (i, x, y, m.jacks[(size_t) i].name, true, jackTip (m, i));
    }
    c.addToggle (T::cRun,    0.55f, 3.85f, 2, ctlTip (m, T::cRun),    "RUN");
    c.addToggle (T::cRandom, 1.35f, 3.85f, 2, ctlTip (m, T::cRandom), "RANDOM");
    c.addChoice (T::cLength, 2.45f, 3.85f, 0.95f, ctlTip (m, T::cLength), "LENGTH");
    c.addKnob (T::cRate,    0.55f, 4.85f, false, false, "RATE",  ctlTip (m, T::cRate));
    c.addKnob (T::cGateLen, 1.35f, 4.85f, false, false, "GATE",  ctlTip (m, T::cGateLen));
    c.addKnob (T::cRange,   2.15f, 4.85f, false, false, "RANGE", ctlTip (m, T::cRange));
    c.addKnob (T::cGlide,   2.95f, 4.85f, false, false, "GLIDE", ctlTip (m, T::cGlide));

    // ---- 16-column field ----
    const float gx0 = 3.85f;                 // grid origin
    const float gx1 = w - 2.35f;             // grid end (right block starts after)
    const float pitch = (gx1 - gx0) / 16.0f;
    const char rowNames[4] = { 'A', 'B', 'C', 'D' };
    for (int r = 0; r < 4; ++r)
    {
        const float y = 0.95f + (float) r * 0.62f;
        c.addLabel (juce::String::charToString (rowNames[r]), gx0 - 0.52f, y - 0.09f, 0.3f, 0.16f);
        c.addLed (16 + r, gx0 - 0.24f, y,
                  "Row " + juce::String::charToString (rowNames[r])
                  + " LED: lit while this row drives the ABCD output.", 0.09f);
        for (int s = 0; s < 16; ++s)
        {
            const int ctl = T::cRowA1 + r * 16 + s;
            c.addKnob (ctl, gx0 + pitch * ((float) s + 0.5f), y, false, false, {}, ctlTip (m, ctl));
        }
    }
    // touch pads (momentary, pressure = click height, active-stage highlight)
    for (int s = 0; s < 16; ++s)
    {
        const float x = gx0 + pitch * ((float) s + 0.5f);
        c.addPad (T::cPad1 + s, s, x, 3.75f, pitch * 0.86f, 0.62f, T::cPressure,
                  ctlTip (m, T::cPad1 + s));
        c.addLabel (juce::String (s + 1), x - 0.15f, 4.12f, 0.3f, 0.13f);
    }
    // one-hot stage pulse jacks
    for (int s = 0; s < 16; ++s)
        c.addJack (T::jStage1 + s, gx0 + pitch * ((float) s + 0.5f), 4.65f, {}, true,
                   jackTip (m, T::jStage1 + s));
    c.addLabel ("STAGE PULSES", gx0 + pitch * 8.0f, 4.95f, 2.4f, 0.13f);

    // ---- right output block ----
    for (int i = 0; i < 5; ++i) // A B C D ABCD
        c.addJack (T::jA + i, w - 1.70f, 0.95f + (float) i * 0.72f,
                   m.jacks[(size_t) (T::jA + i)].name, true, jackTip (m, T::jA + i));
    const int rightCol2[4] = { T::jKv, T::jPressure, T::jGate, T::jKp };
    for (int i = 0; i < 4; ++i)
        c.addJack (rightCol2[i], w - 0.80f, 0.95f + (float) i * 0.72f,
                   m.jacks[(size_t) rightCol2[i]].name, true, jackTip (m, rightCol2[i]));
}

inline void buildSkeleton (ModuleComp& c)
{
    if (c.mod.id == "tkb") buildSkeletonTKB (c);
    else                   buildSkeletonGeneric (c);
}

// --------------------------------------------------------------------------
class PanelView : public juce::Component, public CableHost, private juce::Timer
{
public:
    explicit PanelView (SatkProcessor& p) : proc (p)
    {
        hostPtr = this;

        auto widthOf = [] (satk::Module* m) -> float
        {
            auto id = m->id;
            if (id == "nto")   return 3.4f;
            if (id == "vcm")   return 3.4f;
            if (id == "ring")  return 1.8f;
            if (id == "vcfq")  return 3.0f;
            if (id == "noise") return 1.2f;
            if (id == "rvg")   return 1.8f;
            if (id == "dusg1" || id == "dusg2") return 2.2f;
            if (id == "ssg")   return 3.2f;
            if (id == "key")   return 1.6f;
            if (id == "vca1" || id == "vca2")   return 1.4f;
            if (id == "out")   return 2.6f;
            return 2.0f;
        };

        const std::vector<std::vector<satk::Module*>> rows = {
            { proc.sys.nto, proc.sys.vcm, proc.sys.ring, proc.sys.vcfq, proc.sys.noise, proc.sys.rvg },
            { proc.sys.dusg1, proc.sys.dusg2, proc.sys.ssg, proc.sys.key, proc.sys.vca1, proc.sys.vca2, proc.sys.out },
        };
        rowNames = { "ANIMAL - VOICE", "ANIMAL - CONTROL & OUTPUT",
                     "TOUCH ACTIVATED KEYBOARD SEQUENCER" };

        float maxRowIn = 0;
        for (auto& row : rows)
        {
            float w = 2 * RAIL_IN;
            for (auto* m : row) w += widthOf (m);
            maxRowIn = std::max (maxRowIn, w);
        }
        logicalW = (int) std::ceil (maxRowIn * PX) + 20;
        int y = HEADER_H + 14;
        for (auto& row : rows)
        {
            float rowWIn = 2 * RAIL_IN;
            for (auto* m : row) rowWIn += widthOf (m);
            int x = (logicalW - (int) (rowWIn * PX)) / 2;
            juce::Rectangle<int> rr (x, y, (int) (rowWIn * PX), (int) (MOD_H * PX));
            rowRects.push_back (rr);
            x += (int) (RAIL_IN * PX);
            for (auto* m : row)
            {
                auto* mc = new ModuleComp (*m, widthOf (m), hostPtr, proc);
                buildSkeleton (*mc);
                mods.add (mc);
                addAndMakeVisible (mc);
                mc->setTopLeftPosition (x, y);
                x += mc->getWidth();
            }
            y += (int) (MOD_H * PX) + ROW_GAP;
        }
        // ---- TKB strip (full width, wider-than-tall) ----
        {
            const float wIn = maxRowIn - 2 * RAIL_IN;
            const int x = (logicalW - (int) (maxRowIn * PX)) / 2;
            juce::Rectangle<int> rr (x, y, (int) (maxRowIn * PX), (int) (TKB_H_IN * PX));
            rowRects.push_back (rr);
            auto* mc = new ModuleComp (*proc.sys.tkb, wIn, hostPtr, proc);
            mc->setSize ((int) (wIn * PX), (int) (TKB_H_IN * PX));
            buildSkeleton (*mc);
            mods.add (mc);
            addAndMakeVisible (mc);
            mc->setTopLeftPosition (x + (int) (RAIL_IN * PX), y);
            y += mc->getHeight() + ROW_GAP;
        }
        logicalH = y;
        setSize (logicalW, logicalH);

        // ---- header widgets: zoom control + clear ----
        for (int pct : { 50, 75, 100, 125, 150, 200 })
            zoomBox.addItem (juce::String (pct) + " %", pct);
        zoomBox.setTextWhenNothingSelected ("ZOOM");
        zoomBox.setTooltip ("Zoom: sets the editor to a fixed scale step (50-200%). "
                            "Dragging the window corner resizes continuously; both write the same saved scale.");
        zoomBox.onChange = [this]
        {
            const int id = zoomBox.getSelectedId();
            if (id > 0 && onZoom) onZoom ((float) id / 100.0f);
        };
        addAndMakeVisible (zoomBox);
        zoomBox.setBounds (logicalW - 236, 4, 112, HEADER_H - 8);

        clearBtn.setButtonText ("CLEAR");
        clearBtn.setTooltip ("Remove every patch cable from the panel (knob settings are kept).");
        clearBtn.onClick = [this] { proc.clearAllCables(); };
        addAndMakeVisible (clearBtn);
        clearBtn.setBounds (logicalW - 112, 4, 92, HEADER_H - 8);

        for (auto* mc : mods)
            for (auto* j : mc->jacks)
                allJacks.push_back (j);

        startTimerHz (30);
        setOpaque (true);
    }

    // reflect the current scale in the zoom control (called by the editor)
    void setZoomDisplay (float scale)
    {
        const int pct = juce::roundToInt (scale * 100.0f);
        bool onStep = false;
        for (int step : { 50, 75, 100, 125, 150, 200 })
            if (std::abs (pct - step) <= 1) { zoomBox.setSelectedId (step, juce::dontSendNotification); onStep = true; }
        if (! onStep)
        {
            zoomBox.setSelectedId (0, juce::dontSendNotification);
            zoomBox.setTextWhenNothingSelected ("ZOOM " + juce::String (pct) + "%");
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::headerBg);
        g.setColour (col::ioInk);
        g.setFont (panelFont (15.0f, true));
        g.drawText ("SERGE  ANIMAL  TKB", 14, 0, 500, HEADER_H, juce::Justification::centredLeft);
        g.setColour (col::ioInk.withAlpha (0.55f));
        g.setFont (panelFont (11.0f, false));
        g.drawText ("RENKY DENK  -  SOFTWARE RECREATION", 220, 0, 420, HEADER_H, juce::Justification::centredLeft);
        // boat backgrounds + wood rails + row nameplates
        for (size_t i = 0; i < rowRects.size(); ++i)
        {
            auto rr = rowRects[i].toFloat();
            g.setColour (juce::Colour (0xff0e0d0f));
            g.fillRect (rr.expanded (3.0f));
            g.setGradientFill (juce::ColourGradient (col::wood, rr.getX(), rr.getY(), col::woodDark, rr.getX(), rr.getBottom(), false));
            g.fillRect (juce::Rectangle<float> (rr.getX(), rr.getY(), RAIL_IN * PX, rr.getHeight()));
            g.fillRect (juce::Rectangle<float> (rr.getRight() - RAIL_IN * PX, rr.getY(), RAIL_IN * PX, rr.getHeight()));
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            for (float wx : { rr.getX() + 3.0f, rr.getRight() - RAIL_IN * PX + 3.0f })
                for (int k = 0; k < 3; ++k)
                    g.drawVerticalLine ((int) (wx + (float) k * 3.7f), rr.getY() + 4, rr.getBottom() - 4);
            // boat nameplate on the lower rail edge
            g.setColour (juce::Colour (0xff0e0d0f));
            g.fillRect (juce::Rectangle<float> (rr.getX() - 3, rr.getBottom() - 1, rr.getWidth() + 6, 15.0f));
            g.setColour (col::ioInk.withAlpha (0.8f));
            g.setFont (panelFont (10.0f, true));
            g.drawText (rowNames[(int) i], (int) rr.getX() + 8, (int) rr.getBottom() - 1, 340, 14, juce::Justification::centredLeft);
        }
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        // ---- banana cables ----
        auto cables = proc.cableSnapshot();
        int idx = 0;
        for (auto& c : cables)
        {
            auto* a = findJack (c.modA, c.jackA);
            auto* b = findJack (c.modB, c.jackB);
            if (a && b) drawCable (g, centreOf (a), centreOf (b), cableColour (idx));
            ++idx;
        }
        if (dragSrc != nullptr)
            drawCable (g, centreOf (dragSrc), dragPos, juce::Colours::white.withAlpha (0.75f));
    }

    // ---- CableHost ----
    void jackDragStart (JackW* j) override { dragSrc = j; dragPos = centreOf (j); repaint(); }
    void jackDragMove (juce::Point<float> screen) override
    {
        dragPos = getLocalPoint (nullptr, screen);
        auto* over = jackAt (dragPos);
        for (auto* j : allJacks) j->dropTarget = (j == over && j != dragSrc);
        repaint();
    }
    void jackDragEnd (juce::Point<float> screen) override
    {
        auto pos = getLocalPoint (nullptr, screen);
        auto* dst = jackAt (pos);
        if (dragSrc && dst && dst != dragSrc)
            proc.addCable (proc.sys.engine.indexOf (&dragSrc->mod), dragSrc->jack,
                           proc.sys.engine.indexOf (&dst->mod), dst->jack);
        for (auto* j : allJacks) j->dropTarget = false;
        dragSrc = nullptr;
        repaint();
    }
    void jackRightClick (JackW* j) override
    {
        juce::PopupMenu menu;
        auto cables = proc.cableSnapshot();
        int mIdx = proc.sys.engine.indexOf (&j->mod);
        std::vector<satk::Cable> mine;
        for (auto& c : cables)
            if ((c.modA == mIdx && c.jackA == j->jack) || (c.modB == mIdx && c.jackB == j->jack))
                mine.push_back (c);
        if (mine.empty()) return;
        int id = 1;
        for (auto& c : mine)
        {
            bool aIsThis = (c.modA == mIdx && c.jackA == j->jack);
            auto& other = aIsThis ? *proc.sys.engine.mods[(size_t) c.modB] : *proc.sys.engine.mods[(size_t) c.modA];
            int oj = aIsThis ? c.jackB : c.jackA;
            menu.addItem (id++, "Remove -> " + juce::String (other.id) + " " + other.jacks[(size_t) oj].name);
        }
        menu.addSeparator();
        menu.addItem (999, "Remove all cables here");
        menu.showMenuAsync (juce::PopupMenu::Options(), [this, mine, mIdx, j] (int result)
        {
            if (result == 999) proc.removeCablesAt (mIdx, j->jack);
            else if (result > 0 && result <= (int) mine.size()) proc.removeCable (mine[(size_t) result - 1]);
            repaint();
        });
    }

    int logicalW = 980, logicalH = 1200;
    std::function<void (float)> onZoom;   // set by the editor (writes the shared scale)
    juce::ComboBox zoomBox;               // the visible ZOOM control (U02)
    juce::TextButton clearBtn;

    juce::OwnedArray<ModuleComp> mods;

private:
    void timerCallback() override
    {
        for (auto* mc : mods)
        {
            for (auto* l : mc->leds) l->refresh();
            for (auto* p : mc->pads) p->refresh();
        }
        int cv = proc.cableVersion();
        if (cv != lastCableVer) { lastCableVer = cv; repaint(); }
    }

    JackW* findJack (int modIdx, int jackIdx)
    {
        auto* m = proc.sys.engine.mods[(size_t) modIdx].get();
        for (auto* j : allJacks)
            if (&j->mod == m && j->jack == jackIdx) return j;
        return nullptr;
    }
    juce::Point<float> centreOf (JackW* j)
    {
        return getLocalPoint (j, j->getLocalBounds().getCentre().toFloat());
    }
    JackW* jackAt (juce::Point<float> p)
    {
        for (auto* j : allJacks)
        {
            auto b = getLocalArea (j, j->getLocalBounds().toFloat()).expanded (5.0f);
            if (b.contains (p)) return j;
        }
        return nullptr;
    }
    static juce::Colour cableColour (int i)
    {
        static const juce::Colour cols[] = {
            juce::Colour (0xffc0392b), juce::Colour (0xff23211e), juce::Colour (0xff2e5fb7),
            juce::Colour (0xff2f7d4f), juce::Colour (0xffd9a62e), juce::Colour (0xff77726a) };
        return cols[i % 6];
    }
    static void drawCable (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, juce::Colour col)
    {
        float dist = a.getDistanceFrom (b);
        float sag = juce::jmin (150.0f, 22.0f + dist * 0.16f);
        juce::Point<float> m ((a.x + b.x) * 0.5f, juce::jmax (a.y, b.y) + sag);
        juce::Path p;
        p.startNewSubPath (a);
        p.quadraticTo (m, b);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.strokePath (p, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col.withAlpha (0.88f));
        g.strokePath (p, juce::PathStrokeType (4.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        for (auto pt : { a, b })
        {
            g.setColour (col.darker (0.4f));
            g.fillEllipse (juce::Rectangle<float> (13, 13).withCentre (pt));
            g.setColour (col);
            g.fillEllipse (juce::Rectangle<float> (8, 8).withCentre (pt));
        }
    }

    SatkProcessor& proc;
    CableHost* hostPtr = nullptr;
    std::vector<JackW*> allJacks;
    std::vector<juce::Rectangle<int>> rowRects;
    juce::StringArray rowNames;
    JackW* dragSrc = nullptr;
    juce::Point<float> dragPos;
    int lastCableVer = -1;
};

// --------------------------------------------------------------------------
// Editor. The persisted editor scale (SatkProcessor::get/setEditorScale) is
// the single source of truth: the ZOOM control's discrete steps and corner
// drag-resize both funnel through resized() -> setEditorScale (U01/U02).
class SatkEditor : public juce::AudioProcessorEditor
{
public:
    static constexpr float kMinScale = 0.5f, kMaxScale = 2.4f;

    explicit SatkEditor (SatkProcessor& p) : AudioProcessorEditor (p), proc (p), view (p)
    {
        // capture the persisted scale BEFORE installing size limits:
        // setResizeLimits() constrains the initial 0x0 bounds up to the
        // minimum, which fires resized() and would overwrite the restored
        // value with kMinScale.
        const float s = juce::jlimit (kMinScale, kMaxScale, proc.getEditorScale());

        addAndMakeVisible (view);
        tips = std::make_unique<juce::TooltipWindow> (this, 400); // shared TooltipWindow (U03)

        const float aspect = (float) view.logicalW / (float) view.logicalH;
        setResizable (true, true);
        setResizeLimits ((int) std::lround ((float) view.logicalW * kMinScale),
                         (int) std::lround ((float) view.logicalH * kMinScale),
                         (int) std::lround ((float) view.logicalW * kMaxScale),
                         (int) std::lround ((float) view.logicalH * kMaxScale));
        getConstrainer()->setFixedAspectRatio (aspect);

        view.onZoom = [this] (float f) { setScaleStep (f); };

        setSize ((int) std::lround ((float) view.logicalW * s),
                 (int) std::lround ((float) view.logicalH * s));
    }

    // discrete zoom entry point (also used by the UX tests)
    void setScaleStep (float f)
    {
        f = juce::jlimit (kMinScale, kMaxScale, f);
        setSize ((int) std::lround ((float) view.logicalW * f),
                 (int) std::lround ((float) view.logicalH * f));
    }

    void resized() override
    {
        const float s = juce::jmin ((float) getWidth() / (float) view.logicalW,
                                    (float) getHeight() / (float) view.logicalH);
        view.setTransform (juce::AffineTransform::scale (s));
        view.setTopLeftPosition (0, 0);
        proc.setEditorScale (s);        // drag-resize and zoom share this value
        view.setZoomDisplay (s);
    }
    void paint (juce::Graphics& g) override { g.fillAll (satkui::col::headerBg); }

    juce::TooltipWindow* tooltipWindow() const { return tips.get(); }

    SatkProcessor& proc;
    PanelView view;

private:
    std::unique_ptr<juce::TooltipWindow> tips;
};

} // namespace satkui

using SatkEditor = satkui::SatkEditor;
