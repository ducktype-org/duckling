#pragma once

#include <artifacts/artifacts.hpp>

namespace compiler::driver {

	/**
	 * @brief Get the artifact collection where the compiler should save its own
	 * debug artifacts,such as IR dumps. Note that it is not for the debug info of the
	 * compiled code, but rather about the insights of the compiler itself.
	 *
	 * @return Ref<artifacts::ArtifactCollection>
	 */
	Ref<artifacts::ArtifactCollection> getDebugArtifactCollection();
}
