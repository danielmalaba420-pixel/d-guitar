// D-GUITAR — les 6 guitares : ICI on règle la corde, le micro, le wah, l'ampli et les presets.
// Studio Danova — Daniel Malaba
#pragma once

namespace dguitar
{
static constexpr int kNumGuitars = 6;
static constexpr int kNumKnobs = 7;
static constexpr int kCryNotes = 6;   // voicing caché : CRY en mode NOTES (le guitariste joue tes notes, comme un piano)   // AMP DRIVE, TONE, WAH, DELAY, CHORUS, REVERB, VOLUME

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
    int   samples = 0;       // 0 = corde modélisée ; 1 = vraie guitare électrique (DI) ; 2 = vraie guitare acoustique ; 3 = slide note par note ; 4 = une prise entière par tonalité (CRY)
    float sustainer = 0.0f;  // (échantillons) compense la décroissance : 1 = la note chante sans fin
    bool  vibUp = false;     // vibrato de guitariste : la note monte et revient (jamais en dessous)
    float wide = 0.0f;       // double prise stéréo (guitare doublée gauche/droite) : son large
    float falloff = 0.0f;    // (CRY) au relâchement, la note glisse vers le bas (demi-tons), comme un glissé de fin de phrase
    float autoVibRise = 0.5f;// temps d'arrivée du vibrato automatique (s)
    bool  roundTone = false; // (CRY) son rond et chantant : filtre qui suit chaque note (presque que la fondamentale) ; TONE l'ouvre
    bool  shimmerKnob = false; // (WORSHIP) le 3ᵉ potard règle le shimmer de la reverb (octave au-dessus) au lieu du wah
    bool  glideKnob = false;   // (CRY) le 3ᵉ potard règle la vitesse du glissé (bottleneck) entre deux notes liées
    int   rvSource = 6;        // moteur D-REVERB : source (6 = guitare/plate ; 5 = cordes/hall long, CRY NOTES)
    bool  slideMode = false;   // (MOTION) guitare slide : la note glisse au lieu d'être rejouée, bruits de contact, vibrato de slide
    float shimmerFix = 0.0f;   // (MOTION) shimmer de reverb fixe (0..1), sans potard
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
        // delay croche pointée calé sur le tempo, grande reverb avec shimmer, molette MODULATION = swells (pédale de volume),
        // vibrato discret qui arrive sur les notes tenues (émotion)
        { "WORSHIP", false, false, 6, 0, 2, 2, 10,
          14.0f, 4800, -0.03f, 0.15f, 0.9f, 0.04f, 420, 0, 0.0f,
          0.757f, 1.0f, 0.855f, 0.0f, 2600, 1.0f,
          ModMode::Swell, 0, 5.0f, 0.0f,
          4.5f, 420, 1900, 0.18f, 0.0f, 0.0f,
          2.0f, 18.0f, 0.10f, 1.0f, 90, 6500, 2500, 2.0f, 0.45f, 0.85f, 0.60f,
          { "Ambient", "Lead", "Rythme" },
          { { 25, 55, 55, 55, 25, 80, 74 }, { 40, 60, 25, 50, 10, 55, 74 }, { 50, 55, 0, 30, 15, 35, 76 } },
          { 4.3, 3.1, 1.6, 0.1, -1.5, -3.0, -4.4, -5.8, -7.0 },
          0.8f, 0.3f, 14.0f, 5.0f, 0.55f, false, 0.4f, 0.0f, 1, 0.25f, false, 0.6f, 0.0f, 0.8f, false, true },
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
        // ---------------------------------------------------------------- CRY : les prises de slide de Dan, une prise ENTIÈRE par tonalité
        // (comme les pads par tonalité) : n'importe quel Do joue ta prise en Do, etc. ; une seule tonalité à la fois,
        // changer de touche = fondu enchaîné (3ᵉ potard FONDU) ; lâcher = la prise s'éteint en douceur (3 s).
        // Presets = les 3 modèles : Selah, Aurore, Eden. Tonalités manquantes : transposées (Rubber Band, timbre gardé).
        { "CRY", false, false, 4, 0, 2, 2, 0,
          6.0f, 8000, -0.03f, 0.15f, 0.9f, 0.0f, 3000, 0, 0.0f,
          0.96f, 1.0f, 0.855f, 0.0f, 8000, 0.7f,
          ModMode::Vibrato, 0, 5.2f, 0.0f,
          4.0f, 450, 2200, 0.3f, 0.0f, 0.0f,
          0.0f, 0.0f, 0.0f, 0.0f, 70, 12000, 3000, 0.0f, 0.0f, 0.75f, 0.55f,
          { "Selah", "Aurore", "Eden" },
          { { 0, 50, 49, 0, 0, 0, 76 }, { 0, 50, 49, 0, 0, 0, 76 }, { 0, 50, 49, 0, 0, 0, 76 } },
          { 16.6, 16.6, 16.7, 16.7, 16.8, 17.1, 17.4, 17.8, 18.3 },
          1.0f, -1.0f, 0.0f, 5.2f, 0.35f, false, 0.0f, 0.0f, 4, 0.0f, false, 0.0f, 0.0f, 0.5f, false, false, true },
    };
    // ---------------------------------------------------------------- CRY mode NOTES : chaque touche = sa note, jouée avec le son de tes prises
    // note seule : la note tenue de ta prise (vibrato aplati puis vibrato commun qui arrive après l'attaque, petit glissé d'entrée) ;
    // notes liées : glissé d'une note à l'autre ; accord : vraie corde électrique (stéréo, deux prises) + le son slide qui arrive derrière.
    static const Voicing cryNotes = [] {
        Voicing c = v[5];
        c.mono = false; c.legato = false; c.maxVoices = 28;
        c.glideMs = 30.0f; c.bendUp = 2.0f; c.bendDown = 2.0f; c.strumMs = 18.0f;
        c.releaseMs = 1270.0f;            // queue de la note lâchée (-60 dB en 1,27 s, comme la démo validée)
        c.mod = ModMode::Vibrato; c.vibCents = 30.0f; c.vibRate = 4.2f;   // molette : vibrato plus large
        c.comp = 0.0f; c.rvSource = 5; c.rvSpace = 0.92f; c.rvTone = 0.0f;   // hall long et doux (~5 s), comme l'ambiance de tes prises
        c.preset[0][0] = 0; c.preset[0][1] = 20; c.preset[0][2] = 49; c.preset[0][3] = 22; c.preset[0][4] = 0; c.preset[0][5] = 85; c.preset[0][6] = 76;
        for (int p = 1; p < 3; ++p) for (int k = 0; k < kNumKnobs; ++k) c.preset[p][k] = c.preset[0][k];
        c.autoVibCents = 14.0f; c.autoVibRate = 4.2f; c.autoVibDelay = 0.35f; c.autoVibRise = 0.6f;
        c.samples = 5; c.sustainer = 0.0f; c.vibUp = false; c.falloff = 0.0f; c.roundTone = false;
        c.shimmerKnob = false; c.glideKnob = true;
        for (auto& t : c.trim) t += 3.0;   // même volume que le mode PRISE (mesuré au rendu)
        return c;
    }();
    // ---------------------------------------------------------------- MOTION : guitare slide worship, vraies notes de slide (motion-slide.dgs)
    // jouée doucement : la note arrive en « ouuh » (volume qui monte + glissé d'un ton) ; jouée fort : coup de médiator puis glissé d'arrivée ;
    // notes liées : la bouteille glisse sans rejouer la corde ; accords : la barre sur plusieurs cordes. Grande reverb, delay croche pointée.
    static const Voicing motion = [] {
        Voicing m = v[2];                      // base : WORSHIP (ampli clair au bord de la saturation, même vraie guitare)
        m.name = "MOTION";
        m.mono = false; m.legato = false; m.maxVoices = 14; m.strumMs = 0.0f;
        m.glideMs = 30.0f; m.bendUp = 2.0f; m.bendDown = 2.0f;
        m.releaseMs = 380.0f;                  // les cordes derrière la bouteille sont étouffées : extinction courte et naturelle
        m.swellMs = 0.0f;
        m.mod = ModMode::Vibrato; m.vibCents = 35.0f; m.vibRate = 5.2f;   // molette : vibrato de slide plus large
        m.autoVibCents = 0.0f;    // la prise a déjà son propre vibrato de slide : on n'ajoute rien (son naturel)
        m.autoVibRate = 5.2f; m.autoVibDelay = 0.30f; m.autoVibRise = 0.55f;
        m.sustainer = 0.0f;                    // extinction naturelle de la prise, rien d'ajouté
        m.shimmerKnob = false; m.glideKnob = true; m.rvSource = 5; m.rvSpace = 0.80f; m.rvTone = 0.35f;
        m.presetName[0] = "Worship"; m.presetName[1] = "Lead"; m.presetName[2] = "Ambient";
        //                 drive tone glis delay chor reverb vol
        const float p[3][kNumKnobs] = { { 0, 50, 49, 36, 0, 70, 74 }, { 15, 55, 40, 30, 0, 50, 74 }, { 0, 45, 60, 46, 0, 88, 72 } };
        for (int a = 0; a < 3; ++a) for (int k = 0; k < kNumKnobs; ++k) m.preset[a][k] = p[a][k];
        m.slideMode = true;
        for (auto& t : m.trim) t = 11.9f;     // pas d'ampli : niveau remis à celui des autres guitares (mesuré au rendu)
        m.comp = 0.0f; m.wide = 0.0f;          // ni compresseur ni guitare doublée : la prise telle qu'enregistrée
        m.shimmerFix = 0.0f;                   // pas de shimmer : la guitare telle qu'enregistrée
        return m;
    }();
    if (i == kCryNotes) return cryNotes;
    if (i == 5) return motion;   // la 6ᵉ guitare est MOTION (CRY retiré)
    return v[i < 0 ? 0 : (i >= kNumGuitars ? kNumGuitars - 1 : i)];
}
// la guitare à l'écran (0..5) et le mode CRY -> le voicing utilisé par le moteur
static inline int voicingIndex (int guitar, bool) { return guitar; }   // (CRY et ses modes sont retirés : la 6ᵉ guitare est MOTION)
} // namespace dguitar
