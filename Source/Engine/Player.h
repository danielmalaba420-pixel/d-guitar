// D-GUITAR — joueur de rythmes : arpèges, grattements, basse + accord, basse marchante.
// On tient un accord au clavier ; le joueur le transforme en vraie position de guitare (6 cordes,
// cases 0 à 4) puis le joue selon un motif calé sur le tempo de l'hôte.
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

namespace dguitar
{
// ------------------------------------------------------------------ motifs
// Notation d'un pas (séparés par des espaces) :
//   .  la corde continue de sonner          D  grattement vers le bas (toutes les cordes)
//   d  grattement léger vers le bas          U  grattement vers le haut (4 cordes aiguës)
//   u  grattement léger vers le haut         T  grattement bas sur les 4 cordes aiguës
//   X  coup étouffé (« chuck »)              K  accord court et sec (skank)
//   B  basse (fondamentale)                  A  basse alternée          W  basse marchante
//   3 4 5 6  cordes de ré, sol, si, mi aigu  (on combine avec + : « B+6 »)
//   suffixe ! : accent
struct PatternDef
{
    const char* name;
    int div;               // pas par temps : 2 = croches, 3 = triolets, 4 = doubles croches
    const char* steps;
};

inline const std::array<PatternDef, 19>& patterns()
{
    static const std::array<PatternDef, 19> p { {
        { "ARPEGE CLASSIQUE",   2, "B! 3 4 5 6 5 4 3" },
        { "ARPEGE BALLADE",     4, "B! . 4 5 6 . 5 4 A . 4 5 6 . 5 4" },
        { "ARPEGE DESCENDANT",  2, "B! 6 5 4 A 6 5 4" },
        { "ARPEGE 12/8",        3, "B! 4 5 6 5 4 A 4 5 6 5 4" },
        { "ARPEGE RAPIDE",      4, "B! 4 5 6 4 5 6 5 A 4 5 6 4 5 6 5" },
        { "TRAVIS PICKING",     2, "B!+6 4 A 5 B 4 A+6 5" },
        { "GRATTEMENT FOLK",    2, "D! . D U . U D U" },
        { "GRATTEMENT POP",     4, "D! . d U . U d U D . d U . U d U" },
        { "GRATTEMENT BALLADE", 2, "B! . D . A . D U" },
        { "POP ETOUFFE",        2, "D! . X U . U X U" },
        { "BASSE + ACCORD",     2, "B! . T . A . T ." },
        { "BASSE MARCHANTE",    2, "W! T W T W T W U" },
        { "SKANK",              2, ". K . K! . K . K" },
        { "CCM GRATTEMENT",     4, "D! . d u . u D u D . d u . u D u" },
        { "CCM DOUX",           2, "D! . . U . U D U" },
        { "GOSPEL 12/8",        3, "D! u d D u d D! u d D u d" },
        { "GOSPEL SHUFFLE",     3, "D! . U X . U D! . U X . U" },
        { "GRATTEMENT RAPIDE",  4, "D! u d u D u d u D! u d u D u d u" },
        { "PRAISE RAPIDE",      4, "d u d u D! u d u d u d u D! u d u" },
    } };
    return p;
}

inline int numPatterns() { return (int) patterns().size(); }

class Player
{
public:
    // ce que le joueur demande au moteur de faire
    struct Sink
    {
        virtual ~Sink() = default;
        virtual void strike (int note, float vel) = 0;
        virtual void release (int note) = 0;
        // attaque adoucie (le médiator brosse la corde au lieu de la pincer) : softMs = durée de montée
        virtual void strikeSoft (int note, float vel, float /*softMs*/) { strike (note, vel); }
        // bruit du médiator qui racle les cordes pendant un grattement
        virtual void scrape (float /*vel*/, bool /*up*/, float /*durMs*/) {}
    };

    enum Kind : uint8_t { kRest, kDown, kDownL, kUp, kUpL, kTop, kChuck, kSkank, kBass, kAlt, kWalk, kStr };
    struct Hit { Kind k; int arg; };
    struct Step { std::vector<Hit> hits; bool accent = false; };

    Player() { for (auto& c : ringing) c = -1; }

