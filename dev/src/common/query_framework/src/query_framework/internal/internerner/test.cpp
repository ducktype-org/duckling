#include "key_interner.hpp"
#include <cstdio>
#include <random>
#include <thread>
#include <vector>
#include <map>
#include <set>

static Key makeKey(std::mt19937_64& rng, bool sparse) {
    Key k{};
    k.id = rng() % 101;
    k.hash[0] = rng();
    if (!sparse) { k.hash[1] = rng(); k.hash[2] = rng(); k.hash[3] = rng(); }
    return k;
}

int main() {
    constexpr int kThreads = 16;
    constexpr int kUnique  = 200'000;   // shared pool: every key hit by many threads
    constexpr int kOpsPerThread = 400'000;

    // Build a shared pool of keys (half sparse, half full), deterministic seed.
    std::vector<Key> pool;
    pool.reserve(kUnique);
    {
        std::mt19937_64 rng(42);
        for (int i = 0; i < kUnique; ++i) pool.push_back(makeKey(rng, i % 2 == 0));
    }

    // Each thread interns random keys from the pool, recording key-index -> id.
    std::vector<std::vector<u64>> seen(kThreads, std::vector<u64>(kUnique, ~0ull));
    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t) {
        ts.emplace_back([&, t] {
            std::mt19937_64 rng(1000 + t);
            for (int i = 0; i < kOpsPerThread; ++i) {
                const std::size_t idx = rng() % kUnique;
                KeyID kid = mapToKeyID(pool[idx]);
                u64& prev = seen[t][idx];
                if (prev == ~0ull) prev = kid.id;
                else if (prev != kid.id) { std::fprintf(stderr, "NON-DETERMINISTIC ID!\n"); std::abort(); }
                // spot-check round trip
                if ((i & 0xFFF) == 0) {
                    Key back = kid.getOriginalKey();
                    if (!(back == pool[idx])) { std::fprintf(stderr, "ROUND-TRIP FAILED!\n"); std::abort(); }
                }
            }
        });
    }
    for (auto& th : ts) th.join();

    // Cross-thread determinism: all threads that saw a key must agree on its id.
    std::set<u64> allIds;
    for (std::size_t idx = 0; idx < kUnique; ++idx) {
        u64 agreed = ~0ull;
        for (int t = 0; t < kThreads; ++t) {
            u64 v = seen[t][idx];
            if (v == ~0ull) continue;
            if (agreed == ~0ull) agreed = v;
            else if (agreed != v) { std::fprintf(stderr, "THREADS DISAGREE!\n"); return 1; }
        }
        if (agreed != ~0ull) allIds.insert(agreed);
    }

    const u64 total = KeyInterner::instance().size();
    // Density: ids must be exactly 0..total-1 with no duplicates/gaps.
    if (allIds.size() != total || *allIds.begin() != 0 || *allIds.rbegin() != total - 1) {
        std::fprintf(stderr, "IDs not dense! count=%zu size=%llu\n",
                     allIds.size(), (unsigned long long)total);
        return 1;
    }

    // Full round-trip for every id.
    for (u64 id = 0; id < total; ++id) {
        Key k = KeyID{id}.getOriginalKey();
        KeyID again = mapToKeyID(k);
        if (again.id != id) { std::fprintf(stderr, "REVERSE MISMATCH\n"); return 1; }
    }

    std::printf("OK: %llu unique keys interned, ids dense [0, %llu), "
                "determinism + round-trip verified across %d threads\n",
                (unsigned long long)total, (unsigned long long)total, kThreads);
    return 0;
}