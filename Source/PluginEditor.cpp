#include "PluginEditor.h"
#include <BinaryData.h>

using namespace juce;

namespace
{
constexpr float kW = 1600.0f, kH = 900.0f;
const Colour kAccent (0xffff8a3d);
const Colour kLine (0xff3a2f26);
const Colour kText (0xffd9cfc4);
const Colour kDim (0xff8f8478);
const char* const kLabelsAc[3] = { "BODY", "TONE", "BRIGHT" };
const char* const kLabels[7] = { "AMP DRIVE", "TONE", "WAH", "DELAY", "CHORUS", "REVERB", "VOLUME" };
const Rectangle<float> kStage (160.0f, 242.0f, 1280.0f, 425.0f);
const Rectangle<float> kPitchR (55.0f, 312.0f, 70.0f, 236.0f);
const Rectangle<float> kModR (1475.0f, 312.0f, 70.0f, 236.0f);
const Rectangle<float> kPrevBtn (66.0f, 88.0f, 28.0f, 22.0f);
const Rectangle<float> kNextBtn (394.0f, 88.0f, 28.0f, 22.0f);
const Rectangle<float> kPatPanel (176.0f, 594.0f, 640.0f, 60.0f);
const Rectangle<float> kPatPrev (190.0f, 620.0f, 26.0f, 24.0f);
const Rectangle<float> kPatNext (458.0f, 620.0f, 26.0f, 24.0f);
const Rectangle<float> kPatSpeed (500.0f, 597.0f, 140.0f, 17.0f);
const Rectangle<float> kPatHuman (650.0f, 597.0f, 150.0f, 17.0f);
const Rectangle<float> kNoises (830.0f, 597.0f, 196.0f, 22.0f);   // bouton BRUITS DOIGTS (ACOUSTIC, BLACK)
const int kOpen[6] = { 40, 45, 50, 55, 59, 64 };

Image loadImg (const void* d, int n) { return ImageFileFormat::loadFrom (d, (size_t) n); }

void spaced (Graphics& g, const String& s, Rectangle<float> r, Justification j, const Font& f, Colour c)
{
    g.setColour (c);
    g.setFont (f);
    g.drawText (s, r, j, false);
}
} // namespace

//==============================================================================
DGuitarEditor::DGuitarEditor (DGuitarProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    tfMed  = Typeface::createSystemTypefaceFor (BinaryData::IBMPlexSansCondensedMedium_ttf, BinaryData::IBMPlexSansCondensedMedium_ttfSize);
    tfSemi = Typeface::createSystemTypefaceFor (BinaryData::IBMPlexSansCondensedSemiBold_ttf, BinaryData::IBMPlexSansCondensedSemiBold_ttfSize);
    tfBold = Typeface::createSystemTypefaceFor (BinaryData::IBMPlexSansCondensedBold_ttf, BinaryData::IBMPlexSansCondensedBold_ttfSize);
    tfMono = Typeface::createSystemTypefaceFor (BinaryData::IBMPlexMonoRegular_ttf, BinaryData::IBMPlexMonoRegular_ttfSize);

    // les trois guitares : image + réglage du tracé des cordes (mesuré sur chaque image)
    guitars[0] = { "CLEAN", loadImg (BinaryData::dg_clean_png, BinaryData::dg_clean_pngSize), {}, 1517, 534, 900, 60, 14, 260, 270.4027f, -0.0027f, 10.9005f, 0.0069f, 238, 1225 };
    guitars[1] = { "DIVE",  loadImg (BinaryData::dg_dive_png,  BinaryData::dg_dive_pngSize),  {}, 1517, 534, 900, 60, 14, 260, 270.4027f, -0.0027f, 10.9005f, 0.0069f, 238, 1225 };
    guitars[2] = { "WORSHIP", loadImg (BinaryData::dg_worship_png, BinaryData::dg_worship_pngSize),   {}, 1532, 540, 880, 70, 16, 275, 270.1637f, -0.000654f, 11.2375f, 0.0067151f, 262, 1245 };
    guitars[3] = { "ACOUSTIC", loadImg (BinaryData::dg_acoustic_png, BinaryData::dg_acoustic_pngSize), {}, 1536, 507, 900, 60, 24, 330, 252.0f, -0.003f, 10.8f, 0.00444f, 285, 1230 };
    guitars[4] = { "BLACK", loadImg (BinaryData::dg_black_png, BinaryData::dg_black_pngSize), {}, 1536, 507, 900, 60, 24, 330, 252.0f, -0.003f, 10.8f, 0.00444f, 285, 1230 };
    guitars[5] = { "CRY", loadImg (BinaryData::dg_cry_png,   BinaryData::dg_cry_pngSize),   {}, 1532, 540, 880, 70, 16, 275, 270.1637f, -0.000654f, 11.2375f, 0.0067151f, 262, 1245 };
    for (auto& g : guitars)
    {
        const Image small = g.img.rescaled (g.img.getWidth() / 4, g.img.getHeight() / 4, Graphics::highResamplingQuality);
        g.shadow = Image (Image::ARGB, small.getWidth() + 60, small.getHeight() + 60, true);
        Graphics gs (g.shadow);
        gs.addTransform (AffineTransform::translation (30.0f, 30.0f));
        DropShadow (Colours::black.withAlpha (0.7f), 11, { 6, 12 }).drawForImage (gs, small);
    }

    setResizeLimits (800, 450, 1920, 1080);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setSize (1280, 720);
    startTimerHz (30);
}

