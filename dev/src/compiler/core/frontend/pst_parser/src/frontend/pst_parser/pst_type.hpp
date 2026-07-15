#pragma once

namespace pst {
	/**
	 * @brief Type of file parser, used for parsing context defaults
	 *
	 * Program - Top level is unordered
	 * Script - Top level is ordered
	 */
	enum class PSTType {
		Program,
		Script,
	};
}
