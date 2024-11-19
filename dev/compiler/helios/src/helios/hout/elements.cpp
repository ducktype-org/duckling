#include "elements.hpp"

#include <cmath>
#include <query_framework/query_impl.hpp>
#include <base/variant.hpp>
#include "../scopes/scopes.hpp"
#include <base/unique_pointer.hpp>
#include "base/box.hpp"
#include "base/exceptions.hpp"
#include "base/optional.hpp"
#include "base/str_utils.hpp"
#include "helios/helios_errors.hpp"
#include "helios/helios_result.hpp"
#include "helios/lookup_result.hpp"
#include "helios/scope_symbol_id.hpp"
#include "helios/symbols/symbols.hpp"
#include "lang_definitions/key_spec_op.hpp"
#include "pst_parser/elements/hierarchy/not_statements.hpp"
#include "pst_parser/pst_expr_visitor.hpp"
#include "query_framework/query_int.hpp"
#include "typesystem/higher/kind.hpp"
#include "typesystem/higher/queries/types.hpp"
#include "typesystem/higher/type_desc.hpp"
#include "typesystem/higher/type_info.hpp"
#include "typesystem/higher/types.hpp"
#include "typesystem/higher/value_category.hpp"
#include "visitors.hpp"

namespace compiler::helios::code {
	namespace {
		tsh::TypeInfo getTypeOfKeyword(query::Context& ctx, lang_def::Keyword keyword) {
			const static auto BUILTINS = std::unordered_map<lang_def::Keyword, tsh::TypeInfo>{
				{ lang_def::Keyword::f80, ctx.query<::tsh::QueryFloatType>(80) },
				{ lang_def::Keyword::f64, ctx.query<::tsh::QueryFloatType>(64) },
				{ lang_def::Keyword::f32, ctx.query<::tsh::QueryFloatType>(32) },

				{ lang_def::Keyword::i128, ctx.query<::tsh::QueryIntegralType>({ 128, true }) },
				{ lang_def::Keyword::i64, ctx.query<::tsh::QueryIntegralType>({ 64, true }) },
				{ lang_def::Keyword::i32, ctx.query<::tsh::QueryIntegralType>({ 32, true }) },
				{ lang_def::Keyword::i16, ctx.query<::tsh::QueryIntegralType>({ 16, true }) },
				{ lang_def::Keyword::i8, ctx.query<::tsh::QueryIntegralType>({ 8, true }) },

				{ lang_def::Keyword::u128, ctx.query<::tsh::QueryIntegralType>({ 128, false }) },
				{ lang_def::Keyword::u64, ctx.query<::tsh::QueryIntegralType>({ 64, false }) },
				{ lang_def::Keyword::u32, ctx.query<::tsh::QueryIntegralType>({ 32, false }) },
				{ lang_def::Keyword::u16, ctx.query<::tsh::QueryIntegralType>({ 16, false }) },
				{ lang_def::Keyword::u8, ctx.query<::tsh::QueryIntegralType>({ 8, false }) },

				{ lang_def::Keyword::Bool, ctx.query<::tsh::QueryBoolType>({}) },
			};
			return BUILTINS.at(keyword);
		}

		tsh::TypeDesc<>
			getTypeDescOfTuple(query::Context& ctx, const std::vector<base::Box<Expr>>& elements) {
			std::vector<tsh::ComponentType> tuple_components;
			tuple_components.reserve(elements.size());

			for (auto&& tuple_subtype: elements) {
				// @NOTE: False here means all subtypes of a tuple are immutable.
				tuple_components.emplace_back(tuple_subtype->type_desc.getType(), false);
			}

			// @EXPR: Should ValueCategory be literal?
			return tsh::TypeDesc<>(
				ctx.query<tsh::QueryTupleType>({ tuple_components }),
				tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			);
		}

