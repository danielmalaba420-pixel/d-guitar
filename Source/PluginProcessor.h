#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/GuitarEngine.h"

class DGuitarProcessor : public juce::AudioProcessor
{
public:
    DGuitarProcessor();
    ~DGuitarProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "D-GUITAR"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // choix d'une guitare / d'un preset : remet les 7 potards aux valeurs prévues
    void selectGuitar (int index);
    void selectPreset (int index);
    int getGuitar() const;
    int getPreset() const;
    static const char* knobId (int i);

    // notes jouées avec la souris sur les cordes (envoyées au moteur depuis le fil audio)
    void uiNote (int note, float velocity, bool on);

    // valeurs effectives des molettes (interface + MIDI) et lectures pour l'affichage
    float effectivePitch() const;
    float effectiveMod() const;
    float vizAmp (int string) const { return engine.vizAmp (string); }
    float wahPosition() const { return engine.wahPosition(); }
    float outputLevel() const { return engine.outputLevel(); }
    void copyScope (float* dest, int n) const;   // les n derniers échantillons (mono)

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void applySettings();

    dguitar::Engine engine;
    std::atomic<float>* pGuitar = nullptr;
    std::atomic<float>* pPreset = nullptr;
    std::atomic<float>* pKnob[dguitar::kNumKnobs] {};
    std::atomic<float>* pPitch = nullptr;
    std::atomic<float>* pMod = nullptr;

    std::atomic<float> midiPitch { 0.0f }, midiMod { 0.0f };

    struct UiNote { int note; float vel; bool on; };
    static constexpr int kFifo = 256;
    juce::AbstractFifo fifo { kFifo };
    UiNote fifoData[kFifo];

    static constexpr int kScope = 8192;
    float scope[kScope] {};
    std::atomic<int> scopeW { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DGuitarProcessor)
};