DGuitarEditor::~DGuitarEditor() { stopTimer(); }

Font DGuitarEditor::font (float h, int weight) const
{
    Typeface::Ptr tf = weight == 0 ? tfMed : (weight == 1 ? tfSemi : (weight == 2 ? tfBold : tfMono));
    return Font (FontOptions (tf).withHeight (h));
}

Rectangle<float> DGuitarEditor::slotRect (int i)
{
    const float w = (1512.0f - (float) (dguitar::kNumGuitars - 1) * 16.0f) / (float) dguitar::kNumGuitars;
    return { 44.0f + (float) i * (w + 16.0f), 134.0f, w, 96.0f };
}

Rectangle<float> DGuitarEditor::knobArea (int i) const
{
    const float cw = 1280.0f / 7.0f;
    return { 160.0f + (float) i * cw, 684.0f, cw, 134.0f };
}

float DGuitarEditor::getParam (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

void DGuitarEditor::setParam (const char* id, float v)
{
    if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void DGuitarEditor::resized() { scale = (float) getWidth() / kW; staticScale = 0.0f; }

//==============================================================================
// couche fixe : fond, cadre, vis, boîtes, étiquettes (redessinée seulement au redimensionnement)
void DGuitarEditor::buildStatic (float ps)
{
    staticScale = ps;
    staticImg = Image (Image::ARGB, jmax (1, roundToInt ((float) getWidth() * ps)), jmax (1, roundToInt ((float) getHeight() * ps)), true);
    Graphics g (staticImg);
    g.addTransform (AffineTransform::scale (scale * ps));
    g.setImageResamplingQuality (Graphics::highResamplingQuality);

    g.setGradientFill (ColourGradient (Colour (0xff241d17), kW * 0.5f, kH * 0.42f, Colour (0xff14110e), 0.0f, 0.0f, true));
    g.fillRect (0.0f, 0.0f, kW, kH);
    g.setColour (Colours::white.withAlpha (0.018f));
    for (float x = 0; x < kW; x += 3.0f) g.fillRect (x, 0.0f, 1.0f, kH);

    g.setColour (kLine);
    g.drawRoundedRectangle (8.5f, 8.5f, kW - 17.0f, kH - 17.0f, 18.0f, 1.0f);
    for (auto c : { Point<float> (25, 25), { kW - 25, 25 }, { 25, kH - 25 }, { kW - 25, kH - 25 } })
    {
        g.setGradientFill (ColourGradient (Colour (0xffb9b4aa), c.x - 2, c.y - 2, Colour (0xff3a3733), c.x + 5, c.y + 5, true));
        g.fillEllipse (c.x - 5, c.y - 5, 10, 10);
    }

    // écran du haut (cadre + fond)
    const Rectangle<float> scr (44.0f, 26.0f, 1512.0f, 96.0f);
    g.setColour (Colour (0xff0a0908));
    g.fillRoundedRectangle (scr, 10.0f);
    g.setColour (kLine);
    g.drawRoundedRectangle (scr, 10.0f, 1.0f);
    g.setColour (Colour (0xff231c16));
    g.fillRect (444.0f, 27.0f, 1.0f, 94.0f);
    g.fillRect (1126.0f, 27.0f, 1.0f, 94.0f);
    g.setColour (Colour (0xff1d1712));
    for (float y : { 42.0f, 74.0f, 106.0f }) g.fillRect (445.0f, y, 681.0f, 1.0f);
    g.setColour (Colour (0xff16110d));
    for (int k = 1; k <= 6; ++k) g.fillRect (445.0f + (float) k * 97.0f, 27.0f, 1.0f, 94.0f);
    spaced (g, "GUITARE", { 66.0f, 38.0f, 120.0f, 14.0f }, Justification::left, font (11.0f, 3).withExtraKerningFactor (0.16f), Colour (0xff6f655b));
    spaced (g, "SIGNAL", { 458.0f, 34.0f, 100.0f, 14.0f }, Justification::left, font (11.0f, 3).withExtraKerningFactor (0.16f), Colour (0xff6f655b));

    // scène
    g.setGradientFill (ColourGradient (Colour (0xff2c241d), kStage.getCentreX(), kStage.getY() + kStage.getHeight() * 0.45f,
                                       Colour (0xff0f0c0a), kStage.getX(), kStage.getY(), true));
    g.fillRoundedRectangle (kStage, 14.0f);
    g.setGradientFill (ColourGradient (Colour (0x00000000), 0.0f, kStage.getBottom() - 56.0f, Colour (0x8c000000), 0.0f, kStage.getBottom(), false));
    g.fillRect (kStage.getX(), kStage.getBottom() - 56.0f, kStage.getWidth(), 56.0f);
    g.setColour (Colour (0xff2f2720));
    g.drawRoundedRectangle (kStage, 14.0f, 1.0f);
    spaced (g, "CLIQUE OU GLISSE SUR LES CORDES", { kStage.getRight() - 520.0f, kStage.getBottom() - 28.0f, 496.0f, 14.0f },
            Justification::right, font (11.0f, 3).withExtraKerningFactor (0.2f), Colour (0xff5d544b));

    // molettes : logements + noms
    for (auto r : { kPitchR, kModR })
    {
        g.setColour (Colour (0xff1b1612));
        g.fillRoundedRectangle (r.expanded (4.0f), 13.0f);
        g.setColour (Colour (0xff4a3d33));
        g.drawRoundedRectangle (r, 11.0f, 1.0f);
    }
    spaced (g, "PITCH BEND", { kPitchR.getX() - 15.0f, kPitchR.getBottom() + 12.0f, 100.0f, 18.0f }, Justification::centred, font (14.0f, 1).withExtraKerningFactor (0.16f), kText);
    spaced (g, "MODULATION", { kModR.getX() - 15.0f, kModR.getBottom() + 12.0f, 100.0f, 18.0f }, Justification::centred, font (14.0f, 1).withExtraKerningFactor (0.16f), kText);


    // pied
    spaced (g, "Danova", { 44.0f, 836.0f, 140.0f, 40.0f }, Justification::left, font (28.0f, 1).withExtraKerningFactor (0.04f), Colour (0xffc9622a));
    g.setGradientFill (ColourGradient (Colour (0xffc9622a), 190.0f, 856.0f, Colour (0x00c9622a), 640.0f, 856.0f, false));
    g.fillRect (190.0f, 856.0f, 450.0f, 1.0f);
    g.setGradientFill (ColourGradient (Colour (0xffc9622a), 1410.0f, 856.0f, Colour (0x00c9622a), 960.0f, 856.0f, false));
    g.fillRect (960.0f, 856.0f, 450.0f, 1.0f);
    spaced (g, "D-GUITAR", { 640.0f, 836.0f, 320.0f, 40.0f }, Justification::centred, font (32.0f, 2).withExtraKerningFactor (0.34f), Colour (0xffe9dfd3));
    spaced (g, String::fromUTF8 ("AU \xc2\xb7 VST3 \xc2\xb7 v1.6.2"), { 1340.0f, 836.0f, 216.0f, 40.0f }, Justification::right, font (14.0f, 1).withExtraKerningFactor (0.18f), kDim);
}

//==============================================================================
void DGuitarEditor::drawWheel (Graphics& g, Rectangle<float> r, float v01, bool centreLine)
{
    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (r.toNearestInt());
    g.setColour (Colour (0xff0c0a08));
    g.fillRect (r);
    const float off = std::fmod (v01 * 236.0f * 1.6f, 8.0f);
    for (float y = r.getY() - 8.0f + off; y < r.getBottom(); y += 8.0f)
    {
        g.setColour (Colour (0xff2a2623));
        g.fillRect (r.getX(), y, r.getWidth(), 8.0f);
        g.setColour (Colour (0xff6e6860));
        g.fillRect (r.getX(), y, r.getWidth(), 3.0f);
    }
    ColourGradient grad (Colour (0xd9000000), r.getX(), 0.0f, Colour (0xd9000000), r.getRight(), 0.0f, false);
    grad.addColour (0.38, Colour (0x2effffff));
    grad.addColour (0.55, Colour (0x1a000000));
    g.setGradientFill (grad);
    g.fillRect (r);
    if (centreLine) { g.setColour (kAccent.withAlpha (0.9f)); g.fillRect (r.getX(), r.getCentreY() - 1.0f, r.getWidth(), 2.0f); }
    // repère de la position
    const float y = r.getBottom() - v01 * r.getHeight();
    g.setColour (Colours::white.withAlpha (0.55f));
    g.fillRect (r.getX(), y - 1.0f, r.getWidth(), 2.0f);
}

void DGuitarEditor::drawKnob (Graphics& g, int i, float cx, float cy)
{
    const float v = jlimit (0.0f, 1.0f, getParam (DGuitarProcessor::knobId (i)));
    const float a0 = degreesToRadians (-135.0f), a1 = degreesToRadians (135.0f), av = a0 + (a1 - a0) * v;
    Path track, val;
    track.addCentredArc (cx, cy, 46.0f, 46.0f, 0.0f, a0, a1, true);
    val.addCentredArc (cx, cy, 46.0f, 46.0f, 0.0f, a0, av, true);
    g.setColour (Colour (0xff2b2520));
    g.strokePath (track, PathStrokeType (6.0f));
    g.setColour (kAccent);
    g.strokePath (val, PathStrokeType (6.0f));
    // corps métal
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (cx - 39.0f, cy - 35.0f, 78.0f, 78.0f);
    g.setGradientFill (ColourGradient (Colour (0xffbdb9b0), cx - 22.0f, cy - 26.0f, Colour (0xff4a4642), cx + 30.0f, cy + 34.0f, false));
    g.fillEllipse (cx - 39.0f, cy - 39.0f, 78.0f, 78.0f);
    g.setColour (Colour (0xff26221f));
    g.drawEllipse (cx - 39.0f, cy - 39.0f, 78.0f, 78.0f, 2.0f);
    // capuchon strié
    g.setColour (Colour (0xff2c2926));
    g.fillEllipse (cx - 28.0f, cy - 28.0f, 56.0f, 56.0f);
    g.setColour (Colour (0xff3d3935));
    for (int k = 0; k < 60; ++k)
    {
        const float a = (float) k * MathConstants<float>::twoPi / 60.0f;
        g.drawLine (cx + std::sin (a) * 22.0f, cy - std::cos (a) * 22.0f, cx + std::sin (a) * 28.0f, cy - std::cos (a) * 28.0f, 1.2f);
    }
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.34f), cx - 8.0f, cy - 12.0f, Colours::transparentWhite, cx + 20.0f, cy + 14.0f, true));
    g.fillEllipse (cx - 28.0f, cy - 28.0f, 56.0f, 56.0f);
    // repère
    Graphics::ScopedSaveState ss (g);
    g.addTransform (AffineTransform::rotation (av, cx, cy));
    g.setColour (kAccent.withAlpha (0.25f));
    g.fillRoundedRectangle (cx - 6.0f, cy - 29.0f, 12.0f, 26.0f, 6.0f);
    g.setColour (kAccent);
    g.fillRoundedRectangle (cx - 3.0f, cy - 26.0f, 6.0f, 20.0f, 3.0f);
}

