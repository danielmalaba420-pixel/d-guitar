// D-GUITAR — les 6 guitares : ICI on règle la corde, le micro, le wah, l'ampli et les presets.
// Studio Danova — Daniel Malaba
#pragma once

namespace dguitar
{
static constexpr int kNumGuitars = 6;
static constexpr int kNumKnobs = 7;   // AMP DRIVE, TONE, WAH, DELAY, CHORUS, REVERB, VOLUME

enum class ModMode { Vibrato, Wah, WahVibrato, Whammy, Tremolo, PalmMute, Swell };

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
    // ajouts v1.1
    float fizz = 1.0f;       // coupures entre les étages de l'ampli (1 = clair, < 1 = plus sombre, moins de « sifflement »)
    float room = -1.0f;      // rugosité du baffle (0..1), -1 = automatique
    float autoVibCents = 0;  // vibrato automatique qui s'installe après l'attaque (chanteur)
    float autoVibRate = 5.2f;
    float autoVibDelay = 0.35f;
    bool  acoustic = false;  // guitare acoustique : pas d'ampli, caisse de résonance à la place
    float hex = 0.0f;        // saturation corde par corde avant l'ampli (0..1) : accords plus nets, moins d'intermodulation
    float scoop = 0.0f;      // (CRY) attaque « pleurée » : la note part d'en dessous et monte (demi-tons, notes fortes)
    int   samples = 0;       // 0 = corde modélisée ; 1 = vraie guitare électrique (DI) ; 2 = vraie guitare acoustique ; 3 = slide de Dan (prises avec effets, stéréo)
    float sustainer = 0.0f;  // (échantillons) compense la décroissance : 1 = la note chante sans fin
    bool  vibUp = false;     // vibrato de guitariste : la note monte et revient (jamais en dessous)
    float wide = 0.0f;       // double prise stéréo (guitare doublée gauche/droite) : son large
    float falloff = 0.0f;    // (CRY) au relâchement, la note glisse vers le bas (demi-tons), comme un glissé de fin de phrase
    float autoVibRise = 0.5f;// temps d'arrivée du vibrato automatique (s)
    bool  roundTone = false; // (CRY) son rond et chantant : filtre qui suit chaque note (presque que la fondamentale) ; TONE l'ouvre
    bool  shimmerKnob = false; // (WORSHIP) le 3ᵉ potard règle le shimmer de la reverb (octave au-dessus) au lieu du wah
    bool  glideKnob = false;   // (CRY) le 3ᵉ potard règle la vitesse du glissé (bottleneck) entre deux notes liées
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
          6.0f, 4400, -0.03f, 0.09f, 0.80f, 0.04f, 90, 0, 0.0f,
          0.96f, 1.0f, 0.855f, 0.0f, 2900, 1.0f,
          ModMode::Whammy, 90, 5.6f, 0.0f,
          4.0f, 450, 2200, 0.3f, 0.0f, 0.0f,
          6.0f, 40.0f, 0.18f, 4.0f, 78, 5200, 2600, 2.0f, 0.10f, 0.40f, 0.40f,
          { "Rock", "Surf", "Lead" },
          { { 70, 60, 0, 30, 0, 30, 76 }, { 45, 65, 0, 40, 35, 55, 74 }, { 75, 55, 0, 50, 10, 40, 72 } },
          { 0.0, -2.3, -4.7, -6.9, -8.6, -10.0, -10.9, -11.6, -12.0 },
          0.65f, 0.20f, 0.0f, 5.2f, 0.35f, false, 0.85f, 0.0f, 1, 0.35f, true, 0.8f },
        // ---------------------------------------------------------------- WORSHIP : guitare électrique worship (style Hillsong) : son clair au bord de la saturation,
        // delay croche pointée calé sur le tempo, grande reverb avec shimmer, molette MODULATION = swells (pédale de volume)
        { "WORSHIP", false, false, 6, 0, 2, 2, 10,
          14.0f, 4800, -0.03f, 0.15f, 0.9f, 0.04f, 320, 0, 0.0f,
          0.757f, 1.0f, 0.855f, 0.0f, 2600, 1.0f,
          ModMode::Swell, 0, 5.0f, 0.0f,
          4.5f, 420, 1900, 0.18f, 0.0f, 0.0f,
          2.0f, 18.0f, 0.10f, 1.0f, 90, 6500, 2500, 2.0f, 0.45f, 0.85f, 0.60f,
          { "Ambient", "Lead", "Rythme" },
          { { 25, 55, 55, 55, 25, 80, 74 }, { 40, 60, 25, 50, 10, 55, 74 }, { 50, 55, 0, 30, 15, 35, 76 } },
          { 4.3, 3.1, 1.6, 0.1, -1.5, -3.0, -4.4, -5.8, -7.0 },
          0.8f, 0.3f, 0.0f, 5.0f, 0.35f, false, 0.4f, 0.0f, 1, 0.25f, false, 0.6f, 0.0f, 0.5f, false, true },
        // ---------------------------------------------------------------- ACOUSTIC : folk à cordes acier, caisse de résonance, arpèges
        { "ACOUSTIC", false, false, 12, 0, 2, 2, 16,
          5.5f, 4200, -0.08f, 0.17f, 4.0f, 0.01f, 240, 0, 0.0f,
          0.97f, 1.0f, 0.5f, 0.0f, 9000, 0.7f,
          ModMode::PalmMute, 0, 5.0f, 0.0f,
          3.5f, 450, 2100, 0.3f, 0.0f, 0.0f,
          0.0f, 0.0f, 0.0f, 0.0f, 70, 12000, 3000, 0.0f, 0.25f, 0.50f, 0.60f,
          { "Folk", "Arpèges", "Nylon" },
          { { 55, 55, 50, 15, 0, 30, 76 }, { 50, 60, 60, 30, 20, 45, 74 }, { 75, 40, 35, 10, 0, 25, 76 } },
          { -4.7, -5.0, -5.5, -5.9, -6.4, -6.9, -7.5, -8.1, -8.7 },
          1.0f, -1.0f, 0.0f, 5.2f, 0.35f, true, 0.0f, 0.0f, 2 },
        // ---------------------------------------------------------------- BLACK : acoustique worship (guitare acier seule, chorus, grande reverb, son large)
        { "BLACK", false, false, 12, 0, 2, 2, 16,
          5.5f, 4200, -0.08f, 0.17f, 4.0f, 0.01f, 240, 0, 0.0f,
          0.97f, 1.0f, 0.5f, 0.0f, 9000, 0.7f,
          ModMode::PalmMute, 0, 5.0f, 0.0f,
          3.5f, 450, 2100, 0.3f, 0.0f, 0.0f,
          0.0f, 0.0f, 0.0f, 0.0f, 70, 12000, 3000, 0.0f, 0.35f, 0.80f, 0.65f,
          { "Worship", "Ballade", "Ambient" },
          { { 50, 55, 55, 25, 40, 55, 76 }, { 55, 50, 45, 15, 25, 45, 76 }, { 45, 50, 60, 40, 45, 70, 74 } },
          { -3.5, -3.8, -4.2, -4.6, -5.0, -5.5, -6.0, -6.5, -7.1 },
          1.0f, -1.0f, 0.0f, 5.2f, 0.35f, true, 0.0f, 0.0f, 2, 0.0f, false, 0.35f },
        // ---------------------------------------------------------------- CRY : les prises de slide de Dan (bottleneck), jouées note par note.
        // Notes liées = glissé du bottleneck ; molette MODULATION = vibrato de slide ; 3ᵉ potard = vitesse du glissé.
        // Presets réglés en comparant CRY à tes phrases d'origine (A/B mesuré) : reverb ~2,6 s au même niveau que tes queues,
        // pas de saturation, un soupçon de delay, chorus seulement là où ta prise est plus large que les notes rejouées.
        { "CRY", true, true, 8, 140, 2, 2, 0,
          6.0f, 8000, -0.03f, 0.15f, 0.9f, 0.0f, 450, 0, 0.0f,
          0.96f, 1.0f, 0.855f, 0.0f, 8000, 0.7f,
          ModMode::Vibrato, 45, 5.2f, 0.0f,
          4.0f, 450, 2200, 0.3f, 0.0f, 0.0f,
          0.0f, 0.0f, 0.0f, 0.0f, 70, 12000, 3000, 0.0f, 0.20f, 0.75f, 0.55f,
          { "Angelitus", "Piratas", "Ratones" },
          { { 0, 50, 30, 12, 25, 45, 76 }, { 0, 50, 30, 12, 0, 45, 76 }, { 0, 50, 30, 12, 25, 45, 76 } },
          { 12.9, 12.9, 13.0, 13.2, 13.6, 14.1, 14.7, 15.4, 16.2 },
          1.0f, -1.0f, 0.0f, 5.2f, 0.35f, false, 0.0f, 0.0f, 3, 0.0f, false, 0.0f, 0.0f, 0.5f, false, false, true },
    };
    return v[i < 0 ? 0 : (i >= kNumGuitars ? kNumGuitars - 1 : i)];
}
} // namespace dguitar
