// symbolmusic.cpp
// Subconscious Music Association Engine — prototype
//
// Principle: the machine NEVER decides which symbol fits a song.
// It only RECORDS the human's choice. Symbol<->song links are stored
// human decisions (edges with counts). Vectors/similarity are DERIVED
// from those accumulated picks — the system adds zero structure of its own.
//
// Build:  g++ -std=c++17 -O2 -o symbolmusic symbolmusic.cpp
// Run:    ./symbolmusic
//
// Design: "core" (data model + matrix + similarity) is kept free of I/O so
// it ports cleanly to an embedded/hardware target later. Only main()/UI
// touches std::cin/std::cout.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

// ----------------------------- Data model -----------------------------

struct Symbol {
    int id;
    std::string glyph;  // visual form (may be a "tofu" box on some terminals)
    std::string code;   // neutral, meaningless stable handle, e.g. "S01"
};

struct Song {
    int id;
    std::string title;
    std::string artist;
};

// Associations: songId -> (symbolId -> count of times the human picked it)
using AssocMatrix = std::map<int, std::map<int, int>>;

// ----------------------------- The Library -----------------------------
//
// Symbols use glyphs drawn from undeciphered / ancient scripts purely for
// their meaninglessness. Codes (S01..) are arbitrary, NOT descriptive, so
// they never bias the subconscious pick.

static std::vector<Symbol> buildPalette() {
    // Glyphs sampled from Phaistos Disc, Linear B, Cypriot, Cuneiform blocks.
    // If a glyph doesn't render in your terminal, the code label still works.
    const char* glyphs[] = {
        "\U000101D0", "\U000101D1", "\U000101D2", "\U000101D3", "\U000101D4",
        "\U000101D5", "\U000101D6", "\U000101D7", "\U000101D8", "\U000101D9",
        "\U000101DA", "\U000101DB", "\U000101DC", "\U000101DD", "\U000101DE",
        "\U000101DF", "\U000101E0", "\U000101E1", "\U000101E2", "\U000101E3",
        "\U00010000", "\U00010001", "\U00010002", "\U00010003", "\U00010004",
        "\U00010800", "\U00010801", "\U00010802", "\U00010803", "\U00010804",
        "\U00012000", "\U00012001", "\U00012002", "\U00012003", "\U00012004",
        "\U00012010", "\U00012011", "\U00012012", "\U00012013", "\U00012014",
    };
    const int n = sizeof(glyphs) / sizeof(glyphs[0]);
    std::vector<Symbol> palette;
    palette.reserve(n);
    for (int i = 0; i < n; ++i) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "S%02d", i + 1);
        palette.push_back(Symbol{i + 1, glyphs[i], std::string(buf)});
    }
    return palette;
}

static std::vector<Song> buildSongs() {
    // Hardcoded sample list to exercise the mechanic. IDs are stable.
    return {
        {1,  "Teardrop",                 "Massive Attack"},
        {2,  "Svefn-g-englar",           "Sigur Ros"},
        {3,  "Idioteque",                "Radiohead"},
        {4,  "An Ending (Ascent)",       "Brian Eno"},
        {5,  "Pyramid Song",             "Radiohead"},
        {6,  "Avril 14th",               "Aphex Twin"},
        {7,  "Strobe",                   "deadmau5"},
        {8,  "Re: Stacks",               "Bon Iver"},
        {9,  "天空之城",                  "Joe Hisaishi"},
        {10, "Music for a Found Harm.",  "Penguin Cafe Orch."},
        {11, "Windowlicker",             "Aphex Twin"},
        {12, "Cliffs",                   "Ólafur Arnalds"},
        {13, "Open Eye Signal",          "Jon Hopkins"},
        {14, "Nightcall",                "Kavinsky"},
        {15, "Flim",                     "Aphex Twin"},
    };
}

// ----------------------------- Core engine -----------------------------

class Engine {
public:
    Engine(std::vector<Symbol> syms, std::vector<Song> songs)
        : symbols_(std::move(syms)), songs_(std::move(songs)),
          rng_(std::random_device{}()) {}

    const std::vector<Symbol>& symbols() const { return symbols_; }
    const std::vector<Song>& songs() const { return songs_; }

    const Symbol* symbolById(int id) const {
        for (const auto& s : symbols_) if (s.id == id) return &s;
        return nullptr;
    }
    const Song* songById(int id) const {
        for (const auto& s : songs_) if (s.id == id) return &s;
        return nullptr;
    }

    // --- the human's recorded decision ---
    void recordPick(int songId, int symbolId) {
        assoc_[songId][symbolId] += 1;
    }

    int totalPicksForSong(int songId) const {
        int t = 0;
        auto it = assoc_.find(songId);
        if (it != assoc_.end())
            for (const auto& kv : it->second) t += kv.second;
        return t;
    }

    // Sample k distinct symbols at random for a song view.
    std::vector<int> sampleSymbols(int k) {
        std::vector<int> ids;
        ids.reserve(symbols_.size());
        for (const auto& s : symbols_) ids.push_back(s.id);
        std::shuffle(ids.begin(), ids.end(), rng_);
        if ((int)ids.size() > k) ids.resize(k);
        return ids;
    }

