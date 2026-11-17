#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

// Interface D-GUITAR : dessinée dans un espace fixe de 1600 x 900, mis à l'échelle de la fenêtre.
class DGuitarEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DGuitarEditor (DGuitarProcessor&);
    ~DGuitarEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void stepForTests() { timerCallback(); }

private:
    struct Guitar
    {
        const char* name;
        juce::Image img, shadow;
        float W, H, w, left, top, x0, c0, c1, s0, s1, xn, xb;
    };

    enum class Drag { none, knob, pitch, mod, strings };

    void timerCallback() override;
    juce::Point<float> toDesign (juce::Point<float> p) const { return p / scale; }
    void buildStatic (float pixelScale);
    void drawKnob (juce::Graphics&, int i, float cx, float cy);
    void drawWheel (juce::Graphics&, juce::Rectangle<float> r, float value01, bool centreLine);
    void drawStage (juce::Graphics&);
    void drawScreen (juce::Graphics&);
    int hitString (juce::Point<float> p) const;
    int noteForHit (int string, juce::Point<float> p) const;
    void pluck (int string, juce::Point<float> p);
    void setParam (const char* id, float value01);
    float getParam (const char* id) const;
    juce::Rectangle<float> knobArea (int i) const;
    static juce::Rectangle<float> slotRect (int i);
    juce::Font font (float h, int weight = 1) const;

    DGuitarProcessor& proc;
    float scale = 0.8f;

    juce::Typeface::Ptr tfMed, tfSemi, tfBold, tfMono;
    Guitar guitars[3];

    juce::Image staticImg;
    float staticScale = 0.0f;

    Drag drag = Drag::none;
    int dragKnob = -1;
    float dragStartY = 0.0f, dragStartVal = 0.0f;
    int lastString = -1;
    float pitchHeld = 0.0f;

    struct PendingOff { int note; double at; };
    std::vector<PendingOff> offs;

    // affichage
    float spectrum[26] {};
    float scopeBuf[1024] {};
    juce::dsp::FFT fft { 11 };
    juce::dsp::WindowingFunction<float> window { 2048, juce::dsp::WindowingFunction<float>::hann };
    float fftData[4096] {};
    float vizSm[6] {};
    float timeSec = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DGuitarEditor)
};
