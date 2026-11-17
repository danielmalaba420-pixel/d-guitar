// Banc d'essai hors ligne : joue des phrases MIDI dans le moteur et écrit des WAV (48 kHz, 16 bits stéréo)
#include "../Source/Engine/GuitarEngine.h"
#include <cstdio>
#include <string>
#include <xmmintrin.h>
#include <pmmintrin.h>
using namespace dguitar;

struct Ev { double t; int type; int a; float b; };   // type 0 noteOn(a,b) 1 noteOff(a) 2 pitch(b) 3 mod(b) 4 sustain(a)

static void writeWav (const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (path, "wb"); const uint32_t n = (uint32_t) L.size(), bytes = n * 4;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); }; auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + bytes); std::fwrite ("WAVEfmt ", 1, 8, f); w32 (16); w16 (1); w16 (2); w32 (sr); w32 (sr * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (bytes);
    for (uint32_t i = 0; i < n; ++i)
    {
        const int16_t a = (int16_t) std::lround (std::max (-1.0f, std::min (1.0f, L[i])) * 32767.0f);
        const int16_t b = (int16_t) std::lround (std::max (-1.0f, std::min (1.0f, R[i])) * 32767.0f);
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
}

int main (int argc, char** argv)
{
    _mm_setcsr (_mm_getcsr() | 0x8040);
    const int sr = std::getenv ("SR") ? std::atoi (std::getenv ("SR")) : 48000;
    // usage : render <guitare 0-5> <preset 0-2> <scénario> <sortie.wav> [durée]
    const int gi = std::atoi (argv[1]), pi = std::atoi (argv[2]);
    const std::string sc = argv[3];
    const char* out = argv[4];
    double dur = argc > 5 ? std::atof (argv[5]) : 8.0;

    Engine e; e.prepare (sr);
    Settings s; s.guitar = gi;
    const auto& v = voicing (gi);
    s.drive = v.preset[pi][0] / 100; s.tone = v.preset[pi][1] / 100; s.wah = v.preset[pi][2] / 100;
    s.delay = v.preset[pi][3] / 100; s.chorus = v.preset[pi][4] / 100; s.reverb = v.preset[pi][5] / 100; s.volume = v.preset[pi][6] / 100;
    if (argc > 6) { s.drive = std::atof (argv[6]); }
    if (argc > 7) { s.wah = std::atof (argv[7]); }
    e.setSettings (s);
    if (std::getenv ("TAP")) e.debugTap = std::atoi (std::getenv ("TAP"));

    std::vector<Ev> ev;
    auto on = [&] (double t, int n, float vel) { ev.push_back ({ t, 0, n, vel }); };
    auto off = [&] (double t, int n) { ev.push_back ({ t, 1, n, 0 }); };
    auto pit = [&] (double t, float x) { ev.push_back ({ t, 2, 0, x }); };
    auto mod = [&] (double t, float x) { ev.push_back ({ t, 3, 0, x }); };

    if (sc[0] == 'N') { const int nn = std::atoi (sc.c_str() + 1); on (0.1, nn, 0.8f); off (5.5, nn); }
    else if (sc == "note")      { on (0.1, 57, 0.8f); off (3.5, 57); }                      // La 3 = 220 Hz
    else if (sc == "notes") { const int n[] = { 40, 45, 52, 57, 64, 69, 76 }; for (int i = 0; i < 7; ++i) { on (0.1 + i * 1.2, n[i], 0.8f); off (0.1 + i * 1.2 + 1.1, n[i]); } }
    else if (sc == "chord") { on (0.1, 52, 0.8f); on (0.1, 59, 0.8f); on (0.1, 64, 0.8f); on (0.1, 68, 0.8f); on (0.1, 71, 0.8f); off (3.0, 52); off (3.0, 59); off (3.0, 64); off (3.0, 68); off (3.0, 71); }
    else if (sc == "demo")
    {
        switch (gi)
        {
            case 0: // CLEAN : arpèges puis accords
            { const int n[] = { 52, 59, 64, 68, 71, 68, 64, 59 };
              for (int i = 0; i < 16; ++i) { const int nn = n[i % 8] + (i >= 8 ? 5 : 0); on (0.2 + i * 0.22, nn, 0.65f + 0.1f * (i % 3 == 0)); off (0.2 + i * 0.22 + 0.5, nn); }
              const int c1[] = { 45, 52, 57, 61, 64 }, c2[] = { 50, 57, 62, 66, 69 };
              for (int k : c1) { on (4.0, k, 0.7f); off (6.2, k); }
              for (int k : c2) { on (6.4, k, 0.7f); off (8.6, k); }
              break; }
            case 1: // DIVE : riff d'accords de puissance, solo avec vibrato, plongeon au levier
            { for (int i = 0; i < 8; ++i) { const double t = 0.2 + i * 0.3; const int r = (i % 4 == 3) ? 43 : 40; on (t, r, 0.9f); on (t, r + 7, 0.9f); on (t, r + 12, 0.9f); off (t + 0.2, r); off (t + 0.2, r + 7); off (t + 0.2, r + 12); }
              on (3.0, 64, 0.9f); mod (3.5, 0.0f); for (int i = 0; i <= 20; ++i) mod (3.4 + i * 0.04, i / 20.0f * 0.8f); off (5.2, 64);
              on (5.3, 67, 0.9f); on (5.3, 72, 0.9f); on (5.3, 76, 0.9f);
              for (int i = 0; i <= 40; ++i) pit (6.0 + i * 0.03, -std::pow (i / 40.0f, 0.8f) * 0.9f);
              off (8.0, 67); off (8.0, 72); off (8.0, 76); pit (8.2, 0.0f); break; }
            case 2: // CRY : phrase chantée, bends et vibrato
            { on (0.2, 69, 0.8f); for (int i = 0; i <= 20; ++i) pit (0.8 + i * 0.03, i / 20.0f * 1.0f); mod (1.6, 0.0f); for (int i = 0; i < 20; ++i) mod (1.6 + i * 0.05, std::min (1.0f, i / 6.0f)); off (3.0, 69); pit (3.0, 0.0f);
              on (3.1, 72, 0.9f); for (int i = 0; i < 40; ++i) mod (3.4 + i * 0.06, 0.5f + 0.5f * std::sin (i * 0.4f)); on (4.2, 74, 0.8f); off (4.2, 72); off (6.0, 74);
              on (6.2, 67, 0.8f); on (6.9, 69, 0.8f); on (7.4, 72, 0.9f); mod (7.4, 0.7f); off (9.0, 72); break; }
        }
    }
    if (sc == "wahsweep")
    {
        for (int i = 0; i < 8; ++i) { on (0.0, 40 + 12 * (i % 2) + (i / 2) * 7, 0.9f); }
        for (int i = 0; i <= 400; ++i) mod (0.5 + i * 0.01, i / 400.0f);
    }
    if (sc == "fuzz")
    {
        uint32_t r = 12345u; auto rnd = [&] () { r = r * 1664525u + 1013904223u; return (r >> 8) / 16777216.0; };
        for (double t = 0.05; t < dur - 0.5; t += 0.02 + rnd() * 0.12)
        {
            const double k = rnd();
            if (k < 0.55) on (t, 28 + (int) (rnd() * 72), 0.1f + (float) rnd() * 0.9f);
            else if (k < 0.8) off (t, 28 + (int) (rnd() * 72));
            else if (k < 0.9) pit (t, (float) (rnd() * 2 - 1));
            else mod (t, (float) rnd());
        }
    }
    std::sort (ev.begin(), ev.end(), [] (const Ev& a, const Ev& b) { return a.t < b.t; });

    const int total = (int) (dur * sr);
    std::vector<float> L ((size_t) total), R ((size_t) total);
    size_t ei = 0; int pos = 0; const int blk = 128;
    while (pos < total)
    {
        const int n = std::min (blk, total - pos);
        while (ei < ev.size() && ev[ei].t * sr < pos + n)
        {
            const auto& x = ev[ei++];
            if (x.type == 0) e.noteOn (x.a, x.b); else if (x.type == 1) e.noteOff (x.a); else if (x.type == 2) e.pitchWheel (x.b); else if (x.type == 3) e.modWheel (x.b);
        }
        e.process (L.data() + pos, R.data() + pos, n);
        pos += n;
    }
    float pk = 0; bool bad = false;
    for (int i = 0; i < total; ++i) { pk = std::max (pk, std::max (std::fabs (L[i]), std::fabs (R[i]))); if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) bad = true; }
    std::printf ("%s peak=%.3f finite=%d\n", out, pk, (int) ! bad);
    writeWav (out, L, R, sr);
    return 0;
}
