// Serge Animal TKB v2 — G2u UX suite (satk_uxtests). JUCE-linked, headless.
// Requires a logged-in GUI session and JUCE_MODAL_LOOPS_PERMITTED=1
// (documented constraint, GAUNTLET rev B).
//   U01 — free-shape window (amendment 2026-11-02): no aspect lock, small
//         minimum, max covers the panel at 2.4x; resizing never changes the
//         zoom; window size persists; scrollbars appear when the panel overflows.
//   U02 — the visible ZOOM control (steps + Fit) and Cmd+wheel set the panel
//         zoom without resizing the window; zoom + window size + cables +
//         params survive a full plugin state round-trip.
//   U03 — every interactive widget (knob, jack, button, toggle, TKB pad,
//         menu) returns a tooltip >= 15 chars; a shared TooltipWindow is
//         installed.
//   U04 — headless editor -> PNG snapshot smoke test.
// Exit code = number of failures.
#include "../src/plugin/PluginProcessor.h"
#include "../src/ui/MainView.h"
#include <cstdio>

static int passed = 0, failed = 0;
static void CHECK (bool ok, const char* name, double a = 0, double b = 0)
{
    if (ok) { ++passed; std::printf ("  PASS  %-58s", name); }
    else    { ++failed; std::printf ("  FAIL  %-58s", name); }
    if (std::abs (a) > 0.0 || std::abs (b) > 0.0) std::printf ("  [%.6g / %.6g]", a, b);
    std::printf ("\n");
}
static void GROUP (const char* g) { std::printf ("\n== %s ==\n", g); }

