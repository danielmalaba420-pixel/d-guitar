// Tests hors ligne : le plugin complet traite du son sans erreur, et capture de l'interface en PNG.
// usage : DGuitarTests <dossier de sortie>
#include "PluginProcessor.h"
#include "PluginEditor.h"

static bool snapshot (DGuitarProcessor& proc, int guitar, int note, float scale, const juce::File& out)
{
    proc.selectGuitar (guitar);
    proc.uiNote (note, 0.9f, true);
    {   // on fait sonner un peu pour que cordes, oscilloscope et spectre vivent
        juce::MidiBuffer m;
        for (int b = 0; b < 40; ++b) { juce::AudioBuffer<float> buf (2, 512); proc.processBlock (buf, m); }
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* de = dynamic_cast<DGuitarEditor*> (ed.get());
    if (de == nullptr) return false;
    de->setSize (juce::roundToInt (1600.0f * scale), juce::roundToInt (900.0f * scale));
    for (int i = 0; i < 6; ++i) de->stepForTests();
    auto img = de->createComponentSnapshot (de->getLocalBounds(), true, 1.0f);
    juce::PNGImageFormat png;
    out.deleteFile();
    juce::FileOutputStream os (out);
    return os.openedOk() && png.writeImageToStream (img, os);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::String ("."));
    outDir.createDirectory();
    int failures = 0;

    DGuitarProcessor proc;
    proc.setPlayConfigDetails (0, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    // 1) chaque guitare : une note MIDI, 2 secondes ; sortie finie, audible, qui décroît
    for (int gi = 0; gi < dguitar::kNumGuitars; ++gi)
    {
        proc.selectGuitar (gi);
        proc.prepareToPlay (48000.0, 512);
        float early = 0.0f, late = 0.0f;
        bool finite = true;
        for (int b = 0; b < 300; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);
            if (b == 100) midi.addEvent (juce::MidiMessage::noteOff (1, 57), 0);
            juce::AudioBuffer<float> buf (2, 512);
            proc.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    if (! std::isfinite (buf.getSample (c, i))) finite = false;
            const float m = buf.getMagnitude (0, 512);
            if (b >= 2 && b < 30) early = juce::jmax (early, m);
            if (b >= 280) late = juce::jmax (late, m);
        }
        const bool ok = finite && early > 1.0e-3f && early < 1.0f && late < early;
        std::printf ("%-6s finite=%d early=%.4f late=%.5f %s\n", dguitar::voicing (gi).name, (int) finite, early, late, ok ? "OK" : "FAIL");
        if (! ok) ++failures;
    }

    // 2) pitch bend / molette / sustain / all-notes-off MIDI : pas de NaN, pas d'explosion
    {
        proc.selectGuitar (1);
        proc.prepareToPlay (48000.0, 512);
        bool ok = true;
        for (int b = 0; b < 200; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0) { midi.addEvent (juce::MidiMessage::noteOn (1, 52, (juce::uint8) 110), 0); midi.addEvent (juce::MidiMessage::noteOn (1, 59, (juce::uint8) 110), 4); }
            if (b % 10 == 0) midi.addEvent (juce::MidiMessage::pitchWheel (1, (b * 80) % 16383), 100);
            if (b % 7 == 0) midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, (b * 3) % 128), 200);
            if (b == 50) midi.addEvent (juce::MidiMessage::controllerEvent (1, 64, 127), 0);
            if (b == 120) midi.addEvent (juce::MidiMessage::allNotesOff (1), 10);
            juce::AudioBuffer<float> buf (2, 512);
            proc.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    if (! std::isfinite (buf.getSample (c, i)) || std::abs (buf.getSample (c, i)) > 2.0f) ok = false;
        }
        std::printf ("MIDI (pitch, molette, sustain, all notes off) : %s\n", ok ? "OK" : "FAIL");
        if (! ok) ++failures;
    }

    // 3) état sauvegardé / rechargé
    {
        proc.selectGuitar (2);
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        proc.selectGuitar (0);
        proc.setStateInformation (mb.getData(), (int) mb.getSize());
        const bool ok = proc.getGuitar() == 2;
        std::printf ("etat sauvegarde : %s\n", ok ? "OK" : "FAIL");
        if (! ok) ++failures;
    }

    // 4) captures de l'interface
    if (! snapshot (proc, 0, 52, 1.0f, outDir.getChildFile ("ui-clean.png"))) ++failures;
    if (! snapshot (proc, 1, 45, 1.0f, outDir.getChildFile ("ui-dive.png"))) ++failures;
    if (! snapshot (proc, 2, 64, 0.8f, outDir.getChildFile ("ui-cry-80.png"))) ++failures;

    if (! snapshot (proc, 3, 52, 0.6f, outDir.getChildFile ("ui-acoustic.png"))) ++failures;

    std::printf (failures == 0 ? "TOUT EST OK\n" : "%d ECHEC(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
