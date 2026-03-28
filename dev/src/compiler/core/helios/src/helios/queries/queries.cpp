#include "queries.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/module_tree/queries.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/field.hpp>
#include <frontend/pst_parser/elements/hierarchy/class_elements/method.hpp>
#include <frontend/pst_parser/elements/hierarchy/declarations/all_declarations.hpp>
#include <frontend/pst_parser/elements/hierarchy/not_statements/all_not_statements.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/hout.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/symbols/query_class_of_member.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>
#include <helios_private/errors/errors.hpp>
#include <helios_private/hout_code_generation/class_constructors.hpp>
#include <helios_private/hout_code_generation/default_constructors.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>
#include <typesystem/higher/queries/types.hpp>
#include <typesystem/higher/symbol_type.hpp>
#include <typesystem/higher/type_interface.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/query_errors.hpp>
#include <query_framework/standard_query/query_impl.hpp>

namespace compiler::helios {

	struct IMPLEMENT_QUERY(QueryModuleHOUT, query::QResult<HOUTUnit>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// We want to continue gathering other entities
			// even if some function queries fail,
			// so we store in this variable whether any failure occurred,
			// and return failure at the end if so.
			bool is_failed = false;

			MCRef<std::vector<ScopeID>> scopes_to_process;

			auto scopes_in_module = ctx.query<QueryScopesInModule>(key);
			variant_match(scopes_in_module->value) {
				variant_case(QueryScopesInModuleValue::Success, success) {
					scopes_to_process = &success.scopes;
				}
				variant_case(QueryScopesInModuleValue::Failure, failure) {
					scopes_to_process = &failure.partial_scopes;
					is_failed
						= true;  // we mark the whole query as failed, even if we have some scopes
				}
				variant_default { CORE_UNREACHABLE(); }
			}

			HOUTUnit out;

			std::vector<query::TaskHandle> scheduled_tasks;
			std::vector<SymID>             class_symbols;
			std::set<SymID>                default_ctors;

			auto register_ctor_if_needed = [&](SymID sym) {
				const auto& symbol_type = ctx.query<QueryTypeOfSymbol>(sym)->valueOrThrow();

				// Don't insert any constructors if a type is trivially zero-initializable or not
				// default constructible.
				if (symbol_type.isTriviallyZeroInitializable(ctx)) return;
				if (!symbol_type.isDefaultConstructible(ctx)) return;

				const auto& type = symbol_type.getType();
				if (type.getKind() == tsh::Kind::StaticArray) {
					auto        arr_type = type.as<tsh::StaticArrayAbstractType>();
					const auto& arr_ctor
						= ctx.query<houtgen::QueryDefaultStaticArrayConstructor>(arr_type)
					          ->valueOrThrow();
					default_ctors.insert(arr_ctor.declaration->original_symbol);
				} else if (type.getKind() == tsh::Kind::Class) {
					auto        class_type = type.as<tsh::ClassAbstractType>();
					const auto& class_ctor
						= ctx.query<houtgen::QueryDefaultClassConstructor>(class_type)
					          ->valueOrThrow();
					default_ctors.insert(class_ctor.declaration->original_symbol);
				}
			};

			for (auto scope: *scopes_to_process) {
				auto symbols_in_scope = ctx.query<QuerySymbolsInScope>(scope);

				for (auto sym: *symbols_in_scope) {
					// Register default constructors for all symbols that need them.
					const auto sym_kind = kind(sym);
					if (sym_kind == SymbolKind::Variable || sym_kind == SymbolKind::Const)
						register_ctor_if_needed(sym);

					// grab constants:
					if (kind(sym) == SymbolKind::Const)
						out.glob_data.emplace_back(ctx, sym, HOUTGlobalDataType::Constant);
					if (kind(sym) == SymbolKind::Variable and isGlobalVar(ctx, sym))
						out.glob_data.emplace_back(ctx, sym, HOUTGlobalDataType::Variable);

					// grab functions:
					if (kind(sym) == SymbolKind::Function)
						scheduled_tasks.emplace_back(ctx.schedule<QueryCodeOfFun>(sym));
					if (kind(sym) == SymbolKind::Class) class_symbols.emplace_back(sym);
				}
			}

			for (auto class_sym: class_symbols) {
				// we postpone this past function scheduling, as
				// appendClassConstructors may be time consuming.
				appendImplicitClassConstructors(out.functions, class_sym, ctx);
				auto append_methods_result
					= appendClassMethodsWithFail(out.functions, class_sym, ctx);
				if (append_methods_result) {
					is_failed = true;
					continue;
				}
			}

			appendDefaultConstructors(out.functions, default_ctors, ctx);

			for (auto handler: scheduled_tasks) {
				// we "catch" failure here to continue gathering other functions:
				auto hout_function = ctx.await<QueryCodeOfFun>(handler);
				if (hout_function->hasFailed()) {
					is_failed = true;
					continue;
				} else {
					out.functions.emplace_back(&hout_function->valueOrPanic());
				}
			}

			if (is_failed) return query::Failed();

			return out;
		}

