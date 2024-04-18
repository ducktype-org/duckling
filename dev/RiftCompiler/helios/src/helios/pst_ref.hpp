#pragma once

#include <token_parser_core/parser_ref.hpp>

namespace compiler::helios {

	template<typename Element>
	using PstRef = tpc::ParserCBorrowRef<Element>;


}
