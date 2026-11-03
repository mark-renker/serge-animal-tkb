// Serge Animal TKB v2 — shared visual language (colors, fonts, primitive
// painters). Ported from Paperface 50 src/ui/Look.h — the house style is kept
// EXACTLY (cream 0xffefeae0 faces, black frames, 8-lobed rosette knobs,
// color-coded banana jacks, amber LED rings, Futura lettering, 52 px/inch).
// Additions for this instrument: drawTouchPad (TKB touch plates) and a
// JUCE 8 GlyphArrangement text-width helper replacing the deprecated
// Font::getStringWidthFloat used by PF50.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../engine/EngineCore.h"

namespace satkui {

constexpr float PX = 52.0f;              // pixels per panel inch
constexpr float MOD_H = 5.6f;            // module height, inches (PF50 rows ~5.5)

namespace col {
    const juce::Colour cream       (0xffefeae0);
    const juce::Colour creamShade  (0xffe4ddcf);
    const juce::Colour ink         (0xff211e19);
    const juce::Colour frame       (0xff3a362f);
    const juce::Colour boxLine     (0xff6b665c);
    const juce::Colour wood        (0xff6d4a2f);
    const juce::Colour woodDark    (0xff4e3421);
    const juce::Colour headerBg    (0xff17161a);
    const juce::Colour ioBg        (0xff26231f);
    const juce::Colour ioInk       (0xffd8d2c4);
    const juce::Colour jackWhite   (0xfff6f3ea);
    const juce::Colour jackRed     (0xffbf3a31);
    const juce::Colour jackBlack   (0xff23211e);
    const juce::Colour jackPurple  (0xff7e5ea8);
    const juce::Colour jackGrey    (0xff8f8b82);
    const juce::Colour jackHole    (0xff4a463f);
    const juce::Colour led         (0xfff2a63b);
    const juce::Colour ledOff      (0xff8a7145);
    const juce::Colour knob        (0xff191713);
    const juce::Colour knobHi      (0xff35322c);
    const juce::Colour cable       (0xff3d63c2);
    const juce::Colour padMetal    (0xffb9b4a9);
    const juce::Colour padMetalDn  (0xff8f8a7f);
}

inline juce::Font panelFont (float h, bool bold = false)
{
    auto f = juce::Font (juce::FontOptions ("Futura", h, bold ? juce::Font::bold : juce::Font::plain));
    return f;
}

// JUCE 8 replacement for the deprecated Font::getStringWidthFloat (PF50 used
// it for boxed labels; fixed here via the GlyphArrangement API).
inline float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, ga.getNumGlyphs(), true).getWidth();
}

inline juce::Colour jackColour (satk::JackColor c)
{
    using JC = satk::JackColor;
    switch (c)
    {
        case JC::White:  return col::jackWhite;
        case JC::Red:    return col::jackRed;
        case JC::Black:  return col::jackBlack;
        case JC::Purple: return col::jackPurple;
        case JC::Grey:   return col::jackGrey;
        default:         return col::jackWhite;
    }
}

// 8-lobed black rosette knob with pointer, like the R*S panel drawings
inline void drawKnob (juce::Graphics& g, juce::Rectangle<float> r, float value01, bool /*big*/)
{
    auto c = r.getCentre();
    float R = r.getWidth() * 0.5f;
    juce::Path p;
    const int lobes = 8;
    for (int i = 0; i <= 96; ++i)
    {
        float a = (float) i / 96.0f * juce::MathConstants<float>::twoPi;
        float rad = R * (0.86f + 0.14f * std::cos (a * lobes));
        auto pt = c.getPointOnCircumference (rad, a);
        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
    }
    p.closeSubPath();
    g.setColour (col::knob);
    g.fillPath (p);
    g.setColour (col::knobHi);
    g.strokePath (p, juce::PathStrokeType (1.0f));
    // skirt circle
    g.setColour (col::knobHi.withAlpha (0.55f));
    g.drawEllipse (juce::Rectangle<float> (R * 1.04f, R * 1.04f).withCentre (c), 0.8f);
    // pointer: 270° sweep from -135°  (7 o'clock → 5 o'clock)
    float ang = juce::degreesToRadians (-135.0f + 270.0f * value01);
    auto tip  = c.getPointOnCircumference (R * 0.82f, ang);
    auto tail = c.getPointOnCircumference (R * 0.18f, ang);
    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.drawLine ({ tail, tip }, juce::jmax (1.6f, R * 0.13f));
}

