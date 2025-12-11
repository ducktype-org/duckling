#pragma once

#include <concurrent/collections/hash_map.hpp>
#include <concurrent/workers/worker_data.hpp>

#include <base/collections/stable_hashmap.hpp>

using HashMap  = base::StableHashMap<u64, u64>;
using CHashMap = concurrent::HashMap<u64, u64>;

void work(HashMap& map, concurrent::WDRef worker_data);

void cWork(CHashMap& map, concurrent::WDRef worker_data);
