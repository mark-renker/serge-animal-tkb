// Serge Animal TKB v2 — headless editor -> PNG (satk_snapshot), for the blind
// UI critics (G2u/U04). Ported from PF50 tools/Snapshot.cpp. At foundation the
// editor is the generic placeholder; the stage-3 UI replaces it in place.
// Usage: satk_snapshot <out.png>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../src/plugin/PluginProcessor.h"
#include "../src/ui/MainView.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    const juce::String outPath = argc > 1 ? juce::String (argv[1]) : juce::String ("satk_editor.png");

    SatkProcessor proc;
    proc.prepareToPlay (48000.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    if (ed == nullptr) { std::printf ("no editor\n"); return 1; }

    // show the whole panel at 1:1 (the window would otherwise be clipped to the screen)
    if (auto* se = dynamic_cast<satkui::SatkEditor*> (ed.get()))
        se->setScaleStep (1.0f);

    // flush async ParameterAttachment updates so controls render at value
    for (int i = 0; i < 5; ++i)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

    const float scale = argc > 2 ? (float) std::atof (argv[2]) : 1.25f;
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
    juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile (outPath);
    f.deleteFile();
    juce::FileOutputStream os (f);
    if (! os.openedOk()) { std::printf ("cannot write %s\n", outPath.toRawUTF8()); return 1; }
    juce::PNGImageFormat png;
    if (! png.writeImageToStream (img, os)) { std::printf ("png encode failed\n"); return 1; }

    std::printf ("wrote %s (%dx%d)\n", f.getFullPathName().toRawUTF8(),
                 img.getWidth(), img.getHeight());
    return 0;
}