    // Pick the song with the fewest picks so coverage stays even.
    int leastCoveredSong() {
        int best = -1, bestCount = 1 << 30;
        std::vector<int> ids;
        for (const auto& s : songs_) ids.push_back(s.id);
        std::shuffle(ids.begin(), ids.end(), rng_);  // random tie-break
        for (int id : ids) {
            int c = totalPicksForSong(id);
            if (c < bestCount) { bestCount = c; best = id; }
        }
        return best;
    }

    // --- DERIVED structure (built only from human picks) ---

    // Document frequency: how many songs carry a given symbol at all.
    std::map<int, int> docFrequency() const {
        std::map<int, int> df;
        for (const auto& songEntry : assoc_)
            for (const auto& kv : songEntry.second)
                if (kv.second > 0) df[kv.first] += 1;
        return df;
    }

    // TF-IDF weighted symbol vector for one song.
    // tf = pick count; idf = log(N / df). Downweights symbols slapped on everything.
    std::map<int, double> tfidfVector(int songId, const std::map<int, int>& df,
                                      int nSongsWithPicks) const {
        std::map<int, double> v;
        auto it = assoc_.find(songId);
        if (it == assoc_.end()) return v;
        for (const auto& kv : it->second) {
            int sym = kv.first, tf = kv.second;
            auto dit = df.find(sym);
            int d = (dit != df.end()) ? dit->second : 1;
            double idf = std::log((double)(nSongsWithPicks + 1) / (double)(d));
            v[sym] = (double)tf * (idf > 0 ? idf : 1e-6);
        }
        return v;
    }

    int songsWithPicks() const {
        int n = 0;
        for (const auto& e : assoc_) if (!e.second.empty()) ++n;
        return n;
    }

    static double cosine(const std::map<int, double>& a,
                         const std::map<int, double>& b) {
        double dot = 0, na = 0, nb = 0;
        for (const auto& kv : a) {
            na += kv.second * kv.second;
            auto it = b.find(kv.first);
            if (it != b.end()) dot += kv.second * it->second;
        }
        for (const auto& kv : b) nb += kv.second * kv.second;
        if (na == 0 || nb == 0) return 0.0;
        return dot / (std::sqrt(na) * std::sqrt(nb));
    }

    // Songs most similar to a seed, by cosine in symbol-space.
    std::vector<std::pair<int, double>> similarTo(int seedId) const {
        auto df = docFrequency();
        int N = songsWithPicks();
        auto seed = tfidfVector(seedId, df, N);
        std::vector<std::pair<int, double>> out;
        if (seed.empty()) return out;
        for (const auto& s : songs_) {
            if (s.id == seedId) continue;
            auto v = tfidfVector(s.id, df, N);
            if (v.empty()) continue;
            double c = cosine(seed, v);
            if (c > 0) out.push_back({s.id, c});
        }
        std::sort(out.begin(), out.end(),
                  [](auto& x, auto& y) { return x.second > y.second; });
        return out;
    }

    // All songs carrying a given symbol, ranked by pick count.
    std::vector<std::pair<int, int>> songsForSymbol(int symbolId) const {
        std::vector<std::pair<int, int>> out;
        for (const auto& e : assoc_) {
            auto it = e.second.find(symbolId);
            if (it != e.second.end() && it->second > 0)
                out.push_back({e.first, it->second});
        }
        std::sort(out.begin(), out.end(),
                  [](auto& x, auto& y) { return x.second > y.second; });
        return out;
    }

    const std::map<int, int>* fingerprint(int songId) const {
        auto it = assoc_.find(songId);
        return it == assoc_.end() ? nullptr : &it->second;
    }

    // --- persistence (only associations; palette+songs live in code) ---
    bool save(const std::string& path) const {
        std::ofstream f(path);
        if (!f) return false;
        for (const auto& e : assoc_)
            for (const auto& kv : e.second)
                f << e.first << '\t' << kv.first << '\t' << kv.second << '\n';
        return true;
    }
    bool load(const std::string& path) {
        std::ifstream f(path);
        if (!f) return false;
        assoc_.clear();
        int songId, symId, count;
        while (f >> songId >> symId >> count) assoc_[songId][symId] = count;
        return true;
    }

private:
    std::vector<Symbol> symbols_;
    std::vector<Song> songs_;
    AssocMatrix assoc_;
    std::mt19937 rng_;
};

// ----------------------------- UI layer -----------------------------

static const std::string SAVE_PATH = "associations.dat";

static std::string songLabel(const Song& s) {
    return s.title + "  \u2014  " + s.artist;
}

static std::vector<int> parsePicks(const std::string& line, int maxIndex) {
    std::vector<int> picks;
    std::istringstream is(line);
    int n;
    while (is >> n)
        if (n >= 1 && n <= maxIndex) picks.push_back(n);
    return picks;
}

