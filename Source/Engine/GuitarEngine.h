// D-GUITAR — moteur de guitare électrique 100 % modélisé (aucun échantillon, aucune dépendance JUCE)
// Studio Danova — Daniel Malaba
//
//   MIDI -> corde (guide d'onde numérique à deux lignes : sillet <-> chevalet)
//        -> coup de médiator (impulsion filtrée, position de pincement)
//        -> micro(s) magnétique(s) (vitesse de la corde à une position précise + résonance électrique)
//        -> compresseur -> wah (filtre à état, pédale = molette MOD) -> ampli (2 étages à lampes, suréchantillonné x4)
//        -> tonalité -> baffle (filtres) -> trémolo -> chorus stéréo -> delay ping-pong « bande » -> reverb
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <vector>

#include "Sources.h"
#include "Voicings.h"

namespace dguitar
{
static constexpr float kPi = 3.14159265358979f;
static constexpr int kMaxBlock = 512;
static constexpr float kAmpOut = 0.5f;     // niveau de sortie de l'ampli avant le volume
static constexpr float kInputGain = 8.0f;
static constexpr float kTriangle = 1.0f;     // amplitude du pincement triangulaire
static constexpr float kPulseMix = 1.1f;    // part du « claquement » de médiator   // niveau « guitare » à l'entrée de l'ampli

static inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float db2lin (float db) { return std::pow (10.0f, db * 0.05f); }
static inline float lerpf (float a, float b, float t) { return a + (b - a) * t; }

struct Rng
{
    uint32_t s = 22222u;
    inline float next() { s = s * 1664525u + 1013904223u; return (float) (s >> 8) * (1.0f / 8388608.0f) - 1.0f; }
};

//==============================================================================
// ligne à retard circulaire, lecture fractionnaire (Lagrange 3e ordre)
struct Line
{
    std::vector<float> b;
    int mask = 0, w = 0;
    void init (int n) { int sz = 1; while (sz < n) sz <<= 1; b.assign ((size_t) sz, 0.0f); mask = sz - 1; w = 0; }
    void clear() { std::fill (b.begin(), b.end(), 0.0f); w = 0; }
    inline void push (float x) { w = (w + 1) & mask; b[(size_t) w] = x; }
    inline float at (int k) const { return b[(size_t) ((w - k) & mask)]; }
    inline float tap (float d) const   // d >= 1
    {
        const int i = (int) d;
        const float t = d - (float) i;
        const float c0 = at (i - 1), c1 = at (i), c2 = at (i + 1), c3 = at (i + 2);
        const float w0 = -t * (t - 1.0f) * (t - 2.0f) * (1.0f / 6.0f);
        const float w1 = (t + 1.0f) * (t - 1.0f) * (t - 2.0f) * 0.5f;
        const float w2 = -(t + 1.0f) * t * (t - 2.0f) * 0.5f;
        const float w3 = (t + 1.0f) * t * (t - 1.0f) * (1.0f / 6.0f);
        return c0 * w0 + c1 * w1 + c2 * w2 + c3 * w3;
    }
    inline void addAt (float d, float v)
    {
        const int i = (int) d;
        const float f = d - (float) i;
        b[(size_t) ((w - i) & mask)] += v * (1.0f - f);
        b[(size_t) ((w - i - 1) & mask)] += v * f;
    }
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline float process (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
    void set (float nb0, float nb1, float nb2, float na0, float na1, float na2)
    { b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0; }
    void lowpass (float f, float q, float sr)
    {
        f = std::min (f, sr * 0.45f);
        const float w = 2 * kPi * f / sr, c = std::cos (w), a = std::sin (w) / (2 * q);
        set ((1 - c) * 0.5f, 1 - c, (1 - c) * 0.5f, 1 + a, -2 * c, 1 - a);
    }
    void highpass (float f, float q, float sr)
    {
        const float w = 2 * kPi * f / sr, c = std::cos (w), a = std::sin (w) / (2 * q);
        set ((1 + c) * 0.5f, -(1 + c), (1 + c) * 0.5f, 1 + a, -2 * c, 1 - a);
    }
    void peaking (float f, float q, float db, float sr)
    {
        f = std::min (f, sr * 0.45f);
        const float A = std::pow (10.0f, db / 40.0f), w = 2 * kPi * f / sr, c = std::cos (w), a = std::sin (w) / (2 * q);
        set (1 + a * A, -2 * c, 1 - a * A, 1 + a / A, -2 * c, 1 - a / A);
    }
    void lowShelf (float f, float db, float sr)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = 2 * kPi * f / sr, c = std::cos (w), s = std::sin (w);
        const float al = s / 2 * std::sqrt (2.0f), sq = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
             (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    void highShelf (float f, float db, float sr)
    {
        f = std::min (f, sr * 0.45f);
        const float A = std::pow (10.0f, db / 40.0f), w = 2 * kPi * f / sr, c = std::cos (w), s = std::sin (w);
        const float al = s / 2 * std::sqrt (2.0f), sq = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
             (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
};

// filtre à variables d'état (topologie TPT) : passe-bas, passe-bande
struct SVF
{
    float g = 0.1f, k = 1.0f, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    float lp = 0, bp = 0;
    void set (float fc, float q, float sr)
    {
        fc = clampf (fc, 20.0f, sr * 0.45f);
        g = std::tan (kPi * fc / sr); k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    inline void process (float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        bp = v1; lp = v2;
    }
    void reset() { ic1 = ic2 = lp = bp = 0; }
};

struct OnePole
{
    float a = 0.1f, z = 0;
    void setCutoff (float hz, float sr) { a = 1.0f - std::exp (-2.0f * kPi * std::min (hz, sr * 0.45f) / sr); }
    inline float lp (float x) { z += a * (x - z); return z; }
    inline float hp (float x) { z += a * (x - z); return x - z; }
    void reset() { z = 0; }
};

//==============================================================================
// demi-bande 31 points : suréchantillonnage x2 (un étage) ; deux étages = x4
struct HalfBand
{
    static constexpr int N = 31, H = 15;
    float h[N];
    HalfBand()
    {
        const float beta = 7.0f;
        auto bessel0 = [] (float x) { float s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2.0f * k)) * (x / (2.0f * k)); s += t; } return s; };
        float sum = 0;
        for (int n = 0; n < N; ++n)
        {
            const int m = n - H;
            const float r = (float) m / (float) H;
            const float win = bessel0 (beta * std::sqrt (std::max (0.0f, 1.0f - r * r))) / bessel0 (beta);
            float v = 0;
            if (m == 0) v = 0.5f;
            else if (m & 1) v = std::sin (kPi * (float) m * 0.5f) / (kPi * (float) m);
            h[n] = v * win;
            sum += h[n];
        }
        for (int n = 0; n < N; ++n) h[n] /= sum;   // gain continu = 1
    }
};
static inline const HalfBand& halfBand() { static const HalfBand hb; return hb; }

struct Up2
{
    float buf[64] = {};
    int w = 0;
    void reset() { std::memset (buf, 0, sizeof buf); w = 0; }
    inline float step (float in)
    {
        w = (w + 1) & 63; buf[w] = in;
        const auto& hb = halfBand();
        float y = 0;
        for (int k = 0; k < HalfBand::N; ++k) y += hb.h[k] * buf[(w - k) & 63];
        return y * 2.0f;
    }
    inline void process (float x, float& y0, float& y1)
    {
        y0 = step (x);
        y1 = step (0.0f);
    }
};
struct Down2
{
    float buf[64] = {};
    int w = 0;
    void reset() { std::memset (buf, 0, sizeof buf); w = 0; }
    inline void push (float in) { w = (w + 1) & 63; buf[w] = in; }
    inline float out() const
    {
        const auto& hb = halfBand();
        float y = 0;
        for (int k = 0; k < HalfBand::N; ++k) y += hb.h[k] * buf[(w - k) & 63];
        return y;
    }
};
// cascade x4 : up2 puis 2 x up2 ; descente en miroir
struct Oversampler4
{
    Up2 uA, uB;
    Down2 dA, dB;
    void reset() { uA.reset(); uB.reset(); dA.reset(); dB.reset(); }
    template <typename F>
    inline float process (float x, F&& fn)
    {
        float a0, a1, s0, s1, s2, s3;
        uA.process (x, a0, a1);
        uB.process (a0, s0, s1);
        uB.process (a1, s2, s3);
        s0 = fn (s0); s1 = fn (s1); s2 = fn (s2); s3 = fn (s3);
        dA.push (s0); dA.push (s1);
        const float m0 = dA.out();
        dA.push (s2); dA.push (s3);
        const float m1 = dA.out();
        dB.push (m0); dB.push (m1);
        return dB.out();
    }
};

//==============================================================================

// ---- baffle : réponse impulsionnelle à phase minimale calculée (haut-parleur 12", micro légèrement décalé)
struct CabFir
{
    static constexpr int kTaps = 512;
    std::vector<float> h, buf;
    int pos = 0;

    static void fft (std::vector<std::complex<double>>& a, bool inv)
    {
        const size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; ++i)
        {
            size_t bit = n >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) std::swap (a[i], a[j]);
        }
        for (size_t len = 2; len <= n; len <<= 1)
        {
            const double ang = 2 * 3.14159265358979 / (double) len * (inv ? 1 : -1);
            const std::complex<double> wl (std::cos (ang), std::sin (ang));
            for (size_t i = 0; i < n; i += len)
            {
                std::complex<double> w (1);
                for (size_t j = 0; j < len / 2; ++j)
                {
                    const auto u = a[i + j], v = a[i + j + len / 2] * w;
                    a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl;
                }
            }
        }
        if (inv) for (auto& x : a) x /= (double) n;
    }

