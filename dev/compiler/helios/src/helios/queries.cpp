#include "queries.hpp"

#include <query_framework/query_entry_point.hpp>
#include <query_framework/query_impl.hpp>

#include <base/stable_hashmap.hpp>
#include <pst_parser/pst_visitor.hpp>

#include <base/exceptions.hpp>
#include "scopes/scopes.hpp"
#include "hout/elements.hpp"

#include "symbols/symbols.hpp"

namespace compiler::helios {

	struct IMPLEMENT_QUERY(QueryTopLevelEntities, HOUTUnit) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// go over all to level symbols and get theirs hout
			// store it in some vector or something
			// lookup all and stuff

			auto main_file_root_scope = queryRootScopeOfMainModuleFile(ctx, key);

			auto symbols_in_module_root = ctx.query<QuerySymbolsInScope>(main_file_root_scope);

			HOUTUnit out;

			// grab constants:
			for (auto sym: *symbols_in_module_root)
				if (kind(sym) == SymbolKind::Const) out.glob_data.emplace_back(sym, ctx);


			// grab functions:
			for (auto sym: *symbols_in_module_root)
				if (kind(sym) == SymbolKind::Function)
					out.functions.push_back(ctx.query<QueryCodeOFFun>(sym));

			return out;
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTopLevelEntities);

	struct IMPLEMENT_QUERY(QueryCodeOFFun, HOUTFunction) {
		/**
		 * @brief Query extension to get hout CodeBlock from pst::CodeBlock or pst::CodeBlockOrStmt
		 * Might be changed into query in the future
		 */
		template<class Container>
		static auto queryCodeOfCodeBlock(query::Context& ctx, const Container& container) {
			auto            scope = ctx.query<QueryPrimaryCodeScopeFor>({ container });
			code::CodeBlock block(scope, {});
			for (const auto& stmt: *container) {
				HoutStmtMaker stmt_maker(ctx);
				stmt->acceptVisitor(stmt_maker);
				if (not stmt_maker.empty)
					block.statements.emplace_back(std::move(stmt_maker.out.value()));
			}
			return block;
		}

		struct HoutStmtMaker final: public pst::PstVisitorPanicky {
			query::Context&                       ctx;
			bool                                  empty = false;
			base::Optional<base::Box<code::Stmt>> out;

			HoutStmtMaker(query::Context& ctx): ctx(ctx) {}

			template<class T>
			ScopeID scopeOf(const T& element) {
				return ctx.query<QueryPrimaryCodeScopeFor>({ MCRef<pst::LangElement>(&element) });
			}

			// @TODO: visits for all valid stmt-s

			// @TODO: some stuff in here are also symbols
			// "query symbol in scope" should be able to just work
			// and provide correct symbols for lookup, but same care
			// has to be taken, to ensure consistency between this code and scope states.

			template<class T>
			void output(T&& value) {
				this->out.emplace(makeBox<std::remove_reference_t<T>>(std::forward<T>(value)));
			}

			void visitReturn(const pst::Return& stmt) override {
				if (auto val = stmt.getValue()) {
					auto expr = ctx.query<QueryHoutOfExpr>({ val.value() })
					                .expect("Not handling errors here yet... (return expr)");
					output(code::ReturnStmt(scopeOf(stmt), std::move(expr)));
				} else {
					output(code::VoidReturnStmt(scopeOf(stmt)));
				}
			}

			void visitAlias(const pst::Alias&) override { empty = true; }

			void visitUsing(const pst::Using&) override { empty = true; }

			void visitExprStmt(const pst::ExprStmt& stmt) override {
				auto expr = ctx.query<QueryHoutOfExpr>({ MCRef<pst::ExprElement>(stmt.getExpr()) })
				                .expect("Not handling errors here yet... (ExprStmt)");
				output(code::ExprStmt(scopeOf(stmt), std::move(expr)));
			}

			void visitIf(const pst::If& stmt) override {
				// Get scopes:
				auto outer_scope = scopeOf(stmt);

				// in the future we must also handle here different if-s variants
				// for example: `if (let a = ...) {}`.
				auto condition = ctx.query<QueryHoutOfExpr>({ stmt.getCondition() })
				                     .expect("Not handling errors here yet");

				auto body = queryCodeOfCodeBlock(ctx, stmt.getBody());

				output(code::IfStmt(outer_scope, std::move(condition), std::move(body)));
			}

			void visitVariable(const pst::Variable& stmt) override {
				// @TODO: do something with mut/immut

				// @TODO: error handling

				auto symbol = ctx.query<QuerySymbolOfSTMT>({ MCRef<pst::Stmt>(&stmt) });

				auto symbol_type = ctx.query<QueryTypeOfSymbol>(symbol)->expect(
					"Handling errors is not supported in HOUT yet"
				);

				// for now initial value is assumed to always be present:
				// this will probably change:
				auto initial_value = ctx.query<QueryHoutOfExpr>({ stmt.getValue() })
				                         .expect(" (variable initial value)");

				output(
					code::VariableStmt(scope(symbol), std::move(initial_value), symbol_type, symbol)
				);
			}
		};

		struct HOUTFunctionMaker final: public pst::PstVisitorPanicky {
			query::Context& ctx;
			SymID           original_symbol;

			base::Optional<HOUTFunction> out;

			HOUTFunctionMaker(query::Context& ctx, SymID symbol):
				  ctx(ctx),
				  original_symbol(symbol) {}

			// @TODO: make failure more explicit

			void visitFun(const pst::Fun& stmt) final {
				// @TODO: create function here...
				// - create types, attributes, flags, ...
				// @TODO: rest, flags, attributes, etc

				HOUTFunction output(original_symbol, ctx);

				// Scope of function itself:
				// this scope contains all "function declaration" symbols like parameters
				// auto outer_scope
				// 	= ctx.query<QueryPrimaryCodeScopeFor>({ MCRef<pst::LangElement>(&stmt) });


				// body:

				auto fun_body = stmt.getBody();

				code::CodeBlock function_body = queryCodeOfCodeBlock(ctx, fun_body);
				output.content.body
					= std::make_shared<const code::CodeBlock>(std::move(function_body));


				// parameters:

				std::vector<code::Parameter> parameters;

				for (auto param: *stmt.getParams()) {
					auto param_symbol = ctx.query<QuerySymbolOfSTMT>({ param });
					auto param_name   = name(param_symbol);
					auto param_type   = ctx.query<QueryTypeOfSymbol>({ param_symbol });

					auto value = param->getValue();

					if (param_type->hasError()) {
						// we just fail here, because we can't continue without type
						return;
					}

					if (value.empty()) {
						parameters.emplace_back(
							param_name, param_type->value(), std::nullopt, param_symbol
						);
					} else {
						auto initial_value = ctx.query<QueryHoutOfExpr>({ value.value() });

						if (initial_value.hasError()) {
							// we just fail here, because we can't continue without correct initial expression
							return;
						}

						parameters.emplace_back(
							param_name,
							param_type->value(),
							std::move(initial_value.value()),
							param_symbol
						);
					}
				}

				output.content.parameters
					= std::make_shared<const std::vector<code::Parameter>>(std::move(parameters));

				this->out.emplace(std::move(output));
			}
		};

		static auto provide(Context& ctx, QKey key) -> PResult {
			CORE_ASSERT(
				kind(key) == SymbolKind::Function, "Function creation called on non-function symbol"
			);

			HOUTFunctionMaker func_maker(ctx, key);
			stmt(key)->acceptVisitor(func_maker);

			return func_maker.out.value();
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryCodeOFFun);
}