		tsh::TypeDesc<> getTypeDescOfVariant(
			query::Context& ctx, const std::vector<base::Box<Expr>>& subtypes
		) {
			std::vector<tsh::TypeInfo> variant_subtypes;
			variant_subtypes.reserve(subtypes.size());

			for (auto&& subtype: subtypes)
				variant_subtypes.emplace_back(subtype->type_desc.getType());

			// @EXPR: Should ValueCategory be a literal?
			return tsh::TypeDesc<>(
				ctx.query<tsh::QueryVariantType>({ variant_subtypes }),
				tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			);
		}

		base::Optional<std::vector<base::Box<Expr>>> getVariantExpressions(base::Ref<Expr> expr) {
			if (auto variant = dynamic_cast<VariantConstructorExpr*>(expr.get()); variant)
				return std::move(variant->subtypes);
			return {};
		}

		base::Box<VariantConstructorExpr> constructVariantFrom(
			query::Context& ctx, ScopeID scope, base::Box<Expr> lhs, base::Box<Expr> rhs
		) {
			std::vector<base::Box<Expr>> all_subtypes;

			for (auto&& expr: std::array{ std::move(lhs), std::move(rhs) }) {
				auto subtypes = getVariantExpressions(expr.refMut());
				if (subtypes)
					for (auto&& subtype: *subtypes) all_subtypes.emplace_back(std::move(subtype));
				else
					all_subtypes.emplace_back(std::move(expr));
			}

			return base::Box(new VariantConstructorExpr(ctx, scope, std::move(all_subtypes)));
		}
	}

// visitors:
#define STMT_VISITOR(type) \
	void type::acceptVisitor(HoutStmtVisitor& visitor) const { visitor.visit##type(*this); }
#define EXPR_VISITOR(type) \
	void type::acceptVisitor(HoutExprVisitor& visitor) const { visitor.visit##type(*this); }

	STMT_VISITOR(ReturnStmt);
	STMT_VISITOR(VoidReturnStmt);
	STMT_VISITOR(ExprStmt);
	STMT_VISITOR(IfStmt);
	STMT_VISITOR(VariableStmt);

	EXPR_VISITOR(LiteralValueExpr);
	EXPR_VISITOR(IdentifierExpr);
	EXPR_VISITOR(BinaryOperatorExpr);
	EXPR_VISITOR(UnaryOperatorExpr);
	EXPR_VISITOR(TupleConstructorExpr);
	EXPR_VISITOR(VariantConstructorExpr);
	EXPR_VISITOR(ParenthesisExpr);
	EXPR_VISITOR(KeywordExpr);
	EXPR_VISITOR(LinkedIdentifierExpr);

	constexpr usize INDENT_SIZE = 4;

	void addIndent(std::ostream& out, usize indent) {
		out << std::string().append(indent * INDENT_SIZE, ' ');
	}

	void ReturnStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "return ";
		this->value->debugPrint(out);
		out << "\n";
	}

	void VoidReturnStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "void return\n";
	}