		/**
		 * @brief Appends the default constructors and all the default constructors they call to
		 * the HOUT unit.
		 *
		 * @param out_functions The vector of functions to be modified.
		 * @param ctors Symbol IDs of the top level default constructors generated for the symbols
		 * in scope.
		 * @param ctx The query context.
		 */
		static void appendDefaultConstructors(
			std::vector<CRef<HOUTFunction>>& out_functions,
			const std::set<SymID>&           ctors,
			Context&                         ctx
		) {
			std::set<SymID> all_required_functions;

			// Collect all dependencies - default ctors called by the default ctor, and eliminate
			// duplicates. This is needed to handle default constructors of types like `T[5][3]`,
			// for which the top level default constructor recursively calls the default constructor
			// of `T[3]`. The inner `T[3]` constructor isn't included in the `ctors` set since there
			// are no symbols in scope of type `T[3]`, thus we retrieve it by checking transitive
			// functions calls of the top-level default constructor.
			for (SymID ctor_sym: ctors) {
				auto transitive = ctx.query<QueryTransitiveFunctionCalls>(ctor_sym)->valueOrThrow();
				for (SymID dependency: transitive) {
					auto sym_ref = getSymRef(dependency);

					// Skip all not generated symbols, to prevent double insertion of HOUTFunctions.
					// For example in cases like: `class T { a: i32 = foo(); }`, the SymID of
					// `foo()` will get returned as a result of `QueryTransitiveFunctionCalls` since
					// it's called by the default constructor of `T`. This function was already
					// added when looping through the symbols in scope thus we skip it here.
					if (!std::holds_alternative<houtgen::GeneratedSymbolData>(sym_ref->other))
						continue;
					const auto gsd_data = std::get<houtgen::GeneratedSymbolData>(sym_ref->other);
					// Insert only other default constructors to not insert implicit constructors twice.
					if (gsd_data.isDefaultConstructor()) all_required_functions.insert(dependency);
				}
			}

			// Now insert them into the module.
			for (SymID func_sym: all_required_functions) {
				const auto& hout_res = ctx.query<QueryCodeOfFun>(func_sym)->valueOrThrow();
				out_functions.emplace_back(&hout_res);
			}
		}

		/**
		 * Append the constructors of a class to the provided vector of functions.
		 * @param out_functions The vector of functions to be modified.
		 * @param class_sym The symbol of the class, whose constructors are to be appended.
		 * @param ctx The query context.
		 */
		static void appendImplicitClassConstructors(
			std::vector<CRef<HOUTFunction>>& out_functions, const SymID class_sym, Context& ctx
		) {
			CORE_ASSERT(
				kind(class_sym) == SymbolKind::Class,
				"Invalid argument exception: expected class symbol"
			);

			// For now, we handle only the class's primary constructor.
			// @TODO: #1290 Handle auxiliary constructors.

			const auto class_type = ctx.query<QueryTypeFromDefinition>(class_sym)
			                            ->valueOrThrow()
			                            .getType()
			                            .as<tsh::ClassAbstractType>();
			const auto& implicit_ctor
				= ctx.query<houtgen::QueryImplicitClassConstructor>(class_type)->valueOrThrow();
			out_functions.emplace_back(&implicit_ctor);
		}

