#pragma once

#include <token_parser_core/parser_state.hpp>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/parser_ref.hpp>

namespace pst {
	class Import;

	using tpc::ParserRef;
	using tpc::ParserBorrowRef;
	using tpc::ParserCBorrowRef;
	using tpc::ErrorState;

	class RiftElement: public tpc::Element {};
	
	class RiftParserState: public tpc::ParserState {
		std::vector<tpc::ParserCBorrowRef<pst::Import>> imports;
		typedef decltype(imports) ImportType;

	public:
		RiftParserState(tpc::TokenStream&& tokens, tpc::ErrorState&& err): 
			tpc::ParserState(std::move(tokens), std::move(err)) {}

		void addImport(tpc::ParserCBorrowRef<pst::Import> import);
		const ImportType& getImports() const;
	};

}
