// Serge Animal TKB v2 — panel widgets (jacks, knobs, toggles, buttons, LEDs,
// menus, TKB touch pads) and the module face component with its inch-based
// layout language. Ported from Paperface 50 src/ui/Widgets.h with three v2
// extensions (GAUNTLET rev B, G2u):
//   1. EVERY widget takes a MANDATORY function-description string (>= 15
//      chars, asserted) returned via TooltipClient::getTooltip() — U03.
//   2. TouchPadW: TKB touch plate — momentary, engine-ctl-backed through the
//      MomentaryFlag pattern; click height writes the PRESSURE ctl (0-1);
//      shows the active-stage highlight from the module's stage LEDs.
//   3. ChoiceW: param-attached menu widget (LENGTH / MONITOR selectors).
// PF50's deprecated Font::getStringWidthFloat is fixed via textWidth (Look.h).
#pragma once
#include "Look.h"
#include "Tooltips.h"
#include <juce_audio_processors/juce_audio_processors.h>

class SatkProcessor;

namespace satkui {

class JackW;
struct CableHost
{
    virtual ~CableHost() = default;
    virtual void jackDragStart (JackW*) = 0;
    virtual void jackDragMove (juce::Point<float> screen) = 0;
    virtual void jackDragEnd (juce::Point<float> screen) = 0;
    virtual void jackRightClick (JackW*) = 0;
};

class JackW : public juce::Component, public juce::SettableTooltipClient
{
public:
    JackW (satk::Module& m, int jackIdx, CableHost*& h, const juce::String& functionDescription)
        : mod (m), jack (jackIdx), host (h)
    {
        jassert (functionDescription.length() >= 15);
        setTooltip (functionDescription);
        setRepaintsOnMouseActivity (true);
    }
    void paint (juce::Graphics& g) override
    {
        bool hl = isMouseOverOrDragging() || dropTarget;
        drawJack (g, getLocalBounds().toFloat().reduced (2.0f), mod.jacks[(size_t) jack].color, hl);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! host) return;
        if (e.mods.isPopupMenu()) host->jackRightClick (this);
        else host->jackDragStart (this);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (host && ! e.mods.isPopupMenu()) host->jackDragMove (e.getScreenPosition().toFloat()); }
    void mouseUp   (const juce::MouseEvent& e) override { if (host && ! e.mods.isPopupMenu()) host->jackDragEnd (e.getScreenPosition().toFloat()); }

    satk::Module& mod; int jack; CableHost*& host;
    bool dropTarget = false;
};

class KnobW : public juce::Component, public juce::SettableTooltipClient
{
public:
    KnobW (juce::RangedAudioParameter& p, bool isBig, const juce::String& functionDescription)
        : big (isBig), att (p, [this] (float v) { val = v; repaint(); })
    {
        jassert (functionDescription.length() >= 15);
        setTooltip (functionDescription);
        att.sendInitialUpdate();
    }
    void paint (juce::Graphics& g) override { drawKnob (g, getLocalBounds().toFloat().reduced (1.0f), val, big); }
    void mouseDown (const juce::MouseEvent&) override { att.beginGesture(); startVal = val; }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        float sens = e.mods.isShiftDown() ? 1200.0f : 240.0f;
        att.setValueAsPartOfGesture (juce::jlimit (0.0f, 1.0f, startVal - (float) e.getDistanceFromDragStartY() / sens));
    }
    void mouseUp (const juce::MouseEvent&) override { att.endGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { att.setValueAsCompleteGesture (defaultVal); }
    // plain wheel/trackpad scroll pans the panel (passes up to the viewport);
    // Option(Alt)+wheel fine-adjusts the knob
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (! e.mods.isAltDown()) { juce::Component::mouseWheelMove (e, w); return; }
        att.setValueAsCompleteGesture (juce::jlimit (0.0f, 1.0f, val + (float) w.deltaY * 0.08f));
    }
    float defaultVal = 0.5f;
    bool big; float val = 0, startVal = 0;
    juce::ParameterAttachment att;
};

