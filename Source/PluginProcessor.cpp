#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;

namespace
{
const char* const kKnobIds[dguitar::kNumKnobs] = { "drive", "tone", "wah", "delay", "chorus", "reverb", "volume" };
const char* const kKnobNames[dguitar::kNumKnobs] = { "Amp Drive", "Tone", "Wah", "Delay", "Chorus", "Reverb", "Volume" };
}

// rythme proposé par chaque preset (acoustique : Folk = grattement folk, Arpèges = arpège ballade, Chaleur = jeu direct)
int DGuitarProcessor::presetPattern (int guitar, int preset)
{
    if (guitar == 3) { const int m[3] = { 7, 2, 0 }; return m[juce::jlimit (0, 2, preset)]; }
    return 0;
}

const char* DGuitarProcessor::knobId (int i) { return kKnobIds[juce::jlimit (0, dguitar::kNumKnobs - 1, i)]; }

APVTS::ParameterLayout DGuitarProcessor::createLayout()
{
    APVTS::ParameterLayout layout;
    juce::StringArray guitars;
    for (int i = 0; i < dguitar::kNumGuitars; ++i) guitars.add (dguitar::voicing (i).name);
    auto pct = juce::AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; });

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "guitar", 1 }, "Guitar", guitars, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "preset", 1 }, "Preset", juce::StringArray { "1", "2", "3" }, 0));
    const auto& d = dguitar::voicing (0);
    for (int i = 0; i < dguitar::kNumKnobs; ++i)
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { kKnobIds[i], 1 }, kKnobNames[i],
                                                                 juce::NormalisableRange<float> (0.0f, 1.0f), d.preset[0][i] / 100.0f, pct));
    juce::StringArray pats { "OFF" };
    for (const auto& pd : dguitar::patterns()) pats.add (pd.name);
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "pattern", 1 }, "Rythme", pats, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "pitchbend", 1 }, "Pitch Bend", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "mod", 1 }, "Modulation", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, pct));
    return layout;
}

DGuitarProcessor::DGuitarProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DGUITAR", createLayout())
{
    pGuitar = apvts.getRawParameterValue ("guitar");
    pPreset = apvts.getRawParameterValue ("preset");
    for (int i = 0; i < dguitar::kNumKnobs; ++i) pKnob[i] = apvts.getRawParameterValue (kKnobIds[i]);
    pPitch = apvts.getRawParameterValue ("pitchbend");
    pMod = apvts.getRawParameterValue ("mod");
    pPattern = apvts.getRawParameterValue ("pattern");
    sink.e = &engine;
}

bool DGuitarProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void DGuitarProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    applySettings();
    engine.reset();
    player.prepare (sampleRate);
    fifo.reset();
    midiPitch = 0.0f; midiMod = 0.0f;
}

int DGuitarProcessor::getGuitar() const { return juce::jlimit (0, dguitar::kNumGuitars - 1, juce::roundToInt (pGuitar->load())); }
// jeu d'échantillons voulu : DIVE et CRY = guitare électrique ; ACOUSTIC = acier (Folk), doigts (Arpèges), nylon
int DGuitarProcessor::wantedSet() const
{
    const int g = getGuitar();
    if (g == 1 || g == 2) return 1;
    if (g == 3) return 2 + getPreset();
    return 0;
}

int DGuitarProcessor::getPattern() const { return juce::jlimit (0, dguitar::numPatterns(), juce::roundToInt (pPattern->load())); }

void DGuitarProcessor::setPattern (int p)
{
    p = ((p % (dguitar::numPatterns() + 1)) + dguitar::numPatterns() + 1) % (dguitar::numPatterns() + 1);
    if (auto* q = apvts.getParameter ("pattern"))
    {
        q->beginChangeGesture();
        q->setValueNotifyingHost (q->convertTo0to1 ((float) p));
        q->endChangeGesture();
    }
}

int DGuitarProcessor::getPreset() const { return juce::jlimit (0, 2, juce::roundToInt (pPreset->load())); }

float DGuitarProcessor::effectivePitch() const { return juce::jlimit (-1.0f, 1.0f, pPitch->load() + midiPitch.load()); }
float DGuitarProcessor::effectiveMod() const { return juce::jlimit (0.0f, 1.0f, juce::jmax (pMod->load(), midiMod.load())); }

void DGuitarProcessor::applySettings()
{
    dguitar::Settings s;
    s.guitar = getGuitar();
    s.drive = pKnob[0]->load(); s.tone = pKnob[1]->load(); s.wah = pKnob[2]->load(); s.delay = pKnob[3]->load();
    s.chorus = pKnob[4]->load(); s.reverb = pKnob[5]->load(); s.volume = pKnob[6]->load();
    engine.setSettings (s);
}

