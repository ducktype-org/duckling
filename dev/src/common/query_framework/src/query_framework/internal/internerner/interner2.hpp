#pragma once
//
// KeyInterner v2 -- concurrency-optimized interning of 320-bit Keys into dense
// 64-bit KeyIDs, tuned for read-heavy workloads (4-16 threads, hot repeated keys).
//
// Design goals, in order:
//   1. Lookup of an already-interned key performs ZERO writes to shared memory:
//      no lock word, no reference count, no atomic RMW. Pure loads on
//      read-mostly data stay replicated in every core's cache (MESI 'S' state),
//      so hot lookups generate no coherence traffic at all.
//   2. A small thread-local direct-mapped cache in front: a hit touches only
//      TLS memory (~15 instructions, no shared lines at all).
//   3. Inserts are rare after warmup -> serialized per shard with a spinlock.
//      This keeps the table code trivially correct and makes resize easy.
//
// Correctness model:
//   - Slots are write-once: {key words} are stored first, then the meta word is
//     release-stored. Readers acquire-load meta; nonzero meta implies the key
//     words (and the reverse-map entry for that id) are fully visible.
//   - Resize builds a new table under the shard lock, publishes it with a
//     release store, and RETIRES (does not free) the old one. In-flight readers
//     may keep probing the old table; if they miss there they fall through to
//     the locked path which re-checks the current table. Retired tables are
//     freed only at shutdown; geometric growth bounds the waste to < 1x live.
//   - Keys are never removed, so an empty slot validly terminates a probe.
//
// Distribution-specific tricks kept from v1:
//   - hash[0] is chaotic in BOTH key categories (SHA-256 output) -> used raw
//     for shard selection, slot index and the TL cache index. No re-hashing.
//   - "Sparse" keys (hash[1..3]==0) use 16-byte slots, full keys 40-byte slots.
//   - key.id (< 256) and the KeyID (< 2^55) are packed into the meta word.
//
// Requires C++17. x86-64 and ARM64 friendly (acquire/release loads are plain
// loads/stores on x86; the hot path compiles to straight-line loads+compares).
//
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>
#if defined(__x86_64__) || defined(_M_X64)
  #include <immintrin.h>
#endif

using u64 = std::uint64_t;

struct Key {
    u64 id;         // small, 0..255 supported (you said max ~100 and known)
    u64 hash[4];    // fully chaotic, or hash[0] chaotic + hash[1..3]==0

    friend bool operator==(const Key& a, const Key& b) noexcept {
        // Branchless full compare; both categories go through the same code.
        return ((a.id ^ b.id) | (a.hash[0] ^ b.hash[0]) | (a.hash[1] ^ b.hash[1]) |
                (a.hash[2] ^ b.hash[2]) | (a.hash[3] ^ b.hash[3])) == 0;
    }
};
static_assert(std::is_trivial_v<Key>);

struct KeyID {
    u64 id;
    Key getOriginalKey() const;
    friend bool operator==(KeyID a, KeyID b) noexcept { return a.id == b.id; }
};

namespace ki_detail {

inline void cpuRelax() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    _mm_pause();
#elif defined(__aarch64__)
    asm volatile("yield" ::: "memory");
#else
    std::this_thread::yield();
#endif
}

// Test-and-test-and-set spinlock with exponential backoff (same spirit as
// yours, but spins on a plain load between TAS attempts so waiters don't
// hammer the line with RFOs). Only ever taken on first-time inserts.
class alignas(128) Spinlock {
    std::atomic<std::uint32_t> flag_{0};
public:
    void lock() noexcept {
        u64 reps = 2;
        for (;;) {
            if (flag_.exchange(1, std::memory_order_acquire) == 0) return;
            while (flag_.load(std::memory_order_relaxed) != 0) {
                for (u64 i = 0; i < reps; ++i) cpuRelax();
                if (reps < 128) reps *= 2;
                else std::this_thread::sleep_for(std::chrono::nanoseconds(50));
            }
        }
    }
    void unlock() noexcept { flag_.store(0, std::memory_order_release); }
};

struct LockGuard {
    Spinlock& l;
    explicit LockGuard(Spinlock& s) noexcept : l(s) { l.lock(); }
    ~LockGuard() { l.unlock(); }
};

} // namespace ki_detail

class KeyInterner {
public:
    static KeyInterner& instance() {
        static KeyInterner g;
        return g;
    }