inline void drawJack (juce::Graphics& g, juce::Rectangle<float> r, satk::JackColor jc, bool highlight)
{
    auto c = r.getCentre();
    float R = r.getWidth() * 0.5f;
    auto body = jackColour (jc);
    g.setColour (body);
    g.fillEllipse (r);
    g.setColour (col::ink.withAlpha (0.85f));
    g.drawEllipse (r, juce::jmax (1.0f, R * 0.09f));
    g.setColour (col::jackHole);
    g.fillEllipse (juce::Rectangle<float> (R * 0.78f, R * 0.78f).withCentre (c));
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (juce::Rectangle<float> (R * 0.4f, R * 0.4f).withCentre (c));
    if (highlight)
    {
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawEllipse (r.expanded (2.5f), 2.0f);
    }
}

inline void drawLed (juce::Graphics& g, juce::Rectangle<float> r, float bright)
{
    // the amber concentric-ring "target" indicator
    auto c = r.getCentre();
    float R = r.getWidth() * 0.5f;
    if (bright > 0.02f)
    {
        g.setColour (col::led.withAlpha (0.4f * bright));
        g.fillEllipse (juce::Rectangle<float> (R * 2.6f, R * 2.6f).withCentre (c));
    }
    auto ring = col::led.withAlpha (0.55f + 0.45f * bright);
    for (float f : { 1.0f, 0.68f, 0.38f })
    {
        g.setColour (ring);
        g.drawEllipse (juce::Rectangle<float> (2 * R * f, 2 * R * f).withCentre (c), juce::jmax (1.1f, R * 0.17f));
    }
    g.setColour (bright > 0.35f ? col::led.brighter (0.3f) : col::led.withAlpha (0.8f));
    g.fillEllipse (juce::Rectangle<float> (R * 0.42f, R * 0.42f).withCentre (c));
}

// solid round lamp (R*S TKB row LEDs): dim amber when off, glowing when lit
inline void drawSolidLed (juce::Graphics& g, juce::Rectangle<float> r, float bright)
{
    auto c = r.getCentre();
    const float R = r.getWidth() * 0.5f;
    if (bright > 0.02f)
    {
        g.setColour (col::led.withAlpha (0.35f * bright));
        g.fillEllipse (juce::Rectangle<float> (R * 2.4f, R * 2.4f).withCentre (c));
    }
    auto lamp = juce::Rectangle<float> (R * 1.6f, R * 1.6f).withCentre (c);
    g.setColour (col::led.withBrightness (0.80f + 0.20f * bright));
    g.fillEllipse (lamp);
    g.setColour (juce::Colours::white.withAlpha (0.25f + 0.3f * bright));
    g.fillEllipse (lamp.reduced (R * 0.45f).translated (-R * 0.15f, -R * 0.18f));
    g.setColour (col::ink.withAlpha (0.55f));
    g.drawEllipse (lamp, 1.0f);
}

inline void drawPushButton (juce::Graphics& g, juce::Rectangle<float> r, bool down)
{
    auto c = r.getCentre();
    float R = r.getWidth() * 0.5f;
    g.setColour (col::padMetal);
    g.fillEllipse (r);
    g.setColour (col::ink.withAlpha (0.7f));
    g.drawEllipse (r, 1.2f);
    auto btn = juce::Rectangle<float> (R * 1.34f, R * 1.34f).withCentre (c);
    g.setColour (down ? juce::Colour (0xff8f2721) : col::jackRed);
    g.fillEllipse (btn);
    g.setColour (juce::Colours::white.withAlpha (down ? 0.12f : 0.28f));
    g.fillEllipse (btn.reduced (R * 0.28f).translated (-R * 0.1f, -R * 0.12f));
}

