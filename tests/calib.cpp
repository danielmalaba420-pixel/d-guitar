// Mesure le niveau de sortie de chaque guitare à chaque réglage de DRIVE et propose la table de compensation.
#include "../Source/Engine/GuitarEngine.h"
#include <cstdio>
#include <string>
#if defined(__SSE__) || defined(__x86_64__)
#include <xmmintrin.h>
#define DG_FTZ() _mm_setcsr (_mm_getcsr() | 0x8040)
#else
#define DG_FTZ() ((void) 0)
#endif
using namespace dguitar;
int main()
{
    DG_FTZ();
    const int sr = 48000;
    double target = -22.0;   // dBFS RMS visé
    for (int gi = 0; gi < kNumGuitars; ++gi)
    {
        target = voicing (gi).acoustic ? -27.0 : -22.0; std::printf ("{ // %s\n  ", voicing (gi).name);
        for (int k = 0; k <= 8; ++k)
        {
            Engine e; e.prepare (sr);
            Settings s; s.guitar = gi; s.drive = k / 8.0f; s.tone = 0.5f; s.wah = 0; s.delay = 0; s.chorus = 0; s.reverb = 0; s.volume = 0.75f;
            e.setSettings (s);
            static SampleSet sets[10];
            if (const char* dir = std::getenv ("SAMPLES"))
            {
                const char* nm = gi == 1 || gi == 2 ? "electric-di" : (gi == 3 ? "steel" : (gi == 4 ? "steel1" : (gi == 5 ? "slide-angelitus" : nullptr)));
                if (nm != nullptr && (sets[gi].name == nm || (sets[gi].load (std::string (dir) + "/" + nm + ".dgs") && ! (sets[gi].name = nm).empty()))) e.setSamples (&sets[gi]);
            }
            const int per = sr; const int total = per * 6; double acc = 0; long cnt = 0;
            std::vector<float> L (128), R (128);
            const int notesStd[] = { 45, 52, 57, 64, 52, 59 }, notesCry[] = { 64, 69, 72, 76, 69, 67 };   // CRY : dans sa tessiture (La3..Do6)
            const int* notes = gi == 5 ? notesCry : notesStd;
            for (int pos = 0; pos < total; pos += 128)
            {
                const int step = pos / per;
                if (pos % per < 128) { e.noteOn (notes[step % 6], 0.8f);  }
                if (pos % per >= per - 128) { e.allNotesOff(); }
                e.process (L.data(), R.data(), 128);
                if (pos >= per / 3) for (int i = 0; i < 128; ++i) { acc += (double) L[i] * L[i]; ++cnt; }
            }
            const double rms = std::sqrt (acc / (double) cnt);
            const double db = 20.0 * std::log10 (rms + 1e-9);
            std::printf ("%.1f%s", target - db, k < 8 ? ", " : "");
            std::fflush (stdout);
        }
        std::printf ("\n},\n");
    }
}
