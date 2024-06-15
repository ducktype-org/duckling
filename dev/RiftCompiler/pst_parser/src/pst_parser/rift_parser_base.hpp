#pragma once

#include <algorithm>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/parser_ref.hpp>
#include <token_parser_core/parser_state.hpp>
#include <token_parser_core/automatic.hpp>
#include <base/strongly_typed_id.hpp>
#include <utility>

namespace pst {
	class Import;

	using tpc::ParserBorrowRef;
	using tpc::ParserCBorrowRef;
	using tpc::ParserRef;

	STRONG_TYPEDEF_ID(PstID);

	class RiftElement: public tpc::Element {
	public:
		explicit RiftElement(const dia::SourcePosition& position): source_position(position){};

		[[nodiscard]]
		const dia::SourcePosition& getSourcePosition() const;

		[[nodiscard]]
		const std::vector<ParserCBorrowRef<RiftElement>>& getChildren() const {
			return children;
		}

		[[nodiscard]]
		const std::vector<base::c_borrow_ptr<tpc::Token>>& getTokens() const {
			return tokens;
		}

		[[nodiscard]]
		PstID getID() const {
			return id;
		}

	protected:
		dia::SourcePosition                         source_position;
		std::vector<base::c_borrow_ptr<tpc::Token>> tokens;
		std::vector<ParserCBorrowRef<RiftElement>> children;

		void addToken(const tpc::Token& token);
		void addToken(const base::unique_ptr<tpc::Token>& token);
		void addToken(base::c_borrow_ptr<tpc::Token> token);

		void addTokens(std::ranges::forward_range auto args) {
			//for(const tpc::MaybeToken& token:args) {
				//if (token) {
					//addToken(token.value());
				//}
			//}
		}

		void addChild(ParserCBorrowRef<RiftElement> child);
		template<typename T>
		void addChild(const ParserRef<T>& child) { 
			addChild(child.borrow());
		}

		void addChildren() {}
		template<typename T, typename... Ts>
		void addChildren(const ParserRef<T>& el, Ts... to_add) {
			addChild(el);
			addChildren(to_add...);
		}

		void setLastToken(dia::SourcePosition pos);
	private:
		PstID id = PstID::next();
	};

	using ImportType = tpc::ParserCBorrowRef<pst::Import>;

	class RiftParserState final: public tpc::AutomatedParserState<tpc::GenericAutomatic<RiftParserState>> {
		std::vector<ImportType> imports;

	public:
		RiftParserState(tpc::TokenStream&& tokens, dia::Logger& err):
			  tpc::AutomatedParserState<tpc::GenericAutomatic<RiftParserState>>(std::move(tokens), err) {}

		void addImport(const tpc::ParserCBorrowRef<pst::Import>& import);

		[[nodiscard]]
		auto extractState() && -> std::vector<ImportType> {
			return std::move(imports);
		}

		tpc::GenericAutomatic<RiftParserState> parse() override {
			return {*this};
		}
	};

}

ID_STD_HASH(::pst::PstID);
