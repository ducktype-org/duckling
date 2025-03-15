#include "type_eval.hpp"
#include <helios/hout/elements/expr.hpp>

#include <query_framework/query_impl.hpp>

#include <helios/hout/elements/query_hout_of_expr.hpp>
#include <helios/hout/visitors.hpp>

namespace compiler::helios {

	struct IMPLEMENT_QUERY(EvalExprToType, TypeEval_Result) {
		/**
		 * State representing failure to short-path
		 */
		struct CouldNotEvalShortPath {};

		using ShortPathResult
			= errors::HResult<tsh::AbstractType, CouldNotEvalShortPath, errors::Failed>;

		/**
		 * A visitor to extract types from simple expression fast (i.e. short path it).
		 * @note It might be changed to virtual function on expr in the future for performance.
		 * For now it is kept as a visitor for code simplicity.
		 * @todo Handle reference specification. #608
		 */
		struct ShortPathVisitor: code::HoutExprVisitorPanicky {
			query::Context& ctx;

			ShortPathVisitor(query::Context& ctx): ctx(ctx) {}

			base::Optional<ShortPathResult> result;
			bool                            failed = false;

			void output(ShortPathResult res) {
				CORE_ASSERT(result.empty(), "ShortPathVisitor already has a result");
				result.emplace(res);
			}

			void visitLiteralTypeExpr(const code::LiteralTypeExpr& expr) override {
				output(expr.value_type);
			}

			void visitIdentifierExpr(const code::IdentifierExpr& expr) override {
				// @todo this only works if the identifier is a class.
				// this should be changed in the future
				auto type = ctx.query<QueryTypeFromDefinition>({ expr.symbol });
				if (type->hasValue())
					output(type->value());
				else
					failed = true;
			}

			void visitTupleTypeConstructorExpr(const code::TupleTypeConstructorExpr& expr
			) override {
				std::vector<tsh::SymbolType<>> subtypes;
				for (auto& sub_type: expr.elements) {
					// should we here short-path or not?
					auto sub_type_result = evalHoutExprToType(ctx, sub_type.ref());
					if (sub_type_result.hasError()) {
						failed = true;
						return;
					} else {
						// @todo: False here means all subtypes of a tuple are immutable.
						// this is likely wrong, we will have to change it with
						// type info, expression type, component type refactor
						subtypes.emplace_back(sub_type_result.value());
					}
				}
				output(ctx.query<tsh::QueryTupleType>({ subtypes }));
			}

			void visitVariantTypeConstructorExpr(const code::VariantTypeConstructorExpr& expr
			) override {
				std::vector<tsh::SymbolType<>> subtypes;
				for (auto& sub_type: expr.subtypes) {
					// should we here short-path or not?
					auto sub_type_result = evalHoutExprToType(ctx, sub_type.ref());
					if (sub_type_result.hasError()) {
						failed = true;
						return;
					} else {
						subtypes.emplace_back(sub_type_result.value());
					}
				}
				output(ctx.query<tsh::QueryVariantType>({ subtypes }));
			}

			void visitParenthesisExpr(const code::ParenthesisExpr& expr) override {
				expr.inner->acceptVisitor(*this);
			}
		};

		static auto evalHoutExprToType(query::Context& ctx, CRef<code::Expr> expr) -> PResult {
			ShortPathVisitor visitor(ctx);
			expr->acceptVisitor(visitor);

			if (visitor.failed) return errors::HError(errors::Failed());

			auto short_path_result = visitor.result.value();

			if (short_path_result.hasError()) {
				variant_match(short_path_result.error()) {
					variant_case(errors::Failed, _) { return errors::HError(errors::Failed()); }
					variant_case(CouldNotEvalShortPath, _) {
						throw base::NotYetImplemented("Comp time when short-path eval failed");
					}
					variant_default { CORE_PANIC("Unhandled error in EvalExprToType"); }
				}
			}

			return tsh::SymbolType<>{
				short_path_result.value(),
				tsh::ReferenceKind::Direct,
				tsh::Mutability::Mutable,
			};
		}

		static auto provide(Context& ctx, QKey key) -> PResult {
			auto parsed = ctx.query<QueryHoutOfExpr>({ key.element });
			if (parsed.hasError()) return errors::HError(parsed.error());

			// note: this assert might be changed to a compiler error in the future:
			CORE_ASSERT(
				parsed.value()->expression_type.getType().getKind() == tsh::Kind::Meta,
				"Expression provided to EvalExprToType has non-meta type."
			);

			return evalHoutExprToType(ctx, parsed.value().ref());
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(EvalExprToType);
}
