/*
COST MODEL COMPARISON TEST
===========================
Tests three radix selection strategies against each other:

  Strategy A: Always R-16 (no model, zero overhead)
  Strategy B: Simple entropy threshold (if entropy > 0.85 → R-11, else R-16)
  Strategy C: Our QI IPR-based cost model (the actual qi::sort)

For each dataset, we:
  1. Run all 3 strategy choices
  2. Record actual measured time
  3. Check which strategy made the correct call

This answers: is our cost model better than just using entropy alone?
Or is it even worth having a model at all?
*/

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include "../include/qi_radix.hpp"

using namespace std;
using Clock = chrono::high_resolution_clock;

static double ms(function<void()> fn) {
    auto t = Clock::now();
    fn();
    return chrono::duration<double,milli>(Clock::now()-t).count();
}

// Plain Radix-16 (always, no model)
static void r16(vector<uint32_t>& d) {
    size_t n = d.size();
    if (n <= 1) return;
    auto c0 = make_unique<array<size_t,65536>>(); c0->fill(0);
    auto c1 = make_unique<array<size_t,65536>>(); c1->fill(0);
    for (auto v : d) { ++(*c0)[v&0xFFFF]; ++(*c1)[v>>16]; }
    for (size_t i=1;i<65536;++i) { (*c0)[i]+=(*c0)[i-1]; (*c1)[i]+=(*c1)[i-1]; }
    vector<uint32_t> buf(n);
    for (int i=n-1;i>=0;--i) buf[--(*c0)[d[i]&0xFFFF]]=d[i];
    for (int i=n-1;i>=0;--i) d[--(*c1)[buf[i]>>16]]=buf[i];
}

// Plain Radix-11 (always)
static void r11(vector<uint32_t>& d) {
    size_t n = d.size();
    if (n <= 1) return;
    constexpr uint32_t B=2048, MASK=B-1;
    auto c0=make_unique<array<size_t,B>>(); c0->fill(0);
    auto c1=make_unique<array<size_t,B>>(); c1->fill(0);
    auto c2=make_unique<array<size_t,B>>(); c2->fill(0);
    for (auto v:d){++(*c0)[v&MASK];++(*c1)[(v>>11)&MASK];++(*c2)[(v>>22)&MASK];}
    for(size_t i=1;i<B;++i){(*c0)[i]+=(*c0)[i-1];(*c1)[i]+=(*c1)[i-1];(*c2)[i]+=(*c2)[i-1];}
    vector<uint32_t> buf(n);
    for(int i=n-1;i>=0;--i) buf[--(*c0)[d[i]&MASK]]=d[i];
    for(int i=n-1;i>=0;--i) d[--(*c1)[(buf[i]>>11)&MASK]]=buf[i];
    for(int i=n-1;i>=0;--i) buf[--(*c2)[(d[i]>>22)&MASK]]=d[i];
    d=buf;
}

// Strategy B: simple entropy threshold
static string stratB_pick(const qi::State& st) {
    return (st.averageEntropy > 0.85) ? "R-11" : "R-16";
}

struct Row {
    string  dataset;
    string  qiPick;
    string  entropyPick;
    string  optimalPick;   // which was actually fastest?
    double  msR16;
    double  msR11;
    double  msQi;
    double  msEntropy;
    bool    qiCorrect;     // did QI pick the optimal?
    bool    entCorrect;    // did entropy threshold pick the optimal?
};