	void ExprStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "do ";
		expr->debugPrint(out);
		out << "\n";
	}

	void IfStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "if (";
		condition->debugPrint(out);
		out << ") {\n";
		for (const auto& stmt: body.statements) stmt->debugPrint(out, indent + 1);
		addIndent(out, indent);
		out << "}\n";
	}

	LiteralValueExpr::LiteralValueExpr(query::Context& ctx, ScopeID scope, i64 value):
		  Expr(
			  scope,
			  tsh::TypeDesc<>(
				  // @TODO: Select type of expression based on type of literal.
				  ctx.query<tsh::QueryIntegralType>({ 64 }),
				  tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  )
		  ),
		  value(value) {}

	void LiteralValueExpr::debugPrint(std::ostream& out) const { out << std::to_string(value); }

	void VariableStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "var ";
		out << name(this->helios_symbol).strView();
		out << " : ";

		// this might not be correct:?
		out << this->type.getType().toString();
		out << " = ";
		this->initial_value.value()->debugPrint(out);

		out << ";\n";
	}

	IdentifierExpr::IdentifierExpr(query::Context& ctx, ScopeID scope, SymID symbol):
		  Expr(
			  scope,
			  tsh::TypeDesc<>(
				  ctx.query<QueryTypeOfSymbolOrDefinition>(symbol)->expect(
					  "Handling errors in HOUT is not supported yet"
				  ),
				  tsh::ValueCategory(tsh::primaryCategoryOfSymbol(symbol))
			  )
		  ),
		  symbol(symbol) {}

	void IdentifierExpr::debugPrint(std::ostream& out) const {
		out << base::strConcat("(Symbol ", name(symbol), " (", symbol.customPerfectHash(), "))");
	}

	BinaryOperatorExpr::BinaryOperatorExpr(
		query::Context& ctx,
		ScopeID         scope,
		lexer::Operator op,
		base::Box<Expr> lhs,
		base::Box<Expr> rhs
	):
		  Expr(
			  scope,
			  tsh::TypeDesc<>(
				  // @TODO: Select type of expression based on result type of the operation.
				  ctx.query<tsh::QueryIntegralType>({ 64 }),
				  tsh::ValueCategory(tsh::PrimaryCategory::Temporary)
			  )
		  ),
		  op(op),
		  lhs(std::move(lhs)),
		  rhs(std::move(rhs)) {}

	void BinaryOperatorExpr::debugPrint(std::ostream& out) const {
		lhs->debugPrint(out);
		out << base::strConcat(op.str());
		rhs->debugPrint(out);
	}

	errors::HResult<i64, errors::Failed> LiteralValueExpr::evaluateValue(query::Context&) const {
		return value;
	}

	errors::HResult<i64, errors::Failed> BinaryOperatorExpr::evaluateValue(query::Context& ctx
	) const {
		UNPACK_RESULT(i64 lhs_value =, lhs->evaluateValue(ctx));
		UNPACK_RESULT(i64 rhs_value =, rhs->evaluateValue(ctx));
		if (op.value == "+")
			return lhs_value + rhs_value;
		else if (op.value == "-")
			return lhs_value - rhs_value;
		else if (op.value == "*")
			return lhs_value * rhs_value;
		else if (op.value == "/")
			return lhs_value / rhs_value;
		else if (op.value == "%")
			return lhs_value % rhs_value;
		else if (op.value == "**")
			return std::pow(lhs_value, rhs_value);
		CORE_PANIC("Unknown operator");
	}

	void ParenthesisExpr::debugPrint(std::ostream& out) const {
		out << "(";
		inner->debugPrint(out);
		out << ")";
	}

	ParenthesisExpr::ParenthesisExpr(query::Context&, ScopeID scope, base::Box<Expr> inner):
		  Expr(scope, inner->type_desc),
		  inner(std::move(inner)) {}

	errors::HResult<i64, errors::Failed> ParenthesisExpr::evaluateValue(query::Context& ctx) const {
		return inner->evaluateValue(ctx);
	}

	errors::HResult<i64, errors::Failed> IdentifierExpr::evaluateValue(query::Context& ctx) const {
		return *ctx.query<QueryConstValueOf>(symbol);
	}

	KeywordExpr::KeywordExpr(query::Context& ctx, ScopeID scope, lang_def::Keyword keyword):
		  Expr(
			  scope,
			  tsh::TypeDesc<>(
				  getTypeOfKeyword(ctx, keyword), tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  )
		  ),
		  keyword(keyword) {}

	errors::HResult<i64, errors::Failed> KeywordExpr::evaluateValue(query::Context&) const {
		throw base::NotYetImplemented("Evaluation of keyword values is not implemented yet");
	}

	void KeywordExpr::debugPrint(std::ostream& out) const {
		out << lang_def::keywordToStr(keyword).strView();
	}

	TupleConstructorExpr::TupleConstructorExpr(
		query::Context& ctx, ScopeID scope, std::vector<base::Box<Expr>> elements
	):
		  Expr(scope, getTypeDescOfTuple(ctx, elements)),
		  elements(std::move(elements)) {}

	void TupleConstructorExpr::debugPrint(std::ostream& out) const {
		out << "(";
		for (bool add_comma = false; auto&& e: elements) {
			if (add_comma) out << ", ";
			e->debugPrint(out);
			add_comma = true;
		}
		out << ")";
	}

	errors::HResult<i64, errors::Failed>
		TupleConstructorExpr::evaluateValue(query::Context&) const {
		throw base::NotYetImplemented("Evaluation of tuple values is not implemented yet");
	}

	void VariantConstructorExpr::debugPrint(std::ostream& out) const {
		out << "(";
		for (bool add_pipe = false; auto&& subtype: subtypes) {
			if (add_pipe) out << " | ";
			subtype->debugPrint(out);
			add_pipe = true;
		}
		out << ")";
	}

	errors::HResult<i64, errors::Failed>
		VariantConstructorExpr::evaluateValue(query::Context&) const {
		throw base::NotYetImplemented("Evaluation of variant values is not implemented yet");
	}

	VariantConstructorExpr::VariantConstructorExpr(
		query::Context& ctx, ScopeID scope, std::vector<base::Box<Expr>> subtypes
	):
		  Expr(scope, getTypeDescOfVariant(ctx, subtypes)),
		  subtypes(std::move(subtypes)) {}

	void LinkedIdentifierExpr::debugPrint(std::ostream& out) const {
		for (bool add_dot = false; auto&& symbol: symbols) {
			if (add_dot) out << ".";
			out << name(symbol).str();
			add_dot = true;
		}
	}

	errors::HResult<i64, errors::Failed> LinkedIdentifierExpr::evaluateValue(query::Context& ctx
	) const {
		return *ctx.query<QueryConstValueOf>(symbols.back());
	}

	LinkedIdentifierExpr::LinkedIdentifierExpr(
		query::Context& ctx, ScopeID scope, SymbolList symbols
	):
		  Expr(
			  scope,
			  tsh::TypeDesc<>(
				  ctx.query<QueryTypeOfSymbolOrDefinition>(symbols.back())
					  ->expect("Not handling errors here yet"),
				  tsh::ValueCategory(tsh::PrimaryCategory::Literal)
			  )
		  ),
		  symbols(std::move(symbols)) {}

	UnaryOperatorExpr::UnaryOperatorExpr(
		ScopeID scope, lexer::Operator op, bool prefix, base::Box<Expr> expr
	):
		  Expr(scope, expr->type_desc),
		  op(op),
		  prefix(prefix),
		  expr(std::move(expr)) {}

	void UnaryOperatorExpr::debugPrint(std::ostream& out) const {
		if (prefix) {
			out << op.str();
			expr->debugPrint(out);
		} else {
			expr->debugPrint(out);
			out << op.str();
		}
	}

	errors::HResult<i64, errors::Failed> UnaryOperatorExpr::evaluateValue(query::Context& ctx
	) const {
		UNPACK_RESULT(i64 expr_value =, expr->evaluateValue(ctx));
		if (op.value == "-" && prefix) return -expr_value;
		return errors::HError(errors::Failed());
		// throw base::NotYetImplemented(
		// 	base::strConcat("Not handling prefix:", prefix, " of operator ", op.str())
		// );
	}

	struct HoutIsTypeExprVisitor: public HoutExprVisitor {
		// @TODO czy to nie powinno być roboione na poziomie HELIOS'a, żeby sprawdzać, czy
		// użytkownik nie próbuje użyć typu jako wartości lub odwrotnie? Czyli żeby rzucić błędem,
		// jeśli napisze: `let a: i32 + 13 = 20;`
		explicit HoutIsTypeExprVisitor(query::Context& ctx, ScopeID scope):
			  ctx(ctx),
			  scope(scope) {}

		bool            is_type_expr = false;
		query::Context& ctx;
		ScopeID         scope;

		void visitLiteralValueExpr(const LiteralValueExpr&) override { is_type_expr = false; }

		void visitIdentifierExpr(const IdentifierExpr& expr) override { testSymbol(expr.symbol); }

		void visitBinaryOperatorExpr(const BinaryOperatorExpr&) override { is_type_expr = false; }

		void visitUnaryOperatorExpr(const UnaryOperatorExpr&) override { is_type_expr = false; }

		void visitParenthesisExpr(const ParenthesisExpr& expr) override {
			HoutIsTypeExprVisitor vis(ctx, scope);
			expr.inner->acceptVisitor(vis);
			is_type_expr = vis.is_type_expr;
		}

		void visitKeywordExpr(const KeywordExpr& expr) override {
			using Keyword = lang_def::Keyword;
			switch (expr.keyword) {
			case Keyword::None:
			case Keyword::True:
			case Keyword::False:
				is_type_expr = false;
				break;
			case Keyword::i8:
			case Keyword::i16:
			case Keyword::i32:
			case Keyword::i64:
			case Keyword::i128:
			case Keyword::u8:
			case Keyword::u16:
			case Keyword::u32:
			case Keyword::u64:
			case Keyword::u128:
			case Keyword::f32:
			case Keyword::f64:
			case Keyword::f80:
			case Keyword::Char:
			case Keyword::Bool:
			case Keyword::Vec:
			case Keyword::Set:
			case Keyword::Dict:
			case Keyword::Array:
				is_type_expr = true;
				break;
			default:
				throw base::LogicError("KeywordExpr not yet handled by HoutIsTypeExprVisitor");
			}
		}

		void visitTupleConstructorExpr(const TupleConstructorExpr& tuple) override {
			iterOverExprs(tuple.elements);
		}

		void visitVariantConstructorExpr(const VariantConstructorExpr& variant) override {
			iterOverExprs(variant.subtypes);
		}

		void visitLinkedIdentifierExpr(const LinkedIdentifierExpr& val) override {
			testSymbol(val.symbols.back());
		}

	private:
		void testSymbol(SymID symbol) {
			auto type = *ctx.query<QueryTypeOfSymbol>(symbol);
			if (type.hasError()) {
				// this is a class?
				is_type_expr = true;
			} else {
				switch (type.value().getKind()) {
				case tsh::Kind::Meta:
					is_type_expr = true;
					break;
				default:
					is_type_expr = false;
				}
			}
		}

		void iterOverExprs(const std::vector<base::Box<Expr>>& expressions) {
			is_type_expr = true;
			for (auto& el: expressions) {
				HoutIsTypeExprVisitor vis(ctx, scope);
				el->acceptVisitor(vis);
				if (!vis.is_type_expr) is_type_expr = false;
			}
		}
	};

	/**
	 * @brief Tries to extract a resulting symbol from hout expression.
	 */
	struct HoutResultingSymbolListVisitor: public HoutExprVisitor {
		explicit HoutResultingSymbolListVisitor(query::Context& ctx, ScopeID scope):
			  ctx(ctx),
			  scope(scope) {}

		query::Context& ctx;
		ScopeID         scope;

		base::Optional<SymbolList> symbols;

		void visitBinaryOperatorExpr(const BinaryOperatorExpr&) override {
			// ctx.query<tsh::internal::QueryInterfaceOfClass>()
			throw base::NotYetImplemented("Cannot evaluate symbol after binary operators");
		}

		void visitIdentifierExpr(const IdentifierExpr& val) override {
			symbols = SymbolList{ val.symbol };
		}

		void visitKeywordExpr(const KeywordExpr&) override {
			throw base::NotYetImplemented("Cannot evaluate symbol from Keywords");
		}

		void visitLiteralValueExpr(const LiteralValueExpr&) override {
			throw base::NotYetImplemented("Cannot evaluate symbol from literal values");
		}

		void visitParenthesisExpr(const ParenthesisExpr& val) override {
			HoutResultingSymbolListVisitor vis(ctx, scope);
			val.inner->acceptVisitor(vis);
			symbols = vis.symbols;
		}

		void visitTupleConstructorExpr(const TupleConstructorExpr&) override {
			throw base::NotYetImplemented("Cannot evaluate symbol from tuple");
		}

		void visitVariantConstructorExpr(const VariantConstructorExpr&) override {
			throw base::NotYetImplemented("Cannot evaluate symbol from tuple");
		}

		void visitUnaryOperatorExpr(const UnaryOperatorExpr&) override {
			throw base::NotYetImplemented("Cannot evaluate symbol after unary operators");
		}

		void visitLinkedIdentifierExpr(const LinkedIdentifierExpr& val) override {
			symbols = val.symbols;
		}
	};

	struct PstExprToHoutExprVisitor: public pst::PstExprVisitorPanicky {
		explicit PstExprToHoutExprVisitor(query::Context& ctx, ScopeID scope):
			  ctx(ctx),
			  scope(scope) {}

		query::Context& ctx;
		ScopeID         scope;

		base::Optional<base::Box<Expr>> node;

		void visitExprValue(const pst::expr::ExprValue& stmt) override {
			// @TODO: Change literal value from i64 to something more appropriate.
			node = base::Box(new LiteralValueExpr(ctx, scope, std::stoi(stmt.getValue().str())));
		}

		void visitBinaryOperator(const pst::expr::BinaryOperator& stmt) override {
			PstExprToHoutExprVisitor lhs(ctx, scope);
			PstExprToHoutExprVisitor rhs(ctx, scope);
			stmt.getLeftOperand()->acceptVisitor(lhs);
			stmt.getRightOperand()->acceptVisitor(rhs);

			if (lhs.node && rhs.node) {
				HoutIsTypeExprVisitor lhs_vis_expr(ctx, scope);
				HoutIsTypeExprVisitor rhs_vis_expr(ctx, scope);
				lhs.node.value()->acceptVisitor(lhs_vis_expr);
				rhs.node.value()->acceptVisitor(rhs_vis_expr);

				if (lhs_vis_expr.is_type_expr != rhs_vis_expr.is_type_expr) {
					// @TODO: I think this is not "NotYetImplemented", but rather
					// an invalid syntax, so a compilation error should be raised.
					throw base::NotYetImplemented("Not implemented.");
				}
				if (stmt.getOperator().str()[0] == '|' && lhs_vis_expr.is_type_expr
				    && rhs_vis_expr.is_type_expr) {
					node = constructVariantFrom(
						ctx, scope, std::move(*lhs.node), std::move(*rhs.node)
					);
				} else {
					node = base::Box(new BinaryOperatorExpr(
						ctx, scope, stmt.getOperator(), std::move(*lhs.node), std::move(*rhs.node)
					));
				}
			}
		}

		void visitChainExpr(const pst::expr::ChainExpr& stmt) override {
			// @TODO: Add a compiler log or some kind of information if lookup fails.

			auto literal_expr = Expr::fromPST(ctx, scope, stmt.getLiteral());
			if (!literal_expr) {
				// Report an error?
				return;
			}

			HoutResultingSymbolListVisitor resulting_symbol_vis(ctx, scope);
			literal_expr.value()->acceptVisitor(resulting_symbol_vis);

			CORE_ASSERT(resulting_symbol_vis.symbols, "Failed to get symbols");

			SymbolList looked_up_symbol = std::move(resulting_symbol_vis.symbols.value());

			for (auto&& el: stmt.getChain()) {
				auto pst_access = dynamic_cast<pst::expr::Access*>(el.get());
				CORE_ASSERT(pst_access, "Not handling non-AccessExprs yet");
				CORE_ASSERT(pst_access->getType() == ".", "Not handling .? access operator yet");

				std::cout << "Lookup in: " << name(looked_up_symbol.back()).strView() << " "
						  << pst_access->getName().value.strView() << std::endl;
				auto new_symbols = *ctx.query<QueryLookupInSymbol>(
					{ looked_up_symbol.back(), pst_access->getName().value, true }
				);

				auto new_symbols_single = new_symbols.getAsSingle();
				CORE_ASSERT(
					new_symbols_single.hasValue(), "Access failed because couldn\'t getAsSingle()"
				);

				looked_up_symbol.insert(
					looked_up_symbol.end(),
					new_symbols_single.value().begin(),
					new_symbols_single.value().end()
				);
			}
			dealiasSymbolList(ctx, looked_up_symbol).optValue().ifValue([&](auto&& dealiased) {
				node = base::Box(new LinkedIdentifierExpr(ctx, scope, std::move(dealiased)));
			});
		}

		void visitRoundExpr(const pst::expr::RoundExpr& stmt) override {
			PstExprToHoutExprVisitor vis(ctx, scope);
			stmt.getInner()->acceptVisitor(vis);
			if (vis.node) node = base::Box(new ParenthesisExpr(ctx, scope, std::move(*vis.node)));
		}

		void visitIdentifierLiteral(const pst::expr::IdentifierLiteral& stmt) override {
			auto&& sym_list
				= *ctx.query<QueryLookupInScopeAndParents>({ scope, stmt.getName().value, true });

			auto res = sym_list.getAsSingle();
			if (res.hasError()) {
				// Report an error
				return;
			}

			dealiasSymbolList(ctx, res.value())
				.optValue()
				.ifValue([&](const SymbolList& dealiased) {
					node = base::Box(new IdentifierExpr(ctx, scope, dealiased.back()));
				});
		}

		void visitKeywordLiteral(const pst::expr::KeywordLiteral& stmt) override {
			node = base::Box(new KeywordExpr(ctx, scope, stmt.getKeyword()));
		}

		void visitComma(const pst::expr::Comma& stmt) override {
			std::vector<Box<Expr>> expressions;
			for (auto&& ex: stmt.getExpressions()) {
				PstExprToHoutExprVisitor vis(ctx, scope);
				ex->acceptVisitor(vis);
				if (!vis.node) {
					// Error has occurred.
					return;
				}
				vis.node.ifValue([&](auto&& b) { expressions.emplace_back(std::move(b)); });
			}

			node = base::Box(new TupleConstructorExpr(ctx, scope, std::move(expressions)));
		}

		void visitSuffixOperator(const pst::expr::SuffixOperator& stmt) override {
			// @NOTE: This is a mockup
			PstExprToHoutExprVisitor vis(ctx, scope);
			stmt.getExpr()->acceptVisitor(vis);
			if_opt_some(vis.node, expr) {
				node = base::Box(
					new UnaryOperatorExpr(scope, stmt.getOperator(), false, std::move(expr))
				);
			}
		}

		void visitPrefixOperator(const pst::expr::PrefixOperator& stmt) override {
			// @NOTE: This is a mockup
			PstExprToHoutExprVisitor vis(ctx, scope);
			stmt.getExpr()->acceptVisitor(vis);
			if_opt_some(vis.node, expr) {
				node = base::Box(
					new UnaryOperatorExpr(scope, stmt.getOperator(), true, std::move(expr))
				);
			}
		}
	};

	errors::HResult<base::Box<Expr>, errors::Failed>
		Expr::fromPST(query::Context& ctx, ScopeID scope, const PstRef<pst::ExprElement> root) {
		std::cerr << "\nExpr: \n";
		root->debugPrint(std::cerr);
		std::cerr << '\n';

		PstExprToHoutExprVisitor visitor(ctx, scope);
		root->acceptVisitor(visitor);

		if_opt_some(visitor.node, expr) return std::move(expr);
		return errors::HError(errors::Failed());
	}
}

namespace compiler::helios {

	struct
		IMPLEMENT_QUERY(QueryHoutOfExpr, errors::HResult<base::Box<code::Expr> COMMA errors::Failed>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			ScopeID expr_scope = ctx.query<QueryPrimaryCodeScopeFor>({ key.expr });
			return code::Expr::fromPST(ctx, expr_scope, key.expr);
			CORE_PANIC("Not implemented yet...");
		}

		// @TODO: perhaps add cache
		// Right now its not that simple since QueryHoutOfExpr
		// has to return different expresion tree (unique_ptr).
		// It might not be a problem in the future, so for now it is left without cache.

		static auto load(QKey) -> LoadResult { return {}; }

		static auto store(QKey, PResult res, query::ACD) -> QResult { return res; }
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryHoutOfExpr);

	base::HashT KeyOf_QueryHoutOfExpr::customPerfectHash() const {
		auto hash_1 = this->expr->getID().asInt();

		return hash_1;
	}

};