void DGuitarEditor::drawScreen (Graphics& g)
{
    const int gi = proc.getGuitar(), pi = proc.getPreset();
    const auto& vc = dguitar::voicing (gi);
    spaced (g, "PRESET " + String (pi + 1) + "/3", { 304.0f, 38.0f, 118.0f, 14.0f }, Justification::right, font (11.0f, 3).withExtraKerningFactor (0.16f), Colour (0xff6f655b));
    {
        String st;
        Colour c (0xff6f655b);
        if (gi == 0) st = String::fromUTF8 ("CORDE MODÃLISÃ" "E");
        else if (proc.usesSamples()) { st = "VRAIE GUITARE"; c = Colour (0xff7fb069); }
        else if (proc.sampleStatus() == 1) st = String::fromUTF8 ("CHARGEMENTâ¦");
        else if (proc.sampleStatus() == 3) { st = "SAMPLES ABSENTS"; c = Colour (0xffe5534b); }
        spaced (g, st, { 120.0f, 38.0f, 176.0f, 14.0f }, Justification::right, font (11.0f, 3).withExtraKerningFactor (0.12f), c);
    }
    spaced (g, vc.name, { 66.0f, 52.0f, 340.0f, 44.0f }, Justification::left, font (38.0f, 2).withExtraKerningFactor (0.2f), kAccent);
    // flèches + nom du preset
    for (auto r : { kPrevBtn, kNextBtn })
    {
        g.setColour (kLine);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }
    spaced (g, String::fromUTF8 ("\xe2\x80\xb9"), kPrevBtn, Justification::centred, font (18.0f, 1), Colour (0xffb7ab9d));
    spaced (g, String::fromUTF8 ("\xe2\x80\xba"), kNextBtn, Justification::centred, font (18.0f, 1), Colour (0xffb7ab9d));
    spaced (g, String (vc.presetName[pi]).toUpperCase(), { 100.0f, 88.0f, 288.0f, 22.0f }, Justification::centred, font (14.0f, 3).withExtraKerningFactor (0.08f), Colour (0xffe9dfd3));

    // oscilloscope : on cherche un passage par zéro montant pour une image stable
    proc.copyScope (scopeBuf, 1024);
    int start = 0;
    for (int i = 0; i < 512; ++i) if (scopeBuf[i] <= 0.0f && scopeBuf[i + 1] > 0.0f) { start = i; break; }
    float pk = 0.02f;
    for (int i = 0; i < 512; ++i) pk = jmax (pk, std::abs (scopeBuf[start + i]));
    Path p;
    for (int k = 0; k < 512; ++k)
    {
        const float x = 445.0f + 680.0f * (float) k / 511.0f;
        const float y = 74.0f - jlimit (-1.0f, 1.0f, scopeBuf[start + k] / pk) * 34.0f * jlimit (0.15f, 1.0f, pk * 6.0f);
        if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (kAccent);
    g.strokePath (p, PathStrokeType (1.6f, PathStrokeType::curved));

    // spectre : 26 bandes en échelle log
    for (int b = 0; b < 26; ++b)
    {
        const float h = 2.0f + jlimit (0.0f, 1.0f, spectrum[b]) * 38.0f;
        const float bw = 386.0f / 26.0f;
        g.setColour (b > 21 ? Colour (0xffe5534b) : (b > 16 ? Colour (0xffd9b44a) : kAccent));
        g.fillRoundedRectangle (1148.0f + (float) b * bw, 80.0f - h, bw - 3.0f, h, 1.0f);
    }
    const float bend = proc.effectivePitch(), mod = proc.effectiveMod();
    const float vol = getParam ("volume");
    auto val = [&] (const String& k, const String& v, float x)
    {
        spaced (g, k, { x, 94.0f, 40.0f, 18.0f }, Justification::left, font (12.0f, 3).withExtraKerningFactor (0.1f), kDim);
        spaced (g, v, { x + 34.0f, 94.0f, 80.0f, 18.0f }, Justification::left, font (12.0f, 3).withExtraKerningFactor (0.1f), Colour (0xffe9dfd3));
    };
    val ("MOD", String (roundToInt (mod * 100.0f)) + "%", 1148.0f);
    val ("BEND", String (bend >= 0 ? "+" : "") + String (bend * 2.0f, 1) + " st", 1262.0f);
    val ("VOL", String (roundToInt (vol * 100.0f)) + "%", 1420.0f);
}

//==============================================================================
void DGuitarEditor::drawPattern (Graphics& g)
{
    const int pat = proc.getPattern();
    g.setColour (Colour (0xd8120e0b));
    g.fillRoundedRectangle (kPatPanel, 9.0f);
    g.setColour (pat > 0 ? kAccent.withAlpha (0.55f) : kLine);
    g.drawRoundedRectangle (kPatPanel, 9.0f, 1.0f);
    spaced (g, "RYTHME", { 192.0f, 600.0f, 120.0f, 14.0f }, Justification::left, font (11.0f, 3).withExtraKerningFactor (0.2f), kDim);
    {   // réglages du rythme : vitesse et humanisation (clic = valeur suivante)
        const char* sp[3] = { "VITESSE x1/2", "VITESSE x1", "VITESSE x2" };
        const String hum = "HUMAIN " + String (roundToInt (proc.getHuman() * 100.0f)) + "%";
        int k = 0;
        for (auto r : { kPatSpeed, kPatHuman })
        {
            g.setColour (Colour (0xff1c1612));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (pat > 0 ? kAccent.withAlpha (0.45f) : kLine);
            g.drawRoundedRectangle (r, 4.0f, 1.0f);
            spaced (g, k == 0 ? String (sp[proc.getPatSpeed()]) : hum, r, Justification::centred,
                    font (10.0f, 3).withExtraKerningFactor (0.14f), pat > 0 ? Colour (0xffe0c9a8) : Colour (0xff8a7d70));
            ++k;
        }
    }
    if (proc.getGuitar() == 3 || proc.getGuitar() == 4)
    {   // bruits de doigts (les doigts qui quittent les cordes) : ON / OFF
        const bool on = proc.getNoises();
        g.setColour (Colour (0xd8120e0b));
        g.fillRoundedRectangle (kNoises, 6.0f);
        g.setColour (on ? kAccent.withAlpha (0.7f) : kLine);
        g.drawRoundedRectangle (kNoises, 6.0f, 1.0f);
        spaced (g, on ? "BRUITS DOIGTS : ON" : "BRUITS DOIGTS : OFF", kNoises, Justification::centred,
                font (10.5f, 3).withExtraKerningFactor (0.14f), on ? Colour (0xffe0c9a8) : Colour (0xff8a7d70));
    }
    spaced (g, pat > 0 ? "TIENS UN ACCORD" : "JEU NOTE PAR NOTE", { 300.0f, 600.0f, 184.0f, 14.0f }, Justification::right,
            font (10.0f, 3).withExtraKerningFactor (0.12f), Colour (0xff6f655b));
    for (auto r : { kPatPrev, kPatNext })
    {
        g.setColour (kLine);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }
    spaced (g, String::fromUTF8 ("\xe2\x80\xb9"), kPatPrev, Justification::centred, font (18.0f, 1), Colour (0xffb7ab9d));
    spaced (g, String::fromUTF8 ("\xe2\x80\xba"), kPatNext, Justification::centred, font (18.0f, 1), Colour (0xffb7ab9d));
    const String name = pat > 0 ? String (dguitar::patterns()[(size_t) pat - 1].name) : String ("OFF");
    spaced (g, name, { 218.0f, 620.0f, 238.0f, 24.0f }, Justification::centred, font (15.0f, 2).withExtraKerningFactor (0.1f),
            pat > 0 ? kAccent : Colour (0xffb7ab9d));

    // grille des pas : hauteur = force du coup, le pas joué s'allume
    const int n = proc.patternSteps();
    const float x0 = 500.0f, x1 = 800.0f;
    if (pat > 0 && n > 0)
    {
        const int cur = proc.patternCurrent();
        const float cw = (x1 - x0) / (float) n;
        for (int i = 0; i < n; ++i)
        {
            const int w = proc.patternWeight (i);
            const float h = w == 0 ? 3.0f : (w == 1 ? 10.0f : (w == 2 ? 18.0f : 26.0f));
            const auto r = Rectangle<float> (x0 + (float) i * cw + cw * 0.18f, 640.0f - h, cw * 0.64f, h);
            Colour c = w == 0 ? Colour (0xff3a2f26) : Colour (0xff8a6f58);
            if (i == cur) c = w == 0 ? Colour (0xff6a5444) : kAccent;
            g.setColour (c);
            g.fillRoundedRectangle (r, 1.5f);
        }
    }
    else
        spaced (g, "CHOISIS UN ARPEGE OU UN GRATTEMENT", { x0, 616.0f, x1 - x0, 24.0f }, Justification::centred,
                font (10.0f, 3).withExtraKerningFactor (0.12f), Colour (0xff6f655b));
}

void DGuitarEditor::drawStage (Graphics& g)
{
    const int gi = proc.getGuitar();
    const auto& G = guitars[gi];
    const float sc = G.w / G.W, mod = proc.effectiveMod(), bend = proc.effectivePitch();
    const float wob = MathConstants<float>::twoPi * 5.6f * timeSec;

    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (kStage.toNearestInt());
    g.addTransform (AffineTransform::scale (1.25f).translated (kStage.getX(), kStage.getY()));
    g.setImageResamplingQuality (Graphics::highResamplingQuality);

    const float rot = degreesToRadians (mod * 0.45f * std::sin (wob));
    const float dy = mod * 1.3f * std::sin (wob + 1.1f) - bend * 1.5f;
    const Point<float> pivot (G.left + G.w * 0.25f, G.top + G.H * sc * 0.55f);
    g.addTransform (AffineTransform::translation (0.0f, dy).followedBy (AffineTransform::rotation (rot, pivot.x, pivot.y)));

    const float k = sc * 4.0f;
    g.drawImage (G.shadow, Rectangle<float> (G.left - 30.0f * k, G.top - 30.0f * k + 6.0f, (float) G.shadow.getWidth() * k, (float) G.shadow.getHeight() * k));
    g.drawImage (G.img, Rectangle<float> (G.left, G.top, G.w, G.H * sc));

    // cordes qui vibrent
    const float baseOp = jmin (0.75f, mod * 1.2f + std::abs (bend) * 1.6f);
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < 6; ++i)
        {
            auto fy = [&] (float x) { return G.c0 + G.c1 * (x - G.x0) + ((float) i - 2.5f) * (G.s0 + G.s1 * (x - G.x0)); };
            const float y1 = fy (G.xn), y2 = fy (G.xb), xm = 0.5f * (G.xn + G.xb), ym = 0.5f * (y1 + y2);
            const float a = vizSm[i], w = MathConstants<float>::twoPi * (13.0f + (float) i * 1.7f);
            const float vib = mod * 2.4f * std::sin (wob + (float) i * 0.7f), bo = -bend * 15.0f;
            const float off = (pass == 0 ? -1.0f : 1.0f) * a * std::cos (w * timeSec) + vib + bo;
            const float o = jmax (jmin (1.0f, a / 1.6f) * 0.9f, baseOp);
            if (o < 0.02f) continue;
            Path s;
            s.startNewSubPath (G.left + G.xn * sc, G.top + y1 * sc);
            s.quadraticTo (G.left + xm * sc, G.top + (ym + 2.0f * off) * sc, G.left + G.xb * sc, G.top + y2 * sc);
            g.setColour ((pass == 0 ? Colour (0xffffd9a0) : Colour (0xfffff1d6)).withAlpha (pass == 0 ? o * 0.4f : o));
            g.strokePath (s, PathStrokeType ((pass == 0 ? 3.6f : 2.6f) * sc, PathStrokeType::curved, PathStrokeType::rounded));
        }
}

