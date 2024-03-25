#include "../rift_parser_base.hpp"

namespace pst::detail {
	class Conditions {
	public:
		Conditions() = delete;
		static bool isComma(const RiftParserState& state, usize fwd) {
			return state.ctokens().is(rift_def::Operator::Comma, fwd);
		};
		static bool isSentinel(const RiftParserState& state, usize fwd) {
			return state.ctokens().is(lexer::Token::Type::Sentinel, fwd);
		};
		static bool isCurlyGroup(const RiftParserState& state, usize fwd) {
			return state.ctokens().isBracketGroup(lexer::Token::BracketType::Curly, fwd);
		};
	};

	template<class ParserElement, class Container>
	class forwardBorrowIterator {
	private:
		using internal_iterator = Container::const_iterator;
		internal_iterator it;
	public:

		using value_type = ParserCBorrowRef<ParserElement>;
		using iterator_category = std::forward_iterator_tag;
		using difference_type = internal_iterator::difference_type;
		using reference = value_type;

		explicit forwardBorrowIterator(): it() {}
		forwardBorrowIterator(const forwardBorrowIterator& other): it(other.it) {}
		forwardBorrowIterator(const internal_iterator& other): it(other) {}

		value_type operator*() const {
			return it->borrow();
		}

		forwardBorrowIterator& operator++() {
			++it;
			return this;
		}
		forwardBorrowIterator operator++(int) {
			return iterator(it++);
		}

		bool operator==(const forwardBorrowIterator& other) const {
			return it == other.it;
		}
	};
};