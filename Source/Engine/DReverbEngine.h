// D-REVERB — moteur de reverb (sans dépendance JUCE)
// Studio Danova — Daniel Malaba
//
// Architecture :
//   entrée -> pré-delay -> réflexions précoces (prises multiples + 2 passe-tout de lissage)
//                       -> 6 diffuseurs passe-tout en série (par canal) : densité immédiate
//                       -> réseau de 16 délais bouclés (FDN, matrice de Hadamard),
//                          chaque ligne : délai à modulation aléatoire lissée (Hermite)
//                          + amortissement 3 bandes + 2 passe-tout de densité
//   sortie : gauche = lignes 0-7, droite = lignes 8-15 (stéréo décorrélée, compatible mono)
//         -> coupe-bas / coupe-haut -> largeur M/S -> ducking -> mix
//   option : shimmer (octave au-dessus réinjectée dans la boucle)
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace dreverb
{
static constexpr float kPi = 3.14159265358979f;
static constexpr int kLines = 64;
static constexpr int kTaps = 12;

//==============================================================================
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void init (int maxLen)
    {
        int n = 16;
        while (n < maxLen + 8) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    // lire AVANT push : d = retard en échantillons (d >= 1)
    inline float readInt (int d) const { return buf[(size_t) ((w - d) & mask)]; }
    inline float readLinear (float d) const // interpolation linéaire, d >= 1
    {
        const int di = (int) d;
        const float f = d - (float) di;
        const float y1 = buf[(size_t) ((w - di) & mask)];
        const float y2 = buf[(size_t) ((w - di - 1) & mask)];
        return y1 + f * (y2 - y1);
    }
    inline float read (float d) const // interpolation Hermite, d >= 2
    {
        const int di = (int) d;
        const float f = d - (float) di;
        const float y0 = buf[(size_t) ((w - di + 1) & mask)];
        const float y1 = buf[(size_t) ((w - di) & mask)];
        const float y2 = buf[(size_t) ((w - di - 1) & mask)];
        const float y3 = buf[(size_t) ((w - di - 2) & mask)];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
};

struct Allpass
{
    DelayLine d;
    float len = 100.0f, g = 0.5f;
    inline float process (float x)   // longueur fixe : lecture entière (plus rapide)
    {
        const float dl = d.readInt ((int) len);
        const float v = x - g * dl;
        d.push (v);
        return dl + g * v;
    }
    // bloc de n échantillons (n < longueur) : toutes les lectures tombent sur des valeurs déjà écrites
    inline void processBlock (float* x, int n)
    {
        float* buf = d.buf.data();
        const int mask = d.mask, w0 = d.w, ln = (int) len;
        for (int s = 0; s < n; ++s)
        {
            const float dl = buf[(w0 + s - ln) & mask];
            const float v = x[s] - g * dl;
            buf[(w0 + s) & mask] = v;
            x[s] = dl + g * v;
        }
        d.w = (w0 + n) & mask;
    }
    inline float processMod (float x, float mod)
    {
        const float dl = d.read (len + mod);
        const float v = x - g * dl;
        d.push (v);
        return dl + g * v;
    }
};

struct OnePole
{
    float a = 0.0f, z = 0.0f;
    void setCutoff (float hz, float sr) { a = 1.0f - std::exp (-2.0f * kPi * std::min (hz, sr * 0.45f) / sr); }
    inline float lp (float x) { z += a * (x - z); return z; }
    void reset() { z = 0.0f; }
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0.0f; }
    void set (bool highpass, float hz, float sr, float q = 0.7071f)
    {
        hz = std::max (10.0f, std::min (hz, sr * 0.45f));
        const float w0 = 2.0f * kPi * hz / sr, c = std::cos (w0), s = std::sin (w0);
        const float al = s / (2.0f * q), a0 = 1.0f + al;
        if (highpass) { b0 = (1 + c) * 0.5f / a0; b1 = -(1 + c) / a0; b2 = b0; }
        else          { b0 = (1 - c) * 0.5f / a0; b1 = (1 - c) / a0;  b2 = b0; }
        a1 = -2.0f * c / a0;
        a2 = (1.0f - al) / a0;
    }
    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

// sinus rapide, phase dans [0,1)
static inline float fastSin (float ph)
{
    float x = ph * 2.0f - 1.0f;              // [-1,1)
    float y = 4.0f * x * (1.0f - std::fabs (x)); // parabole
    return -(0.225f * (y * std::fabs (y) - y) + y);
}

//==============================================================================
// Transposition d'une octave (shimmer) : deux têtes de lecture croisées
struct OctaveUp
{
    DelayLine d;
    float win = 2880.0f, ph = 0.0f;
    void init (float sr) { win = 0.060f * sr; d.init ((int) win + 64); ph = 0.0f; }
    inline float process (float x)
    {
        d.push (x);
        float out = 0.0f;
        for (int h = 0; h < 2; ++h)
        {
            float p = ph + 0.5f * (float) h;
            if (p >= 1.0f) p -= 1.0f;
            const float delay = (1.0f - p) * win + 3.0f;
            const float wgt = std::sin (kPi * p);
            out += d.read (delay) * wgt * wgt;
        }
        ph += 1.0f / win; // rapport 2 : le retard diminue d'un échantillon par échantillon
        if (ph >= 1.0f) ph -= 1.0f;
        return out;
    }
};

//==============================================================================
struct Params
{
    int   algorithm   = 0;      // 0 Plate, 1 Room, 2 Hall, 3 Cathedral
    float preDelayMs  = 20.0f;
    float size        = 1.0f;   // 0.4 .. 1.8
    float decaySec    = 2.0f;   // RT60 dans le médium
    float hfRatio     = 0.45f;  // RT60 aigu / RT60 médium
    float hfFreq      = 4500.0f;
    float lfRatio     = 1.1f;   // RT60 grave / RT60 médium
    float lfFreq      = 250.0f;
    float diffusion   = 1.0f;   // 0..1
    float modDepth    = 0.5f;   // 0..1
    float modRate     = 0.5f;   // Hz de base
    float erLevel     = 0.5f;
    float lateLevel   = 1.0f;
    float lowCutHz    = 120.0f;
    float highCutHz   = 12000.0f;
    float width       = 1.0f;   // 0 mono .. 1 normal .. 1.5 large
    float shimmer     = 0.0f;   // 0..1
    float duck        = 0.0f;   // 0..1
    float duckReleaseMs = 300.0f;
    float mix         = 0.3f;   // 0..1
};

// modulation aléatoire lissée (pas de vibrato régulier : pas de « chorus » audible sur les notes tenues)
struct SmoothRandom
{
    uint32_t seed = 1;
    float target = 0.0f, a = 0.0f, b = 0.0f, coef = 0.001f;
    int count = 0, period = 1000;
    void init (uint32_t s, float rateHz, float sr)
    {
        seed = s * 2654435761u + 12345u;
        setRate (rateHz, sr);
        target = next(); a = b = target;
        count = (int) (s * 7919u) % period;
    }
    void setRate (float rateHz, float sr)
    {
        rateHz = std::max (0.05f, rateHz);
        period = std::max (16, (int) (sr / rateHz));
        coef = 1.0f - std::exp (-2.0f * kPi * rateHz * 0.9f / sr);
    }
    inline float next() { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) * (2.0f / 16777216.0f) - 1.0f; }
    inline float tick()
    {
        if (--count <= 0) { count = period; target = next(); }
        a += coef * (target - a);
        b += coef * (a - b);
        return b;
    }
};

struct AlgoDef { float minMs, maxMs, apMinMs, apMaxMs, erSpreadMs, gain; };
static inline AlgoDef algoDef (int a)
{
    switch (a)
    {
        case 0:  return { 15.0f, 55.0f, 8.3f, 27.1f, 28.0f, 1.00f }; // Plate : dense tout de suite
        case 1:  return { 13.0f, 47.0f, 7.1f, 23.9f, 40.0f, 1.00f }; // Room
        case 2:  return { 23.0f, 79.0f, 3.7f, 13.9f, 70.0f, 1.00f }; // Hall
        default: return { 31.0f, 113.0f, 4.3f, 17.3f, 95.0f, 1.00f }; // Cathedral
    }
}

static inline bool isPrime (int n)
{
    if (n < 2) return false;
    for (int i = 2; i * i <= n; ++i) if (n % i == 0) return false;
    return true;
}
static inline int nextPrime (int n) { while (! isPrime (n)) ++n; return n; }

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        const int maxLine = (int) (0.113f * 1.9f * sr) + 512;
        for (int i = 0; i < kLines; ++i)
        {
            lines[i].init (maxLine);
            loopAp[i].d.init ((int) (0.0173f * 1.9f * sr) + 256);
            loopAp2[i].d.init ((int) (0.006f * 1.9f * sr) + 256);
        }
        static const float dl[kDiff] = { 1.31f, 1.93f, 2.87f, 4.11f, 5.83f, 8.17f };
        static const float dr[kDiff] = { 1.43f, 2.11f, 3.07f, 4.39f, 6.29f, 7.73f };
        for (int k = 0; k < kDiff; ++k)
        {
            diffL[k].d.init ((int) (0.012f * sr) + 64); diffL[k].len = dl[k] * 0.001f * sr;
            diffR[k].d.init ((int) (0.012f * sr) + 64); diffR[k].len = dr[k] * 0.001f * sr;
        }
        static const float el[2] = { 2.33f, 4.73f }, er[2] = { 2.71f, 4.31f };
        for (int k = 0; k < 2; ++k)
        {
            erApL[k].d.init ((int) (0.006f * sr) + 64); erApL[k].len = el[k] * 0.001f * sr; erApL[k].g = 0.6f;
            erApR[k].d.init ((int) (0.006f * sr) + 64); erApR[k].len = er[k] * 0.001f * sr; erApR[k].g = 0.6f;
        }
        preL.init ((int) (0.26f * sr)); preR.init ((int) (0.26f * sr));
        erL.init ((int) (0.2f * sr));   erR.init ((int) (0.2f * sr));
        shimL.init (sr); shimR.init (sr);
        shimLpL.setCutoff (6000.0f, sr); shimLpR.setCutoff (6000.0f, sr);
        erLpL.setCutoff (7000.0f, sr);   erLpR.setCutoff (7000.0f, sr);
        for (int i = 0; i < kLines; ++i) mod[i].init ((uint32_t) (i + 1), 0.5f, sr / (float) kModHop);
        duckGain = 1.0f; env = 0.0f;
        algoLoaded = -1;
        setParams (p, true);
        reset();
    }

    void reset()
    {
        for (int i = 0; i < kLines; ++i) { lines[i].clear(); loopAp[i].d.clear(); loopAp2[i].d.clear(); dLo[i] = dHi[i] = 0.0f; }
        for (int k = 0; k < kDiff; ++k) { diffL[k].d.clear(); diffR[k].d.clear(); }
        for (int k = 0; k < 2; ++k) { erApL[k].d.clear(); erApR[k].d.clear(); }
        preL.clear(); preR.clear(); erL.clear(); erR.clear();
        shimL.d.clear(); shimR.d.clear(); shimLpL.reset(); shimLpR.reset(); erLpL.reset(); erLpR.reset();
        hpL.reset(); hpR.reset(); lpL.reset(); lpR.reset();
        fbL = fbR = 0.0f; env = 0.0f; duckGain = 1.0f; duckTarget = 1.0f;
        for (int s = 0; s < kBlock; ++s) fbBufL[s] = fbBufR[s] = 0.0f;
        for (int i = 0; i < kLines; ++i) { curLen[i] = tgtLen[i]; delayNow[i] = tgtLen[i] + modSamples + 3.0f; delayStep[i] = 0.0f; apY[i] = 0.0f; }
        modCount = 0;
        curPre = tgtPre;
    }

    void setParams (const Params& np, bool force = false)
    {
        p = np;
        if (p.algorithm != algoLoaded || force)
        {
            algoLoaded = p.algorithm;
            def = algoDef (p.algorithm);
            for (int i = 0; i < kLines; ++i)
            {
                // longueurs entrelacées : chaque moitié (gauche / droite) couvre toute la plage
                const int j = (i < kLines / 2) ? (2 * i) : (2 * (i - kLines / 2) + 1);
                const float t = (float) j / (float) (kLines - 1);
                const float ms = def.minMs * std::pow (def.maxMs / def.minMs, t);
                baseMs[i] = (float) nextPrime ((int) std::lround (ms * 48.0f)) / 48.0f;
                const int k = (i * 13) % kLines;
                const float ta = (float) k / (float) (kLines - 1);
                apMs[i] = (float) nextPrime ((int) std::lround ((def.apMinMs + (def.apMaxMs - def.apMinMs) * ta) * 48.0f)) / 48.0f;
                ap2Ms[i] = (float) nextPrime ((int) std::lround ((0.9f + 2.6f * (float) ((i * 19) % kLines) / (float) (kLines - 1)) * 48.0f)) / 48.0f;
            }
            for (int i = 0; i < kLines / 2; ++i)
                outSign[i] = (((i * 0x9E37u) >> 3) & 1u) ? -1.0f : 1.0f;
            static const float tapBase[kTaps] = { 0.043f, 0.107f, 0.151f, 0.239f, 0.311f, 0.397f, 0.493f, 0.587f, 0.683f, 0.781f, 0.887f, 1.0f };
            for (int k = 0; k < kTaps; ++k)
            {
                tapMsL[k] = tapBase[k] * def.erSpreadMs;
                tapMsR[k] = (tapBase[k] * 0.93f + 0.031f) * def.erSpreadMs;
                const float g = std::pow (0.80f, (float) k);
                tapGL[k] = g * ((k % 3 == 1) ? -1.0f : 1.0f);
                tapGR[k] = g * ((k % 3 == 2) ? -1.0f : 1.0f);
            }
        }

        const float size = std::max (0.3f, std::min (p.size, 1.9f));
        const float rt   = std::max (0.1f, p.decaySec);
        const float rtHi = std::max (0.05f, rt * std::max (0.05f, std::min (p.hfRatio, 1.0f)));
        const float rtLo = std::max (0.05f, rt * std::max (0.3f, std::min (p.lfRatio, 2.0f)));
        const float diff = std::max (0.0f, std::min (p.diffusion, 1.0f));

        for (int i = 0; i < kLines; ++i)
        {
            tgtLen[i] = baseMs[i] * size * 0.001f * sr;
            loopAp[i].len = apMs[i] * (0.6f + 0.4f * size) * 0.001f * sr;
            loopAp[i].g = 0.62f * diff;
            loopAp2[i].len = ap2Ms[i] * 0.001f * sr;
            loopAp2[i].g = 0.5f * diff;
            const float tSec = (tgtLen[i] + loopAp[i].len + loopAp2[i].len) / sr;
            gMid[i] = std::pow (10.0f, -3.0f * tSec / rt);
            gHi[i]  = std::pow (10.0f, -3.0f * tSec / rtHi);
            gLo[i]  = std::pow (10.0f, -3.0f * tSec / rtLo);
            gHiD[i] = gHi[i] - gMid[i];
            gLoD[i] = gLo[i] - gMid[i];
            mod[i].setRate (p.modRate * (0.6f + 1.2f * (float) ((i * 5) % kLines) / (float) kLines), sr / (float) kModHop);  // 0,6x .. 1,8x
        }
        for (int k = 0; k < kDiff; ++k)
        {
            const float g = (k < 2 ? 0.72f : (k < 4 ? 0.66f : 0.6f)) * diff;
            diffL[k].g = g; diffR[k].g = g;
        }
        cHi = 1.0f - std::exp (-2.0f * kPi * std::min (p.hfFreq, sr * 0.45f) / sr);
        cLo = 1.0f - std::exp (-2.0f * kPi * std::min (p.lfFreq, sr * 0.45f) / sr);
        modSamples = p.modDepth * 0.00025f * sr * (0.5f + 0.5f * size);
        tgtPre = std::max (1.0f, p.preDelayMs * 0.001f * sr);
        hpL.set (true, p.lowCutHz, sr);  hpR.set (true, p.lowCutHz, sr);
        lpL.set (false, p.highCutHz, sr); lpR.set (false, p.highCutHz, sr);

        shimAmt = std::max (0.0f, std::min (p.shimmer, 1.0f)) * 0.42f * std::min (1.0f, 2.5f / rt);

        attCoef = 1.0f - std::exp (-1.0f / (0.012f * sr));
        relCoef = 1.0f - std::exp (-1.0f / (std::max (30.0f, p.duckReleaseMs) * 0.001f * sr));
        envCoef = 1.0f - std::exp (-1.0f / (0.030f * sr));
        slew    = 1.0f - std::exp (-1.0f / (0.080f * sr));
        slewHop = 1.0f - std::exp (-(float) kModHop / (0.080f * sr));
        lateNorm = 0.50f / std::sqrt (std::max (0.4f, rt));
    }

    const Params& getParams() const { return p; }
    float getDuckGain() const { return duckGain; }

    void process (float* L, float* R, int n)
    {
        for (int pos = 0; pos < n; pos += kBlock)
            processBlock (L + pos, R + pos, std::min (kBlock, n - pos));
    }