int main() {
    constexpr size_t N = 1'000'000;
    mt19937_64 rng(42);

    cout << "\n";
    cout << "================================================================\n";
    cout << "  COST MODEL COMPARISON\n";
    cout << "  Strategy A: Always R-16 (no model)\n";
    cout << "  Strategy B: Entropy threshold > 0.85 → R-11 else R-16\n";
    cout << "  Strategy C: Our QI IPR cost model (qi::sort)\n";
    cout << "  N = 1,000,000  |  All native C++  |  -O3 -march=native\n";
    cout << "================================================================\n\n";

    struct DS { string name; vector<uint32_t> data; };
    vector<DS> sets;

    // 1. Full random 32-bit (high entropy)
    { vector<uint32_t> d(N); uniform_int_distribution<uint32_t> dist(0,UINT32_MAX);
      for(auto& x:d) x=dist(rng); sets.push_back({"RANDOM (full 32-bit)", move(d)}); }

    // 2. Values 0-7 only (very low entropy)
    { vector<uint32_t> d(N); uniform_int_distribution<uint32_t> dist(0,7);
      for(auto& x:d) x=dist(rng); sets.push_back({"LOW-RANGE (0-7)", move(d)}); }

    // 3. Values 0-65535 (medium entropy, fits in R-16 low bucket)
    { vector<uint32_t> d(N); uniform_int_distribution<uint32_t> dist(0,65535);
      for(auto& x:d) x=dist(rng); sets.push_back({"MED-RANGE (0-65535)", move(d)}); }

    // 4. Power-law clustered (real-world user IDs)
    { vector<uint32_t> d(N); uniform_real_distribution<double> u(0,1);
      for(auto& x:d) x=(uint32_t)(pow(u(rng),8.0)*500000.0);
      sets.push_back({"CLUSTERED (Power-Law)", move(d)}); }

    // 5. Mostly sorted (95% ordered)
    { vector<uint32_t> d(N); iota(d.begin(),d.end(),0u);
      uniform_int_distribution<size_t> pos(0,N-1);
      for(size_t i=0;i<N/20;++i) swap(d[pos(rng)],d[pos(rng)]);
      sets.push_back({"ALMOST-SORTED (95%)", move(d)}); }

    // 6. Duplicate heavy (1000 unique values)
    { vector<uint32_t> d(N); uniform_int_distribution<uint32_t> dist(0,999);
      for(auto& x:d) x=dist(rng); sets.push_back({"DUPLICATE-HEAVY (1k unique)", move(d)}); }

    // 7. Sawtooth (repeating 0..999 pattern)
    { vector<uint32_t> d(N); for(size_t i=0;i<N;++i) d[i]=i%1000;
      sets.push_back({"SAWTOOTH (mod 1000)", move(d)}); }

    // 8. CRC-32 style hashes (word-like distribution)
    { vector<uint32_t> d(N); uniform_int_distribution<uint32_t> dist(0,UINT32_MAX);
      for(auto& x:d){ x=dist(rng); x=(x^(x>>16))*0x45d9f3b; x=(x^(x>>16)); }
      sets.push_back({"HASH-MIXED (CRC style)", move(d)}); }

    vector<Row> rows;
    int qi_wins=0, ent_wins=0, r16_wins=0, ties=0;

    for (auto& ds : sets) {
        Row row;
        row.dataset = ds.name;

        // Get QI analysis
        qi::State st = qi::analyze(ds.data);
        row.qiPick      = (st.recommendedRadix==qi::Radix::R16)?"R-16":
                          (st.recommendedRadix==qi::Radix::R11)?"R-11":"R-8";
        row.entropyPick = stratB_pick(st);

        // Time R-16
        auto d=ds.data; row.msR16=ms([&]{ r16(d); });
        // Time R-11
        d=ds.data; row.msR11=ms([&]{ r11(d); });
        // Time qi::sort (full pipeline: sense + sort)
        d=ds.data; row.msQi=ms([&]{ qi::sort(d); });
        // Time entropy strategy (same sort, different pick decision)
        d=ds.data;
        if (row.entropyPick=="R-11") row.msEntropy=ms([&]{ r11(d); });
        else                          row.msEntropy=ms([&]{ r16(d); });
        // Add sensing overhead to entropy strategy (fair comparison: both pay for analysis)
        row.msEntropy += st.analysisTimeMs;

        // Which was actually optimal?
        double best = min(row.msR16, row.msR11);
        row.optimalPick = (row.msR16 < row.msR11 * 0.97) ? "R-16" :
                          (row.msR11 < row.msR16 * 0.97) ? "R-11" : "TIE";

        row.qiCorrect  = (row.qiPick  == row.optimalPick || row.optimalPick=="TIE");
        row.entCorrect = (row.entropyPick == row.optimalPick || row.optimalPick=="TIE");

        // Score wins (lower is better, use 3% tolerance)
        double best3 = best * 1.03;
        bool qi_ok  = row.msQi <= best3 * 1.03 || row.optimalPick=="TIE";
        bool ent_ok = row.msEntropy <= best3 * 1.03 || row.optimalPick=="TIE";
        bool r16_ok = row.msR16 <= best3 || row.optimalPick=="TIE";

        if (row.optimalPick=="TIE") { ties++; }
        else {
            // Strict wins
            if (row.qiCorrect) qi_wins++;
            if (row.entCorrect) ent_wins++;
            if (row.optimalPick=="R-16") r16_wins++;
        }

        rows.push_back(row);
    }

    // Print detailed results
    cout << left
         << setw(30) << "Dataset"
         << setw(10) << "Optimal"
         << setw(10) << "QI picks"
         << setw(12) << "Ent. picks"
         << setw(12) << "R-16 (ms)"
         << setw(12) << "R-11 (ms)"
         << setw(12) << "qi (ms)"
         << setw(12) << "ent (ms)"
         << "QI[PASS]  Ent[PASS]\n";
    cout << string(120,'-') << "\n";

    for (auto& r : rows) {
        cout << setw(30) << r.dataset
             << setw(10) << r.optimalPick
             << setw(10) << r.qiPick
             << setw(12) << r.entropyPick
             << setw(12) << fixed << setprecision(2) << r.msR16
             << setw(12) << r.msR11
             << setw(12) << r.msQi
             << setw(12) << r.msEntropy
             << (r.qiCorrect  ? "  [PASS]" : "  [FAIL]")
             << (r.entCorrect ? "   [PASS]" : "   [FAIL]") << "\n";
    }

    int nondecisive = (int)sets.size() - ties;

    cout << string(120,'-') << "\n\n";
    cout << "================================================================\n";
    cout << "  SCORECARD (on " << nondecisive << " decisive datasets)\n";
    cout << "================================================================\n";
    cout << "  Always R-16 (no model)          : wins when optimal is R-16 = "
         << r16_wins << "/" << nondecisive << "\n";
    cout << "  Entropy threshold > 0.85         : correct picks = "
         << ent_wins << "/" << nondecisive << "\n";
    cout << "  Our QI IPR cost model (qi::sort) : correct picks = "
         << qi_wins << "/" << nondecisive << "\n";
    cout << "  Ties (both equal, within 3%)     : " << ties << "\n";
    cout << "================================================================\n\n";

    cout << "VERDICT: ";
    if (qi_wins > ent_wins)
        cout << "QI model beats entropy threshold (" << qi_wins << " vs " << ent_wins << " correct picks)\n";
    else if (ent_wins > qi_wins)
        cout << "Entropy threshold beats QI model (" << ent_wins << " vs " << qi_wins << " correct picks)\n";
    else
        cout << "QI model and entropy threshold tied (" << qi_wins << " each)\n";
    cout << "\n";

    return 0;
}