static void pump (int times = 4, int ms = 30)
{
    for (int i = 0; i < times; ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
}

// walk the component tree collecting our interactive widget types
struct WidgetCensus
{
    int knobs = 0, jacks = 0, toggles = 0, buttons = 0, pads = 0, menus = 0, textButtons = 0;
    int badTooltips = 0;
    juce::StringArray offenders;

    void visit (juce::Component* c)
    {
        juce::String tip;
        bool interactive = false;
        if      (auto* w1 = dynamic_cast<satkui::KnobW*> (c))     { ++knobs;   tip = w1->getTooltip(); interactive = true; }
        else if (auto* w2 = dynamic_cast<satkui::JackW*> (c))     { ++jacks;   tip = w2->getTooltip(); interactive = true; }
        else if (auto* w3 = dynamic_cast<satkui::ToggleW*> (c))   { ++toggles; tip = w3->getTooltip(); interactive = true; }
        else if (auto* w4 = dynamic_cast<satkui::ButtonW*> (c))   { ++buttons; tip = w4->getTooltip(); interactive = true; }
        else if (auto* w5 = dynamic_cast<satkui::TouchPadW*> (c)) { ++pads;    tip = w5->getTooltip(); interactive = true; }
        else if (auto* w6 = dynamic_cast<juce::ComboBox*> (c))    { ++menus;   tip = w6->getTooltip(); interactive = true; }
        else if (auto* w7 = dynamic_cast<juce::TextButton*> (c))  { ++textButtons; tip = w7->getTooltip(); interactive = true; }

        if (interactive && tip.trim().length() < 15)
        {
            ++badTooltips;
            if (offenders.size() < 8) offenders.add (c->getName() + " [" + tip + "]");
        }
        for (auto* child : c->getChildren())
            visit (child);
    }
};

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::printf ("Serge Animal TKB v2 — UX suite (G2u)\n");

    SatkProcessor proc;
    proc.prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> edBase (proc.createEditor());
    auto* ed = dynamic_cast<satkui::SatkEditor*> (edBase.get());
    if (ed == nullptr)
    {
        std::printf ("FATAL: editor is not a SatkEditor\n");
        return 1;
    }
    pump(); // flush async ParameterAttachment initial updates

    const int W = ed->view.logicalW, H = ed->view.logicalH;
    std::printf ("editor logical size: %dx%d\n", W, H);

    // ---------------------------------------------------------------- U01 --
    GROUP ("U01 free-shape window, independent of zoom");
    {
        auto* con = ed->getConstrainer();
        CHECK (con != nullptr, "editor has a constrainer");
        CHECK (con->getFixedAspectRatio() == 0.0, "no aspect lock (free shape)", con->getFixedAspectRatio());
        CHECK (con->getMinimumWidth() <= 480 && con->getMinimumHeight() <= 320
               && con->getMaximumWidth() >= (int) std::lround (W * 2.4),
               "resize range: small minimum, max covers panel at 2.4x",
               con->getMinimumWidth(), con->getMaximumWidth());

        ed->setScaleStep (1.0f);
        bool zoomFixed = true, sizePersisted = true;
        for (auto wh : { juce::Point<int> (1400, 500), juce::Point<int> (500, 900),
                         juce::Point<int> (900, 700), juce::Point<int> (W, H + satkui::HEADER_H) })
        {
            ed->setSize (wh.x, wh.y);
            if (std::abs (ed->getZoom() - 1.0f) > 1e-4f || std::abs (proc.getEditorScale() - 1.0f) > 1e-4f) zoomFixed = false;
            if (proc.getEditorW() != wh.x || proc.getEditorH() != wh.y) sizePersisted = false;
        }
        CHECK (zoomFixed, "resizing the window never changes the zoom", ed->getZoom());
        CHECK (sizePersisted, "window size is persisted as resized", proc.getEditorW(), proc.getEditorH());

        ed->setSize (600, 400);
        pump (1);
        CHECK (ed->viewport.getVerticalScrollBar().isVisible() && ed->viewport.getHorizontalScrollBar().isVisible(),
               "small window: panel scrolls (both scrollbars)");
        const auto before = ed->viewport.getViewPosition();
        ed->view.onPan ({ -120, -80 });
        CHECK (ed->viewport.getViewPosition() == before + juce::Point<int> (120, 80),
               "drag on empty panel pans the view 1:1", ed->viewport.getViewPositionX(), ed->viewport.getViewPositionY());
    }

    // ---------------------------------------------------------------- U02 --
    GROUP ("U02 zoom control (steps, Fit, Cmd+wheel) + full plugin state round-trip");
    {
        ed->setSize (900, 700);
        ed->zoomBox.setSelectedId (150, juce::sendNotificationSync);
        pump (2);
        CHECK (std::abs (proc.getEditorScale() - 1.5f) < 0.01f,
               "ZOOM 150% writes persisted zoom 1.5", proc.getEditorScale());
        CHECK (ed->getWidth() == 900 && ed->getHeight() == 700,
               "zoom step leaves the window size alone", ed->getWidth(), ed->getHeight());
        CHECK (ed->viewport.getViewedComponent()->getWidth() == (int) std::ceil ((float) W * 1.5f),
               "panel content scales with zoom", ed->viewport.getViewedComponent()->getWidth(), W * 1.5);

        ed->zoomBox.setSelectedId (satkui::SatkEditor::kFitId, juce::sendNotificationSync);
        pump (2);
        auto* content = ed->viewport.getViewedComponent();
        CHECK (content->getWidth() <= ed->viewport.getWidth() && content->getHeight() <= ed->viewport.getHeight()
               && (content->getWidth() >= ed->viewport.getWidth() - 2 || content->getHeight() >= ed->viewport.getHeight() - 2),
               "Fit shows the whole panel, filling one dimension", ed->getZoom());

        const float zBefore = ed->getZoom();
        juce::Point<int> at (200, 150);
        ed->viewport.onCmdWheel (2.0f, at);
        CHECK (std::abs (ed->getZoom() - juce::jmin (2.4f, zBefore * 2.0f)) < 0.01f,
               "Cmd+wheel zooms toward the pointer", ed->getZoom(), zBefore * 2.0);

        ed->zoomBox.setSelectedId (75, juce::sendNotificationSync);
        pump (2);
        CHECK (std::abs (proc.getEditorScale() - 0.75f) < 0.01f,
               "ZOOM 75% writes persisted zoom 0.75", proc.getEditorScale());

        // author a distinctive state: zoom 1.25 in a 1000x640 window, extra cable, param tweak
        ed->setSize (1000, 640);
        ed->setZoom (1.25f);
        const int ringIdx = proc.sys.engine.indexOf (proc.sys.ring);
        const int vcfqIdx = proc.sys.engine.indexOf (proc.sys.vcfq);
        const bool cableAdded = proc.addCable (ringIdx, satk::RING::jOut, vcfqIdx, satk::VCFQ::jAgc);
        CHECK (cableAdded, "UI cable-add path accepts a new cable");
        auto* freqParam = proc.apvts.getParameter ("nto_0");
        freqParam->setValueNotifyingHost (0.777f);
        pump (2);

        juce::MemoryBlock state;
        proc.getStateInformation (state);

        SatkProcessor proc2;
        proc2.prepareToPlay (48000.0, 512);
        proc2.setStateInformation (state.getData(), (int) state.getSize());

        CHECK (std::abs (proc2.getEditorScale() - 1.25f) < 0.01f,
               "editor zoom survives get/setStateInformation", proc2.getEditorScale());
        CHECK (proc2.getEditorW() == 1000 && proc2.getEditorH() == 640,
               "window size survives get/setStateInformation", proc2.getEditorW(), proc2.getEditorH());

        auto cables2 = proc2.cableSnapshot();
        bool extraFound = false;
        for (auto& c : cables2)
            if (satk::Cable { ringIdx, satk::RING::jOut, vcfqIdx, satk::VCFQ::jAgc } == c)
                extraFound = true;
        CHECK (cables2.size() == 8 && extraFound,
               "cables survive the round-trip (7 defaults + 1 added)", (double) cables2.size());

        const float restored = proc2.apvts.getParameter ("nto_0")->getValue();
        CHECK (std::abs (restored - 0.777f) < 1e-4f,
               "params survive the round-trip (nto_0)", restored);

        // a fresh editor on the restored processor opens at the restored zoom + size
        std::unique_ptr<juce::AudioProcessorEditor> ed2b (proc2.createEditor());
        auto* ed2 = dynamic_cast<satkui::SatkEditor*> (ed2b.get());
        pump (2);
        CHECK (ed2 != nullptr && std::abs (ed2->getZoom() - 1.25f) < 0.01f
               && ed2->getWidth() == 1000 && ed2->getHeight() == 640,
               "fresh editor opens at the restored zoom and window size",
               ed2 ? ed2->getZoom() : 0.0, ed2 ? ed2->getWidth() : 0);
    }

    // ---------------------------------------------------------------- U03 --
    GROUP ("U03 tooltips on every interactive widget + shared TooltipWindow");
    {
        WidgetCensus census;
        census.visit (ed);
        std::printf ("  widgets: %d knobs, %d jacks, %d toggles, %d pads, %d menus, %d text buttons\n",
                     census.knobs, census.jacks, census.toggles, census.pads,
                     census.menus, census.textButtons);
        for (auto& o : census.offenders) std::printf ("    offender: %s\n", o.toRawUTF8());

        CHECK (census.jacks == 113, "all 113 frozen jacks present as widgets", census.jacks);
        CHECK (census.pads == 16, "16 TKB touch pads present", census.pads);
        CHECK (census.knobs >= 100, "knob widgets present (incl. 64 TKB rows)", census.knobs);
        CHECK (census.toggles >= 6, "toggle widgets present", census.toggles);
        CHECK (census.menus >= 3, "menu widgets present (LENGTH, MONITOR, ZOOM)", census.menus);
        CHECK (census.textButtons >= 1, "CLEAR button present", census.textButtons);
        CHECK (census.badTooltips == 0,
               "every interactive widget has a tooltip >= 15 chars", census.badTooltips);
        CHECK (ed->tooltipWindow() != nullptr && ed->tooltipWindow()->getParentComponent() == ed,
               "shared TooltipWindow installed on the editor");
    }

    // ---------------------------------------------------------------- U04 --
    GROUP ("U04 headless editor snapshot smoke");
    {
        ed->setScaleStep (1.0f);
        pump (3);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        CHECK (img.isValid() && img.getWidth() >= W - 2 && img.getHeight() >= H + satkui::HEADER_H - 2
               && ! ed->viewport.getVerticalScrollBar().isVisible()
               && ! ed->viewport.getHorizontalScrollBar().isVisible(),
               "createComponentSnapshot renders the full editor",
               img.getWidth(), img.getHeight());
        // not all cream: the snapshot must contain panel + header + jack pixels
        int distinct = 0;
        juce::SortedSet<int> seen;
        for (int y = 0; y < img.getHeight(); y += 37)
            for (int x = 0; x < img.getWidth(); x += 37)
                seen.add ((int) (img.getPixelAt (x, y).getARGB() & 0xffffff));
        distinct = seen.size();
        CHECK (distinct >= 8, "snapshot has real panel content (distinct colors)", distinct);
    }

    std::printf ("\n%d passed, %d failed\n", passed, failed);
    return failed;
}
