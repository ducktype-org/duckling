#pragma once

#include <token_parser_core/parser_ref.hpp>

namespace compiler::helios {

	/**
	 * @brief Borrow reference to node in PST
	 *
	 * @tparam Element Element the reference points to
	 */
	template<typename Element>
	using PstRef = tpc::ParserCBorrowRef<Element>;

}
