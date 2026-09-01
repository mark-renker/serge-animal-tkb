// Serge Animal TKB v2 — G2u UX suite (satk_uxtests). JUCE-linked, headless.
// Requires a logged-in GUI session and JUCE_MODAL_LOOPS_PERMITTED=1
// (documented constraint, GAUNTLET rev B).
//   U01 — constrainer range >= 0.5x-2.4x, aspect-locked; the persisted value
//         IS the actual editor scale (a float).
//   U02 — the visible ZOOM control sets discrete steps by writing that same
//         persisted scale; drag-resize writes it too; scale + cables + params
//         survive a full plugin state round-trip.
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
    GROUP ("U01 constrained aspect-locked resize, persisted scale = actual scale");
    {
        auto* con = ed->getConstrainer();
        CHECK (con != nullptr, "editor has a constrainer");
        const double aspect = (double) W / (double) H;
        CHECK (std::abs (con->getFixedAspectRatio() - aspect) < 1e-3,
               "aspect ratio locked to the panel", con->getFixedAspectRatio(), aspect);
        CHECK (con->getMinimumWidth() <= (int) std::lround (W * 0.5)
               && con->getMaximumWidth() >= (int) std::lround (W * 2.4),
               "resize range covers 0.5x-2.4x base width",
               con->getMinimumWidth(), con->getMaximumWidth());

        bool scaleTracks = true;
        for (float s : { 0.5f, 0.9f, 1.0f, 1.7f, 2.4f })
        {
            ed->setSize ((int) std::lround ((float) W * s), (int) std::lround ((float) H * s));
            if (std::abs (proc.getEditorScale() - s) > 0.01f) scaleTracks = false;
        }
        CHECK (scaleTracks, "drag-resize path writes the persisted scale float",
               proc.getEditorScale());
    }

    // ---------------------------------------------------------------- U02 --
    GROUP ("U02 zoom control + full plugin state round-trip");
    {
        // the UI-visible zoom control drives the same persisted scale
        ed->view.zoomBox.setSelectedId (150, juce::sendNotificationSync);
        pump (2);
        CHECK (std::abs (proc.getEditorScale() - 1.5f) < 0.01f,
               "ZOOM 150% writes persisted scale 1.5", proc.getEditorScale());
        CHECK (ed->getWidth() == (int) std::lround ((float) W * 1.5f),
               "editor window follows the zoom step", ed->getWidth(), (double) W * 1.5);

        ed->view.zoomBox.setSelectedId (75, juce::sendNotificationSync);
        pump (2);
        CHECK (std::abs (proc.getEditorScale() - 0.75f) < 0.01f,
               "ZOOM 75% writes persisted scale 0.75", proc.getEditorScale());

        // author a distinctive state: scale 1.25, extra cable, param tweak
        ed->setScaleStep (1.25f);
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
               "editor scale survives get/setStateInformation", proc2.getEditorScale());

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

        // a fresh editor on the restored processor opens at the restored scale
        std::unique_ptr<juce::AudioProcessorEditor> ed2 (proc2.createEditor());
        pump (2);
        CHECK (std::abs ((float) ed2->getWidth() / (float) W - 1.25f) < 0.01f,
               "fresh editor opens at the restored scale",
               (double) ed2->getWidth() / (double) W);
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
        CHECK (img.isValid() && img.getWidth() >= W - 2 && img.getHeight() >= H - 2,
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
