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

	template<typename State>
	class PSTAutomatic;

	class RiftElement: public tpc::Element {
	public:
		explicit RiftElement(const dia::SourcePosition& position):
			  source_position(position),
			  id(PstID::next()) {}

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

		/**
		 * @return Whether an element is just a statement aggregate.
		 * As of 30.05.2024 there are 3 statement aggregates:
		 * * CodeBlock
		 * * CodeBlockOrStmt
		 * * TopLevel
		 */
		[[nodiscard]]
		virtual bool isStatementAggregate() const {
			return false;
		}

		/**
		 * @return if element is a statements
		 */
		[[nodiscard]]
		virtual bool isStatement() const {
			return false;
		}

		template<typename X>
		friend class PSTAutomatic;

	protected:
		dia::SourcePosition                         source_position;
		std::vector<base::c_borrow_ptr<tpc::Token>> tokens;
		std::vector<ParserCBorrowRef<RiftElement>>  children;

		void addToken(const tpc::Token& token);
		void addToken(const base::unique_ptr<tpc::Token>& token);
		void addToken(base::c_borrow_ptr<tpc::Token> token);

		void addTokens(std::ranges::forward_range auto args) {
			// for(const tpc::MaybeToken& token:args) {
			// if (token) {
			// addToken(token.value());
			//}
			//}
		}

		template<std::derived_from<RiftElement> El>
		void addChild(base::Optional<ParserRef<El>>& el) {
			if (el) addChild(el.value().borrow());
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
}

ID_STD_HASH(::pst::PstID);