		/**
		 * Append the methods of a class to the provided vector of functions.
		 * @param out_functions The vector of functions to be modified.
		 * @param class_sym The symbol of the class, whose methods are to be appended.
		 * @param ctx The query context.
		 *
		 * @return Whether any method queries failed.
		 */
		static bool appendClassMethodsWithFail(
			std::vector<CRef<HOUTFunction>>& out_functions, const SymID class_sym, Context& ctx
		) {
			CORE_ASSERT(
				kind(class_sym) == SymbolKind::Class,
				"Invalid argument exception: expected class symbol"
			);

			const auto class_type = ctx.query<QueryTypeFromDefinition>(class_sym)
			                            ->valueOrThrow()
			                            .getType()
			                            .as<tsh::ClassAbstractType>();


			auto methods = class_type.getInterface(ctx)->getMethodsView();


			bool is_failed = false;

			for (const auto& method: methods) {
				// @TODO: #1956 remove this if when ZST refs are supported
				// we fail here, because otherwise we try to lower a self pointer to a ZST type and
				// llvm panics. This check is put inside the for, to only check it if the methods
				// are actually present, and to provide a more specific error location.
				if (not class_type.carriesInformation(ctx)) {
					ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
						"Methods of zero-sized classes are not yet implemented due to ZST not "
						"being properly supported yet.",
						symbolPst(method.getSymbol()).map([&](auto pst) {
							return pst.unlock(ctx)->getSourcePosition();
						})
					));
					return true;  // failed
				}


				auto method_sym  = method.getSymbol();
				auto hout_method = ctx.query<QueryCodeOfFun>(method_sym);
				if (hout_method->hasFailed()) {
					is_failed = true;
					continue;
				} else {
					out_functions.emplace_back(&hout_method->valueOrPanic());
				}
			}
			return is_failed;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryModuleHOUT);

	struct IMPLEMENT_QUERY(QueryModuleHOUTRecursively, query::QResult<std::vector<CRef<HOUTUnit>>>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			auto current_unit = &ctx.query<QueryModuleHOUT>(key)->valueOrThrow();

			auto submodules = ctx.query<frontend::QuerySubmodules>(key);

			if (submodules->empty()) return std::vector<CRef<HOUTUnit>>{ current_unit };

			std::vector<std::vector<CRef<HOUTUnit>>> sub_results;
			sub_results.reserve(submodules->size());

			size_t total_elements = 1;

			for (auto submodule: *submodules) {
				auto sub_hout
					= ctx.query<QueryModuleHOUTRecursively>(submodule.second).valueOrThrow();
				total_elements += sub_hout.size();

				sub_results.emplace_back(std::move(sub_hout));
			}

			std::vector<CRef<HOUTUnit>> out;
			out.reserve(total_elements);

			out.emplace_back(current_unit);

			for (auto& sub_vec: sub_results) out.insert(out.end(), sub_vec.begin(), sub_vec.end());

			return out;
		}

		QUERY_AUTO_CACHE_COPY
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryModuleHOUTRecursively);

	struct IMPLEMENT_QUERY(QueryTopLevelEntities, query::QResult<HOUTUnit>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// go over all top level symbols and get theirs hout
			// store it in some vector or something
			// lookup all and stuff

			auto main_file_root_scope = queryRootScopeOfMainModuleFile(ctx, key);

			auto symbols_in_module_root = ctx.query<QuerySymbolsInScope>(main_file_root_scope);

			HOUTUnit out;

			for (auto sym: *symbols_in_module_root) {
				// grab constants:
				if (kind(sym) == SymbolKind::Const)
					out.glob_data.emplace_back(ctx, sym, HOUTGlobalDataType::Constant);
				if (kind(sym) == SymbolKind::Variable and isGlobalVar(ctx, sym))
					out.glob_data.emplace_back(ctx, sym, HOUTGlobalDataType::Variable);
				// grab functions:
				if (kind(sym) == SymbolKind::Function)
					out.functions.emplace_back(&ctx.query<QueryCodeOfFun>(sym)->valueOrThrow());
			}

			return out;
		}

		QUERY_AUTO_CACHE_CREF
	};

	QUERY_IMPLEMENTATION_BOILERPLATE(QueryTopLevelEntities);
}
