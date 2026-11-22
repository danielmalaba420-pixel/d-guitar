// D-GUITAR — quel fichier d'échantillons (.dgs) chaque guitare / preset utilise.
// Un seul endroit, partagé par le plugin ET les outils de rendu/tests : ce qu'on écoute en test est ce que joue le plugin.
#pragma once

namespace dguitar
{
// 0 = aucun (corde modélisée)
static constexpr const char* kSampleSetNames[] = { "", "electric-di", "steel", "finger", "nylon", "steel1",
                                                   "cry-prise-selah", "cry-prise-aurore", "cry-prise-eden",     // CRY mode PRISE (une prise par tonalité)
                                                   "cry-notes-selah", "cry-notes-aurore", "cry-notes-eden",     // (anciens CRY, plus utilisés)
                                                   "motion-slide" };                                            // MOTION : vraie guitare slide, une note par touche
static constexpr int kNumSampleSets = (int) (sizeof (kSampleSetNames) / sizeof (kSampleSetNames[0]));

inline const char* sampleSetName (int id) { return (id >= 0 && id < kNumSampleSets) ? kSampleSetNames[id] : ""; }

// guitare : 0 CLEAN, 1 DIVE, 2 WORSHIP, 3 ACOUSTIC, 4 BLACK, 5 MOTION ; preset 0..2 ; cryPrise : CRY en mode PRISE (sinon NOTES)
inline int wantedSampleSet (int guitar, int preset, bool cryPrise = false)
{
    preset = preset < 0 ? 0 : (preset > 2 ? 2 : preset);
    if (guitar == 1 || guitar == 2) return 1;          // guitare électrique (DI)
    if (guitar == 3) return 2 + preset;                // ACOUSTIC : Folk (acier), Arpèges (doigts), Nylon
    if (guitar == 4) return 5;                         // BLACK : acier seule (Steel1)
    if (guitar == 5) { (void) cryPrise; return 12; }   // MOTION : vraie guitare slide (motion-slide.dgs)
    return 0;
}
} // namespace dguitar