void DGuitarEditor::paint (Graphics& g)
{
    const float ps = jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (staticImg.isNull() || std::abs (staticScale - ps) > 0.01f) buildStatic (ps);
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (staticImg, getLocalBounds().toFloat());

    g.addTransform (AffineTransform::scale (scale));

    drawScreen (g);

    // choix des guitares
    const int cur = proc.getGuitar();
    for (int i = 0; i < dguitar::kNumGuitars; ++i)
    {
        const auto r = slotRect (i);
        const bool on = i == cur;
        g.setGradientFill (ColourGradient (Colour (0xff1e1814), 0.0f, r.getY(), Colour (0xff110e0b), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 10.0f);
        if (on)
        {
            g.setColour (kAccent.withAlpha (0.16f));
            g.drawRoundedRectangle (r.expanded (3.0f), 12.0f, 5.0f);
            g.setColour (kAccent);
        }
        else g.setColour (kLine);
        g.drawRoundedRectangle (r, 10.0f, 1.0f);
        const auto& im = guitars[i].img;
        const float asp = (float) im.getWidth() / (float) im.getHeight();
        const float fw = jmin (66.0f * asp, 300.0f), fh = fw / asp;
        g.setOpacity (on ? 1.0f : 0.62f);
        g.drawImage (im, Rectangle<float> (r.getCentreX() - fw * 0.5f, r.getY() + 4.0f + (66.0f - fh) * 0.5f, fw, fh));
        g.setOpacity (1.0f);
        spaced (g, guitars[i].name, { r.getX(), r.getBottom() - 28.0f, r.getWidth(), 20.0f }, Justification::centred,
                font (16.0f, 2).withExtraKerningFactor (0.24f), on ? kAccent : Colour (0xffb7ab9d));
    }

    drawStage (g);
    drawPattern (g);

    drawWheel (g, kPitchR, 0.5f + 0.5f * proc.effectivePitch(), true);
    drawWheel (g, kModR, proc.effectiveMod(), false);

    for (int i = 0; i < 7; ++i)
    {
        const auto a = knobArea (i);
        String lab = kLabels[i];
        if ((cur == 3 || cur == 4) && i < 3) lab = kLabelsAc[i];
        if (cur == 2 && i == 2) lab = "SHIMMER";
        if (cur == 5 && i == 2) lab = "FONDU";
        spaced (g, lab, a.withHeight (18.0f), Justification::centred, font (15.0f, 1).withExtraKerningFactor (0.18f), Colour (0xffc9a98c));
        drawKnob (g, i, a.getCentreX(), a.getY() + 18.0f + 6.0f + 50.0f);
    }
}

//==============================================================================
void DGuitarEditor::timerCallback()
{
    timeSec += 1.0f / 30.0f;
    const double now = Time::getMillisecondCounterHiRes();
    for (size_t i = 0; i < offs.size();)
        if (offs[i].at <= now) { proc.uiNote (offs[i].note, 0.0f, false); offs.erase (offs.begin() + (long) i); }
        else ++i;

    for (int i = 0; i < 6; ++i)
    {
        const float t = proc.vizAmp (i);
        vizSm[i] += (t - vizSm[i]) * 0.7f;
    }

    // la molette de pitch revient au centre quand on la lâche
    if (drag != Drag::pitch)
    {
        const float pb = getParam ("pitchbend");
        if (std::abs (pb) > 0.002f)
        {
            if (auto* p = proc.apvts.getParameter ("pitchbend"))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 (pb * 0.72f));
                p->endChangeGesture();
            }
        }
        else if (pb != 0.0f) setParam ("pitchbend", 0.0f);
    }

    // spectre
    proc.copyScope (fftData, 2048);
    window.multiplyWithWindowingTable (fftData, 2048);
    fft.performFrequencyOnlyForwardTransform (fftData);
    const float sr = 48000.0f;
    for (int b = 0; b < 26; ++b)
    {
        const float f0 = 60.0f * std::pow (2.0f, (float) b * 0.205f), f1 = 60.0f * std::pow (2.0f, (float) (b + 1) * 0.205f);
        const int k0 = jlimit (1, 1023, (int) (f0 / sr * 2048.0f)), k1 = jlimit (k0 + 1, 1024, (int) (f1 / sr * 2048.0f) + 1);
        float m = 0.0f;
        for (int k = k0; k < k1; ++k) m = jmax (m, fftData[k]);
        const float db = Decibels::gainToDecibels (m / 256.0f, -90.0f);
        const float target = jlimit (0.0f, 1.0f, (db + 62.0f) / 56.0f);
        spectrum[b] = target > spectrum[b] ? target : spectrum[b] * 0.86f + target * 0.14f;
    }
    repaint();
}

