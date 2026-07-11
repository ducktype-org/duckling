#include "shadow_entry.hpp"

#include <vm/core/safe/exceptions.hpp>

#include <format>

namespace vm {
	ShadowEntry::ShadowEntry(const ShadowEntry& other):
		  last_write(other.last_write),
		  last_read_epoch(other.last_read_epoch),
		  last_read_vc(
			  other.last_read_vc ? std::make_unique<VectorClock>(*other.last_read_vc) : nullptr
		  ) {}

	ShadowEntry& ShadowEntry::operator=(const ShadowEntry& other) {
		if (this == &other) return *this;
		last_write      = other.last_write;
		last_read_epoch = other.last_read_epoch;
		last_read_vc
			= other.last_read_vc ? std::make_unique<VectorClock>(*other.last_read_vc) : nullptr;
		return *this;
	}

	void ShadowEntry::reportRace(const char* race_type, Epoch earlier, Epoch current) {
		throw exceptions::VMDataRaceException(std::format(
			"{}: {}@t{} vs {}@t{}",
			race_type,
			earlier.clock(),
			earlier.tid().asInt(),
			current.clock(),
			current.tid().asInt()
		));
	}
}
