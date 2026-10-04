#pragma once

namespace artifacts {
	/**
	 * @brief If true, the root ArtifactCollection validates the stored build id
	 * against the current compiler's BUILD_ID and wipes the directory on mismatch.
	 *
	 * Compile-time constant - flip locally when debugging artifact formats across
	 * compiler builds.
	 */
	constexpr bool CHECK_BUILD_ID = true;
}