//==============================================================================
int DGuitarEditor::hitString (Point<float> p) const
{
    const auto& G = guitars[proc.getGuitar()];
    const Point<float> in = (p - kStage.getTopLeft()) / 1.25f;
    const float sc = G.w / G.W, xm = 0.5f * (G.xn + G.xb);
    const float sp = (G.s0 + G.s1 * (xm - G.x0)) * sc;
    int best = -1;
    float bd = 1.0e9f;
    for (int i = 0; i < 6; ++i)
    {
        const float y = G.top + (G.c0 + G.c1 * (xm - G.x0) + ((float) i - 2.5f) * (G.s0 + G.s1 * (xm - G.x0))) * sc;
        const float d = std::abs (in.y - y);
        if (d < bd) { bd = d; best = i; }
    }
    return bd < sp * 1.3f ? best : -1;
}

int DGuitarEditor::noteForHit (int s, Point<float> p) const
{
    const auto& G = guitars[proc.getGuitar()];
    const float sc = G.w / G.W;
    const float X = ((p.x - kStage.getX()) / 1.25f - G.left) / sc;
    const float d = (X - G.xn) / (G.xb - G.xn);
    int fret = 0;
    if (d > 0.015f && d <= 0.5f) fret = jlimit (1, 12, roundToInt (-12.0f * std::log2 (1.0f - d)));
    return kOpen[s] + fret;
}