    // ------------------------------------------------------------------ intern
    KeyID intern(const Key& k) {
        assert(k.id < 256 && "widen kIdBits if key.id can exceed 255");

        // L0: thread-local direct-mapped cache. Hit = no shared memory at all.
        TLEntry& e = tlEntry(k);
        if (e.kidPlus1 != 0 && e.key == k) return KeyID{e.kidPlus1 - 1};

        const KeyID r = globalFindOrInsert(k);
        e.key = k;                       // TLS write, invisible to other cores
        e.kidPlus1 = r.id + 1;
        return r;
    }

    // Lock-free, wait-free reverse lookup. Valid for ids obtained from intern()
    // (directly or handed over with ordinary synchronization).
    Key lookup(u64 id) const {
        assert(id < next_.load(std::memory_order_relaxed));
        const Key* block = blocks_[id >> kBlockBits].load(std::memory_order_acquire);
        return block[id & kBlockMask];
    }

    u64 size() const { return next_.load(std::memory_order_relaxed); }

    KeyInterner(const KeyInterner&) = delete;
    KeyInterner& operator=(const KeyInterner&) = delete;

private:
    // ------------------------------------------------------------- parameters
    static constexpr unsigned kShardBits  = 6;                   // 64 shards
    static constexpr std::size_t kShards  = std::size_t(1) << kShardBits;
    static constexpr std::size_t kInitCap = 256;                 // slots/table
    static constexpr unsigned kTLBits     = 9;                   // 512-entry TL cache (24 KiB)
    static constexpr std::size_t kTLSize  = std::size_t(1) << kTLBits;
    static constexpr unsigned kBlockBits  = 16;                  // reverse map blocks
    static constexpr std::size_t kBlockSize = std::size_t(1) << kBlockBits;
    static constexpr std::size_t kBlockMask = kBlockSize - 1;
    static constexpr std::size_t kMaxBlocks = std::size_t(1) << 16; // 2^32 keys

    // meta word: [63]=published | [62:55]=key.id (8b) | [54:0]=KeyID (55b)
    static constexpr u64 kPub     = u64(1) << 63;
    static constexpr unsigned kIdShift = 55;
    static constexpr u64 kKidMask = (u64(1) << kIdShift) - 1;

    static constexpr u64 kGolden  = 0x9E3779B97F4A7C15ull;
    static constexpr u64 kMiss    = ~u64(0);

    static u64 encodeMeta(u64 id, u64 kid) noexcept { return kPub | (id << kIdShift) | kid; }
    static u64 metaId(u64 m) noexcept  { return (m >> kIdShift) & 0xFF; }
    static u64 metaKid(u64 m) noexcept { return m & kKidMask; }

    static bool isSparse(const Key& k) noexcept {
        return (k.hash[1] | k.hash[2] | k.hash[3]) == 0;
    }
    // Shard by TOP bits of h0, index tables/TL-cache by LOW bits: independent.
    static std::size_t shardOf(u64 h0) noexcept { return h0 >> (64 - kShardBits); }
    static u64 idxMix(u64 h0, u64 id) noexcept { return h0 ^ (id * kGolden); }

    // ------------------------------------------------------------ table slots
    // Write-once slots. meta==0 <=> empty (terminates probes; keys never leave).
    struct SparseSlot {                          // 16 B, 4 per cache line
        std::atomic<u64> meta;
        std::atomic<u64> h0;
    };
    struct FullSlot {                            // 40 B
        std::atomic<u64> meta;
        std::atomic<u64> h[4];
    };

    template <class SlotT>
    struct Table {
        u64 mask;                                // capacity - 1 (power of two)
        SlotT* slots;
    };

    template <class SlotT>
    static Table<SlotT>* makeTable(std::size_t cap) {
        auto* t = new Table<SlotT>;
        t->mask = cap - 1;
        t->slots = new SlotT[cap]();             // zero-initialized => all empty
        return t;
    }

    // ------------------------------------------------------------- the shards
    struct alignas(128) Shard {
        ki_detail::Spinlock lock;                          // taken on insert only
        std::atomic<Table<SparseSlot>*> sparse{nullptr};   // current tables
        std::atomic<Table<FullSlot>*>   full{nullptr};
        u64 sparseCount = 0;                               // guarded by lock
        u64 fullCount = 0;                                 // guarded by lock
        std::vector<void*> retiredSlots;                   // guarded by lock
        std::vector<void*> retiredTables;                  // guarded by lock
    };

