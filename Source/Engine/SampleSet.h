// D-GUITAR — jeux d'échantillons (vraies guitares enregistrées).
// Fichier .dgs : "DGS1", u32 fréquence, u32 nombre de zones, puis pour chaque zone :
//   u8 note racine, u8 vélocité max, u8 alternance (aller/retour du médiator), u8 canaux,
//   u8 type (0 = note, 1 = bruit de relâchement), 3 octets libres, u32 longueur, puis PCM 16 bits entrelacé.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace dguitar
{
struct SampleZone
{
    int root = 60, vel = 127, rr = 0, ch = 1, kind = 0;
    int frames = 0;
    std::vector<int16_t> pcm;
    // enveloppe RMS (pas de 10 ms) pour le « sustainer » et point de boucle (notes tenues sans fin)
    std::vector<float> env;
    int envHop = 441;
    int loopStart = -1, loopEnd = -1, xfade = 0;
    int relPos = 0;   // (slide) fin de la partie tenue : après, la queue naturelle (reverb, échos) de l'enregistrement
    int attackPos = 0; // (slide) début de la partie tenue (après le glissé d'entrée de la prise)

    inline float at (int i, int c) const { return (float) pcm[(size_t) (i * ch + c)] * (1.0f / 32768.0f); }
    float envAt (double pos) const
    {
        if (env.empty()) return 1.0f;
        const int k = std::min ((int) env.size() - 1, std::max (0, (int) (pos / envHop)));
        return env[(size_t) k];
    }
};

struct SampleSet
{
    std::string name;
    bool pad = false;   // « DGP1 » : une prise entière par tonalité (CRY) : boucle longue, fondu déjà cuit dans la prise
    bool notes = false; // « DGN1 » : CRY mode notes : note seule (0), accord tenu (2), corde électrique (3) ; boucle cuite
    int sr = 44100;
    std::vector<SampleZone> zones;
    float peakEnv = 1.0f;

    bool load (const std::string& path)
    {
        FILE* f = std::fopen (path.c_str(), "rb");
        if (f == nullptr) return false;
        char magic[4];
        uint32_t hdr[2];
        bool ok = std::fread (magic, 1, 4, f) == 4 && magic[0] == 'D' && magic[1] == 'G' && (magic[2] == 'S' || magic[2] == 'P' || magic[2] == 'N') && magic[3] == '1'
                  && std::fread (hdr, 4, 2, f) == 2;
        if (ok)
        {
            pad = magic[2] == 'P';
            notes = magic[2] == 'N';
            sr = (int) hdr[0];
            zones.resize (hdr[1]);
            for (auto& z : zones)
            {
                uint8_t b[8];
                uint32_t len = 0;
                if (std::fread (b, 1, 8, f) != 8 || std::fread (&len, 4, 1, f) != 1) { ok = false; break; }
                z.root = b[0]; z.vel = b[1]; z.rr = b[2]; z.ch = std::max (1, (int) b[3]); z.kind = b[4];
                {   // 18 bits : fin de la partie tenue ; 6 bits : glissé d'entrée (pas de 10 ms)
                    const int packed = (int) b[5] | ((int) b[6] << 8) | ((int) b[7] << 16);
                    if (pad || notes) z.relPos = packed;   // début de boucle (choisi à la fabrication), 0 = pas de boucle
                    else
                    {
                        z.relPos = packed & 0x3FFFF;
                        z.attackPos = (packed >> 18) * (int) (hdr[0] / 100);
                    }
                }
                z.frames = (int) len;
                z.pcm.resize ((size_t) len * (size_t) z.ch);
                if (std::fread (z.pcm.data(), 2, z.pcm.size(), f) != z.pcm.size()) { ok = false; break; }
                analyse (z);
            }
        }
        std::fclose (f);
        if (! ok) zones.clear();
        return ok && ! zones.empty();
    }

    // la zone la plus proche (hauteur) dans la bonne couche de vélocité ; rr alterne les coups aller/retour
    const SampleZone* pick (int note, float vel01, int rr, int kind = 0) const
    {
        const int v = (int) std::lround (vel01 * 127.0f);
        int bestDist = 1 << 20;
        for (const auto& z : zones) if (z.kind == kind) bestDist = std::min (bestDist, std::abs (z.root - note) * 2 + (z.root < note ? 1 : 0));
        if (bestDist == (1 << 20)) return nullptr;
        // couches à la hauteur retenue
        const SampleZone* chosen = nullptr;
        int chosenVel = 1 << 20;
        for (const auto& z : zones)
        {
            if (z.kind != kind || std::abs (z.root - note) * 2 + (z.root < note ? 1 : 0) != bestDist) continue;
            if (z.vel >= v && z.vel < chosenVel) chosenVel = z.vel;
        }
        if (chosenVel == (1 << 20))   // vélocité au-dessus de toutes les couches : la plus forte
            for (const auto& z : zones)
                if (z.kind == kind && std::abs (z.root - note) * 2 + (z.root < note ? 1 : 0) == bestDist) chosenVel = std::max (chosenVel == (1 << 20) ? 0 : chosenVel, z.vel);
        int n = 0;
        for (const auto& z : zones)
            if (z.kind == kind && z.vel == chosenVel && std::abs (z.root - note) * 2 + (z.root < note ? 1 : 0) == bestDist) ++n;
        int want = n > 0 ? rr % n : 0;
        for (const auto& z : zones)
            if (z.kind == kind && z.vel == chosenVel && std::abs (z.root - note) * 2 + (z.root < note ? 1 : 0) == bestDist)
            {
                if (want-- == 0) { chosen = &z; break; }
            }
        return chosen;
    }

private:
    void analyse (SampleZone& z)
    {
        z.envHop = std::max (1, sr / 100);
        const int nb = z.frames / z.envHop;
        z.env.assign ((size_t) std::max (1, nb), 0.0f);
        for (int b = 0; b < nb; ++b)
        {
            double s = 0;
            for (int i = 0; i < z.envHop; ++i)
                for (int c = 0; c < z.ch; ++c) { const double x = z.at (b * z.envHop + i, c); s += x * x; }
            z.env[(size_t) b] = (float) std::sqrt (s / (z.envHop * z.ch)) + 1.0e-6f;
        }
        // lissage (300 ms) : on suit la décroissance, pas les battements
        for (int pass = 0; pass < 2; ++pass)
        {
            std::vector<float> e = z.env;
            for (int b = 0; b < nb; ++b)
            {
                double s = 0; int k = 0;
                for (int j = std::max (0, b - 15); j < std::min (nb, b + 16); ++j) { s += e[(size_t) j]; ++k; }
                z.env[(size_t) b] = (float) (s / std::max (1, k));
            }
        }
        if (notes)
        {   // CRY mode notes : boucle cuite à la fabrication (note seule, accord tenu) ; la corde électrique s'éteint seule
            if (z.relPos > 0 && z.relPos < z.frames - sr / 2) { z.loopStart = z.relPos; z.loopEnd = z.frames - sr / 20; }
            else { z.loopStart = z.loopEnd = -1; }
            z.xfade = 0;
            return;
        }
        if (z.kind != 0) return;
        if (pad)
        {   // prise par tonalité : boucle sur les 70 % finaux de la prise (le fondu enchaîné est déjà dans le fichier)
            z.loopStart = z.relPos > 0 && z.relPos < z.frames - sr ? z.relPos : (int) (0.3 * z.frames);
            z.loopEnd = z.frames - sr / 20;
            z.xfade = 0;
            return;
        }
        // boucle : la dernière seconde avant le fondu final, longueur = nombre entier de périodes
        const double f0 = 440.0 * std::pow (2.0, (z.root - 69) / 12.0);
        const int period = (int) std::lround (sr / f0);
        const int end = z.relPos > 0 && z.relPos < z.frames ? z.relPos - (int) (0.02 * sr) : z.frames - (int) (0.16 * sr);
        int len = std::max (period, (int) std::lround (0.9 * sr / period) * period);
        if (end - len <= (int) (0.6 * sr))
        {   // note courte (phrases de slide) : boucle plus courte, pour que la note tenue ne s'éteigne pas
            len = std::max (period, (int) std::lround (0.28 * sr / period) * period);
            if (end - len <= (int) (0.20 * sr)) len = std::max (period, (int) std::lround (0.15 * sr / period) * period);
            if (end - len <= (int) (0.10 * sr)) return;
        }
        z.loopEnd = end;
        z.loopStart = end - len;
        z.xfade = std::min (len / 3, (int) (0.06 * sr));
    }
};
} // namespace dguitar