    void prepare (double sampleRate) { sr = sampleRate; reset(); }
    void reset()
    {
        held.clear(); queue.clear(); started = false; stepIdx = -1; walkIdx = 0; pendingStep = -1;
        for (auto& c : ringing) c = -1;
        current.store (-1);
    }

    // 0 = jeu direct, 1..N = motif
    void setPattern (int p)
    {
        p = std::max (0, std::min (numPatterns(), p));
        if (p == pat) return;
        pat = p;
        if (pat > 0) parse (patterns()[(size_t) pat - 1]);
        stepIdx = -1;
    }
    int pattern() const { return pat; }
    bool active() const { return pat > 0; }

    // vitesse du motif (0,5 = moitié, 2 = deux fois plus vite) et humanisation (0..1 : décalages, nuances, main moins régulière)
    void setSpeed (double x) { speed = std::max (0.25, std::min (4.0, x)); }
    void setHuman (float h) { human = std::max (0.0f, std::min (1.0f, h)); }

    void setTempo (double bpm) { tempo = std::max (40.0, std::min (240.0, bpm)); }
    void setHost (bool isPlaying, double ppq) { hostPlaying = isPlaying; hostPpq = ppq; }

    // ------------------------------------------------ notes du clavier
    void noteOn (int note, float vel, Sink& s)
    {
        for (auto& h : held) if (h.note == note) return;
        sinceChange = 0;
        const bool first = held.empty();
        held.push_back ({ note, vel });
        revoice (s);
        if (first)
        {
            releaseCountdown = -1;
            if (! hostPlaying) { beat = 0.0; started = true; stepIdx = -1; startDelay = (int) (0.012 * sr); walkIdx = 0; }
            else
            {
                started = true;
                const double st = beat * div * speed;
                const double frac = st - std::floor (st);
                if (frac < 0.4) { stepIdx = (int) std::floor (st) - 1; startDelay = (int) (0.010 * sr); }
                else startDelay = 0;
            }
        }
    }

    void noteOff (int note, Sink& s)
    {
        held.erase (std::remove_if (held.begin(), held.end(), [note] (const Held& h) { return h.note == note; }), held.end());
        if (held.empty()) releaseCountdown = (int) (0.035 * sr);   // court délai : on peut enchaîner les accords sans couper
        else { sinceChange = 0; revoice (s); }
    }

    void allOff (Sink& s)
    {
        held.clear(); queue.clear(); started = false;
        for (auto& c : ringing) if (c >= 0) { s.release (c); c = -1; }
        current.store (-1);
    }

    // ------------------------------------------------ avance de n échantillons
    void advance (int n, Sink& s)
    {
        // événements programmés (cordes d'un grattement, étouffés…)
        for (size_t i = 0; i < queue.size();)
        {
            queue[i].when -= n;
            if (queue[i].when <= 0)
            {
                const auto e = queue[i];
                queue.erase (queue.begin() + (long) i);
                fire (e, s);
            }
            else ++i;
        }

        sinceChange += n;
        if (pendingStep >= 0 && sinceChange >= settle())
        {   // l'accord est complet : on joue le pas retenu
            const int ps = pendingStep; pendingStep = -1;
            if (! held.empty()) playStep (steps[(size_t) ps], s);
        }
        if (releaseCountdown >= 0)
        {
            releaseCountdown -= n;
            if (releaseCountdown < 0 && held.empty())
            {
                started = false; queue.clear();
                for (auto& c : ringing) if (c >= 0) { s.release (c); c = -1; }
                current.store (-1);
            }
        }

        const double dBeat = (double) n * tempo / 60.0 / sr;
        if (hostPlaying) beat = hostPpq; else beat += dBeat;
        if (hostPlaying) hostPpq += dBeat;

        if (! started || pat == 0 || held.empty() || steps.empty()) return;
        if (startDelay > 0) { startDelay -= n; if (startDelay > 0) return; }

        const int idx = (int) std::floor (beat * div * speed + 1.0e-6);
        if (idx != stepIdx)
        {
            // en mode libre, un pas manqué (bloc trop long) n'est pas rattrapé : on joue le dernier
            stepIdx = idx;
            const int si = ((idx % (int) steps.size()) + (int) steps.size()) % (int) steps.size();
            if (si == 0) walkIdx = 0;
            current.store (si);
            // les doigts arrivent sur les touches à des moments un peu différents : tant que l'accord
            // n'est pas complet (< 30 ms), on attend, sinon on entend des notes fausses (« notes d'ornement »)
            if (sinceChange < settle()) pendingStep = si;
            else playStep (steps[(size_t) si], s);
        }
    }

