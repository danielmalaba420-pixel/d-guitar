# D-GUITAR

Instrument AU + VST3 (Studio Danova) : 3 guitares électriques à modélisation physique, sans samples.

- **CLEAN** : son clair, trémolo d'ampli
- **DIVE** : son saturé (ampli 3 étages), levier au pitch bend
- **CRY** : lead chantant, bends, vibrato, glissés

Moteur : `Source/Engine/GuitarEngine.h` (guide d'onde numérique, pincement triangulaire, micro à vitesse, ampli à lampes suréchantillonné x4, baffle à phase minimale, chorus, delay synchronisé au tempo, reverb D-REVERB).
Compilation : `cmake -B build -G Xcode && cmake --build build --config Release`. Le workflow GitHub Actions construit l'installateur Mac (.pkg) sur la branche `builds-mac`.
Tests : `tests/` (rendu hors ligne, calibration des niveaux, comparaison avec une guitare réelle).