    // ------------------------------------------------------ write-free lookups
    static u64 findSparse(const Table<SparseSlot>* t, u64 id, u64 h0) noexcept {
        u64 idx = idxMix(h0, id) & t->mask;
        for (;;) {
            const SparseSlot& s = t->slots[idx];
            const u64 m = s.meta.load(std::memory_order_acquire);
            if (m == 0) return kMiss;
            if (metaId(m) == id && s.h0.load(std::memory_order_relaxed) == h0)
                return metaKid(m);
            idx = (idx + 1) & t->mask;
        }
    }
    static u64 findFull(const Table<FullSlot>* t, const Key& k) noexcept {
        u64 idx = idxMix(k.hash[0], k.id) & t->mask;
        for (;;) {
            const FullSlot& s = t->slots[idx];
            const u64 m = s.meta.load(std::memory_order_acquire);
            if (m == 0) return kMiss;
            if (metaId(m) == k.id &&
                s.h[0].load(std::memory_order_relaxed) == k.hash[0] &&
                s.h[1].load(std::memory_order_relaxed) == k.hash[1] &&
                s.h[2].load(std::memory_order_relaxed) == k.hash[2] &&
                s.h[3].load(std::memory_order_relaxed) == k.hash[3])
                return metaKid(m);
            idx = (idx + 1) & t->mask;
        }
    }

    KeyID globalFindOrInsert(const Key& k) {
        Shard& s = shards_[shardOf(k.hash[0])];
        // L1: shared table, loads only. Lines stay in 'S' state across cores.
        if (isSparse(k)) {
            const u64 kid = findSparse(s.sparse.load(std::memory_order_acquire), k.id, k.hash[0]);
            if (kid != kMiss) return KeyID{kid};
        } else {
            const u64 kid = findFull(s.full.load(std::memory_order_acquire), k);
            if (kid != kMiss) return KeyID{kid};
        }
        return insertSlow(s, k);                 // first sighting of this key
    }

    // ------------------------------------------------------------ insert path
    KeyID insertSlow(Shard& s, const Key& k) {
        ki_detail::LockGuard g(s.lock);
        const u64 kid = isSparse(k) ? insertSparseLocked(s, k) : insertFullLocked(s, k);
        return KeyID{kid};
    }

    u64 insertSparseLocked(Shard& s, const Key& k) {
        Table<SparseSlot>* t = s.sparse.load(std::memory_order_relaxed);
        // Re-check: another thread may have inserted while we took the lock.
        u64 kid = findSparse(t, k.id, k.hash[0]);
        if (kid != kMiss) return kid;

        if ((s.sparseCount + 1) * 10 > (t->mask + 1) * 6)        // > 0.6 load
            t = grow<SparseSlot>(s.sparse, s, [](const SparseSlot& os, SparseSlot& ns) {
                    ns.h0.store(os.h0.load(std::memory_order_relaxed), std::memory_order_relaxed);
                });

        u64 idx = idxMix(k.hash[0], k.id) & t->mask;
        while (t->slots[idx].meta.load(std::memory_order_relaxed) != 0)
            idx = (idx + 1) & t->mask;

        kid = allocateId(k);
        t->slots[idx].h0.store(k.hash[0], std::memory_order_relaxed);
        t->slots[idx].meta.store(encodeMeta(k.id, kid), std::memory_order_release);
        ++s.sparseCount;
        return kid;
    }

    u64 insertFullLocked(Shard& s, const Key& k) {
        Table<FullSlot>* t = s.full.load(std::memory_order_relaxed);
        u64 kid = findFull(t, k);
        if (kid != kMiss) return kid;

        if ((s.fullCount + 1) * 10 > (t->mask + 1) * 6)
            t = grow<FullSlot>(s.full, s, [](const FullSlot& os, FullSlot& ns) {
                    for (int i = 0; i < 4; ++i)
                        ns.h[i].store(os.h[i].load(std::memory_order_relaxed), std::memory_order_relaxed);
                });

        u64 idx = idxMix(k.hash[0], k.id) & t->mask;
        while (t->slots[idx].meta.load(std::memory_order_relaxed) != 0)
            idx = (idx + 1) & t->mask;

        kid = allocateId(k);
        for (int i = 0; i < 4; ++i)
            t->slots[idx].h[i].store(k.hash[i], std::memory_order_relaxed);
        t->slots[idx].meta.store(encodeMeta(k.id, kid), std::memory_order_release);
        ++s.fullCount;
        return kid;
    }