    // pour l'affichage : nombre de pas du motif, pas courant, contenu d'un pas
    int numSteps() const { return (int) steps.size(); }
    int currentStep() const { return current.load(); }
    int stepWeight (int i) const   // 0 rien, 1 léger, 2 note, 3 accent
    {
        if (i < 0 || i >= (int) steps.size() || steps[(size_t) i].hits.empty()) return 0;
        const auto& st = steps[(size_t) i];
        if (st.accent) return 3;
        for (auto h : st.hits) if (h.k == kDownL || h.k == kUpL || h.k == kChuck) return 1;
        return 2;
    }
    bool chordHeld() const { return ! held.empty(); }

private:
    struct Held { int note; float vel; };
    struct Event { int when; int note; float vel; bool on; int str; float soft = 0.0f; };

    static int pcOf (int n) { return ((n % 12) + 12) % 12; }

    void parse (const PatternDef& d)
    {
        div = d.div;
        steps.clear();
        const char* c = d.steps;
        while (*c)
        {
            while (*c == ' ') ++c;
            if (! *c) break;
            Step st;
            while (*c && *c != ' ')
            {
                const char ch = *c++;
                switch (ch)
                {
                    case 'D': st.hits.push_back ({ kDown, 0 }); break;
                    case 'd': st.hits.push_back ({ kDownL, 0 }); break;
                    case 'U': st.hits.push_back ({ kUp, 0 }); break;
                    case 'u': st.hits.push_back ({ kUpL, 0 }); break;
                    case 'T': st.hits.push_back ({ kTop, 0 }); break;
                    case 'X': st.hits.push_back ({ kChuck, 0 }); break;
                    case 'K': st.hits.push_back ({ kSkank, 0 }); break;
                    case 'B': st.hits.push_back ({ kBass, 0 }); break;
                    case 'A': st.hits.push_back ({ kAlt, 0 }); break;
                    case 'W': st.hits.push_back ({ kWalk, 0 }); break;
                    case '!': st.accent = true; break;
                    default:
                        if (ch >= '1' && ch <= '6') st.hits.push_back ({ kStr, ch - '1' });
                        break;
                }
            }
            steps.push_back (st);
        }
    }

    // --- position de guitare à partir des notes tenues
    void revoice (Sink& s)
    {
        static const int open[6] = { 40, 45, 50, 55, 59, 64 };
        if (held.empty()) return;
        int lo = held[0].note;
        float vsum = 0.0f;
        bool pcs[12] = {};
        for (auto& h : held) { lo = std::min (lo, h.note); pcs[pcOf (h.note)] = true; vsum += h.vel; }
        vel = vsum / (float) held.size();
        const int root = pcOf (lo);
        int npc = 0;
        for (bool b : pcs) npc += b ? 1 : 0;
        if (npc == 1) pcs[(root + 7) % 12] = true;   // une seule touche : quinte ajoutée (accord « power »)
        minor = pcs[(root + 3) % 12] && ! pcs[(root + 4) % 12];

        std::array<int, 6> v {};
        v.fill (-1);
        // corde de basse : la plus grave qui peut jouer la fondamentale en cases 0 à 4
        int bs = -1;
        for (int st = 0; st < 3 && bs < 0; ++st)
            for (int f = 0; f <= 4; ++f) if (pcOf (open[st] + f) == root) { bs = st; v[(size_t) st] = open[st] + f; break; }
        if (bs < 0) { bs = 0; v[0] = open[0] + ((root - pcOf (open[0]) + 12) % 12); }
        bassString = bs;

        bool covered[12] = {};
        covered[root] = true;
        for (int st = bs + 1; st < 6; ++st)
        {
            int best = -1;
            float bestScore = -1.0e9f;
            for (int f = 0; f <= 4; ++f)
            {
                const int n = open[st] + f;
                if (! pcs[pcOf (n)]) continue;
                bool dup = false;
                for (int k = 0; k < st; ++k) if (v[(size_t) k] == n) dup = true;
                if (dup) continue;
                float sc = -(float) f * 0.6f;
                if (! covered[pcOf (n)]) sc += 5.0f;
                if (f == 0) sc += 0.8f;   // les cordes à vide sonnent
                if (sc > bestScore) { bestScore = sc; best = n; }
            }
            if (best < 0)   // aucune note de l'accord accessible : on cherche plus haut (cases 5 à 7)
                for (int f = 5; f <= 7 && best < 0; ++f) if (pcs[pcOf (open[st] + f)]) best = open[st] + f;
            v[(size_t) st] = best;
            if (best >= 0) covered[pcOf (best)] = true;
        }

        // les cordes qui changent de note se taisent (les doigts quittent la case)
        for (int st = 0; st < 6; ++st)
            if (ringing[(size_t) st] >= 0 && ringing[(size_t) st] != v[(size_t) st] && ! isWalkNote (st))
            {
                s.release (ringing[(size_t) st]);
                ringing[(size_t) st] = -1;
            }
        voicing = v;
    }

