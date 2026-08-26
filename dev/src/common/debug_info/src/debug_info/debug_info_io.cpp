#include "debug_info_io.hpp"

#include <ser/base/all.hpp>
#include <ser/ser.hpp>
#include <ser/std/all.hpp>

#include <cstddef>
#include <expected>
#include <istream>
#include <iterator>
#include <ostream>
#include <span>
#include <utility>
#include <vector>

namespace debug_info {

	std::expected<DebugInfo, std::string> loadFromStream(std::istream& in) {
		const std::vector<char> raw{ std::istreambuf_iterator<char>(in),
			                         std::istreambuf_iterator<char>() };

		// A stream that is not debug info is an answer, not a failure: nothing throws out of
		// here, and the caller decides what to do without one.
		auto info = ::ser::read<DebugInfo>(
			std::span<const std::byte>{ reinterpret_cast<const std::byte*>(raw.data()), raw.size() },
			DI_STREAM
		);
		if (!info) return std::unexpected(info.err().message());

		return std::move(*info).take();
	}

	void saveToStream(const DebugInfo& info, std::ostream& out) {
		// Writing what we are holding cannot fail on the data, so a code here is a bug.
		std::vector<std::byte> bytes;
		if (const auto r = ::ser::write(bytes, info, DI_STREAM); !r)
			CORE_PANIC("Failed to serialize debug info: ", r.err().message());

		out.write(
			reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())
		);
	}

}  // namespace debug_info
