#include "debug_info_io.hpp"

#include <ser/base/all.hpp>
#include <ser/ser.hpp>
#include <ser/std/all.hpp>

#include <algorithm>
#include <cstddef>
#include <expected>
#include <istream>
#include <iterator>
#include <optional>
#include <ostream>
#include <span>
#include <utility>
#include <vector>

namespace debug_info {

	namespace {
		/**
		 * @brief Puts the offset-keyed vectors in order, so that one DebugInfo always produces
		 * the same bytes.
		 */
		void sortByKey(DebugInfo& info) {
			for (auto& [name, function]: info.functions) {
				std::ranges::sort(
					function.parameter_indexes_to_metadata,
					{},
					&std::pair<u64, VariableMetadata>::first
				);
				std::ranges::sort(
					function.instr_offsets_to_metadata,
					{},
					&std::pair<u64, InstructionMetadata>::first
				);
				std::ranges::sort(
					function.instr_offsets_to_variable_init,
					{},
					&std::pair<u64, VariableMetadata>::first
				);
			}
		}

		/**
		 * @brief The read-side half of the same rule: which vector came back out of order, if
		 * any.
		 * @note Checked here rather than inside a `ser` hook because the caller gets a message
		 * naming the vector, which an error code off the wire could not carry.
		 */
		std::optional<std::string> findUnsorted(const DebugInfo& info) {
			for (const auto& [name, function]: info.functions) {
				if (!std::ranges::is_sorted(
						function.parameter_indexes_to_metadata,
						{},
						&std::pair<u64, VariableMetadata>::first
					))
					return "parameter_indexes_to_metadata is not sorted by index";
				if (!std::ranges::is_sorted(
						function.instr_offsets_to_metadata,
						{},
						&std::pair<u64, InstructionMetadata>::first
					))
					return "instr_offsets_to_metadata is not sorted by offset";
				if (!std::ranges::is_sorted(
						function.instr_offsets_to_variable_init,
						{},
						&std::pair<u64, VariableMetadata>::first
					))
					return "instr_offsets_to_variable_init is not sorted by offset";
			}
			return std::nullopt;
		}
	}  // namespace

	std::expected<DebugInfo, std::string> loadFromStream(std::istream& in) {
		const std::vector<char> raw{ std::istreambuf_iterator<char>(in),
			                         std::istreambuf_iterator<char>() };

		// A stream that is not debug info is an answer, not a failure: nothing throws out of
		// here, and the caller decides what to do without one.
		auto info = ::ser::read<DebugInfo>(std::span<const std::byte>{
			reinterpret_cast<const std::byte*>(raw.data()), raw.size() });
		if (!info) return std::unexpected(info.err().message());

		DebugInfo value = std::move(*info).take();
		if (const auto unsorted = findUnsorted(value); unsorted.has_value())
			return std::unexpected(*unsorted);

		return value;
	}

	void saveToStream(const DebugInfo& info, std::ostream& out) {
		// Sorted on a copy, because the caller's object is const and the order is a property of
		// the file rather than of the object.
		DebugInfo sorted = info;
		sortByKey(sorted);

		// Writing what we are holding cannot fail on the data, so a code here is a bug.
		std::vector<std::byte> bytes;
		if (const auto r = ::ser::write(bytes, sorted); !r)
			CORE_PANIC("Failed to serialize debug info: ", r.err().message());

		out.write(
			reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())
		);
	}

}  // namespace debug_info
