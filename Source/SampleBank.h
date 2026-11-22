// D-GUITAR — chargement des vraies guitares (échantillons) en arrière-plan.
// Le fil audio demande un jeu (wanted) ; ce fil le charge depuis le disque et le publie.
// Un jeu n'est libéré que lorsque le fil audio a confirmé utiliser le nouveau (acked == published).
#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>
#include "Engine/SampleSet.h"

class SampleBank : private juce::Thread
{
public:
    enum Status { none = 0, loading = 1, ready = 2, missing = 3 };

    SampleBank() : juce::Thread ("D-GUITAR samples") { startThread (juce::Thread::Priority::low); }
    ~SampleBank() override
    {
        stopThread (4000);
        published.store (nullptr);
    }

    static const char* setName (int id)
    {
        static const char* names[] = { "", "electric-di", "steel", "finger", "nylon", "steel1", "pad-angelitus", "pad-piratas", "pad-ratones" };
        return names[juce::jlimit (0, 5, id)];
    }

    // fil audio : quel jeu faut-il (0 = aucun) ; renvoie le jeu publié et confirme son utilisation
    const dguitar::SampleSet* audioFetch (int wantedId)
    {
        wanted.store (wantedId);
        auto* p = published.load();
        acked.store (p);
        return p;
    }

    int status() const { return state.load(); }

    static juce::Array<juce::File> searchFolders()
    {
        const auto sub = juce::String ("Studio Danova/D-GUITAR/Samples");
        const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
        return { juce::File ("/Library/Application Support/Studio Danova/D-GUITAR/Samples"),   // installateur (clients)
                 home.getChildFile ("Downloads/D-Plugins/D-GUITAR/Samples"),   // dossier D-Plugins de Dan
                 juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile (sub),
                 juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile (sub),
                 juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory).getChildFile (sub) };
    }

private:
    void run() override
    {
        while (! threadShouldExit())
        {
            const int w = wanted.load();
            if (w != loadedId)
            {
                if (w == 0) { state.store (none); loadedId = 0; publish (nullptr); }
                else
                {
                    state.store (loading);
                    auto set = std::make_shared<dguitar::SampleSet>();
                    bool ok = false;
                    for (auto& dir : searchFolders())
                    {
                        const auto f = dir.getChildFile (juce::String (setName (w)) + ".dgs");
                        if (f.existsAsFile() && set->load (f.getFullPathName().toStdString())) { ok = true; break; }
                        if (threadShouldExit()) return;
                    }
                    loadedId = w;
                    if (ok) { set->name = setName (w); publish (set); state.store (ready); }
                    else { publish (nullptr); state.store (missing); }
                }
            }
            // libère les anciens jeux dès que le fil audio a pris le nouveau
            if (acked.load() == published.load())
            {
                auto* cur = published.load();
                retired.erase (std::remove_if (retired.begin(), retired.end(), [cur] (const std::shared_ptr<dguitar::SampleSet>& s) { return s.get() != cur; }),
                               retired.end());
            }
            wait (40);
        }
    }

    void publish (std::shared_ptr<dguitar::SampleSet> s)
    {
        if (s != nullptr) retired.push_back (s);   // garde la propriété (le jeu courant reste dans la liste)
        published.store (s.get());
    }

    std::atomic<int> wanted { 0 }, state { none };
    int loadedId = 0;
    std::atomic<const dguitar::SampleSet*> published { nullptr }, acked { nullptr };
    std::vector<std::shared_ptr<dguitar::SampleSet>> retired;   // tous les jeux encore en mémoire (dont le courant)
};