inline void drawToggle (juce::Graphics& g, juce::Rectangle<float> r, int pos, int nPos)
{
    // bat toggle on a hex nut (as on Serge / R*S panels): the short tapered
    // lever points toward the selected position
    auto c0 = r.getCentre();
    const float nutR = juce::jmin (r.getWidth(), r.getHeight()) * 0.30f;
    juce::Path nut;
    for (int k = 0; k < 6; ++k)
    {
        const float a = juce::MathConstants<float>::pi / 3.0f * (float) k + juce::MathConstants<float>::pi / 6.0f;
        const juce::Point<float> p (c0.x + nutR * std::cos (a), c0.y + nutR * std::sin (a));
        if (k == 0) nut.startNewSubPath (p); else nut.lineTo (p);
    }
    nut.closeSubPath();
    g.setColour (juce::Colour (0xffb9b5ac));
    g.fillPath (nut);
    g.setColour (col::ink.withAlpha (0.8f));
    g.strokePath (nut, juce::PathStrokeType (1.0f));
    g.setColour (juce::Colour (0xff6d6a64));
    g.fillEllipse (juce::Rectangle<float> (nutR * 0.9f, nutR * 0.9f).withCentre (c0));

    const float t = nPos <= 1 ? 0.5f : (float) pos / (float) (nPos - 1); // 0 = up
    // lever + ball stay >= ~10 % inside the widget bounds in either position
    // (the ball was being clipped at the edge in one throw)
    const float len = r.getHeight() * 0.34f;
    const juce::Point<float> tip (c0.x, c0.y + (t - 0.5f) * 2.0f * len);
    if (std::abs (t - 0.5f) > 0.01f)
    {
        juce::Path lever;
        const float w0 = nutR * 0.42f, w1 = nutR * 0.26f;
        lever.addQuadrilateral (c0.x - w0, c0.y, c0.x + w0, c0.y, tip.x + w1, tip.y, tip.x - w1, tip.y);
        g.setColour (juce::Colour (0xffd9d6cf));
        g.fillPath (lever);
        g.setColour (col::ink.withAlpha (0.75f));
        g.strokePath (lever, juce::PathStrokeType (0.9f));
    }
    const float ballR = nutR * 0.42f;
    g.setColour (juce::Colour (0xffeceae4));
    g.fillEllipse (juce::Rectangle<float> (2 * ballR, 2 * ballR).withCentre (tip));
    g.setColour (col::ink.withAlpha (0.8f));
    g.drawEllipse (juce::Rectangle<float> (2 * ballR, 2 * ballR).withCentre (tip), 0.9f);
}

