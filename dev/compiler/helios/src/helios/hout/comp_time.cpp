#include "comp_time.hpp"
#include "elements/expr.hpp"

#include <query_framework/query_impl.hpp>

#include "elements/query_hout_of_expr.hpp"
#include "visitors.hpp"

namespace compiler::helios {

    struct IMPLEMENT_QUERY(EvalExprToType, TypeEvalResult) {

        using ShortPathResult = errors::HResult<tsh::TypeInfo, CouldNotEvalShortPath, errors::Failed>;

        /**
         * A visitor to extract types from simple expression fast (i.e. short path it).
         * @note It might be changed to virtual function on expr in the future for performance.
         * For now it is kept as a visitor for code simplicity.
         */
        struct ShortPathVisitor: code::HoutExprVisitorPanicky {
            query::Context& ctx;
            ShortPathVisitor(query::Context& ctx): ctx(ctx) {}

            base::Optional<ShortPathResult> result;
            bool failed = false;

            void output(ShortPathResult res) {
                CORE_ASSERT(result.empty(), "ShortPathVisitor already has a result");
                result.emplace(res);
            }

            void visitKeywordExpr(const code::KeywordExpr& expr) override {
                // note: this logic will be moved to hout-creation, 
                // once we have TypeLiteral hout element

				using Keyword = lang_def::Keyword;
				switch (expr.keyword) {
				case Keyword::i8:
                    output(ctx.query<tsh::QueryIntegralType>({8, true}));
                    break;
				case Keyword::i16:
                    output(ctx.query<tsh::QueryIntegralType>({16, true}));
                    break;
				case Keyword::i32:
                    output(ctx.query<tsh::QueryIntegralType>({32, true}));
                    break;
				case Keyword::i64:
                    output(ctx.query<tsh::QueryIntegralType>({64, true}));
                    break;
				case Keyword::i128:
                    output(ctx.query<tsh::QueryIntegralType>({128, true}));
                    break;
				case Keyword::u8:
                    output(ctx.query<tsh::QueryIntegralType>({8, false}));
                    break;
				case Keyword::u16:
                    output(ctx.query<tsh::QueryIntegralType>({16, false}));
                    break;
				case Keyword::u32:
                    output(ctx.query<tsh::QueryIntegralType>({32, false}));
                    break;
				case Keyword::u64:
                    output(ctx.query<tsh::QueryIntegralType>({64, false}));
                    break;
				case Keyword::u128:
                    output(ctx.query<tsh::QueryIntegralType>({128, false}));
                    break;
				case Keyword::f32:
                    output(ctx.query<tsh::QueryFloatType>(32));
                    break;
				case Keyword::f64:
                    output(ctx.query<tsh::QueryFloatType>(64));
                    break;
				case Keyword::f80:
                    output(ctx.query<tsh::QueryFloatType>(80));
                    break;
				case Keyword::Char:
                    output(ctx.query<tsh::QueryCharType>({ }));
                    break;
				case Keyword::Bool:
                    output(ctx.query<tsh::QueryBoolType>({ }));
                    break;

				default:
				    CORE_PANIC("KeywordExpr not yet handled by HoutIsTypeExprVisitor");
				}
			}

            void visitIdentifierExpr(const code::IdentifierExpr& expr) override {
                // @todo this only works is the identifier is a class.
                // this should be changed in the future
                auto type = ctx.query<QueryTypeFromDefinition>({expr.symbol});
                if (type->hasValue()) {
                    output(type->value());
                } else {
                    failed = true;
                }
            }

			void visitTupleConstructorExpr(const code::TupleConstructorExpr& expr) override {
				// @todo hout2.0: this is incorrect, type of this expression
				// will be a meta in the future
				output(expr.type_desc.getType()); 
			}
			
			void visitVariantConstructorExpr(const code::VariantConstructorExpr& expr) override {
				// @todo hout2.0: this is incorrect, type of this expression
				// will be a meta in the future
				output(expr.type_desc.getType()); 
			}

            void visitParenthesisExpr(const code::ParenthesisExpr& expr) override {
                expr.inner->acceptVisitor(*this);
            }

        };



        static auto provide(Context& ctx, QKey key) -> PResult {
            // todo: query type from expr
            // get type from virtual functions
            auto parsed = ctx.query<QueryHoutOfExpr>({ key.expr });
            if (parsed.hasError()) {
                return errors::HError(parsed.error());
            }
            
            ShortPathVisitor visitor(ctx);
            parsed.value()->acceptVisitor(visitor);

            if (visitor.failed) {
                return errors::HError(errors::Failed());
            }

            auto short_path_result = visitor.result.value();

            if (short_path_result.hasError()) {
                variant_match(short_path_result.error()) {
                    variant_case(errors::Failed, _) {
                        return errors::HError(errors::Failed());
                    }
                    variant_case(CouldNotEvalShortPath, _) {
                        throw base::NotYetImplemented("Comp time when short-path eval failed");
                    }
                    variant_default {
                        CORE_PANIC("Unhandler error in EvalExprToType");
                    }
                }
            }

            return short_path_result.value();
        }

        QUERY_AUTO_CACHE_COPY
    };

    QUERY_IMPLEMENTATION_BOILERPLATE(EvalExprToType);


     base::HashT KeyOf_EvalExprToType::customPerfectHash() const {
        return expr->getID().asInt();
     }
}