void DGuitarProcessor::selectGuitar (int index)
{
    index = juce::jlimit (0, dguitar::kNumGuitars - 1, index);
    auto set = [this] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            p->endChangeGesture();
        }
    };
    const auto& d = dguitar::voicing (index);
    set ("guitar", (float) index);
    set ("preset", 0.0f);
    for (int i = 0; i < dguitar::kNumKnobs; ++i) set (kKnobIds[i], d.preset[0][i] / 100.0f);
    set ("pattern", (float) presetPattern (index, 0));
}

void DGuitarProcessor::selectPreset (int index)
{
    index = ((index % 3) + 3) % 3;
    const auto& d = dguitar::voicing (getGuitar());
    auto set = [this] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (v));
            p->endChangeGesture();
        }
    };
    set ("preset", (float) index);
    for (int i = 0; i < dguitar::kNumKnobs; ++i) set (kKnobIds[i], d.preset[index][i] / 100.0f);
    if (getGuitar() == 3) set ("pattern", (float) presetPattern (3, index));
}

void DGuitarProcessor::uiNote (int note, float velocity, bool on)
{
    int s1, n1, s2, n2;
    fifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0) { fifoData[s1] = { note, velocity, on }; fifo.finishedWrite (1); }
}

void DGuitarProcessor::copyScope (float* dest, int n) const
{
    n = juce::jmin (n, kScope);
    int w = scopeW.load (std::memory_order_acquire);
    for (int i = 0; i < n; ++i) dest[i] = scope[(w - n + i + kScope * 2) % kScope];
}

void DGuitarProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    buffer.clear();
    if (buffer.getNumChannels() < 2 || total == 0) return;

    applySettings();
    engine.setSamples (bank.audioFetch (wantedSet()));
    engineSamples.store (engine.usingSamples());
    engine.releaseNoises = ! player.active();
    {
        const int pat = getPattern();
        if (pat != player.pattern())
        {
            player.allOff (sink);
            player.setPattern (pat);
        }
    }
    bool hostPlaying = false;
    double ppq = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) { engine.setTempo (*bpm); player.setTempo (*bpm); }
            if (pos->getIsPlaying())
                if (auto q = pos->getPpqPosition()) { hostPlaying = true; ppq = *q; }
        }
    player.setHost (hostPlaying, ppq);

    // notes jouées à la souris
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead (fifo.getNumReady(), s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) { const auto& e = fifoData[s1 + i]; if (e.on) engine.noteOn (e.note, e.vel); else engine.noteOff (e.note); }
        for (int i = 0; i < n2; ++i) { const auto& e = fifoData[s2 + i]; if (e.on) engine.noteOn (e.note, e.vel); else engine.noteOff (e.note); }
        fifo.finishedRead (n1 + n2);
    }

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    int pos = 0;

    auto render = [&] (int upTo)
    {
        const int n = upTo - pos;
        if (n <= 0) return;
        engine.pitchWheel (effectivePitch());
        engine.modWheel (effectiveMod());
        if (! player.active())
        {
            engine.process (L + pos, R + pos, n);
            pos = upTo;
            return;
        }
        // avec un rythme : on avance par petits morceaux pour que les coups tombent au bon moment
        while (pos < upTo)
        {
            const int c = juce::jmin (32, upTo - pos);
            player.advance (c, sink);
            engine.process (L + pos, R + pos, c);
            pos += c;
        }
    };

    for (const auto meta : midi)
    {
        render (juce::jlimit (pos, total, meta.samplePosition));
        const auto m = meta.getMessage();
        if (m.isNoteOn()) { if (player.active()) player.noteOn (m.getNoteNumber(), m.getFloatVelocity(), sink); else engine.noteOn (m.getNoteNumber(), m.getFloatVelocity()); }
        else if (m.isNoteOff()) { if (player.active()) player.noteOff (m.getNoteNumber(), sink); else engine.noteOff (m.getNoteNumber()); }
        else if (m.isPitchWheel()) midiPitch = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
        else if (m.isControllerOfType (1)) midiMod = (float) m.getControllerValue() / 127.0f;
        else if (m.isControllerOfType (64)) engine.sustain (m.getControllerValue() >= 64);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { player.allOff (sink); engine.allNotesOff(); }
    }
    render (total);

    // oscilloscope de l'écran
    int w = scopeW.load (std::memory_order_relaxed);
    for (int i = 0; i < total; ++i) { scope[w] = 0.5f * (L[i] + R[i]); w = (w + 1) % kScope; }
    scopeW.store (w, std::memory_order_release);
}

void DGuitarProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, destData);
}

void DGuitarProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* DGuitarProcessor::createEditor() { return new DGuitarEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DGuitarProcessor(); }
