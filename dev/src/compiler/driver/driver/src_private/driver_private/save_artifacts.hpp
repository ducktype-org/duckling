#pragma once

namespace compiler::driver {
	/**
	 * @brief Save compilation artifacts to disk: serializes the query graph and metadata, reclaims
	 * on-disk caches orphaned by this compilation, and flushes everything to disk.
	 */
	void saveArtifacts();
}