    bool isWalkNote (int) const { return false; }

    float humanVel (float base)
    {
        rng = rng * 1664525u + 1013904223u;
        const float r = ((float) (rng >> 9) / 8388608.0f) * 2.0f - 1.0f;
        return std::max (0.05f, std::min (1.0f, base * (1.0f + (0.02f + 0.22f * human) * r)));
    }
    int jitter (float ms)
    {
        rng = rng * 1664525u + 1013904223u;
        return (int) ((float) (rng >> 9) / 8388608.0f * ms * (0.3f + 5.0f * human) * 0.001f * (float) sr);
    }

    void schedule (int when, int note, float v, bool on, int str, float soft = 0.0f)
    {
        if (note < 0) return;
        if (when <= 0 && on) { fire ({ 0, note, v, true, str, soft }, *sinkNow); return; }
        queue.push_back ({ std::max (1, when), note, v, on, str, soft });
    }

    void fire (const Event& e, Sink& s)
    {
        if (e.on)
        {
            if (e.str >= 0 && ringing[(size_t) e.str] >= 0 && ringing[(size_t) e.str] != e.note) s.release (ringing[(size_t) e.str]);
            if (e.soft > 0.0f) s.strikeSoft (e.note, e.vel, e.soft); else s.strike (e.note, e.vel);
            if (e.str >= 0) ringing[(size_t) e.str] = e.note;
        }
        else
        {
            s.release (e.note);
            if (e.str >= 0 && ringing[(size_t) e.str] == e.note) ringing[(size_t) e.str] = -1;
        }
    }

    // grattement : les cordes partent l'une après l'autre (le médiator traverse l'accord).
    // Ce qui rend un grattement naturel :
    //  - la traversée dure moins longtemps quand on gratte vite (sinon les coups se chevauchent et « bavent »)
    //  - le médiator brosse les cordes : attaque un peu adoucie, surtout pour les coups légers et les montées
    //  - le bruit du médiator qui racle les cordes
    //  - la main ne touche pas toujours toutes les cordes, et les montées sont plus légères sur les graves
    float rnd01() { rng = rng * 1664525u + 1013904223u; return (float) (rng >> 9) / 8388608.0f; }

    void strum (int from, int to, float v, float spreadMs, bool shortChord, int muteAfterMs)
    {
        const int dir = to >= from ? 1 : -1;
        int count = 0;
        for (int st = from;; st += dir) { if (voicing[(size_t) st] >= 0) ++count; if (st == to) break; }
        if (count == 0) return;
        // durée d'un pas du motif : la traversée n'en prend qu'une part
        const double stepMs = 60000.0 / (tempo * div * speed);
        const float maxTotal = (float) std::max (6.0, std::min (60.0, stepMs * 0.32));
        float per = count > 1 ? std::min (spreadMs, maxTotal / (float) (count - 1)) : 0.0f;
        per *= 1.0f + human * (rnd01() - 0.5f) * 0.5f;   // chaque coup de main est un peu différent
        const bool up = dir < 0;
        const float soft = (up ? 2.5f : 1.2f) + 4.0f * std::max (0.0f, 0.8f - v);
        int t = jitter (3.0f);
        int k = 0;
        float total = 0.0f;
        for (int st = from;; st += dir)
        {
            const int n = voicing[(size_t) st];
            if (n >= 0)
            {
                // la première corde touchée sonne le plus ; en montée, les graves à peine effleurées
                float w = 1.0f - 0.05f * (float) k;
                if (up && st <= 2) w *= 0.7f;
                const float vv = humanVel (v * w);
                schedule (t, n, vv, true, st, soft + rnd01() * 1.5f);
                if (shortChord) schedule (t + (int) (muteAfterMs * 0.001 * sr), n, 0.0f, false, st);
                const float d = per * (0.85f + 0.3f * rnd01());
                t += (int) (d * 0.001f * (float) sr);
                total += d;
                ++k;
            }
            if (st == to) break;
        }
        if (sinkNow != nullptr) sinkNow->scrape (v, up, total + 18.0f);
    }