    // hp, lp : coupures ; pkHz/pkDb : bosse de présence ; room : rugosité du cône / peigne du micro (0..1)
    void build (float sr, float hp, float lp, float pkHz, float pkDb, float room)
    {
        const int N = 4096;
        std::vector<std::complex<double>> spec ((size_t) N);
        auto gauss = [] (double f, double f0, double sigOct) { const double o = std::log2 (std::max (f, 1.0) / f0); return std::exp (-0.5 * o * o / (sigOct * sigOct)); };
        for (int k = 0; k <= N / 2; ++k)
        {
            const double f = std::max (1.0, (double) k * sr / N);
            double m = 1.0 / std::sqrt (1.0 + std::pow (hp / f, 4.0));                 // coupure grave (12 dB/oct)
            m /= std::sqrt (1.0 + std::pow (f / lp, 8.0));                             // coupure aiguë raide (48 dB/oct)
            m *= std::pow (10.0, (3.5 * gauss (f, 105.0, 0.28)) / 20.0);                // résonance du baffle
            m *= std::pow (10.0, (pkDb * gauss (f, pkHz, 0.34)) / 20.0);                 // présence
            m *= std::pow (10.0, (-3.0 * gauss (f, 1150.0, 0.30)) / 20.0);               // creux médium du cône
            m *= std::pow (10.0, (-2.0 * gauss (f, 420.0, 0.45)) / 20.0);
            const double lf = std::log2 (f);
            const double rip = 1.3 * std::sin (lf * 9.1 + 0.4) + 0.9 * std::sin (lf * 17.3 + 1.7) + 0.6 * std::sin (lf * 31.0 + 0.2); // modes du cône
            const double band = gauss (f, 1800.0, 1.6);
            m *= std::pow (10.0, (room * rip * band) / 20.0);
            m *= 1.0 + 0.16 * room * std::cos (2 * 3.14159265358979 * f * 0.00026) * band;  // peigne micro/cône
            m = std::max (m, 1e-5);
            spec[(size_t) k] = std::log (m);
            if (k > 0 && k < N / 2) spec[(size_t) (N - k)] = spec[(size_t) k];
        }
        fft (spec, true);                         // cepstre réel
        for (int i = 1; i < N / 2; ++i) spec[(size_t) i] *= 2.0;
        for (int i = N / 2 + 1; i < N; ++i) spec[(size_t) i] = 0.0;
        fft (spec, false);
        for (auto& x : spec) x = std::exp (x);
        fft (spec, true);
        h.assign ((size_t) kTaps, 0.0f);
        double sum = 0;
        for (int i = 0; i < kTaps; ++i)
        {
            const double w = i < kTaps / 2 ? 1.0 : 0.5 * (1.0 + std::cos (3.14159265358979 * (double) (i - kTaps / 2) / (double) (kTaps / 2)));
            h[(size_t) i] = (float) (spec[(size_t) i].real() * w);
            sum += std::abs (h[(size_t) i]);
        }
        buf.assign ((size_t) kTaps * 2, 0.0f);
        pos = 0;
        (void) sum;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; }
    inline float process (float x)
    {
        if (h.empty()) return x;
        pos = (pos == 0) ? kTaps - 1 : pos - 1;
        buf[(size_t) pos] = buf[(size_t) pos + kTaps] = x;
        const float* b = buf.data() + pos;
        float acc = 0;
        for (int i = 0; i < kTaps; ++i) acc += h[(size_t) i] * b[i];
        return acc;
    }
};

struct Settings
{
    int   guitar = 0;
    float drive = 0.2f, tone = 0.55f, wah = 0.0f, delay = 0.4f, chorus = 0.3f, reverb = 0.6f, volume = 0.75f;   // 0..1
};

//==============================================================================
// Une corde de guitare : guide d'onde à deux lignes
struct StringVoice
{
    static constexpr int kLine = 16384;
    Line R, Lc;                 // R : sillet -> chevalet ; Lc : chevalet -> sillet
    float hexLp = 0.0f;         // filtre avant la saturation corde par corde
    float gTgt = 0.0f;          // perte visée : on y glisse doucement (un saut de perte fait un clic au micro)
    bool active = false, gate = false, releasing = false, held = false;
    int note = -1;
    uint32_t age = 0;
    float curSemi = 60, tgtSemi = 60, glideK = 0;
    float vel = 0.8f;
    // boucle
    float p = 0.3f, g = 0.99f, relG = 1.0f, relP = 0.3f;
    float apA = -0.12f, ax1[2] = { 0, 0 }, ay1[2] = { 0, 0 }, lp1 = 0;
    float Mcur = 100, Mtgt = 100, Mstep = 0;
    float prevA = 0, prevB = 0;
    float env = 0;
    int   hop = 0;
    // excitation
    int   excN = 0, excLen = 0;
    float excW = 20, excAmp = 0, excNoise = 0;
    float excPos = 0.15f;
    // enveloppes
    float swellT = 0, swellLen = 0;
    float releaseT = 0;
    float quick = 0;
    Rng rng;
    SVF sq;      // bruit de frottement (slide)
    float lastSemi = 60, lastSemiNoise = 60, pkGain = 1.0f;
    uint32_t life = 0;