class ToggleW : public juce::Component, public juce::SettableTooltipClient
{
public:
    ToggleW (juce::RangedAudioParameter& p, int nPositions, const juce::String& functionDescription)
        : n (nPositions), att (p, [this] (float v) { idx = (int) std::lround (v); repaint(); })
    {
        jassert (functionDescription.length() >= 15);
        setTooltip (functionDescription);
        att.sendInitialUpdate();
    }
    void paint (juce::Graphics& g) override { drawToggle (g, getLocalBounds().toFloat(), idx, n); }
    void mouseDown (const juce::MouseEvent&) override
    {
        int next = (idx + 1) % n;
        att.setValueAsCompleteGesture ((float) next);
    }
    int n, idx = 0;
    juce::ParameterAttachment att;
};

class ButtonW : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ButtonW (const juce::String& functionDescription)
    {
        jassert (functionDescription.length() >= 15);
        setTooltip (functionDescription);
    }
    std::function<void (bool)> onPress; // true = down
    void paint (juce::Graphics& g) override { drawPushButton (g, getLocalBounds().toFloat().reduced (1.0f), down); }
    void mouseDown (const juce::MouseEvent&) override { down = true; repaint(); if (onPress) onPress (true); }
    void mouseUp   (const juce::MouseEvent&) override { down = false; repaint(); if (onPress) onPress (false); }
    bool down = false;
};

class LedW : public juce::Component, public juce::SettableTooltipClient
{
public:
    LedW (satk::Module& m, int ledIdx, const juce::String& functionDescription) : mod (m), idx (ledIdx)
    {
        jassert (functionDescription.length() >= 15);
        setTooltip (functionDescription);
        setInterceptsMouseClicks (false, false);
    }
    void paint (juce::Graphics& g) override
    {
        if (solid) drawSolidLed (g, getLocalBounds().toFloat(), bright);
        else       drawLed (g, getLocalBounds().toFloat(), bright);
    }
    bool solid = false; // hardware-style round lamp (TKB row LEDs)
    void refresh()
    {
        float b = idx < (int) mod.leds.size() ? mod.leds[(size_t) idx] : 0.0f;
        if (std::fabs (b - bright) > 0.03f) { bright = b; repaint(); }
    }
    satk::Module& mod; int idx; float bright = 0;
};

// Param-attached menu (choice ctls with >= 3 steps: TKB LENGTH, OUT MONITOR).
class ChoiceW : public juce::ComboBox
{
public:
    ChoiceW (juce::RangedAudioParameter& p, const juce::String& functionDescription)
    {
        jassert (functionDescription.length() >= 15);
        setTooltip (functionDescription);
        if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (&p))
            addItemList (ch->choices, 1);
        else
            addItemList (p.getAllValueStrings(), 1);
        att = std::make_unique<juce::ComboBoxParameterAttachment> (p, *this);
    }

private:
    std::unique_ptr<juce::ComboBoxParameterAttachment> att;
};

// TKB touch plate. Momentary (MomentaryFlag pattern: onTouch -> processor
// setMomentary on the pad ctl), click height writes the shared PRESSURE ctl
// parameter (0 at the pad's bottom edge, 1 at its top), and the pad lights up
// while its stage is active (polls the module's stage LED, timer-driven via
// refresh() like LedW).
class TouchPadW : public juce::Component, public juce::SettableTooltipClient
{
public:
    TouchPadW (satk::Module& m, int stageIdx, const juce::String& functionDescription,
               std::function<void (bool)> onTouchFn, juce::RangedAudioParameter& pressureParam)
        : mod (m), stage (stageIdx), onTouch (std::move (onTouchFn)),
          pressAtt (pressureParam, [this] (float v) { pressure = v; if (down) repaint(); })
    {
        jassert (functionDescription.length() >= 15);
        setTooltip (functionDescription);
        setRepaintsOnMouseActivity (true);
        pressAtt.sendInitialUpdate();
    }
    void paint (juce::Graphics& g) override
    {
        drawTouchPad (g, getLocalBounds().toFloat(), down, pressure, active,
                      juce::String (stage + 1));
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        down = true;
        pressAtt.beginGesture();
        pressAtt.setValueAsPartOfGesture (pressureFromY (e));
        if (onTouch) onTouch (true);
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        pressAtt.setValueAsPartOfGesture (pressureFromY (e));
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        pressAtt.endGesture();
        down = false;
        if (onTouch) onTouch (false);
        repaint();
    }
    void refresh() // active-stage highlight, polled at UI rate
    {
        bool a = stage >= 0 && stage < (int) mod.leds.size()
                 && mod.leds[(size_t) stage] > 0.5f;
        if (a != active) { active = a; repaint(); }
    }

