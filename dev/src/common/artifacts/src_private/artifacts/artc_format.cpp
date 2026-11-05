// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "artc_format.hpp"

#include <base/misc/int_conv.hpp>

#include <charconv>
#include <limits>

namespace artifacts::detail {
	namespace {
		constexpr char ARTC_DELIM = ';';

		/// Reads a `.artc` body front to back; every read fails rather than overruns.
		class ArtcReader final {
			std::string_view rest;

		public:
			explicit ArtcReader(std::string_view content): rest(content) {}

			/// Reads one `ARTC_DELIM`-terminated decimal field that fits in `u32`.
			base::Optional<u32> number() {
				const auto delim = rest.find(ARTC_DELIM);
				if (delim == std::string_view::npos) return {};
				const auto field = rest.substr(0, delim);

				u64 value = 0;
				const auto [stop, ec]
					= std::from_chars(field.data(), field.data() + field.size(), value);
				// The whole field has to be the number: a trailing character means the file is
				// not what we think it is.
				if (ec != std::errc{} || stop != field.data() + field.size()) return {};
				if (value > std::numeric_limits<u32>::max()) return {};
				rest = rest.substr(delim + 1);
				return static_cast<u32>(value);
			}

			/// Reads `size` bytes, if that many are left.
			base::Optional<std::string_view> bytes(u32 size) {
				if (size > rest.size()) return {};
				const auto result = rest.substr(0, size);
				rest              = rest.substr(size);
				return result;
			}
		};
	}

	base::Optional<std::vector<StoredBlob>> parseArtc(std::string_view content) {
		ArtcReader reader(content);
		const auto count = reader.number();
		if (!count.has_value()) return {};

		std::vector<StoredBlob> blobs;
		for (u32 i = 0; i < count.value(); i++) {
			const auto name_size = reader.number();
			if (!name_size.has_value()) return {};
			const auto name = reader.bytes(name_size.value());
			if (!name.has_value()) return {};
			const auto data_size = reader.number();
			if (!data_size.has_value()) return {};
			const auto data = reader.bytes(data_size.value());
			if (!data.has_value()) return {};

			const auto* begin = reinterpret_cast<const byte*>(data.value().data());
			blobs.push_back({ .name = std::string(name.value()),
			                  .data = std::vector<byte>(begin, begin + data.value().size()) });
		}
		return blobs;
	}

	void writeArtc(
		std::ostream& out, const std::vector<std::pair<std::string_view, base::RawView>>& blobs
	) {
		out << blobs.size() << ARTC_DELIM;
		for (const auto& [name, data]: blobs) {
			out << name.size() << ARTC_DELIM << name;
			out << data.size() << ARTC_DELIM;
			out.write(
				reinterpret_cast<const char*>(data.getBegin()), base::safeIntConv<i64>(data.size())
			);
		}
	}
}