    void init() { R.init (kLine); Lc.init (kLine); }
    void clear()
    {
        R.clear(); Lc.clear(); lp1 = 0; ax1[0] = ax1[1] = ay1[0] = ay1[1] = 0; prevA = prevB = 0; env = 0; excN = excLen = 0; sq.reset();
    }
};

//==============================================================================
class Engine
{
public:
    Engine() { voices.resize (16); }

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& v : voices) { v.init(); v.active = false; }
        reverb.prepare (sampleRate);
        chL.init ((int) (0.06f * sr) + 8); chR.init ((int) (0.06f * sr) + 8);
        dlA.init ((int) (2.2f * sr)); dlB.init ((int) (2.2f * sr));
        setGuitar (0, true);
        applySettings (true);
        reset();
    }

    void reset()
    {
        for (auto& v : voices) { v.clear(); v.active = false; v.gate = false; }
        held.clear();
        pending.clear();
        wahSvf.reset(); wah2.reset(); pkEq.reset(); os.reset();
        for (auto& b : cabFilt) b.reset();
        cab.reset(); sagEnv = 0; for (auto& b : bodyF) b.reset(); bodyHp.reset(); noteAge = 9.0f; wahT = 9.0f;
        toneLo.reset(); toneHi.reset(); midPre.reset(); preHp.reset(); stageLp.reset(); couple.reset(); dcOut.reset();
        chL.clear(); chR.clear(); dlA.clear(); dlB.clear();
        dlFbA.reset(); dlFbB.reset(); dlHpA.reset(); dlHpB.reset();
        reverb.reset();
        compEnv = 0; compGain = 1; wahEnv = 0; wahPos = 0; wahAmt = 0;
        bendS = 0; modS = 0; vibPh = 0; tremPh = 0; chPh1 = 0; chPh2 = 0.37f; wowPh = 0;
        limiter = 1.0f;
        for (auto& a : viz) a.store (0.0f);
    }

    void setGuitar (int g, bool force = false)
    {
        g = std::max (0, std::min (g, kNumGuitars - 1));
        if (g == guitar && ! force) return;
        guitar = g;
        V = &voicing (g);
        for (auto& v : voices) { v.clear(); v.active = false; v.gate = false; }
        held.clear(); pending.clear();
        configure();
    }

    void setSettings (const Settings& s)
    {
        if (s.guitar != guitar) setGuitar (s.guitar);
        st = s;
        applySettings (false);
    }

    void setTempo (double bpm) { bpmHost = (float) std::max (40.0, std::min (240.0, bpm)); }

    // ------------------------------------------------------------ MIDI
    void noteOn (int note, float velocity)
    {
        velocity = clampf (velocity, 0.02f, 1.0f);
        if (V->mono)
        {
            doNoteOn (note, velocity, 0);
            return;
        }
        // accord : étalement type « grattage » (notes proches dans le temps)
        const uint64_t now = clock;
        if (now - lastOnClock > (uint64_t) (0.03f * sr)) strumIdx = 0;
        lastOnClock = now;
        const int idx = strumIdx++;
        int delay = 0;
        if (V->strumMs > 0 && idx > 0)
            delay = (int) (std::min (idx, 7) * V->strumMs * (0.65f + 0.35f * (rngUi.next() + 1.0f)) * 0.001f * sr);
        if (delay <= 0) doNoteOn (note, velocity, 0);
        else pending.push_back ({ note, velocity, delay });
    }

    // note jouée tout de suite, sans l'étalement automatique (utilisé par le joueur de rythmes)
    void strike (int note, float velocity) { doNoteOn (note, clampf (velocity, 0.02f, 1.0f), 0); }

    void noteOff (int note)
    {
        // note encore en attente d'étalement : on l'annule
        for (size_t i = 0; i < pending.size(); ++i) if (pending[i].note == note) { pending.erase (pending.begin() + (long) i); return; }
        if (V->mono)
        {
            for (size_t i = 0; i < held.size(); ++i) if (held[i] == note) { held.erase (held.begin() + (long) i); break; }
            for (auto& v : voices)
                if (v.active && v.gate && v.note == note)
                {
                    if (! held.empty() && V->legato) { v.note = held.back(); v.tgtSemi = (float) held.back(); setGlide (v, 1.0f); }
                    else releaseVoice (v);
                }
            return;
        }
        for (auto& v : voices)
            if (v.active && v.gate && v.note == note)
            {
                if (sustainDown) v.held = true; else releaseVoice (v);
            }
    }

    void sustain (bool down)
    {
        sustainDown = down;
        if (! down) for (auto& v : voices) if (v.active && v.held) { v.held = false; if (v.gate) releaseVoice (v); }
    }

    void allNotesOff()
    {
        pending.clear(); held.clear();
        for (auto& v : voices) if (v.active && v.gate) releaseVoice (v);
    }

    void pitchWheel (float v) { bendTgt = clampf (v, -1.0f, 1.0f); }
    void modWheel (float v) { modTgt = clampf (v, 0.0f, 1.0f); }

    // ------------------------------------------------------------ lectures pour l'interface
    int debugTap = 0;   // essais : 1 = micro seul, 2 = après wah, 0 = chaîne complète
    float vizAmp (int i) const { return viz[(size_t) i].load(); }
    float wahPosition() const { return wahShown.load(); }
    float outputLevel() const { return outLevel.load(); }
    const Settings& settings() const { return st; }

    //==========================================================================
    void process (float* outL, float* outR, int n)
    {
        int done = 0;
        while (done < n)
        {
            const int m = std::min (kMaxBlock, n - done);
            processChunk (outL + done, outR + done, m);
            done += m;
        }
    }

