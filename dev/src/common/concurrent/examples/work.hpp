#pragma once

#include <concurrent/collections/hash_map.hpp>
#include <base/collections/stable_hashmap.hpp>

using HashMap = base::StableHashMap<u64, u64>;
using CHashMap = concurrent::HashMap<u64, u64>;

void work(HashMap& map);
void cWork(CHashMap& map);
