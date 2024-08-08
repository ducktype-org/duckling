#pragma once

#include "../rift_parser_state.hpp"

namespace pst::detail {

	/**
	 * @brief State conditions used for parsing lists.
	 *
	 * @note They are defined in a class this way so that they can be template arguments.
	 */
	class Conditions {
	public:
		Conditions() = delete;

		static bool isComma(const RiftParserState& state, i64 fwd) {
			return state[fwd].is(rift_def::Special::Comma);
		}

		static bool isSemicolon(const RiftParserState& state, i64 fwd) {
			return state[fwd].is(rift_def::Special::Semicolon);
		}

		static bool isSentinel(const RiftParserState& state, i64 fwd) {
			return state[fwd].is(lexer::Token::Type::Sentinel);
		}

		static bool isCurlyGroup(const RiftParserState& state, i64 fwd) {
			return state[fwd].isBracketGroup(lexer::Token::BracketType::Curly);
		}

		static bool isAssignOrSemicolon(const RiftParserState& st, i64 fwd) {
			return st[fwd].is(rift_def::Operator::Assign)
			    || st[fwd].is(rift_def::Special::Semicolon);
		}

		static bool isAssign(const RiftParserState& st, i64 fwd) {
			return st[fwd].is(rift_def::Operator::Assign);
		}

		static bool isBlockGroup(const RiftParserState& st, i64 fwd) {
			return st[fwd].isBracketGroup(lexer::Token::Curly)
			    && not st[fwd - 1].is(rift_def::Operator::Colon);
		}

		static bool isImplementsOrBlockGroup(const RiftParserState& st, i64 fwd) {
			return st[fwd].is(rift_def::Keyword::Implements)
			    || (st[fwd].isBracketGroup(lexer::Token::Curly)
			        && not st[fwd - 1].is(rift_def::Operator::Colon));
		}
	};

	class NameGetters {
	public:
		NameGetters() = delete;

		static std::string parameterList() { return "function parameter"; }

		static std::string returnList() { return "function return type"; }

		static std::string inheritanceList() { return "inheritance"; }

		static std::string attributeArgList() { return "attribute argument"; }

		static std::string classInitList() { return "initialization"; }
	};

	/**
	 * @brief Borrow Iterator for Containers of ParserRef (like std::vector<ParserRef<T> >).
	 * It is needed because ParserRef beeing base::unique_ptr cannot be "copied".
	 * This iterator returns ParserCBorrowRef when dereferenced
	 * which is a wrapper for base::borrow_ptr.
	 *
	 * @tparam ParserElement Element contained in the reference
	 * @tparam Container Container that of parserRefs to the @p ParserElement .
	 */
	template<class ParserElement, class Container>
	class ForwardBorrowIterator {
	private:
		using internal_iterator = typename Container::const_iterator;
		internal_iterator it;

	public:
		using value_type        = ParserCBorrowRef<ParserElement>;
		using iterator_category = std::random_access_iterator_tag;
		using difference_type   = typename internal_iterator::difference_type;
		using reference         = value_type;

		explicit ForwardBorrowIterator(): it() {}

		ForwardBorrowIterator(const ForwardBorrowIterator& other): it(other.it) {}

		ForwardBorrowIterator(const internal_iterator& other): it(other) {}

		explicit ForwardBorrowIterator(const ParserRef<ParserElement>* ptr): it(ptr) {}

		value_type operator*() const { return it->borrow(); }

		value_type operator[](difference_type diff) const { return it[diff]->borrow(); }

		ForwardBorrowIterator& operator++() {
			++it;
			return *this;
		}

		ForwardBorrowIterator operator++(int) { return iterator(it++); }

		ForwardBorrowIterator& operator--() {
			--it;
			return *this;
		}

		ForwardBorrowIterator operator--(int) { return iterator(it--); }

		ForwardBorrowIterator& operator+=(difference_type diff) {
			it += diff;
			return *this;
		}

		ForwardBorrowIterator operator+(const difference_type diff) const { return it + diff; }

		friend ForwardBorrowIterator
			operator+(const difference_type diff, const ForwardBorrowIterator& iter) {
			return iter.it + diff;
		}

		ForwardBorrowIterator& operator-=(difference_type diff) {
			it -= diff;
			return *this;
		}

		ForwardBorrowIterator operator-(const difference_type diff) const { return it - diff; }

		difference_type operator-(const ForwardBorrowIterator& other) const {
			return it - other.it;
		}

		bool operator==(const ForwardBorrowIterator& other) const { return it == other.it; }

		auto operator<=>(const ForwardBorrowIterator& other) const { return it <=> other.it; }
	};

#define DECLARE_CONST_ELEMENT_ITERATOR(container, TypeOfElement)                              \
	using const_iterator = detail::ForwardBorrowIterator<TypeOfElement, decltype(container)>; \
	const_iterator begin() const { return container.cbegin(); }                               \
	const_iterator end() const { return container.cend(); }
};
