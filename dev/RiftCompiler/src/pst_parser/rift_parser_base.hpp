#pragma once

#include <token_parser_core/base_element.hpp>
#include <token_parser_core/parser_ref.hpp>
#include <token_parser_core/parser_state.hpp>
#include <utility>

namespace pst {
	class Import;

	using tpc::ParserBorrowRef;
	using tpc::ParserCBorrowRef;
	using tpc::ParserRef;

	class RiftElement: public tpc::Element {
	public:
		explicit RiftElement(dia::SourcePosition position): source_position(std::move(position)){};

		[[nodiscard]]
		const dia::SourcePosition& getSourcePosition() const;

	private:
		dia::SourcePosition source_position;
	};

	class RiftParserState: public tpc::ParserState {
		std::vector<tpc::ParserCBorrowRef<pst::Import>> imports;
		typedef decltype(imports)                       ImportType;

	public:
		RiftParserState(tpc::TokenStream&& tokens, dia::ErrorState&& err):
			  tpc::ParserState(std::move(tokens), std::move(err)) {}

		void addImport(const tpc::ParserCBorrowRef<pst::Import>& import);
		[[nodiscard]]
		const ImportType& getImports() const;
	};

}
