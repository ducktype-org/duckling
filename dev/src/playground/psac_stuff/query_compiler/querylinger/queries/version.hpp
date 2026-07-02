#pragma once

#include <base/types/ints.hpp>

#include <query_framework/query_int.hpp>

#include <string>
#include <string_view>

namespace ql {
	inline constexpr std::string_view QUERYLINGER_VERSION = "Querylinger version -0.05";

	/// Trivial query proving the framework wiring end to end; returns the version string.
	struct VersionKey {
		bool operator==(const VersionKey&) const = default;

		[[nodiscard]] u64 queryUnstablePerfectHash() const { return 0; }
	};

	DECLARE_QUERY(QueryCompilerVersion, VersionKey, std::string, ({ .uses_qresult = false }));

	/// How many times the version query was actually computed (cache misses); test hook.
	usize versionProvideCalls();
}
