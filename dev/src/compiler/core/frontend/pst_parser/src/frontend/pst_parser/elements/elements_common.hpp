#pragma once

#include "../access.hpp"
#include "../context_options.hpp"  // IWYU pragma: export
#include "../pst_state_forward.hpp"
#include "../utility.hpp"          // IWYU pragma: export
#include "elements_list.hpp"       // IWYU pragma: export

#include <base/types/ints.hpp>

#define PST_WHILE(condition) while (!state.isSkipping() && (condition))

#define PST_RETURN                          \
	if (state.isSkipping()) return nullptr; \
	return

namespace pst {
	using lang_def::Keyword;
	using lang_def::NamedOperator;
	using lang_def::Special;
	using lexer::Operator;

	class PstVisitor;

	namespace expr {
		class PstExprVisitor;
	}
}

namespace pst::internal {

	/**
	 * @brief State conditions used for parsing lists.
	 *
	 * @note They are defined in a class this way so that they can be template arguments.
	 */
	class Conditions final {
	public:
		Conditions() = delete;

		static bool isComma(const TokenStream& state, i64 fwd);
		static bool isSemicolon(const TokenStream& state, i64 fwd);
		static bool isSentinel(const TokenStream& state, i64 fwd);
		static bool isCurlyGroup(const TokenStream& state, i64 fwd);
		static bool isAssignOrSemicolon(const TokenStream& st, i64 fwd);
		static bool isAssignOrCommaOrEnd(const TokenStream& st, i64 fwd);
		static bool isAssign(const TokenStream& st, i64 fwd);

		/**
		 * @brief This is to differentiate blocks from template specification
		 */
		static bool isBlockGroup(const TokenStream& st, i64 fwd);
		static bool isImplementsOrBlockGroup(const TokenStream& st, i64 fwd);

		template<lang_def::Keyword key>
		static bool is(const TokenStream& st, i64 fwd) {
			return isKeyword(st, fwd, key);
		}

	private:
		static bool isKeyword(const TokenStream& st, i64 fwd, lang_def::Keyword key);
	};

	/**
	 * @brief These are helper static functions returning names that can be passed to templates.
	 */
	class NameGetters final {
	public:
		NameGetters() = delete;

		static std::string parameterList() { return "function parameter"; }

		static std::string nestedImportList() { return "nested import"; }

		static std::string flowPatternList() { return "flow pattern"; }

		static std::string returnList() { return "function return type"; }

		static std::string inheritanceList() { return "inheritance"; }

		static std::string attributeArgList() { return "attribute argument"; }

		static std::string classInitList() { return "initialization"; }

		static std::string callList() { return "call"; }

		static std::string templateList() { return "template"; }
	};

	/**
	 * @brief Borrow Iterator for Containers of Box (like std::vector<Box<T> >).
	 * It is needed because Box beeing Box cannot be "copied".
	 *
	 * @tparam ParserElement Element contained in the reference
	 * @tparam Container Container that of Boxs to the @p ParserElement .
	 */
	template<class ParserElement, class Container>
	class ForwardBorrowIterator final {
	private:
		using internal_iterator = typename Container::const_iterator;
		internal_iterator it;

	public:
		using value_type        = AccessLocked<ParserElement>;
		using iterator_category = std::random_access_iterator_tag;
		using difference_type   = typename internal_iterator::difference_type;
		using reference         = value_type;

		explicit ForwardBorrowIterator(): it() {}

		ForwardBorrowIterator(const ForwardBorrowIterator& other): it(other.it) {}

		ForwardBorrowIterator(const internal_iterator& other): it(other) {}

		value_type operator*() const { return it->give(); }

		value_type operator[](difference_type diff) const { return it[diff]->give(); }

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

		friend ForwardBorrowIterator operator+(
			const difference_type diff, const ForwardBorrowIterator& iter
		) {
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

#define DECLARE_CONST_ELEMENT_ITERATOR(container, TypeOfElement)                                \
	using const_iterator = internal::ForwardBorrowIterator<TypeOfElement, decltype(container)>; \
	const_iterator begin() const { return container.cbegin(); }                                 \
	const_iterator end() const { return container.cend(); }
};
