#pragma once

#include "../meta.hpp"
#include "code_block.hpp"

#include <base/variant.hpp>

namespace pst {
	class CodeBlockOrStmtIterator;

	/**
	 * @brief Code Block or Statement.
	 */
	class CodeBlockOrStmt final: public NotStmt {
		NAMED_CHILD_OPT(stmt, Stmt);
		NAMED_CHILD_OPT(code_block, CodeBlock);

	public:
		enum class Type { SingleStmt, CodeBlock };

		explicit CodeBlockOrStmt(const dia::SourcePosition& position): NotStmt(position) {
			this->element_kind = ElementKind::CodeBlockOrStmt;
		}

		static MBox<CodeBlockOrStmt> parse(
			LangParserState& state, CodeBlock::CodeBlockType code_block_order_type
		);
		~CodeBlockOrStmt() final = default;
		void     dprint(std::ostream& out) const final;
		HashAlg& addElementDataToStableHash(HashAlg&) const override;

		using const_iterator = CodeBlockOrStmtIterator;
		[[nodiscard]]
		const_iterator begin() const;
		[[nodiscard]]
		const_iterator end() const;

		[[nodiscard]]
		Type getType() const;

		/**
		 * @brief Get the stored statement. Panics if is in code block state.
		 */
		[[nodiscard]]
		AccessLocked<Stmt> getStmt() const;

		[[nodiscard]]
		std::string elementType() const override {
			return "Code Block or Statement";
		}

		[[nodiscard]]
		bool isStatementAggregate() const final {
			return true;
		}
	};

	/**
	 * @brief A special iterator that can iterate over bot a code block or a single statement
	 */
	class CodeBlockOrStmtIterator final {
	private:
		using code_block_iterator = typename CodeBlock::const_iterator;
		using stmt_iterator       = std::pair<AccessLocked<Stmt>, long>;
		std::variant<code_block_iterator, stmt_iterator> it;

		void checkDifferent(const CodeBlockOrStmtIterator& other) const {
			if ((std::holds_alternative<code_block_iterator>(it)
			     && std::holds_alternative<stmt_iterator>(other.it))
			    || (std::holds_alternative<code_block_iterator>(other.it)
			        && std::holds_alternative<stmt_iterator>(it))) {
				CORE_PANIC("Bad code block operation");
			}
		}

	public:
		using value_type        = AccessLocked<Stmt>;
		using iterator_category = std::random_access_iterator_tag;
		using difference_type   = typename code_block_iterator::difference_type;
		using reference         = value_type;

		explicit CodeBlockOrStmtIterator(): it() {}

		CodeBlockOrStmtIterator(const CodeBlockOrStmtIterator& other) = default;

		CodeBlockOrStmtIterator(const code_block_iterator& other): it(other) {}

		CodeBlockOrStmtIterator(AccessLocked<Stmt> other, int diff = 0):
			  it(stmt_iterator{ other, diff }) {}

		value_type operator*() const {
			variant_match(it) {
				variant_case(code_block_iterator, cb_it) { return *cb_it; }
				variant_case(stmt_iterator, stmt_it) {
					if (stmt_it.second != 0)
						CORE_PANIC("Bad code block iterator dereference");
					else
						return stmt_it.first;
				}
			}
			CORE_UNREACHABLE();
		}

		value_type operator[](difference_type diff) const {
			variant_match(it) {
				variant_case(code_block_iterator, cb_it) { return *(cb_it + diff); }
				variant_case(stmt_iterator, stmt_it) {
					if (stmt_it.second + diff == 0)
						return stmt_it.first;
					else
						CORE_PANIC("Bad code block iterator dereference");
				}
			}
			CORE_UNREACHABLE();
		}

		CodeBlockOrStmtIterator& operator++() {
			variant_match(it) {
				variant_case(code_block_iterator, cb_it) {
					++cb_it;
					return *this;
				}
				variant_case(stmt_iterator, stmt_it) {
					++stmt_it.second;
					return *this;
				}
			}
			CORE_UNREACHABLE();
		}

		CodeBlockOrStmtIterator operator++(int) {
			CodeBlockOrStmtIterator cpy = *this;
			return ++cpy;
		}

		CodeBlockOrStmtIterator& operator--() {
			variant_match(it) {
				variant_case(code_block_iterator, cb_it) {
					--cb_it;
					return *this;
				}
				variant_case(stmt_iterator, stmt_it) {
					--stmt_it.second;
					return *this;
				}
			}
			CORE_UNREACHABLE();
		}

		CodeBlockOrStmtIterator operator--(int) {
			CodeBlockOrStmtIterator cpy = *this;
			return --cpy;
		}

		CodeBlockOrStmtIterator& operator+=(difference_type diff) {
			variant_match(it) {
				variant_case(code_block_iterator, cb_it) {
					cb_it += diff;
					return *this;
				}
				variant_case(stmt_iterator, stmt_it) {
					stmt_it.second += diff;
					return *this;
				}
			}
			CORE_UNREACHABLE();
		}

		CodeBlockOrStmtIterator operator+(const difference_type diff) const {
			CodeBlockOrStmtIterator cpy = *this;
			return cpy += diff;
		}

		friend CodeBlockOrStmtIterator operator+(
			const difference_type diff, const CodeBlockOrStmtIterator& iter
		) {
			return iter + diff;
		}

		CodeBlockOrStmtIterator& operator-=(difference_type diff) { return (*this) += -diff; }

		CodeBlockOrStmtIterator operator-(const difference_type diff) const {
			CodeBlockOrStmtIterator cpy = *this;
			return cpy -= diff;
		}

		difference_type operator-(const CodeBlockOrStmtIterator& other) const {
			checkDifferent(other);
			variant_match(it) {
				variant_case(code_block_iterator, cb_it) {
					auto other_it = std::get<code_block_iterator>(other.it);
					return cb_it - other_it;
				}
				variant_case(stmt_iterator, stmt_it) {
					auto other_it = std::get<stmt_iterator>(other.it);
					return stmt_it.second - other_it.second;
				}
			}
			CORE_UNREACHABLE();
		}

		bool operator==(const CodeBlockOrStmtIterator& other) const {
			checkDifferent(other);
			variant_match(it) {
				variant_case(code_block_iterator, cb_it) {
					auto other_it = std::get<code_block_iterator>(other.it);
					return cb_it == other_it;
				}
				variant_case(stmt_iterator, stmt_it) {
					auto other_it = std::get<stmt_iterator>(other.it);
					return stmt_it.second == other_it.second;
				}
			}
			CORE_UNREACHABLE();
		}

		auto operator<=>(const CodeBlockOrStmtIterator& other) const {
			checkDifferent(other);
			variant_match(it) {
				variant_case(code_block_iterator, cb_it) {
					auto other_it = std::get<code_block_iterator>(other.it);
					return cb_it <=> other_it;
				}
				variant_case(stmt_iterator, stmt_it) {
					auto other_it = std::get<stmt_iterator>(other.it);
					return stmt_it.second <=> other_it.second;
				}
			}
			CORE_UNREACHABLE();
		}
	};

}
