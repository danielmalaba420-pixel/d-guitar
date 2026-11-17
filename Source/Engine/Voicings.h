// D-GUITAR — les 6 guitares : ICI on règle la corde, le micro, le wah, l'ampli et les presets.
// Studio Danova — Daniel Malaba
#pragma once

namespace dguitar
{
static constexpr int kNumGuitars = 3;
static constexpr int kNumKnobs = 7;   // AMP DRIVE, TONE, WAH, DELAY, CHORUS, REVERB, VOLUME

enum class ModMode { Vibrato, Wah, WahVibrato, Whammy, Tremolo };

struct Voicing
{
    const char* name;
    // jeu
    bool  mono;            // une seule note à la fois
    bool  legato;          // (mono) une note liée glisse au lieu d'être regrattée
    int   maxVoices;
    float glideMs;         // portamento des notes liées
    float bendUp, bendDown;     // molette de pitch bend, en demi-tons
    float strumMs;         // étalement d'un accord (effet « main qui gratte »)
    // corde
    float t60;             // durée de résonance (s) pour un La 220 Hz
    float loopFc;          // brillance de la corde (Hz) : filtre de boucle
    float disp;            // raideur de la corde (passe-tout de dispersion, négatif)
    float pickPos;         // où on pince la corde (0..1 depuis le sillet)
    float pickMs;          // largeur du « coup de médiator » (ms) : petit = brillant
    float pickNoise;       // bruit d'attaque
    float releaseMs;       // étouffement à la fin de la note
    float swellMs;         // 0 = attaque normale ; sinon « volume swell » (pédale de volume)
    float slideNoise;      // frottement du bottleneck pendant un glissando
    // micros : position (0..1 depuis le sillet) et poids ; deux micros possibles
    float pkA, wA, pkB, wB;
    float pkResHz, pkResQ;   // résonance électrique du micro
    // modulation (molette MOD)
    ModMode mod;
    float vibCents, vibRate;
    float tremDepth;         // trémolo d'ampli (mode Tremolo)
    // wah
    float wahQ, wahMin, wahMax, wahBase, wahAuto, wahFormant;
    // ampli
    float driveMinDb, driveMaxDb;
    float bias;             // asymétrie (lampes)
    float preMidDb;         // bosse de médiums avant la saturation
    float cabHp, cabLp, cabPeakHz, cabPeakDb;
    float comp;             // compresseur (0..1)
    float rvSpace, rvTone;  // caractère de la reverb
    // presets : nom + 7 potards (0..100)
    const char* presetName[3];
    float preset[3][kNumKnobs];
    float trim[9];          // correction de niveau (dB) pour DRIVE = 0, 12.5 %, ... 100 % (mesurée)
};

static inline const Voicing& voicing (int i)
{
    static const Voicing v[kNumGuitars] = {
        // ---------------------------------------------------------------- CLEAN : cristallin, trémolo, chorus
        { "CLEAN", false, false, 8, 0, 2, 2, 12,
          12.0f, 16000, -0.12f, 0.20f, 0.5f, 0.06f, 160, 0, 0.0f,
          0.855f, 0.5f, 0.757f, 0.5f, 6200, 0.9f,
          ModMode::Tremolo, 8, 5.0f, 0.5f,
          4.0f, 450, 2200, 0.3f, 0.0f, 0.0f,
          0.0f, 14.0f, 0.04f, 0.0f, 70, 6800, 3000, 1.0f, 0.45f, 0.50f, 0.60f,
          { "Cristal", "Arpèges", "Funk" },
          { { 10, 65, 0, 35, 40, 45, 78 }, { 15, 60, 0, 55, 50, 60, 74 }, { 20, 70, 10, 15, 25, 20, 80 } },
          { 1.5, 0.6, -0.4, -1.5, -2.6, -3.6, -4.6, -5.5, -6.3 } },
        // ---------------------------------------------------------------- DIVE : humbucker chevalet, levier (whammy)
        { "DIVE", false, false, 6, 0, 3, 24, 5,
          7.0f, 6500, -0.16f, 0.09f, 0.45f, 0.10f, 90, 0, 0.0f,
          0.96f, 1.0f, 0.855f, 0.0f, 3700, 1.8f,
          ModMode::Whammy, 20, 5.8f, 0.0f,
          4.0f, 450, 2200, 0.3f, 0.0f, 0.0f,
          6.0f, 60.0f, 0.22f, 4.0f, 90, 4500, 2800, 2.0f, 0.10f, 0.40f, 0.40f,
          { "Rock", "Surf", "Lead" },
          { { 70, 60, 0, 30, 0, 30, 76 }, { 45, 65, 0, 40, 35, 55, 74 }, { 75, 55, 0, 50, 10, 40, 72 } },
          { 2.7, -0.3, -3.3, -5.9, -8.1, -9.8, -11.3, -12.2, -12.8 } },
        // ---------------------------------------------------------------- CRY : lead qui chante (bends, vibrato, voyelles)
        { "CRY", true, true, 3, 70, 2, 2, 0,
          10.0f, 6000, -0.12f, 0.15f, 0.7f, 0.06f, 300, 0, 0.0f,
          0.757f, 1.0f, 0.855f, 0.0f, 3000, 1.6f,
          ModMode::WahVibrato, 34, 5.5f, 0.0f,
          3.5f, 450, 2100, 0.45f, 0.20f, 0.50f,
          10.0f, 48.0f, 0.18f, 3.0f, 80, 4800, 1800, 3.5f, 0.30f, 0.45f, 0.45f,
          { "Blues", "Chant", "Saturé" },
          { { 55, 60, 10, 25, 10, 35, 76 }, { 65, 55, 25, 40, 15, 45, 74 }, { 80, 65, 10, 20, 0, 25, 70 } },
          { -5.3, -6.8, -8.3, -9.6, -10.7, -11.6, -12.3, -12.8, -13.0 } },
    };
    return v[i < 0 ? 0 : (i >= kNumGuitars ? kNumGuitars - 1 : i)];
}
} // namespace dguitar
