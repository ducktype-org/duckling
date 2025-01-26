#pragma once

#include <base/ref.hpp>

namespace compiler::mir {
	struct MirLocal;

	/**
	 * @brief Reference to MIR Local variable data.
	 */
	using LocalRef = CRef<MirLocal>;

	/**
	 * @brief Optional reference to MIR Local variable data.
	 */
	using LocalMRef = MCRef<MirLocal>;
}