    int walkNote()
    {
        // fondamentale, tierce, quinte, sixte, en montant sur les cordes graves
        static const int majSteps[4] = { 0, 4, 7, 9 }, minSteps[4] = { 0, 3, 7, 10 };
        const int base = voicing[(size_t) bassString];
        const int off = (minor ? minSteps : majSteps)[walkIdx % 4];
        ++walkIdx;
        return base + off;
    }

    void playStep (const Step& st, Sink& s)
    {
        sinkNow = &s;
        const float acc = st.accent ? 1.12f : 1.0f;
        const float base = std::max (0.25f, vel) * acc;
        const int top = 5, bass = bassString;
        for (auto h : st.hits)
        {
            switch (h.k)
            {
                case kDown:  // coup accentué : toute la main ; sinon on ne touche pas toujours la basse
                    strum (st.accent || rnd01() < 0.4f ? bass : std::min (bass + 1, 2), top - (rnd01() < 0.15f * human ? 1 : 0), base * 0.85f, 9.0f, false, 0); break;
                case kDownL: strum (std::max (bass + 1, 2), top, base * 0.50f, 7.0f, false, 0); break;
                case kUp:    strum (top, rnd01() < 0.5f ? 2 : 3, base * 0.62f, 7.0f, false, 0); break;
                case kUpL:   strum (top, 3, base * 0.40f, 6.0f, false, 0); break;
                case kTop:   strum (2, top, base * 0.75f, 7.0f, false, 0); break;
                case kChuck:
                    // coup étouffé : la main retombe sur les cordes, on n'entend que le claquement
                    for (int k = 0; k < 6; ++k) if (ringing[(size_t) k] >= 0) { s.release (ringing[(size_t) k]); ringing[(size_t) k] = -1; }
                    strum (bass, top, base * 0.30f, 5.0f, true, 22);
                    break;
                case kSkank: strum (top, 2, base * 0.70f, 4.0f, true, 95); break;
                case kBass:  schedule (jitter (2.0f), voicing[(size_t) bass], humanVel (base), true, bass); break;
                case kAlt:
                {
                    const int st2 = std::min (bass + 1, 3);
                    schedule (jitter (2.0f), voicing[(size_t) st2], humanVel (base * 0.88f), true, st2);
                    break;
                }
                case kWalk:  schedule (jitter (2.0f), walkNote(), humanVel (base), true, bass); break;
                case kStr:
                {
                    const int st2 = std::max (h.arg, bass);
                    schedule (jitter (2.0f), voicing[(size_t) st2], humanVel (base * (h.arg >= 4 ? 0.78f : 0.85f)), true, st2);
                    break;
                }
                case kRest: break;
            }
        }
    }

    double sr = 48000.0, tempo = 100.0, beat = 0.0, hostPpq = 0.0;
    bool hostPlaying = false, started = false, minor = false;
    int sinceChange = 1 << 30, pendingStep = -1;
    int settle() const { return (int) (0.03 * sr); }
    int pat = 0, div = 2, stepIdx = -1, startDelay = 0, releaseCountdown = -1, bassString = 0, walkIdx = 0;
    float vel = 0.8f, human = 0.35f;
    double speed = 1.0;
    uint32_t rng = 0x2468aceu;
    std::vector<Held> held;
    std::vector<Step> steps;
    std::vector<Event> queue;
    std::array<int, 6> voicing { { -1, -1, -1, -1, -1, -1 } };
    std::array<int, 6> ringing {};
    std::atomic<int> current { -1 };
    Sink* sinkNow = nullptr;
};

} // namespace dguitar