    satk::Module& mod;
    int stage;              // stage LED index polled for the highlight
    bool down = false, active = false;
    float pressure = 0.0f;

private:
    float pressureFromY (const juce::MouseEvent& e) const
    {
        float h = (float) juce::jmax (1, getHeight());
        return juce::jlimit (0.0f, 1.0f, 1.0f - e.position.y / h);
    }
    std::function<void (bool)> onTouch;
    juce::ParameterAttachment pressAtt;
};

// ------------------------------------------------------- module face --------
class ModuleComp : public juce::Component
{
public:
    ModuleComp (satk::Module& m, float widthInches, CableHost*& hostRef, SatkProcessor& p)
        : mod (m), wIn (widthInches), host (hostRef), proc (p)
    {
        setSize ((int) (wIn * PX), (int) (MOD_H * PX));
        // empty panel space passes clicks up to the PanelView (drag-to-pan);
        // the widgets on the panel still receive their own clicks
        setInterceptsMouseClicks (false, true);
    }

    struct TextItem { juce::String text; juce::Rectangle<float> r; float h; bool boxed; };
    std::vector<TextItem> labels;
    std::vector<std::pair<juce::Rectangle<float>, Glyph>> glyphs;
    std::vector<juce::Rectangle<float>> boxes;      // section frames
    std::vector<juce::Rectangle<float>> scales;     // "−   +" attenuverter scales
    std::vector<juce::Line<float>> lines;           // connector graphics
    std::vector<std::pair<juce::Rectangle<float>, juce::Colour>> fills; // filled devices (TKB pad strip)
    bool darkFace = false;
    // composite panels: widgets added after use(&other) bind to that module
    // (e.g. NOISE inside the RVG panel, VCA 2 inside the dual-VCA panel)
    satk::Module* cur = nullptr;
    satk::Module& target() { return cur != nullptr ? *cur : mod; }
    void use (satk::Module* m) { cur = m; }
    juce::String titleText;                         // overrides mod.title when set
    bool showTitle = true;                          // TKB carries its title bottom-right instead
    static constexpr float kMinLabelIn = 0.14f;     // smallest label height (inches)

    static juce::Rectangle<float> sq (float x, float y, float r) { return { x - r, y - r, 2 * r, 2 * r }; }
    juce::Rectangle<int> pxr (juce::Rectangle<float> inches) const
    {
        return (inches * PX).getSmallestIntegerContainer();
    }