static void associateLoop(Engine& eng) {
    std::cout << "\n=== Association loop ===\n"
              << "For each song, type the numbers of the symbols that FEEL right "
              << "(space-separated,\nup to a few). Enter = skip this song. "
              << "'q' = stop.\n";
    const int SHOW = 7;  // symbols shown per view
    while (true) {
        int songId = eng.leastCoveredSong();
        const Song* song = eng.songById(songId);
        if (!song) break;

        auto sampled = eng.sampleSymbols(SHOW);
        std::cout << "\n\u266A  " << songLabel(*song)
                  << "   (picks so far: " << eng.totalPicksForSong(songId) << ")\n";
        for (size_t i = 0; i < sampled.size(); ++i) {
            const Symbol* sym = eng.symbolById(sampled[i]);
            std::cout << "  [" << (i + 1) << "] " << sym->glyph
                      << "  " << sym->code << "\n";
        }
        std::cout << "> ";

        std::string line;
        if (!std::getline(std::cin, line)) break;
        if (line == "q" || line == "Q") break;

        auto picks = parsePicks(line, (int)sampled.size());
        for (int p : picks) eng.recordPick(songId, sampled[p - 1]);
        if (!picks.empty())
            std::cout << "  recorded " << picks.size() << " pick(s).\n";
    }
    eng.save(SAVE_PATH);
    std::cout << "Saved.\n";
}

static void showFingerprint(Engine& eng) {
    std::cout << "\nSong id? ";
    std::string line; std::getline(std::cin, line);
    int id = std::atoi(line.c_str());
    const Song* song = eng.songById(id);
    if (!song) { std::cout << "No such song.\n"; return; }
    const auto* fp = eng.fingerprint(id);
    std::cout << "\nFingerprint of " << songLabel(*song) << ":\n";
    if (!fp || fp->empty()) { std::cout << "  (no picks yet)\n"; return; }
    std::vector<std::pair<int, int>> rows(fp->begin(), fp->end());
    std::sort(rows.begin(), rows.end(),
              [](auto& a, auto& b) { return a.second > b.second; });
    for (auto& r : rows) {
        const Symbol* s = eng.symbolById(r.first);
        std::cout << "  " << s->glyph << " " << s->code << "  x" << r.second << "\n";
    }
}

static void similarSongs(Engine& eng) {
    std::cout << "\nSeed song id? ";
    std::string line; std::getline(std::cin, line);
    int id = std::atoi(line.c_str());
    const Song* seed = eng.songById(id);
    if (!seed) { std::cout << "No such song.\n"; return; }
    auto ranked = eng.similarTo(id);
    std::cout << "\nSongs that feel like " << songLabel(*seed) << ":\n";
    if (ranked.empty()) { std::cout << "  (need more picks across songs)\n"; return; }
    int shown = 0;
    for (auto& r : ranked) {
        const Song* s = eng.songById(r.first);
        std::printf("  %.3f  %s\n", r.second, songLabel(*s).c_str());
        if (++shown >= 10) break;
    }
}

static void playlistBySymbol(Engine& eng) {
    std::cout << "\nSymbol code (e.g. S03)? ";
    std::string code; std::getline(std::cin, code);
    int symId = -1;
    for (const auto& s : eng.symbols())
        if (s.code == code) { symId = s.id; break; }
    if (symId < 0) { std::cout << "No such symbol.\n"; return; }
    auto rows = eng.songsForSymbol(symId);
    std::cout << "\nPlaylist for " << code << ":\n";
    if (rows.empty()) { std::cout << "  (no songs carry this symbol yet)\n"; return; }
    for (auto& r : rows) {
        const Song* s = eng.songById(r.first);
        std::cout << "  (" << r.second << ")  " << songLabel(*s) << "\n";
    }
}

static void listSongs(const Engine& eng) {
    std::cout << "\nSongs:\n";
    for (const auto& s : eng.songs())
        std::cout << "  " << s.id << ". " << songLabel(s) << "\n";
}

int main() {
    Engine eng(buildPalette(), buildSongs());
    if (eng.load(SAVE_PATH))
        std::cout << "Loaded existing associations from " << SAVE_PATH << ".\n";

    while (true) {
        std::cout << "\n========== Subconscious Music Association ==========\n"
                  << "  1) Associate (the loop)\n"
                  << "  2) Show a song's symbol fingerprint\n"
                  << "  3) Find similar songs (by seed)\n"
                  << "  4) Generate playlist by symbol\n"
                  << "  5) List songs\n"
                  << "  6) Save\n"
                  << "  0) Quit\n> ";
        std::string line;
        if (!std::getline(std::cin, line)) break;
        if (line == "1") associateLoop(eng);
        else if (line == "2") showFingerprint(eng);
        else if (line == "3") similarSongs(eng);
        else if (line == "4") playlistBySymbol(eng);
        else if (line == "5") listSongs(eng);
        else if (line == "6") { eng.save(SAVE_PATH); std::cout << "Saved.\n"; }
        else if (line == "0") { eng.save(SAVE_PATH); break; }
    }
    std::cout << "Bye.\n";
    return 0;
}
