#pragma once
//
// KeyInterner: concurrent interning of 320-bit Keys into dense 64-bit KeyIDs.
//
//  - mapToKeyID(key) always returns the same KeyID for the same Key.
//  - IDs are dense: 0, 1, 2, ... in (approximate) first-arrival order, no gaps.
//  - KeyID::getOriginalKey() is lock-free.
//  - Exploits the key distribution:
//      * "sparse" keys (hash[1..3] == 0) are stored as 16 bytes, not 40;
//      * hash[0] is already chaotic (SHA-256 output), so it is used directly
//        as the hash value and for shard selection -- no re-hashing.
//
// Requires C++17 (uses std::shared_mutex, structured bindings).
//
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

using u64 = std::uint64_t;

struct Key {
    u64 id;         // small, 0..~100
    u64 hash[4];    // 256 bits; either fully chaotic, or only hash[0] chaotic and hash[1..3]==0

    friend bool operator==(const Key& a, const Key& b) noexcept {
        return a.id == b.id &&
               a.hash[0] == b.hash[0] && a.hash[1] == b.hash[1] &&
               a.hash[2] == b.hash[2] && a.hash[3] == b.hash[3];
    }
};

struct KeyID {
    u64 id;
    Key getOriginalKey() const;   // defined below, works via the global interner

    friend bool operator==(KeyID a, KeyID b) noexcept { return a.id == b.id; }
};

class KeyInterner {
public:
    static KeyInterner& instance() {
        static KeyInterner g;     // thread-safe magic-static singleton
        return g;
    }

    KeyID intern(const Key& k) {
        const bool sparse = isSparse(k);
        Shard& s = shards_[shardOf(k)];

        // ---- Fast path: key already interned (shared lock, scales with readers)
        {
            std::shared_lock lock(s.mutex);
            if (sparse) {
                auto it = s.sparse.find(SparseKey{k.id, k.hash[0]});
                if (it != s.sparse.end()) return KeyID{it->second};
            } else {
                auto it = s.full.find(k);
                if (it != s.full.end()) return KeyID{it->second};
            }
        }

        // ---- Slow path: insert under exclusive lock (double-checked)
        std::unique_lock lock(s.mutex);
        if (sparse) {
            auto [it, inserted] = s.sparse.try_emplace(SparseKey{k.id, k.hash[0]}, 0);
            if (!inserted) return KeyID{it->second};
            it->second = allocateAndPublish(k);
            return KeyID{it->second};
        } else {
            auto [it, inserted] = s.full.try_emplace(k, 0);
            if (!inserted) return KeyID{it->second};
            it->second = allocateAndPublish(k);
            return KeyID{it->second};
        }
    }

    // Reverse lookup. Valid only for ids previously returned by intern()
    // (received on this thread, or handed over with normal synchronization,
    // which any correct program transferring the KeyID already provides).
    Key lookup(u64 id) const {
        assert(id < next_.load(std::memory_order_relaxed));
        const Key* block = blocks_[id >> kBlockBits].load(std::memory_order_acquire);
        return block[id & kBlockMask];
    }

    u64 size() const { return next_.load(std::memory_order_relaxed); }

    KeyInterner(const KeyInterner&) = delete;
    KeyInterner& operator=(const KeyInterner&) = delete;

private:
    KeyInterner() = default;

    // ---------------- configuration ----------------
    static constexpr unsigned kShardBits = 8;                       // 256 shards
    static constexpr std::size_t kShards = std::size_t(1) << kShardBits;
    static constexpr unsigned kBlockBits = 16;                      // 65536 keys / block
    static constexpr std::size_t kBlockSize = std::size_t(1) << kBlockBits;
    static constexpr std::size_t kBlockMask = kBlockSize - 1;
    static constexpr std::size_t kMaxBlocks = std::size_t(1) << 16; // 2^32 keys total; bump if needed

    // ---------------- helpers ----------------
    static bool isSparse(const Key& k) noexcept {
        return (k.hash[1] | k.hash[2] | k.hash[3]) == 0;
    }
    static std::size_t shardOf(const Key& k) noexcept {
        // hash[0] is chaotic in both categories; top bits pick the shard,
        // (bucket selection inside the maps uses the low bits / modulo).
        return (k.hash[0] >> (64 - kShardBits)) & (kShards - 1);
    }
    static u64 mix(u64 h0, u64 id) noexcept {
        // h0 is already a high-quality hash; just fold the small id in.
        return h0 ^ (id * 0x9E3779B97F4A7C15ull);
    }

    struct SparseKey {
        u64 id, h0;
        friend bool operator==(SparseKey a, SparseKey b) noexcept {
            return a.id == b.id && a.h0 == b.h0;
        }
    };
    struct SparseHash {
        std::size_t operator()(SparseKey k) const noexcept { return mix(k.h0, k.id); }
    };
    struct FullHash {
        std::size_t operator()(const Key& k) const noexcept { return mix(k.hash[0], k.id); }
    };

    struct alignas(64) Shard {                 // pad to a cache line to avoid false sharing
        std::shared_mutex mutex;
        std::unordered_map<SparseKey, u64, SparseHash> sparse;  // 16-byte keys
        std::unordered_map<Key,       u64, FullHash>   full;    // 40-byte keys
    };

    // Called with the shard's unique_lock held, only for a brand-new key.
    // Global counter => dense ids 0,1,2,... with no gaps.
    u64 allocateAndPublish(const Key& k) {
        const u64 id = next_.fetch_add(1, std::memory_order_relaxed);
        slot(id) = k;                          // write reverse entry before the id escapes
        return id;
    }

    Key& slot(u64 id) {
        const std::size_t b = id >> kBlockBits;
        assert(b < kMaxBlocks && "KeyInterner capacity exceeded; raise kMaxBlocks");
        Key* block = blocks_[b].load(std::memory_order_acquire);
        if (!block) {
            Key* fresh = new Key[kBlockSize]();          // value-initialized
            if (blocks_[b].compare_exchange_strong(block, fresh,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire)) {
                block = fresh;                            // we published it
            } else {
                delete[] fresh;                           // somebody beat us to it
            }
        }
        return block[id & kBlockMask];
    }

    // ---------------- state ----------------
    std::array<Shard, kShards> shards_{};
    std::atomic<u64> next_{0};
    std::array<std::atomic<Key*>, kMaxBlocks> blocks_{};  // append-only, never freed/moved
};

// ---------------- public interface ----------------

inline KeyID mapToKeyID(Key k) {
    return KeyInterner::instance().intern(k);
}

inline Key KeyID::getOriginalKey() const {
    return KeyInterner::instance().lookup(id);
}