void DGuitarEditor::pluck (int s, Point<float> p)
{
    const int note = noteForHit (s, p);
    const float vel = 0.72f + 0.25f * Random::getSystemRandom().nextFloat();
    proc.uiNote (note, vel, true);
    offs.push_back ({ note, Time::getMillisecondCounterHiRes() + 650.0 });
}

void DGuitarEditor::mouseDown (const MouseEvent& e)
{
    const auto p = toDesign (e.position);
    drag = Drag::none;
    for (int i = 0; i < dguitar::kNumGuitars; ++i)
        if (slotRect (i).contains (p)) { if (i != proc.getGuitar()) proc.selectGuitar (i); return; }
    if (kPrevBtn.expanded (4.0f).contains (p)) { proc.selectPreset (proc.getPreset() - 1); return; }
    if (kNextBtn.expanded (4.0f).contains (p)) { proc.selectPreset (proc.getPreset() + 1); return; }
    if (kPatPrev.expanded (4.0f).contains (p)) { proc.setPattern (proc.getPattern() - 1); return; }
    if (kPatNext.expanded (4.0f).contains (p)) { proc.setPattern (proc.getPattern() + 1); return; }
    if (kPatSpeed.contains (p)) { proc.setParamValue ("patspeed", (float) ((proc.getPatSpeed() + 1) % 3)); repaint(); return; }
    if ((proc.getGuitar() == 3 || proc.getGuitar() == 4) && kNoises.contains (p)) { proc.setParamValue ("noises", proc.getNoises() ? 0.0f : 1.0f); repaint(); return; }
    if (kPatHuman.contains (p))
    {   // 0 -> 25 -> 50 -> 75 -> 100 % -> 0
        const int step = (roundToInt (proc.getHuman() * 4.0f) + 1) % 5;
        proc.setParamValue ("humanize", step * 0.25f); repaint(); return;
    }
    if (kPatPanel.contains (p)) return;

    if (kPitchR.expanded (6.0f).contains (p))
    {
        drag = Drag::pitch;
        if (auto* q = proc.apvts.getParameter ("pitchbend")) q->beginChangeGesture();
        mouseDrag (e);
        return;
    }
    if (kModR.expanded (6.0f).contains (p))
    {
        drag = Drag::mod;
        if (auto* q = proc.apvts.getParameter ("mod")) q->beginChangeGesture();
        mouseDrag (e);
        return;
    }
    for (int i = 0; i < 7; ++i)
        if (knobArea (i).withTrimmedTop (16.0f).contains (p))
        {
            drag = Drag::knob; dragKnob = i; dragStartY = p.y;
            dragStartVal = getParam (DGuitarProcessor::knobId (i));
            if (auto* q = proc.apvts.getParameter (DGuitarProcessor::knobId (i))) q->beginChangeGesture();
            return;
        }
    if (kStage.contains (p))
    {
        drag = Drag::strings;
        lastString = hitString (p);
        if (lastString >= 0) pluck (lastString, p);
    }
}