    // Rebuild into a 2x table under the shard lock; publish with release;
    // retire (don't free) the old arrays -- concurrent readers may still be
    // probing them. Total retired memory < 1x live (geometric growth).
    template <class SlotT, class CopyKeyFn>
    Table<SlotT>* grow(std::atomic<Table<SlotT>*>& current, Shard& s, CopyKeyFn copyKey) {
        Table<SlotT>* oldT = current.load(std::memory_order_relaxed);
        Table<SlotT>* newT = makeTable<SlotT>((oldT->mask + 1) * 2);
        for (u64 i = 0; i <= oldT->mask; ++i) {
            const SlotT& os = oldT->slots[i];
            const u64 m = os.meta.load(std::memory_order_relaxed);   // we hold the lock
            if (m == 0) continue;
            u64 idx = reinsertIndex(os, m) & newT->mask;
            while (newT->slots[idx].meta.load(std::memory_order_relaxed) != 0)
                idx = (idx + 1) & newT->mask;
            copyKey(os, newT->slots[idx]);
            newT->slots[idx].meta.store(m, std::memory_order_relaxed); // pre-publish OK:
        }                                                              // table not visible yet
        current.store(newT, std::memory_order_release);
        s.retiredSlots.push_back(oldT->slots);
        s.retiredTables.push_back(oldT);
        return newT;
    }
    static u64 reinsertIndex(const SparseSlot& s, u64 m) noexcept {
        return idxMix(s.h0.load(std::memory_order_relaxed), metaId(m));
    }
    static u64 reinsertIndex(const FullSlot& s, u64 m) noexcept {
        return idxMix(s.h[0].load(std::memory_order_relaxed), metaId(m));
    }

    // ------------------------------------------------- id + reverse map (v1)
    u64 allocateId(const Key& k) {
        const u64 id = next_.fetch_add(1, std::memory_order_relaxed);
        assert(id < kKidMask && "KeyID space exhausted");
        reverseSlot(id) = k;         // written BEFORE the meta release-store,
        return id;                   // so anyone who can see the id can read it
    }

    Key& reverseSlot(u64 id) {
        const std::size_t b = id >> kBlockBits;
        assert(b < kMaxBlocks && "raise kMaxBlocks");
        Key* block = blocks_[b].load(std::memory_order_acquire);
        if (!block) {
            Key* fresh = new Key[kBlockSize]();
            if (blocks_[b].compare_exchange_strong(block, fresh,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire))
                block = fresh;
            else
                delete[] fresh;
        }
        return block[id & kBlockMask];
    }

    // ------------------------------------------------------ thread-local cache
    struct TLEntry {                 // 48 B; direct-mapped by low bits of h0
        Key key;
        u64 kidPlus1;                // 0 = empty (so the all-zero Key is unambiguous)
    };
    static_assert(std::is_trivial_v<TLEntry>);   // zero-init in .tbss, no TLS guard

    static TLEntry& tlEntry(const Key& k) noexcept {
        static thread_local TLEntry cache[kTLSize];
        return cache[idxMix(k.hash[0], k.id) & (kTLSize - 1)];
    }

    // ----------------------------------------------------------------- state
    KeyInterner() {
        for (auto& s : shards_) {
            s.sparse.store(makeTable<SparseSlot>(kInitCap), std::memory_order_relaxed);
            s.full.store(makeTable<FullSlot>(kInitCap), std::memory_order_relaxed);
        }
    }
    ~KeyInterner() {
        for (auto& s : shards_) {
            for (void* p : s.retiredSlots)  ::operator delete[](p);   // best effort
            for (void* p : s.retiredTables) ::operator delete(p);
            delete[] s.sparse.load(std::memory_order_relaxed)->slots;
            delete   s.sparse.load(std::memory_order_relaxed);
            delete[] s.full.load(std::memory_order_relaxed)->slots;
            delete   s.full.load(std::memory_order_relaxed);
        }
        for (auto& b : blocks_) delete[] b.load(std::memory_order_relaxed);
    }

    std::array<Shard, kShards> shards_;
    std::atomic<u64> next_{0};
    std::array<std::atomic<Key*>, kMaxBlocks> blocks_{};
};

// -------------------------------------------------------------- public API
inline KeyID mapToKeyID(Key k) {
    return KeyInterner::instance().intern(k);
}
inline Key KeyID::getOriginalKey() const {
    return KeyInterner::instance().lookup(id);
}