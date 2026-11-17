// D-REVERB — les 8 sources et la traduction des 4 macros en réglages du moteur.
// C'est ICI qu'on ajuste le son de chaque source (durées, couleur, pré-delay...).
#pragma once

#include "DReverbEngine.h"

namespace dreverb
{
struct SourceDef
{
    const char* name;
    const char* algoName;
    int   algorithm;            // 0 Plate, 1 Room, 2 Hall, 3 Cathedral
    float decayMin, decayMax;   // SPACE : durée de la queue (s)
    float sizeMin, sizeMax;     // SPACE : taille
    float preNear, preFar;      // DISTANCE : pré-delay (ms) proche -> loin
    float erNear, erFar;        // DISTANCE : niveau des réflexions précoces
    float hfDark, hfBright;     // TONE : RT aigu / RT médium
    float cutDark, cutBright;   // TONE : coupe-haut (Hz)
    float hfFreq;
    float lfRatio;
    float lowCut;               // coupe-bas (Hz)
    float modDepth, modRate;
    float shimmer;
    float width;               // 0..1 (au-delà de 1, la reverb se creuse en mono)
    float duckRelease;          // ms
    // valeurs par défaut des macros (0..1)
    float defSpace, defDistance, defTone, defDuck, defMix;
};

static constexpr int kNumSources = 8;

static inline const SourceDef& sourceDef (int i)
{
    static const SourceDef defs[kNumSources] = {
        // name          algo         a  decay       size        pre        er         hf           cut            hfF    lf    lowc  mod         shim  wid   rel   defaults
        { "LEAD VOCAL", "PLATE",     0, 0.7f, 3.6f, 0.75f, 1.25f, 70.0f,  6.0f, 0.20f, 0.05f, 0.30f, 0.75f, 5500.0f, 13000.0f, 5000.0f, 0.95f, 190.0f, 0.55f, 0.60f, 0.00f, 1.00f, 320.0f, 0.42f, 0.35f, 0.55f, 0.35f, 0.22f },
        { "CHOIR",      "HALL",      2, 1.4f, 5.5f, 0.85f, 1.45f, 45.0f,  0.0f, 0.55f, 0.25f, 0.30f, 0.70f, 5000.0f, 12000.0f, 4200.0f, 1.10f, 150.0f, 0.40f, 0.45f, 0.00f, 1.00f, 450.0f, 0.45f, 0.50f, 0.50f, 0.20f, 0.28f },
        { "PIANO",      "HALL",      2, 0.9f, 4.2f, 0.70f, 1.25f, 35.0f,  0.0f, 0.50f, 0.20f, 0.28f, 0.62f, 4500.0f, 11000.0f, 3800.0f, 1.05f, 120.0f, 0.35f, 0.40f, 0.00f, 1.00f, 380.0f, 0.40f, 0.45f, 0.45f, 0.15f, 0.20f },
        { "SYNTH LEAD", "PLATE",     0, 1.2f, 6.0f, 0.95f, 1.45f, 45.0f,  8.0f, 0.12f, 0.05f, 0.40f, 0.85f, 6500.0f, 15000.0f, 6000.0f, 0.85f, 280.0f, 0.20f, 0.40f, 0.00f, 1.00f, 280.0f, 0.50f, 0.40f, 0.60f, 0.50f, 0.26f },
        { "PAD",        "CATHEDRAL", 3, 3.0f, 12.0f, 1.00f, 1.80f, 30.0f, 0.0f, 0.25f, 0.10f, 0.35f, 0.75f, 5500.0f, 13000.0f, 4500.0f, 1.20f, 110.0f, 0.50f, 0.30f, 0.30f, 1.00f, 600.0f, 0.50f, 0.60f, 0.55f, 0.00f, 0.40f },
        { "STRINGS",    "HALL",      2, 1.6f, 6.5f, 0.95f, 1.60f, 30.0f,  0.0f, 0.50f, 0.25f, 0.30f, 0.68f, 5000.0f, 12000.0f, 4000.0f, 1.15f, 100.0f, 0.55f, 0.40f, 0.00f, 1.00f, 500.0f, 0.48f, 0.55f, 0.50f, 0.00f, 0.30f },
        { "GUITAR",     "PLATE",     0, 0.8f, 4.0f, 0.80f, 1.30f, 30.0f,  5.0f, 0.25f, 0.10f, 0.38f, 0.80f, 6000.0f, 14000.0f, 5500.0f, 0.90f, 180.0f, 0.25f, 0.45f, 0.00f, 1.00f, 300.0f, 0.40f, 0.40f, 0.55f, 0.15f, 0.22f },
        { "DRUMS",      "ROOM",      1, 0.25f, 1.8f, 0.60f, 1.35f, 18.0f, 0.0f, 0.75f, 0.45f, 0.35f, 0.80f, 6000.0f, 14000.0f, 5000.0f, 0.85f,  90.0f, 0.25f, 0.50f, 0.00f, 1.00f, 180.0f, 0.38f, 0.35f, 0.55f, 0.00f, 0.15f },
    };
    return defs[std::max (0, std::min (i, kNumSources - 1))];
}

static inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
static inline float clamp01 (float v) { return std::max (0.0f, std::min (v, 1.0f)); }

// Traduit source + macros (0..1) en paramètres du moteur
static inline Params makeParams (int source, float space, float distance, float tone, float duck, float mix)
{
    const SourceDef& d = sourceDef (source);
    space = clamp01 (space); distance = clamp01 (distance); tone = clamp01 (tone);
    Params p;
    p.algorithm = d.algorithm;
    // la durée progresse de façon exponentielle : réglage fin en bas, long en haut
    p.decaySec = d.decayMin * std::pow (d.decayMax / d.decayMin, space);
    p.size = lerp (d.sizeMin, d.sizeMax, space);
    p.preDelayMs = lerp (d.preNear, d.preFar, distance);
    p.erLevel = lerp (d.erNear, d.erFar, distance);
    p.lateLevel = lerp (0.85f, 1.15f, distance);
    // plus loin = un peu plus sombre
    const float t = clamp01 (tone - 0.18f * (distance - 0.5f));
    p.hfRatio = lerp (d.hfDark, d.hfBright, t);
    p.highCutHz = d.cutDark * std::pow (d.cutBright / d.cutDark, t);
    p.hfFreq = d.hfFreq * lerp (0.8f, 1.25f, t);
    p.lfRatio = d.lfRatio;
    p.lfFreq = 250.0f;
    p.lowCutHz = d.lowCut;
    p.diffusion = 1.0f;
    p.modDepth = d.modDepth;
    p.modRate = d.modRate;
    p.shimmer = d.shimmer;
    p.width = d.width;
    p.duck = clamp01 (duck);
    p.duckReleaseMs = d.duckRelease;
    p.mix = clamp01 (mix);
    return p;
}

} // namespace dreverb