    JackW* addJack (int jackIdx, float x, float y, const juce::String& label,
                    bool small, const juce::String& tip, float labelW = 1.1f)
    {
        auto* j = new JackW (target(), jackIdx, host, tip);
        jacks.add (j);
        addAndMakeVisible (j);
        float r = small ? 0.125f : 0.165f;
        j->setBounds (pxr (sq (x, y, r + 0.05f)));
        if (label.isNotEmpty())
            labels.push_back ({ label, { x - labelW / 2, y + r + 0.05f, labelW, 0.17f }, 0.145f, false });
        return j;
    }
    KnobW* addKnob (int ctlIdx, float x, float y, bool big, bool withScale,
                    const juce::String& label, const juce::String& tip)
    {
        auto& pid = ctlParam (ctlIdx);
        auto* k = new KnobW (pid, big, tip);
        k->defaultVal = target().ctls[(size_t) ctlIdx].def;
        knobs.add (k);
        addAndMakeVisible (k);
        float r = big ? 0.31f : 0.25f;
        k->setBounds (pxr (sq (x, y, r)));
        if (big) arches.push_back ({ x - r - 0.08f, y - r - 0.1f, 2 * (r + 0.08f), r + 0.18f });
        if (withScale) scales.push_back ({ x - 0.42f, y + r + 0.03f, 0.84f, 0.16f });
        if (label.isNotEmpty())
            labels.push_back ({ label, { x - 0.6f, y + r + (withScale ? 0.2f : 0.04f), 1.2f, 0.17f }, 0.15f, false });
        return k;
    }
    void addDiamond (float x, float y, float r = 0.34f)
    {
        addLine (x, y - r, x + r, y); addLine (x + r, y, x, y + r);
        addLine (x, y + r, x - r, y); addLine (x - r, y, x, y - r);
    }
    std::vector<juce::Rectangle<float>> arches;
    ToggleW* addToggle (int ctlIdx, float x, float y, int n, const juce::String& tip,
                        const juce::String& label = {})
    {
        auto* t = new ToggleW (ctlParam (ctlIdx), n, tip);
        toggles.add (t);
        addAndMakeVisible (t);
        t->setBounds (pxr ({ x - 0.14f, y - 0.26f, 0.28f, 0.52f }));
        if (label.isNotEmpty())
            labels.push_back ({ label, { x - 0.6f, y + 0.3f, 1.2f, 0.17f }, 0.15f, false });
        return t;
    }
    ChoiceW* addChoice (int ctlIdx, float x, float y, float wInches, const juce::String& tip,
                        const juce::String& label = {})
    {
        auto* cb = new ChoiceW (ctlParam (ctlIdx), tip);
        choices.add (cb);
        addAndMakeVisible (cb);
        cb->setBounds (pxr ({ x - wInches / 2, y - 0.17f, wInches, 0.34f }));
        if (label.isNotEmpty())
            labels.push_back ({ label, { x - 0.7f, y + 0.2f, 1.4f, 0.17f }, 0.15f, false });
        return cb;
    }
    ButtonW* addButton (int ctlIdx, float x, float y, const juce::String& tip, float r = 0.24f);
    TouchPadW* addPad (int padCtlIdx, int stageIdx, float x, float y,
                       float wInches, float hInches, int pressureCtlIdx, const juce::String& tip);
    LedW* addLed (int ledIdx, float x, float y, const juce::String& tip, float r = 0.13f)
    {
        auto* l = new LedW (target(), ledIdx, tip);
        leds.add (l);
        addAndMakeVisible (l);
        l->setBounds (pxr (sq (x, y, r)));
        return l;
    }
    void addLabel (const juce::String& t, float x, float y, float w = 1.2f, float h = 0.15f, bool boxed = false)
    {
        h = juce::jmax (h, kMinLabelIn); // legibility floor at 100 % zoom
        labels.push_back ({ t, { x - w / 2, y, w, h + 0.03f }, h, boxed });
    }
    void addGlyph (Glyph gl, float x, float y, float w = 0.3f, float h = 0.2f)
    {
        glyphs.push_back ({ { x - w / 2, y - h / 2, w, h }, gl });
    }
    void addBox (float x, float y, float w, float h) { boxes.push_back ({ x, y, w, h }); }
    void addLine (float x1, float y1, float x2, float y2) { lines.push_back ({ x1, y1, x2, y2 }); }
    void addFill (float x, float y, float w, float h, juce::Colour c) { fills.push_back ({ { x, y, w, h }, c }); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (darkFace ? col::ioBg : col::cream);
        g.fillRect (b);
        g.setColour (darkFace ? col::ioBg.brighter (0.12f) : col::creamShade);
        g.fillRect (b.removeFromBottom (4.0f));
        g.setColour (col::frame);
        g.drawRect (getLocalBounds().toFloat(), 1.4f);

        auto inkCol = darkFace ? col::ioInk : col::ink;
        if (! darkFace && showTitle)
        {
            g.setColour (inkCol);
            // long names wrap to two lines like the STS panel titles
            // ("SMOOTH / STEPPED / GENERATOR") instead of truncating
            const float avail = (float) getWidth() - 8.0f;
            const auto oneLine = panelFont (0.205f * PX, true);
            const auto& title = titleText.isNotEmpty() ? titleText : juce::String (mod.title);
            if (textWidth (oneLine, title) * 0.78f <= avail) // fits with mild squeeze
            {
                g.setFont (oneLine);
                g.drawFittedText (title,
                                  juce::Rectangle<float> (4.0f, 0.20f * PX, avail, 0.28f * PX).getSmallestIntegerContainer(),
                                  juce::Justification::centred, 1, 0.62f);
            }
            else
            {
                g.setFont (panelFont (0.17f * PX, true));
                g.drawFittedText (title,
                                  juce::Rectangle<float> (4.0f, 0.12f * PX, avail, 0.46f * PX).getSmallestIntegerContainer(),
                                  juce::Justification::centred, 2, 0.8f);
            }
        }

        for (auto& [fr, fc] : fills)
        {
            g.setColour (fc);
            g.fillRoundedRectangle (fr * PX, 6.0f);
        }
        g.setColour (darkFace ? col::ioInk.withAlpha (0.5f) : col::boxLine);
        for (auto& bx : boxes) g.drawRect (bx * PX, 1.1f);
        for (auto& ln : lines) g.drawLine ({ ln.getStart() * PX, ln.getEnd() * PX }, 1.1f);
        // candy-arch outlines over large knobs (per the panel drawings)
        for (auto& a : arches)
        {
            auto rr = a * PX;
            juce::Path p;
            p.startNewSubPath (rr.getX(), rr.getBottom());
            p.lineTo (rr.getX(), rr.getBottom() - rr.getHeight() * 0.28f);
            p.addArc (rr.getX(), rr.getY(), rr.getWidth(), rr.getHeight() * 1.44f,
                      -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, false);
            p.lineTo (rr.getRight(), rr.getBottom());
            g.strokePath (p, juce::PathStrokeType (1.1f));
        }

        for (auto& [r, gl] : glyphs)
        {
            juce::Graphics::ScopedSaveState ss (g);
            drawGlyph (g, r * PX, gl);
        }
        for (auto& t : labels)
        {
            auto rr = t.r * PX;
            bool dim = t.text.startsWith ("~"); // software-only hint label, kept recessive
            auto txt = dim ? t.text.substring (1) : t.text;
            g.setColour (dim ? inkCol.withAlpha (0.42f) : inkCol);
            g.setFont (panelFont ((dim ? t.h * 0.85f : t.h) * PX, false));
            g.drawFittedText (txt, rr.getSmallestIntegerContainer(), juce::Justification::centred,
                              txt.containsChar ('\n') ? 2 : 1, 0.7f);
            if (t.boxed)
            {
                g.setColour (col::boxLine);
                auto tw = juce::jmax (24.0f, textWidth (panelFont (t.h * PX), t.text) + 8.0f);
                g.drawRect (juce::Rectangle<float> (tw, rr.getHeight()).withCentre (rr.getCentre()), 0.9f);
            }
        }
        for (auto& s : scales)
        {
            auto rr = s * PX;
            g.setColour (inkCol);
            g.setFont (panelFont (rr.getHeight() * 0.95f));
            g.drawText ("-", rr.removeFromLeft (rr.getWidth() * 0.25f), juce::Justification::centredLeft);
            auto rr2 = s * PX;
            g.drawText ("+", rr2.removeFromRight (rr2.getWidth() * 0.25f), juce::Justification::centredRight);
            auto mid = s * PX;
            g.drawLine (mid.getX() + mid.getWidth() * 0.22f, mid.getCentreY(),
                        mid.getRight() - mid.getWidth() * 0.22f, mid.getCentreY(), 0.8f);
        }
    }

    satk::Module& mod;
    float wIn;
    CableHost*& host;
    SatkProcessor& proc;
    juce::OwnedArray<JackW> jacks;
    juce::OwnedArray<KnobW> knobs;
    juce::OwnedArray<ToggleW> toggles;
    juce::OwnedArray<ButtonW> buttons;
    juce::OwnedArray<ChoiceW> choices;
    juce::OwnedArray<TouchPadW> pads;
    juce::OwnedArray<LedW> leds;

private:
    juce::RangedAudioParameter& ctlParam (int ctlIdx);
};

} // namespace satkui