private:
    // Traitement par blocs de kBlock échantillons : chaque ligne du réseau calcule tout le bloc
    // d'un coup (accès mémoire continus). Possible car la plus courte ligne dure bien plus que kBlock.
    // réflexions précoces : écrit le bloc, puis lit les 12 prises (toutes plus longues que le bloc)
    static void earlyBlock (DelayLine& d, const float* x, float* out, const float* tapMs, const float* tapG, float scale, int n)
    {
        float* buf = d.buf.data();
        const int mask = d.mask, w0 = d.w;
        for (int s = 0; s < n; ++s) { buf[(w0 + s) & mask] = x[s]; out[s] = 0.0f; }
        for (int k = 0; k < kTaps; ++k)
        {
            const int t = 1 + (int) (tapMs[k] * scale);
            const float g = tapG[k];
            for (int s = 0; s < n; ++s) out[s] += g * buf[(w0 + s + 1 - t) & mask];
        }
        d.w = (w0 + n) & mask;
    }

    void processBlock (float* L, float* R, const int n)
    {
        const float mix = std::max (0.0f, std::min (p.mix, 1.0f));
        const float wetG = std::sin (mix * kPi * 0.5f);
        const float dryG = std::cos (mix * kPi * 0.5f);
        const float duckDepth = std::max (0.0f, std::min (p.duck, 1.0f));
        const float erScale = 0.001f * sr * (0.5f + 0.5f * std::min (p.size, 1.6f));

        float inA[kBlock], inB[kBlock], erA[kBlock], erB[kBlock];

        // ---- 1) entrée : pré-delay, réflexions précoces, diffusion (+ retour shimmer du bloc précédent)
        for (int s = 0; s < n; ++s)
        {
            curPre += slew * (tgtPre - curPre);
            preL.push (L[s]); preR.push (R[s]);
            inA[s] = preL.read (curPre + 2.0f);
            inB[s] = preR.read (curPre + 2.0f);
        }
        earlyBlock (erL, inA, erA, tapMsL, tapGL, erScale, n);
        earlyBlock (erR, inB, erB, tapMsR, tapGR, erScale, n);
        for (int s = 0; s < n; ++s)
        {
            const float eL = erA[s], eR = erB[s];
            erA[s] = erLpL.lp (0.8f * eL + 0.25f * eR) * 0.42f;
            erB[s] = erLpR.lp (0.8f * eR + 0.25f * eL) * 0.42f;
        }
        for (int k = 0; k < 2; ++k) { erApL[k].processBlock (erA, n); erApR[k].processBlock (erB, n); }
        for (int s = 0; s < n; ++s) { inA[s] += fbBufL[s]; inB[s] += fbBufR[s]; }
        for (int k = 0; k < kDiff; ++k) { diffL[k].processBlock (inA, n); diffR[k].processBlock (inB, n); }

        // ---- 2) modulation : nouvelle cible de retard par ligne, rampe sur le bloc
        for (int i = 0; i < kLines; ++i)
        {
            curLen[i] += slewHop * (tgtLen[i] - curLen[i]);
            const float target = curLen[i] + modSamples * mod[i].tick() + modSamples + 3.0f;
            delayStep[i] = (target - delayNow[i]) * (1.0f / (float) kBlock);
        }

        // ---- 3) chaque ligne : lecture fractionnaire passe-tout, amortissement 3 bandes, 2 passe-tout
        for (int i = 0; i < kLines; ++i)
        {
            DelayLine& ln = lines[i];
            float dn = delayNow[i], y = apY[i], lo = dLo[i], hi = dHi[i];
            const float st = delayStep[i], gm = gMid[i], gh = gHiD[i], gl = gLoD[i];
            float* o = blk[i];
            const float dEnd = dn + st * (float) n;
            const int k0 = (int) (dn + st - 0.25f), k1 = (int) (dEnd - 0.25f);
            if (k0 == k1)
            {
                // cas courant : la partie entière du retard ne change pas sur le bloc,
                // le coefficient passe-tout est interpolé (pas de division par échantillon)
                const float f0 = dn + st - (float) k0, f1 = dEnd - (float) k0;
                const float e0 = (1.0f - f0) / (1.0f + f0), e1 = (1.0f - f1) / (1.0f + f1);
                const float de = (n > 1) ? (e1 - e0) / (float) (n - 1) : 0.0f;
                float eta = e0;
                const float* buf = ln.buf.data();
                const int mask = ln.mask, w0 = ln.w;
                for (int s = 0; s < n; ++s)
                {
                    // l'écriture du bloc se fait plus tard : position de lecture décalée de s
                    const float u0 = buf[(w0 - k0 + s) & mask];
                    const float u1 = buf[(w0 - k0 - 1 + s) & mask];
                    y = eta * (u0 - y) + u1;
                    eta += de;
                    lo += cLo * (y - lo);
                    hi += cHi * (y - hi);
                    o[s] = gm * y + gh * (y - hi) + gl * lo;
                }
                dn = dEnd;
            }
            else
            {
                for (int s = 0; s < n; ++s)
                {
                    dn += st;
                    const int k = (int) (dn - 0.25f);   // partie fractionnaire gardée dans [0,25 ; 1,25[
                    const float f = dn - (float) k;
                    const float eta = (1.0f - f) / (1.0f + f);
                    const float u0 = ln.readInt (k - s);
                    const float u1 = ln.readInt (k + 1 - s);
                    y = eta * (u0 - y) + u1;
                    lo += cLo * (y - lo);
                    hi += cHi * (y - hi);
                    o[s] = gm * y + gh * (y - hi) + gl * lo;
                }
            }
            delayNow[i] = dn; apY[i] = y; dLo[i] = lo; dHi[i] = hi;
            loopAp[i].processBlock (o, n);
            loopAp2[i].processBlock (o, n);
        }

        // ---- 4) sorties : moitié gauche / moitié droite, signes alternés
        float wL[kBlock] {}, wR[kBlock] {};
        for (int i = 0; i < kLines / 2; ++i)
        {
            const float gL = outSign[i], gR = outSign[kLines / 2 - 1 - i];
            const float* oa = blk[i];
            const float* ob = blk[i + kLines / 2];
            for (int s = 0; s < n; ++s) { wL[s] += gL * oa[s]; wR[s] += gR * ob[s]; }
        }

        // ---- 5) matrice de Hadamard (vectorisée sur le bloc), puis réinjection
        for (int h = 1; h < kLines; h <<= 1)
            for (int i = 0; i < kLines; i += (h << 1))
                for (int j = i; j < i + h; ++j)
                {
                    float* x = blk[j];
                    float* z = blk[j + h];
                    for (int s = 0; s < kBlock; ++s) { const float a = x[s], c = z[s]; x[s] = a + c; z[s] = a - c; }
                }
        for (int i = 0; i < kLines; ++i)
        {
            const float* x = blk[i];
            const float* in = (i & 1) ? inB : inA;
            const float sg = ((i & 2) ? -1.0f : 1.0f) * kInNorm;
            DelayLine& ln = lines[i];
            float* buf = ln.buf.data();
            const int mask = ln.mask, w0 = ln.w;
            for (int s = 0; s < n; ++s) buf[(w0 + s) & mask] = x[s] * kHadNorm + in[s] * sg + 1.0e-18f;
            ln.w = (w0 + n) & mask;
        }

        // ---- 6) sortie : shimmer, filtres, largeur, ducking, mélange
        // ducking : cible recalculée une fois par bloc (log / pow coûteux), suivie échantillon par échantillon
        for (int s = 0; s < n; ++s)
        {
            const float lateL = wL[s] * kOutNorm * lateNorm, lateR = wR[s] * kOutNorm * lateNorm;
            if (shimAmt > 0.0005f)
            {
                fbBufL[s] = std::tanh (shimLpL.lp (shimL.process (lateL)) * shimAmt * 2.0f) * 0.5f;
                fbBufR[s] = std::tanh (shimLpR.lp (shimR.process (lateR)) * shimAmt * 2.0f) * 0.5f;
            }
            else { fbBufL[s] = fbBufR[s] = 0.0f; }

            float outL = lateL * p.lateLevel + erA[s] * p.erLevel;
            float outR = lateR * p.lateLevel + erB[s] * p.erLevel;
            outL = lpL.process (hpL.process (outL));
            outR = lpR.process (hpR.process (outR));
            const float m = 0.5f * (outL + outR), sd = 0.5f * (outL - outR) * p.width;
            outL = m + sd; outR = m - sd;

            const float inL = L[s], inR = R[s];
            env += envCoef * (std::max (std::fabs (inL), std::fabs (inR)) - env);
            duckGain += (duckTarget < duckGain ? attCoef : relCoef) * (duckTarget - duckGain);
            L[s] = dryG * inL + wetG * duckGain * outL;
            R[s] = dryG * inR + wetG * duckGain * outR;
        }
        duckTarget = 1.0f;
        if (duckDepth > 0.001f)
        {
            const float db = 20.0f * std::log10 (env + 1.0e-9f);
            const float amt = std::max (0.0f, std::min ((db + 46.0f) / 30.0f, 1.0f));
            duckTarget = std::pow (10.0f, -(duckDepth * 20.0f * amt) / 20.0f);
        }
    }


    static constexpr int kDiff = 6;
    static constexpr float kHadNorm = 0.125f;       // 1 / sqrt(kLines)
    static constexpr float kInNorm  = 1.0f;         // énergie par ligne constante (niveau identique quelle que soit la taille du réseau)
    static constexpr float kOutNorm = 0.25f;        // calibré : même niveau de reverb que la version 1.0
    float sr = 48000.0f;
    Params p;
    AlgoDef def {};
    int algoLoaded = -1;

    DelayLine lines[kLines];
    Allpass loopAp[kLines], loopAp2[kLines];
    float dLo[kLines] {}, dHi[kLines] {}, gHiD[kLines] {}, gLoD[kLines] {}, cLo = 0.03f, cHi = 0.4f;
    float delayNow[kLines] {}, delayStep[kLines] {}, apY[kLines] {};
    int modCount = 0;
    float slewHop = 0.02f;
    static constexpr int kModHop = 16;
    static constexpr int kBlock = kModHop;
    float blk[kLines][kBlock] {};
    float fbBufL[kBlock] {}, fbBufR[kBlock] {};
    float duckTarget = 1.0f;
    SmoothRandom mod[kLines];
    float baseMs[kLines] {}, apMs[kLines] {}, ap2Ms[kLines] {}, tgtLen[kLines] {}, curLen[kLines] {};
    float gMid[kLines] {}, gHi[kLines] {}, gLo[kLines] {};
    float modSamples = 0.0f;
    float outSign[kLines / 2] {};

    Allpass diffL[kDiff], diffR[kDiff], erApL[2], erApR[2];
    DelayLine preL, preR, erL, erR;
    float tapMsL[kTaps] {}, tapMsR[kTaps] {}, tapGL[kTaps] {}, tapGR[kTaps] {};
    OnePole erLpL, erLpR, shimLpL, shimLpR;
    OctaveUp shimL, shimR;
    Biquad hpL, hpR, lpL, lpR;

    float tgtPre = 960.0f, curPre = 960.0f, slew = 0.001f;
    float fbL = 0.0f, fbR = 0.0f, shimAmt = 0.0f, lateNorm = 0.3f;
    float env = 0.0f, duckGain = 1.0f, attCoef = 0.01f, relCoef = 0.001f, envCoef = 0.01f;
};

} // namespace dreverb