private:
    struct Pending { int note; float vel; int delay; };

    float sr = 48000.0f;
    float hexK = 0.3f;
    int guitar = 0;
    const Voicing* V = &voicing (0);
    Settings st;
    std::vector<StringVoice> voices;
    std::vector<int> held;
    std::vector<Pending> pending;
    bool sustainDown = false;
    uint32_t ageCounter = 0;
    uint64_t clock = 0, lastOnClock = 0;
    int strumIdx = 0;
    Rng rngUi;
    float bpmHost = 100.0f;

    // contrôleurs lissés
    float bendTgt = 0, bendS = 0, modTgt = 0, modS = 0;
    Rng vrng; float w1 = 0, w2 = 0;
    float vibPh = 0, tremPh = 0, chPh1 = 0, chPh2 = 0.37f, wowPh = 0;

    // chaîne
    Biquad pkEq, midPre, preHp, toneLo, toneHi, cabFilt[5], dcOut, stage2Hp;
    OnePole stageLp, couple, couple2, stage2Lp, stage3Lp;
    CabFir cab;
    float sagEnv = 0, sagRel = 0.0002f, sagAtt = 0.002f, pg1 = 1, pg2 = 1, pg3 = 1;
    SVF wahSvf, wah2;
    Biquad bodyF[6], bodyHp;
    float noteAge = 9.0f, wahT = 9.0f, brightK = 1.0f, sweepS = 0, sweepPh = 0;
    Oversampler4 os;
    float compEnv = 0, compGain = 1, compAtt = 0.01f, compRel = 0.0001f;
    float wahEnv = 0, wahPos = 0, wahAmt = 0, wahPosSm = 0;
    float preGain = 1, gain2 = 1, stageBias = 0.1f, postGain = 1, outGain = 1;
    float limiter = 1.0f;
    Line chL, chR, dlA, dlB;
    OnePole dlFbA, dlFbB, dlHpA, dlHpB;
    float dlTime = 0.45f * 48000.0f, dlTimeSm = 0.45f * 48000.0f, dlFb = 0.3f, dlWet = 0.3f;
    float chDepth = 0, chMix = 0;
    dreverb::Engine reverb;
    int lastRvSig = -1;

    std::array<std::atomic<float>, 6> viz {};
    std::atomic<float> wahShown { 0.0f }, outLevel { 0.0f };

    float monoBuf[kMaxBlock], sigL[kMaxBlock], sigR[kMaxBlock];

    //==========================================================================
    void configure()
    {
        hexK = 1.0f - std::exp (-2.0f * kPi * 3200.0f / sr);
        const Voicing& v = *V;
        pkEq.lowpass (v.pkResHz, v.pkResQ, sr);
        // petite bosse de résonance, puis coupure naturelle
        midPre.peaking (900.0f, 0.8f, v.preMidDb, sr);
        preHp.highpass (80.0f, 0.7f, sr);
        stageLp.setCutoff (6500.0f * v.fizz, sr * 4.0f);
        couple.setCutoff (35.0f, sr * 4.0f);
        couple2.setCutoff (90.0f, sr * 4.0f);
        stage2Lp.setCutoff (7500.0f * v.fizz, sr * 4.0f);
        stage3Lp.setCutoff (5200.0f * v.fizz, sr * 4.0f);
        sagAtt = 1.0f - std::exp (-1.0f / (0.004f * sr));
        sagRel = 1.0f - std::exp (-1.0f / (0.12f * sr));
        cab.build (sr, v.cabHp, v.cabLp, v.cabPeakHz, v.cabPeakDb, v.room >= 0.0f ? v.room : (v.driveMaxDb > 20.0f ? 1.0f : 0.5f));
        stageBias = v.bias;
        cabFilt[0].highpass (v.cabHp, 0.7f, sr);
        cabFilt[1].lowpass (v.cabLp, 0.7f, sr);
        cabFilt[2].lowpass (v.cabLp * 1.15f, 0.62f, sr);
        cabFilt[3].peaking (v.cabPeakHz, 1.1f, v.cabPeakDb, sr);
        cabFilt[4].peaking (110.0f, 1.2f, 3.0f, sr);   // résonance du baffle
        dcOut.highpass (18.0f, 0.7f, sr);
        compAtt = 1.0f - std::exp (-1.0f / (0.004f * sr));
        compRel = 1.0f - std::exp (-1.0f / (0.200f * sr));
        lastRvSig = -1;
        applySettings (true);
    }

    void applySettings (bool force)
    {
        const Voicing& v = *V;
        const float d = clampf (st.drive, 0, 1);
        const float driveDb = lerpf (v.driveMinDb, v.driveMaxDb, std::pow (d, 1.15f));
        preGain = db2lin (driveDb);
        gain2 = 1.0f + 0.04f * preGain;
        gain2 = std::min (gain2, 8.0f);
        // 3 étages en cascade : le gain total est réparti, le serrage des graves augmente avec le drive
        pg1 = std::pow (preGain, 0.55f);
        pg2 = std::pow (preGain, 0.30f) * 1.4f;
        pg3 = 1.0f + 0.5f * std::pow (preGain, 0.25f);
        preHp.highpass (lerpf (70.0f, 170.0f, d), 0.7f, sr);
        // compensation de niveau : mesurée au banc d'essai (voir tests), approchée par une loi simple
        postGain = levelComp (d);

        const float tn = clampf (st.tone, 0, 1) - 0.5f;
        toneLo.lowShelf (180.0f, -tn * 5.0f, sr);
        toneHi.highShelf (2600.0f, tn * 14.0f, sr);

        wahAmt = clampf (st.wah, 0, 1);
        brightK = 0.6f + 0.9f * wahAmt;
        if (v.acoustic)
        {
            const float b = 0.2f + 1.6f * d;     // le potard BODY règle la chaleur de la caisse
            bodyHp.highpass (72.0f, 0.7f, sr);
            bodyF[0].peaking (100.0f, 5.0f, 4.0f * b, sr);    // air de la rosace
            bodyF[1].peaking (200.0f, 4.0f, 4.0f * b, sr);    // table d'harmonie
            bodyF[2].peaking (285.0f, 5.0f, 1.0f * b, sr);    // fond
            bodyF[3].peaking (420.0f, 4.0f, 0.0f * b, sr);
            bodyF[4].peaking (1100.0f, 1.2f, -2.0f * b, sr);  // creux « boîte »
            bodyF[5].peaking (3300.0f, 0.9f, 0.0f, sr);       // présence des cordes acier
        }
        chDepth = clampf (st.chorus, 0, 1);
        chMix = 0.55f * chDepth;
        dlFb = 0.22f + 0.45f * clampf (st.delay, 0, 1);
        dlWet = 0.62f * clampf (st.delay, 0, 1);
        outGain = std::pow (clampf (st.volume, 0, 1) / 0.75f, 1.6f);

        const float rvMix = 0.55f * clampf (st.reverb, 0, 1);
        const int sig = (int) (rvMix * 1000.0f) + 1000 * guitar;
        if (sig != lastRvSig || force)
        {
            lastRvSig = sig;
            reverb.setParams (dreverb::makeParams (6, v.rvSpace, 0.45f, v.rvTone, 0.0f, rvMix), force);
        }
    }

    // compensation de niveau (dB) selon le potard DRIVE : table mesurée au banc d'essai (tests/calib.cpp)
    float levelComp (float knob) const
    {
        const float pos = clampf (knob, 0.0f, 1.0f) * 8.0f;
        const int i = std::min (7, (int) pos);
        const float t = pos - (float) i;
        return db2lin (lerpf (V->trim[i], V->trim[i + 1], t));
    }

    //==========================================================================
    static void setGlide (StringVoice& v, float k) { v.glideK = k; }

    float glideCoef (float ms) const { return ms <= 0.5f ? 1.0f : 1.0f - std::exp (-1.0f / (ms * 0.001f * sr)); }

    void releaseVoice (StringVoice& v)
    {
        v.gate = false; v.releasing = true; v.held = false;
        v.releaseT = V->releaseMs * 0.001f;
    }

    void quickKill (StringVoice& v, float seconds)
    {
        v.gate = false; v.releasing = true; v.held = false; v.releaseT = seconds;
    }

    StringVoice* allocVoice()
    {
        StringVoice* best = nullptr;
        const int limit = std::min ((int) voices.size(), V->maxVoices + 4);   // quelques voix en fin d'étouffement en plus
        for (int i = 0; i < limit; ++i) if (! voices[(size_t) i].active) { best = &voices[(size_t) i]; break; }
        if (best == nullptr)
        {
            uint32_t oldest = 0xffffffffu;
            for (int i = 0; i < limit; ++i)
            {
                auto& v = voices[(size_t) i];
                const uint32_t score = v.releasing ? v.age : v.age + 0x40000000u;
                if (score < oldest) { oldest = score; best = &v; }
            }
        }
        return best;
    }

    int stringOf (int note) const
    {
        static const int open[6] = { 40, 45, 50, 55, 59, 64 };
        int best = 0;
        for (int i = 0; i < 6; ++i) if (open[i] <= note) best = i;
        return best;
    }

    void doNoteOn (int note, float vel, int)
    {
        // visuel : la corde concernée vibre
        const int si = stringOf (note);
        viz[(size_t) si].store (std::max (viz[(size_t) si].load(), 9.0f * (0.6f + 0.4f * vel)));

        if (V->mono)
        {
            held.erase (std::remove (held.begin(), held.end(), note), held.end());
            const bool legatoNow = V->legato && ! held.empty();
            held.push_back (note);
            if (! legatoNow) { noteAge = 0; wahT = 0; }
            if (legatoNow)
            {
                for (auto& v : voices)
                    if (v.active && v.gate)
                    {
                        v.note = note; v.tgtSemi = (float) note; v.glideK = glideCoef (V->glideMs);
                        // petit « hammer-on » : la corde est ré-énergisée en douceur
                        startExcitation (v, vel * 0.30f, true);
                        return;
                    }
            }
            for (auto& v : voices) if (v.active && v.gate) quickKill (v, 0.04f);
        }
        else
        {
            noteAge = 0;
            // même note rejouée : on réutilise la voix (une corde ne peut sonner qu'une fois)
            for (auto& v : voices) if (v.active && v.note == note) { quickKill (v, 0.05f); }
        }
        StringVoice* v = allocVoice();
        if (v == nullptr) return;
        v->clear();
        v->active = true; v->gate = true; v->releasing = false; v->held = false;
        v->note = note; v->age = ++ageCounter; v->vel = vel;
        v->rng.s = 7777u + (uint32_t) note * 131u + ageCounter * 17u;
        {   // accord humain : chaque note est très légèrement désaccordée (±1,5 centième), et le chanteur (CRY) « cueille » la note par en dessous
            const float det = V->legato ? 0.0f : (V->driveMaxDb > 30.0f ? 0.004f : 0.015f) * v->rng.next();   // saturé : presque pas de désaccord (sinon battements « métalliques »)
            v->tgtSemi = (float) note + det;
            v->curSemi = v->tgtSemi;
            v->glideK = glideCoef (V->glideMs);
            if (V->legato && V->glideMs > 0.5f)
            {   // la note « pleure » : attaque prise d'en dessous puis montée (bend), plus large quand on tape fort
                const float big = V->scoop * clampf ((vel - 0.55f) / 0.35f, 0.0f, 1.0f);
                v->curSemi = v->tgtSemi - (0.16f + 0.08f * vel) - big;
                v->glideK = glideCoef (48.0f + 30.0f * big);
            }
            v->lastSemi = v->curSemi;
        }
        v->swellT = 0; v->swellLen = V->swellMs > 0 ? V->swellMs * 0.001f * sr * (1.25f - 0.5f * vel) : 0;
        v->hop = 0; v->life = 0;
        retune (*v, true);
        startExcitation (*v, vel, false);
    }

    void startExcitation (StringVoice& v, float amp, bool soft)
    {
        const float f = 440.0f * std::exp2 ((v.curSemi - 69.0f) / 12.0f);
        const float harder = clampf (v.vel, 0, 1);
        float widthMs = V->pickMs * (1.35f - 0.6f * harder);
        if (soft) widthMs *= 2.2f;
        v.excW = std::max (3.0f, widthMs * 0.001f * sr);
        v.excLen = (int) (v.excW * 1.6f);
        v.excN = 0;
        v.excAmp = std::pow (amp, 1.1f) * (soft ? 1.0f : 1.0f);
        v.excNoise = V->pickNoise * (soft ? 0.3f : 1.0f) * (0.5f + harder);
        v.excPos = V->pickPos * (0.82f + 0.36f * (v.rng.next() * 0.5f + 0.5f));
        (void) f;
        if (! soft)
        {
            // pincement physique : la corde est tirée en triangle (sommet à la position du médiator) puis relâchée.
            // Spectre en 1/n² comme sur une vraie guitare ; l'impulsion courte ne garde que le « claquement » du médiator.
            const float M = v.Mcur;
            const int Mi = (int) M;
            const float A = 0.5f * v.excAmp * kTriangle;
            // le médiator glisse en relâchant la corde : deux sommets voisins (60 % / 40 %) évitent le creux d'un seul point de pincement
            const float wts[2] = { 0.6f, 0.4f }, posk[2] = { 1.0f, 1.17f };
            for (int q = 0; q < 2; ++q)
            {
                const float apex = std::max (2.0f, std::min (M - 3.0f, v.excPos * posk[q] * M));
                for (int k = 1; k < Mi - 1; ++k)
                {
                    const float y = (float) k < apex ? (float) k / apex : (M - (float) k) / std::max (1.0f, M - apex);
                    v.R.addAt ((float) k, A * wts[q] * y);
                    v.Lc.addAt (M - (float) k, A * wts[q] * y);
                }
            }
            v.excAmp *= (V->acoustic ? 0.4f : 1.0f) * kPulseMix * (0.6f + 0.8f * harder) * clampf (std::pow (f / 200.0f, 2.0f), 0.05f, 3.0f);   // les cordes graves claquent moins, les aiguës plus
        }
    }

    // calcule longueur de ligne, perte et brillance pour la hauteur courante
    void retune (StringVoice& v, bool first)
    {
        const float semi = v.curSemi + pitchOffsetSemi;
        float f = 440.0f * std::exp2 ((semi - 69.0f) / 12.0f);
        f = clampf (f, 10.0f, sr * 0.4f);
        float t60 = V->t60 * std::pow (220.0f / f, 0.35f);
        float fc = V->loopFc * std::pow (std::max (f, 40.0f) / 220.0f, 0.25f);
        if (V->acoustic) fc *= brightK;
        if (V->mod == ModMode::PalmMute) { t60 /= (1.0f + 7.0f * modS); fc *= 1.0f - 0.55f * modS; }   // paume sur le chevalet : étouffé et sombre
        t60 = clampf (t60, 0.12f, 30.0f);
        float p = std::exp (-2.0f * kPi * std::min (fc, sr * 0.45f) / sr);
        const float trt = 1.0f / f;
        float G = std::pow (10.0f, -3.0f * trt / t60);
        float gl = G;
        float pl = p;
        if (v.releasing)
        {
            const float rg = std::pow (10.0f, -3.0f * trt / std::max (0.02f, v.releaseT));
            gl = std::min (G, rg);
            // le filtre de boucle reste le même : changer sa phase déplaçait d'un coup la longueur de corde (= clic)
        }
        v.apA = V->disp;
        const float w0 = 2.0f * kPi * f / sr;
        std::complex<float> z = std::polar (1.0f, -w0);
        std::complex<float> Hap = (v.apA + z) / (1.0f + v.apA * z);
        std::complex<float> Hlp = (1.0f - pl) / (1.0f - pl * z);
        const float phAp = -std::arg (Hap) * 2.0f;
        const float phLp = -std::arg (Hlp);
        float Df = (phAp + phLp) / w0;
        if (Df < 0) Df += 2.0f * kPi / w0;
        const float magLp = std::abs (Hlp);
        v.pkGain = std::pow (220.0f / std::max (f, 30.0f), 2.1f) * 0.9f;
        v.p = pl;
        v.gTgt = std::min (0.99999f, gl / std::max (0.2f, magLp));
        if (first) v.g = v.gTgt;
        float M = (sr / f - 2.0f - Df) * 0.5f;
        M = clampf (M, 6.0f, (float) (StringVoice::kLine - 16));
        v.Mtgt = M;
        if (first) { v.Mcur = M; v.Mstep = 0; }
        else v.Mstep = (M - v.Mcur) / 16.0f;
    }

    float pitchOffsetSemi = 0;   // bend + vibrato (global, en demi-tons)

    //==========================================================================
    inline float voiceSample (StringVoice& v)
    {
        // portamento
        if (v.curSemi != v.tgtSemi)
        {
            v.curSemi += (v.tgtSemi - v.curSemi) * v.glideK;
            if (std::fabs (v.tgtSemi - v.curSemi) < 1.0e-4f) v.curSemi = v.tgtSemi;
        }
        if ((v.hop++ & 15) == 0)
        {
            // le retard suit la hauteur : on recalcule toutes les 16 échantillons
            if (v.releasing || v.curSemi != v.lastSemi || pitchOffsetSemi != lastPitchOffset || (v.hop & 255) == 1)
            {
                retune (v, false);
                v.lastSemi = v.curSemi;
            }
        }
        v.Mcur += v.Mstep;
        if ((v.Mstep > 0 && v.Mcur > v.Mtgt) || (v.Mstep < 0 && v.Mcur < v.Mtgt)) { v.Mcur = v.Mtgt; v.Mstep = 0; }

        const float M = v.Mcur;
        const float bridgeIn = v.R.tap (M);
        const float nutIn = v.Lc.tap (M);

        // chevalet : dispersion (2 passe-tout) + perte + brillance, inversion
        float x = bridgeIn;
        for (int k = 0; k < 2; ++k)
        {
            const float y = v.apA * x + v.ax1[k] - v.apA * v.ay1[k];
            v.ax1[k] = x; v.ay1[k] = y; x = y;
        }
        v.lp1 = (1.0f - v.p) * x + v.p * v.lp1 + 1.0e-20f;
        v.g += (v.gTgt - v.g) * 0.0025f;
        const float bridgeOut = -v.g * v.lp1;
        const float nutOut = -0.9993f * nutIn;
        v.R.push (nutOut);
        v.Lc.push (bridgeOut);

        // excitation : impulsion en cosinus surélevé + un peu de bruit, injectée à la position de pincement
        if (v.excN < v.excLen)
        {
            const float n = (float) v.excN;
            float e = 0;
            if (n < v.excW) e = 0.5f * (1.0f - std::cos (2.0f * kPi * n / v.excW));
            const float win = 1.0f - n / (float) v.excLen;
            e += v.excNoise * v.rng.next() * win;
            e *= v.excAmp * 0.5f;
            v.R.addAt (std::max (1.0f, v.excPos * M), e);
            v.Lc.addAt (std::max (1.0f, (1.0f - v.excPos) * M), e);
            ++v.excN;
        }

        // micros : vitesse de la corde à la position du micro
        // la bobine du micro a une largeur (~1 cm) : on moyenne la vitesse sur deux points voisins, ce qui comble les creux trop profonds du peigne
        auto pickAt = [&] (float pos) { return v.R.tap (std::max (1.0f, pos * M)) + v.Lc.tap (std::max (1.0f, (1.0f - pos) * M)); };
        const float dA = 0.5f * (pickAt (V->pkA) + pickAt (V->pkA + 0.014f));
        float out = V->wA * (dA - v.prevA);
        v.prevA = dA;
        if (V->wB != 0.0f)
        {
            const float dB = 0.5f * (pickAt (V->pkB) + pickAt (V->pkB + 0.014f));
            out += V->wB * (dB - v.prevB);
            v.prevB = dB;
        }
        // à hauteur égale, la vitesse croît avec la fréquence : on compense
        out *= v.pkGain;

        // bruit de frottement du bottleneck (glissando)
        if (V->slideNoise > 0.0f)
        {
            const float speed = std::fabs (v.curSemi - v.lastSemiNoise) * sr;   // demi-tons/s
            v.lastSemiNoise = v.curSemi;
            const float amt = clampf (speed / 40.0f, 0.0f, 1.0f);
            if (amt > 0.001f)
            {
                v.sq.set (2400.0f + 3000.0f * amt, 1.2f, sr);
                v.sq.process (v.rng.next());
                out += v.sq.bp * V->slideNoise * 0.06f * amt;
            }
        }

        // « clic » du médiator / du doigt sur la corde : bref bruit filtré à l'attaque
        if (v.life < (uint32_t) (0.011f * sr) && V->slideNoise <= 0.0f)
        {
            const float k = 1.0f - (float) v.life / (0.011f * sr);
            v.sq.set (3200.0f + 1800.0f * v.vel, 0.9f, sr);
            v.sq.process (v.rng.next());
            out += v.sq.bp * k * k * (0.018f + 0.05f * v.vel * v.vel) * v.pkGain * (V->acoustic ? 0.12f : 1.0f);
        }

        const float aPre = std::fabs (out);
        v.env += (aPre - v.env) * (aPre > v.env ? 0.2f : 0.0005f);

        // volume swell
        float gainEnv = 1.0f;
        if (v.swellLen > 0.0f)
        {
            const float s = std::min (1.0f, v.swellT / v.swellLen);
            gainEnv = s * s * (3.0f - 2.0f * s);
            v.swellT += 1.0f;
        }
        out *= gainEnv;

        ++v.life;
        if (v.excN >= v.excLen && v.env < 1.0e-7f && (float) v.life > 6.0f * M + 64.0f) v.active = false;
        return out;
    }

    float lastPitchOffset = 0;

    //==========================================================================
    void processChunk (float* outL, float* outR, int n)
    {
        const Voicing& v = *V;

        // ---- notes en attente (étalement d'accord)
        for (size_t i = 0; i < pending.size();)
        {
            pending[i].delay -= n;
            if (pending[i].delay <= 0) { const auto pn = pending[i]; pending.erase (pending.begin() + (long) i); doNoteOn (pn.note, pn.vel, 0); }
            else ++i;
        }

        // ---- contrôleurs
        const float smK = 1.0f - std::exp (-1.0f / (0.008f * sr));
        const float modK = 1.0f - std::exp (-1.0f / (0.015f * sr));
        const float vibIncBase = 2.0f * kPi * v.vibRate / sr;

        // vibrato / wah / trémolo : valeurs par échantillon
        for (int i = 0; i < n; ++i)
        {
            bendS += (bendTgt - bendS) * smK;
            modS += (modTgt - modS) * modK;
            w1 += (vrng.next() - w1) * 0.00015f; w2 += (vrng.next() - w2) * 0.00012f;   // dérives lentes (main humaine)
            vibPh += vibIncBase * (1.0f + 0.07f * 200.0f * w1); if (vibPh > 2.0f * kPi) vibPh -= 2.0f * kPi;
            float bs = bendS >= 0 ? bendS * v.bendUp : (v.mod == ModMode::Whammy ? -v.bendDown * std::pow (-bendS, 1.45f) : bendS * v.bendDown);
            float vibC = 0;
            noteAge += 1.0f / sr;
            {
                const bool manual = v.mod != ModMode::Wah && v.mod != ModMode::PalmMute;
                float cents = manual ? modS * v.vibCents : 0.0f;
                if (v.autoVibCents > 0.0f)
                {   // vibrato de chanteur : la note d'abord droite, puis le vibrato s'installe
                    const float t = clampf ((noteAge - v.autoVibDelay) / 0.5f, 0.0f, 1.0f);
                    cents += v.autoVibCents * t * t * (3.0f - 2.0f * t);
                }
                vibC = cents * (1.0f + 0.15f * 200.0f * w2) * std::sin (vibPh) * 0.01f;   // centièmes -> demi-tons
            }
            lastPitchOffset = pitchOffsetSemi;
            pitchOffsetSemi = bs + vibC;
            float acc = 0;
            if (V->hex > 0.0f)
            {   // saturation corde par corde (comme un micro hexaphonique) : chaque corde sature seule,
                // les accords gardent leurs notes au lieu de produire des sons parasites d'intermodulation
                const float h = V->hex, K = 22.0f, lpk = hexK;
                for (auto& sv : voices)
                    if (sv.active)
                    {
                        const float d = voiceSample (sv);
                        sv.hexLp += (d - sv.hexLp) * lpk;
                        acc += (1.0f - h) * d + h * std::tanh (sv.hexLp * K) * (1.6f / K);
                    }
            }
            else
                for (auto& sv : voices) if (sv.active) acc += voiceSample (sv);
            monoBuf[i] = acc;
        }

        // ---- niveau d'affichage des cordes (décroissance)
        {
            const float dt = (float) n / sr;
            for (int s = 0; s < 6; ++s) viz[(size_t) s].store (viz[(size_t) s].load() * std::exp (-dt * (1.0f + (float) s * 0.18f)));
        }

        if (debugTap == 1) { for (int i = 0; i < n; ++i) { outL[i] = outR[i] = monoBuf[i]; } return; }
        // ---- micro (résonance électrique) + compresseur doux
        for (int i = 0; i < n; ++i)
        {
            float x = pkEq.process (monoBuf[i]) * kInputGain;
            if (v.comp > 0.0f)
            {
                const float a = std::fabs (x);
                compEnv += (a - compEnv) * (a > compEnv ? compAtt : compRel);
                const float lvl = std::max (compEnv, 1.0e-5f);
                const float thr = 0.10f;
                float gr = 1.0f;
                if (lvl > thr) gr = std::pow (lvl / thr, -(1.0f - 1.0f / 4.0f) * v.comp);
                compGain += (gr - compGain) * 0.01f;
                x *= compGain * (1.0f + 0.9f * v.comp);
            }
            monoBuf[i] = x;
        }

        // ---- wah-wah (pédale « cry baby ») : filtre passe-bande résonnant qui balaie les médiums, devant l'ampli
        const bool usesWah = ! v.acoustic && (v.mod == ModMode::Wah || v.mod == ModMode::WahVibrato || wahAmt > 0.001f);
        if (usesWah && wahAmt > 0.001f)
        {
            for (int i = 0; i < n; ++i)
            {
                if ((i & 7) == 0)
                {
                    // à chaque attaque la pédale s'ouvre (« ouah ») puis retombe lentement ; la molette prend ensuite la main
                    wahT += 8.0f / sr;
                    const float sw = (1.0f - std::exp (-wahT / 0.06f)) * std::exp (-wahT / 0.9f) * (1.0f / 0.78f);
                    const float drive = clampf (v.wahAuto * sw * (1.0f - modS) + modS, 0.0f, 1.0f);
                    float pos = v.wahBase + (1.0f - v.wahBase) * drive;
                    if (v.mod != ModMode::Wah) pos += 0.3f * modS * (1.0f - v.wahBase);
                    wahPos = clampf (pos, 0.0f, 1.0f);
                }
                wahPosSm += (wahPos - wahPosSm) * 0.004f;
                const float x = monoBuf[i];
                if ((i & 7) == 0)
                {
                    const float fc = v.wahMin * std::pow (v.wahMax / v.wahMin, wahPosSm);
                    wahSvf.set (fc, v.wahQ, sr);
                }
                wahSvf.process (x);
                const float bpN = wahSvf.bp / v.wahQ;                 // passe-bande à gain 1 au centre
                const float y = (0.45f * x + 3.4f * bpN) * 0.55f;     // pédale : bosse d'environ +11 dB qui glisse
                monoBuf[i] = x * (1.0f - wahAmt) + y * wahAmt;
            }
            wahShown.store (wahPosSm);
        }
        else
        {
            wahPosSm += (v.wahBase - wahPosSm) * 0.05f;
            wahShown.store (wahPosSm);
        }

        if (debugTap == 2) { for (int i = 0; i < n; ++i) { outL[i] = outR[i] = monoBuf[i]; } return; }
        // ---- guitare acoustique : caisse de résonance au lieu de l'ampli
        if (v.acoustic)
        {
            for (int i = 0; i < n; ++i)
            {
                float x = bodyHp.process (monoBuf[i]);
                for (int k = 0; k < 6; ++k) x = bodyF[k].process (x);
                x = toneLo.process (x);
                x = toneHi.process (x);
                x = dcOut.process (x);
                monoBuf[i] = x * postGain * kAmpOut * 3.0f;
            }
        }
        else
        // ---- ampli : bosse de médiums, 2 étages à lampes (x4), tonalité, baffle
        for (int i = 0; i < n; ++i)
        {
            float x = preHp.process (monoBuf[i]);
            x = midPre.process (x);
            const float a1 = pg1, a2 = pg2, a3 = pg3, bias = stageBias;
            const float y = os.process (x, [&] (float s)
            {
                // étage 1 (triode, asymétrique) -> couplage -> étage 2 -> filtre -> étage 3 (plus doux)
                float u = std::tanh (a1 * s + bias) - std::tanh (bias);
                u = stageLp.lp (u);
                u = couple.hp (u);
                u = std::tanh (a2 * u + bias * 0.8f) - std::tanh (bias * 0.8f);
                u = stage2Lp.lp (u);
                u = couple2.hp (u);
                u = u / (1.0f + 0.15f * std::abs (u));
                u = std::tanh (a3 * u * 0.9f + bias * 0.4f) - std::tanh (bias * 0.4f);
                return stage3Lp.lp (u);
            });
            float z = y * postGain;
            z = dcOut.process (z);
            z = toneLo.process (z);
            z = toneHi.process (z);
            // étage de puissance : compression (sag) puis saturation douce
            sagEnv += ((std::abs (z) > sagEnv) ? sagAtt : sagRel) * (std::abs (z) - sagEnv);
            z = z / (1.0f + 0.7f * sagEnv);
            z = std::tanh (z * 1.15f) * 0.9f;
            z = cab.process (z);
            monoBuf[i] = z * kAmpOut;
        }

        // ---- trémolo d'ampli (CLEAN)
        if (v.mod == ModMode::Tremolo && modS > 0.001f)
        {
            const float inc = 2.0f * kPi * v.vibRate / sr;
            for (int i = 0; i < n; ++i)
            {
                tremPh += inc; if (tremPh > 2.0f * kPi) tremPh -= 2.0f * kPi;
                monoBuf[i] *= 1.0f - v.tremDepth * modS * (0.5f + 0.5f * std::sin (tremPh));
            }
        }

        // ---- chorus stéréo
        {
            const float i1 = 2.0f * kPi * 0.43f / sr, i2 = 2.0f * kPi * 0.57f / sr;
            const float baseA = 0.012f * sr, baseB = 0.016f * sr, dep = (0.0016f + 0.0030f * chDepth) * sr;
            for (int i = 0; i < n; ++i)
            {
                const float x = monoBuf[i];
                chL.push (x); chR.push (x);
                chPh1 += i1; if (chPh1 > 2.0f * kPi) chPh1 -= 2.0f * kPi;
                chPh2 += i2; if (chPh2 > 2.0f * kPi) chPh2 -= 2.0f * kPi;
                const float wl = chL.tap (baseA + dep * std::sin (chPh1));
                const float wr = chR.tap (baseB + dep * std::sin (chPh2 + 1.7f));
                sigL[i] = x * (1.0f - 0.3f * chMix) + chMix * wl;
                sigR[i] = x * (1.0f - 0.3f * chMix) + chMix * wr;
            }
        }

        // ---- delay ping-pong « bande » (temps = croche pointée du tempo de l'hôte)
        if (dlWet > 0.002f)
        {
            float tSec = 0.75f * 60.0f / bpmHost;
            while (tSec > 0.9f) tSec *= 0.5f;
            while (tSec < 0.15f) tSec *= 2.0f;
            dlTime = tSec * sr;
            dlFbA.setCutoff (3300.0f, sr); dlFbB.setCutoff (3300.0f, sr);
            dlHpA.setCutoff (160.0f, sr); dlHpB.setCutoff (160.0f, sr);
            for (int i = 0; i < n; ++i)
            {
                dlTimeSm += (dlTime - dlTimeSm) * 0.0005f;
                wowPh += 2.0f * kPi * 0.6f / sr; if (wowPh > 2.0f * kPi) wowPh -= 2.0f * kPi;
                const float T = dlTimeSm + 0.0004f * sr * std::sin (wowPh);
                const float wa = dlA.tap (T), wb = dlB.tap (T);
                const float in = 0.5f * (sigL[i] + sigR[i]);
                auto sat = [] (float s) { return std::tanh (s * 1.2f) * 0.85f; };
                const float fa = sat (dlHpA.hp (dlFbA.lp (wb))) * dlFb;
                const float fb = sat (dlHpB.hp (dlFbB.lp (wa))) * dlFb;
                dlA.push (in + fa);
                dlB.push (fb);
                sigL[i] += dlWet * wa;
                sigR[i] += dlWet * wb;
            }
        }

        // ---- reverb (moteur D-REVERB, source « guitare »)
        reverb.process (sigL, sigR, n);

        // ---- volume + limiteur doux
        float peak = 0;
        for (int i = 0; i < n; ++i)
        {
            float l = sigL[i] * outGain, r = sigR[i] * outGain;
            l = softClip (l); r = softClip (r);
            outL[i] = l; outR[i] = r;
            peak = std::max (peak, std::max (std::fabs (l), std::fabs (r)));
        }
        outLevel.store (std::max (peak, outLevel.load() * 0.9f));
        clock += (uint64_t) n;
        (void) limiter;
    }

    static inline float softClip (float x)
    {
        const float a = std::fabs (x);
        if (a < 0.85f) return x;
        const float s = x < 0 ? -1.0f : 1.0f;
        return s * (0.85f + 0.15f * std::tanh ((a - 0.85f) / 0.15f));
    }
};
} // namespace dguitar