inline void drawTouchPad (juce::Graphics& g, juce::Rectangle<float> r,
                          bool down, float pressure, bool activeStage,
                          const juce::String& number = {})
{
    auto rr = r.reduced (r.getWidth() * 0.05f, 0.0f).withTrimmedTop (r.getHeight() * 0.03f);
    const float cx = rr.getCentreX(), t = rr.getY(), h = rr.getHeight(), w = rr.getWidth();
    // R*S pad: a tall oval whose lower part is cut off by the strip edge
    juce::Path egg;
    egg.addEllipse (rr.getX(), t, w, h * 1.32f);
    juce::Path clip;
    clip.addRectangle (rr.getX() - 2.0f, t - 2.0f, w + 4.0f, h + 2.0f);
    juce::Graphics::ScopedSaveState clipScope (g);
    g.reduceClipRegion (clip);

    auto gold = down ? juce::Colour (0xff8f7a35) : juce::Colour (0xffb09a45);
    if (down)
        gold = gold.interpolatedWith (juce::Colour (0xffe0b84f),
                                      0.55f * juce::jlimit (0.0f, 1.0f, pressure));
    g.setColour (gold);
    g.fillPath (egg);
    if (activeStage)
    {
        g.setColour (col::led.withAlpha (0.20f));
        g.fillPath (egg);
    }
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.strokePath (egg, juce::PathStrokeType (juce::jmax (1.0f, w * 0.025f)));
    // stage LED dot near the top, engraved number below it (as on the panel)
    const float dR = juce::jmax (2.0f, w * 0.065f);
    auto dot = juce::Rectangle<float> (2 * dR, 2 * dR).withCentre ({ cx, t + h * 0.12f });
    if (activeStage)
    {
        g.setColour (juce::Colour (0xffe8452f).withAlpha (0.45f));
        g.fillEllipse (dot.expanded (dR * 1.1f));
    }
    g.setColour (activeStage ? juce::Colour (0xffff5138) : juce::Colour (0xff8a2b1e));
    g.fillEllipse (dot);
    if (number.isNotEmpty())
    {
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.setFont (panelFont (juce::jmax (8.0f, w * 0.30f), true));
        g.drawText (number, juce::Rectangle<float> (rr.getX(), t + h * 0.17f, w, h * 0.16f).getSmallestIntegerContainer(),
                    juce::Justification::centred);
    }
}

