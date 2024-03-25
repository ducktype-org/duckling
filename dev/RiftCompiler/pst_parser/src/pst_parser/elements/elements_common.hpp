#include "../rift_parser_base.hpp"

namespace pst::detail {
	class Conditions {
	public:
		Conditions() = delete;

		static bool isComma(const RiftParserState& state, usize fwd) {
			return state.ctokens().is(rift_def::Operator::Comma, fwd);
		}

		static bool isSentinel(const RiftParserState& state, usize fwd) {
			return state.ctokens().is(lexer::Token::Type::Sentinel, fwd);
		}

		static bool isCurlyGroup(const RiftParserState& state, usize fwd) {
			return state.ctokens().isBracketGroup(lexer::Token::BracketType::Curly, fwd);
		}
	};

	template<class ParserElement, class Container>
	class forwardBorrowIterator {
	private:
		using internal_iterator = Container::const_iterator;
		internal_iterator it;

	public:
		using value_type        = ParserCBorrowRef<ParserElement>;
		using iterator_category = std::random_access_iterator_tag;
		using difference_type   = internal_iterator::difference_type;
		using reference         = value_type;

		explicit forwardBorrowIterator(): it() {}

		forwardBorrowIterator(const forwardBorrowIterator& other): it(other.it) {}

		forwardBorrowIterator(const internal_iterator& other): it(other) {}

		explicit forwardBorrowIterator(const ParserRef<ParserElement>* ptr): it(ptr) {}

		value_type operator*() const { return it->borrow(); }

		value_type operator[](difference_type diff) const { return it[diff]->borrow(); }

		forwardBorrowIterator& operator++() {
			++it;
			return *this;
		}

		forwardBorrowIterator operator++(int) { return iterator(it++); }

		forwardBorrowIterator& operator--() {
			--it;
			return *this;
		}

		forwardBorrowIterator operator--(int) { return iterator(it--); }

		forwardBorrowIterator& operator+=(difference_type diff) {
			it += diff;
			return *this;
		}

		forwardBorrowIterator operator+(const difference_type diff) const { return it + diff; }

		friend forwardBorrowIterator
			operator+(const difference_type diff, const forwardBorrowIterator& iter) {
			return iter.it + diff;
		}

		forwardBorrowIterator& operator-=(difference_type diff) {
			it -= diff;
			return *this;
		}

		forwardBorrowIterator operator-(const difference_type diff) const { return it - diff; }

		difference_type operator-(const forwardBorrowIterator& other) const {
			return it - other.it;
		}

		bool operator==(const forwardBorrowIterator& other) const { return it == other.it; }

		auto operator<=>(const forwardBorrowIterator& other) const { return it <=> other.it; }
	};

#define DECLARE_CONST_ELEMENT_ITERATOR(container, TypeOfElement)                              \
	using const_iterator = detail::forwardBorrowIterator<TypeOfElement, decltype(container)>; \
	const_iterator begin() const { return container.cbegin(); }                               \
	const_iterator end() const { return container.cend(); }
};