void DGuitarEditor::mouseDrag (const MouseEvent& e)
{
    const auto p = toDesign (e.position);
    if (drag == Drag::knob && dragKnob >= 0)
    {
        const float sens = e.mods.isShiftDown() ? 600.0f : 200.0f;
        setParam (DGuitarProcessor::knobId (dragKnob), jlimit (0.0f, 1.0f, dragStartVal + (dragStartY - p.y) / sens));
    }
    else if (drag == Drag::pitch)
        setParam ("pitchbend", jlimit (-1.0f, 1.0f, 1.0f - 2.0f * (p.y - kPitchR.getY()) / kPitchR.getHeight()));
    else if (drag == Drag::mod)
        setParam ("mod", jlimit (0.0f, 1.0f, 1.0f - (p.y - kModR.getY()) / kModR.getHeight()));
    else if (drag == Drag::strings)
    {
        const int s = hitString (p);
        if (s >= 0 && s != lastString) { pluck (s, p); lastString = s; }
    }
}

void DGuitarEditor::mouseUp (const MouseEvent&)
{
    if (drag == Drag::knob && dragKnob >= 0) { if (auto* q = proc.apvts.getParameter (DGuitarProcessor::knobId (dragKnob))) q->endChangeGesture(); }
    else if (drag == Drag::pitch) { if (auto* q = proc.apvts.getParameter ("pitchbend")) q->endChangeGesture(); }
    else if (drag == Drag::mod) { if (auto* q = proc.apvts.getParameter ("mod")) q->endChangeGesture(); }
    drag = Drag::none; dragKnob = -1; lastString = -1;
}

void DGuitarEditor::mouseDoubleClick (const MouseEvent& e)
{
    const auto p = toDesign (e.position);
    for (int i = 0; i < 7; ++i)
        if (knobArea (i).withTrimmedTop (16.0f).contains (p))
            setParam (DGuitarProcessor::knobId (i), dguitar::voicing (proc.getGuitar()).preset[proc.getPreset()][i] / 100.0f);
}

void DGuitarEditor::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const auto p = toDesign (e.position);
    for (int i = 0; i < 7; ++i)
        if (knobArea (i).contains (p))
            setParam (DGuitarProcessor::knobId (i), jlimit (0.0f, 1.0f, getParam (DGuitarProcessor::knobId (i)) + w.deltaY * 0.15f));
}