// small waveform glyphs used beside jacks in the drawings
enum class Glyph { Saw, Sine, PulseUp, PulseDown, RampUp, RampDown, ArcAR };
inline void drawGlyph (juce::Graphics& g, juce::Rectangle<float> r, Glyph gl)
{
    juce::Path p;
    auto b = r.reduced (1.0f);
    switch (gl)
    {
        case Glyph::Saw:
            p.startNewSubPath (b.getX(), b.getBottom());
            p.lineTo (b.getX() + b.getWidth() * 0.5f, b.getY());
            p.lineTo (b.getX() + b.getWidth() * 0.5f, b.getBottom());
            p.lineTo (b.getRight(), b.getY());
            break;
        case Glyph::Sine:
        {
            for (int i = 0; i <= 24; ++i)
            {
                float x = b.getX() + b.getWidth() * (float) i / 24.0f;
                float y = b.getCentreY() - std::sin ((float) i / 24.0f * juce::MathConstants<float>::twoPi) * b.getHeight() * 0.42f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            break;
        }
        case Glyph::PulseUp:
            p.startNewSubPath (b.getX(), b.getBottom());
            p.lineTo (b.getX() + b.getWidth() * 0.25f, b.getBottom());
            p.lineTo (b.getX() + b.getWidth() * 0.25f, b.getY());
            p.lineTo (b.getX() + b.getWidth() * 0.75f, b.getY());
            p.lineTo (b.getX() + b.getWidth() * 0.75f, b.getBottom());
            p.lineTo (b.getRight(), b.getBottom());
            break;
        case Glyph::PulseDown:
            p.startNewSubPath (b.getX(), b.getY());
            p.lineTo (b.getX() + b.getWidth() * 0.25f, b.getY());
            p.lineTo (b.getX() + b.getWidth() * 0.25f, b.getBottom());
            p.lineTo (b.getX() + b.getWidth() * 0.75f, b.getBottom());
            p.lineTo (b.getX() + b.getWidth() * 0.75f, b.getY());
            p.lineTo (b.getRight(), b.getY());
            break;
        case Glyph::RampUp:
            p.startNewSubPath (b.getX(), b.getBottom());
            p.lineTo (b.getRight() - b.getWidth() * 0.3f, b.getY());
            p.lineTo (b.getRight(), b.getY());
            break;
        case Glyph::RampDown:
            p.startNewSubPath (b.getX(), b.getY());
            p.lineTo (b.getRight() - b.getWidth() * 0.3f, b.getBottom());
            p.lineTo (b.getRight(), b.getBottom());
            break;
        case Glyph::ArcAR:
            p.startNewSubPath (b.getX(), b.getBottom());
            p.quadraticTo (b.getCentreX(), b.getY() - b.getHeight() * 0.2f, b.getRight(), b.getBottom());
            break;
    }
    g.setColour (col::ink);
    g.strokePath (p, juce::PathStrokeType (1.2f));
}

// ---------------------------------------------------------------------------
// House LookAndFeel for the stock JUCE widgets the panel still uses
// (ComboBox menus: TKB LENGTH, OUT MONITOR, header ZOOM; the CLEAR TextButton;
// popup menus; tooltip window) — cream faces, ink lettering, Futura, square
// frames, amber highlight; per-component colour ids override for the dark
// header widgets. Installed once on the editor; children inherit it.
class SatkLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // visible resize grip: three bright diagonal ridges on a dark tab
    void drawCornerResizer (juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging) override
    {
        const float W = (float) w, H = (float) h;
        juce::Path tab;
        tab.addTriangle (W, 0.0f, W, H, 0.0f, H);
        g.setColour (juce::Colour (0xff0e0d0f).withAlpha (0.85f));
        g.fillPath (tab);
        g.setColour (isMouseOver || isMouseDragging ? juce::Colours::white : col::cream);
        for (float f : { 0.25f, 0.50f, 0.75f })
            g.drawLine (W * f, H - 1.5f, W - 1.5f, H * f, 2.2f);
    }

    SatkLookAndFeel()
    {
        setColour (juce::ComboBox::backgroundColourId, col::cream);
        setColour (juce::ComboBox::textColourId,       col::ink);
        setColour (juce::ComboBox::outlineColourId,    col::frame);
        setColour (juce::ComboBox::arrowColourId,      col::ink);
        setColour (juce::PopupMenu::backgroundColourId,            col::cream);
        setColour (juce::PopupMenu::textColourId,                  col::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::led.withAlpha (0.85f));
        setColour (juce::PopupMenu::highlightedTextColourId,       col::ink);
        setColour (juce::TextButton::buttonColourId,   col::cream);
        setColour (juce::TextButton::buttonOnColourId, col::creamShade);
        setColour (juce::TextButton::textColourOffId,  col::ink);
        setColour (juce::TextButton::textColourOnId,   col::ink);
        setColour (juce::TooltipWindow::backgroundColourId, col::cream);
        setColour (juce::TooltipWindow::textColourId,       col::ink);
        setColour (juce::TooltipWindow::outlineColourId,    col::frame);
        setColour (juce::Label::textColourId, col::ink);
    }

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        return panelFont (juce::jmin (13.0f, (float) box.getHeight() * 0.62f));
    }
    juce::Font getPopupMenuFont() override { return panelFont (13.5f); }
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return panelFont (juce::jmin (13.0f, (float) buttonHeight * 0.58f), true);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                       int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRect (r.reduced (0.5f));
        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRect (r, 1.2f);
        auto a = r.removeFromRight ((float) height * 0.85f).reduced ((float) height * 0.3f);
        juce::Path p;
        p.addTriangle (a.getX(), a.getY(), a.getRight(), a.getY(), a.getCentreX(), a.getBottom());
        g.setColour (box.findColour (juce::ComboBox::arrowColourId)
                        .withAlpha (box.isEnabled() ? 0.9f : 0.3f));
        g.fillPath (p);
    }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (5, 1, box.getWidth() - box.getHeight() - 7, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setColour (juce::Label::textColourId, box.findColour (juce::ComboBox::textColourId));
    }
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& backgroundColour,
                               bool isOver, bool isDown) override
    {
        auto r = b.getLocalBounds().toFloat();
        auto c = backgroundColour;
        if (isDown)      c = c.contrasting (0.08f);
        else if (isOver) c = c.contrasting (0.04f);
        g.setColour (c);
        g.fillRect (r.reduced (0.5f));
        g.setColour (b.findColour (juce::TextButton::textColourOffId).withAlpha (0.8f));
        g.drawRect (r, 1.2f);
    }
};

} // namespace satkui
