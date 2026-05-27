#include "queries.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/hout.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/queries/global_data_queries.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_id_utils.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>
#include <helios_private/errors/duplicated_definition.hpp>
#include <helios_private/hout_creation/definition_generation/class_constructors.hpp>
#include <helios_private/hout_creation/definition_generation/default_constructors.hpp>
#include <helios_private/hout_creation/definition_generation/to_string_methods.hpp>
#include <helios_private/hout_creation/definition_generation/tuple_constructor.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>

#include <query_framework/query_errors.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <query_framework/utils/query_failed_try.hpp>

#include <set>

namespace compiler::helios {

	struct IMPLEMENT_QUERY(QueryModuleHOUT, query::QResult<HOUTUnit>) {
		static auto provide(Context& ctx, QKey key) -> PResult {
			// We want to continue gathering other entities
			// even if some function queries fail,
			// so we store in this variable whether any failure occurred,
			// and return failure at the end if so.
			bool is_failed = false;

			// @TODO: #2496 maybe remove the following machinery.
			// This query schedules other queries, so we can't interrupt it in the middle of
			// execution, as then we might not await some of the scheduled queries, which is
			// currently a bug.
			auto run_no_interrupt = [&is_failed]<typename Func>(Func&& func) {
				auto ok = query::runFuncWithQueryFailedHandling(std::forward<Func>(func));
				if (ok.status().isBad()) is_failed = true;
			};

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
			std::set<SymID>                additional_ctors;
			std::set<SymID>                additional_tostrings;
			// Keeps track of mangled names processed within the current module
			// to detect duplicated function declarations at the HOUT level.
			std::set<base::StrID> processed_mangled_names;

			auto register_ctor_and_tostring_if_needed = [&](SymID sym) {
				const auto& symbol_type = ctx.query<QueryTypeOfSymbol>(sym)->valueOrThrow();
				const auto& type        = symbol_type.getType();

				// @TODO: #2509 Handle nested tuples
				if (type.getKind() == tsh::Kind::Tuple) {
					auto        tuple_type = type.as<tsh::TupleAbstractType>();
					const auto& tuple_ctor
						= ctx.query<defgen::QueryTuplePackConstructor>(tuple_type)->valueOrThrow();
					additional_ctors.insert(tuple_ctor.declaration->original_symbol);
					const auto& tuple_tostring
						= ctx.query<defgen::QueryToStringMethod>(tuple_type)->valueOrThrow();
					additional_tostrings.insert(tuple_tostring.declaration->original_symbol);
					return;
				}

				// Don't insert any constructors if a type is trivially zero-initializable or not
				// default constructible.
				if (symbol_type.isTriviallyZeroInitializable(ctx)) return;
				if (!symbol_type.isDefaultConstructible(ctx)) return;

				if (type.getKind() == tsh::Kind::StaticArray) {
					auto        arr_type = type.as<tsh::StaticArrayAbstractType>();
					const auto& arr_ctor
						= ctx.query<defgen::QueryDefaultStaticArrayConstructor>(arr_type)
					          ->valueOrThrow();
					default_ctors.insert(arr_ctor.declaration->original_symbol);
				} else if (type.getKind() == tsh::Kind::Class) {
					auto        class_type = type.as<tsh::ClassAbstractType>();
					const auto& class_ctor
						= ctx.query<defgen::QueryDefaultClassConstructor>(class_type)->valueOrThrow();
					default_ctors.insert(class_ctor.declaration->original_symbol);
				}
			};
			auto register_ctor_and_tostring_if_needed_no_interrupt
				= [&run_no_interrupt, &register_ctor_and_tostring_if_needed](SymID sym) {
					  run_no_interrupt([&] { register_ctor_and_tostring_if_needed(sym); });
				  };

			auto try_append_global_data = [&](SymID sym) {
				auto hout_global = ctx.query<QueryHOUTGlobalData>(sym);
				if (hout_global->hasFailed()) {
					is_failed = true;
					return;
				}
				out.glob_data.emplace_back(&hout_global->valueOrPanic());
			};

			for (auto scope: *scopes_to_process) {
				Ref symbols_in_scope = ctx.query<QuerySymbolsInScope>(scope);
				if (symbols_in_scope->hasFailed()) {
					is_failed = true;
					continue;
				}

				for (auto sym: symbols_in_scope->valueOrPanic()) {
					// Register default constructors for all symbols that need them.
					const auto sym_kind = kind(sym);
					if (sym_kind == SymbolKind::Variable || sym_kind == SymbolKind::Const)
						register_ctor_and_tostring_if_needed_no_interrupt(sym);

					// grab constants:
					if (kind(sym) == SymbolKind::Const) try_append_global_data(sym);
					if (kind(sym) == SymbolKind::Variable and isGlobalVar(ctx, sym))
						try_append_global_data(sym);

					// grab functions:
					if (kind(sym) == SymbolKind::Function)
						scheduled_tasks.emplace_back(ctx.schedule<QueryCodeOfFun>(sym));
					if (kind(sym) == SymbolKind::Class) class_symbols.emplace_back(sym);
				}
			}

			appendToStringForSimpleTypes(out.functions, ctx);

			for (auto class_sym: class_symbols) {
				// we postpone this past function scheduling, as
				// appendClassConstructors may be time consuming.
				run_no_interrupt([&] {
					appendImplicitClassConstructors(out.functions, class_sym, ctx);
					auto append_methods_result
						= appendClassMethodsWithFail(out.functions, class_sym, ctx);
					if (append_methods_result) is_failed = true;
				});
			}

			run_no_interrupt([&] {
				appendDefaultConstructors(out.functions, default_ctors, ctx);
				for (SymID ctor_sym: additional_ctors) {
					const auto& hout_res = ctx.query<QueryCodeOfFun>(ctor_sym)->valueOrThrow();
					out.functions.emplace_back(&hout_res);
				}
			});

			run_no_interrupt([&] {
				for (SymID tostring_sym: additional_tostrings) {
					const auto& hout_res = ctx.query<QueryCodeOfFun>(tostring_sym)->valueOrThrow();
					out.functions.emplace_back(&hout_res);
				}
			});

			for (auto handler: scheduled_tasks) {
				// we "catch" failure here to continue gathering other functions:
				auto hout_function = ctx.await<QueryCodeOfFun>(handler);
				if (hout_function->hasFailed()) {
					is_failed = true;
					continue;
				} else {
					auto& func     = hout_function->valueOrPanic();
					SymID func_sym = func.declaration->original_symbol;
					// NOTE: Utilizing the mangler here is a bit hacky, but should work without issues.
					base::StrID mangled_name
						= ctx.query<compiler::helios::mangler::QueryMangledSymbol>({ func_sym });

					if (mangled_name.isBad()) {
						out.functions.emplace_back(&func);
						continue;
					}

					if (processed_mangled_names.contains(mangled_name)) {
						auto stable_pos  = func.declaration->origin.getStablePosition().value();
						auto symbol_name = std::string(func.declaration->original_name.strView());

						ctx.logInt(makeBox<dia_int::DuplicatedDefinitionError>(
							symbol_name, stable_pos, "here"
						));
						is_failed = true;
						continue;
					}

					processed_mangled_names.insert(mangled_name);
					out.functions.emplace_back(&func);
				}
			}

			std::set<SymID>                 unique_funcs;
			std::vector<CRef<HOUTFunction>> deduplicated_functions;
			for (auto f: out.functions)
				if (unique_funcs.insert(f->declaration->original_symbol).second)
					deduplicated_functions.push_back(f);
			out.functions = std::move(deduplicated_functions);

			if (is_failed) return query::Failed();

			return out;
		}

		/**
		 * @brief Appends the compiler-generated methods for simple types.
		 * @param out_functions The vector of functions to be modified.
		 * @param ctx The query context.
		 */
		static void appendToStringForSimpleTypes(
			std::vector<CRef<HOUTFunction>>& out_functions, Context& ctx
		) {
			auto append_to_string_for_simple_type = [&](tsh::AbstractType type) {
				const auto to_string_method
					= ctx.query<QueryCodeOfFun>(defgen::toStringSymForType(ctx, type));
				out_functions.emplace_back(&to_string_method->valueOrThrow());
			};

			for (auto size: { 8, 16, 32, 64 }) {
				append_to_string_for_simple_type(tsh::getIntegralType(
					ctx, u64(size), tsh::IntegralAbstractType::Signedness::Signed
				));
				append_to_string_for_simple_type(tsh::getIntegralType(
					ctx, u64(size), tsh::IntegralAbstractType::Signedness::Unsigned
				));
			}

			for (auto size: { 32, 64 })
				append_to_string_for_simple_type(tsh::getFloatType(ctx, u64(size)));

			append_to_string_for_simple_type(tsh::getCharType());
			append_to_string_for_simple_type(tsh::getBoolType());
			append_to_string_for_simple_type(tsh::getStringType());
			append_to_string_for_simple_type(tsh::getUnitType());
		}

		/**
		 * @brief Appends the default constructors and all the default constructors they call to
		 * the HOUT unit.
		 *
		 * @param out_functions The vector of functions to be modified.
		 * @param ctors Symbol IDs of the top level default constructors generated for the
		 * symbols in scope.
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
					if (!std::holds_alternative<defgen::GeneratedSymbolData>(sym_ref->other))
						continue;
					const auto gsd_data = std::get<defgen::GeneratedSymbolData>(sym_ref->other);
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
				= ctx.query<defgen::QueryImplicitClassConstructor>(class_type)->valueOrThrow();
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

			auto run_if_to_string = [](const SymID sym, auto&& action) {
				auto sym_ref = getSymRef(sym);
				variant_match(sym_ref->other) {
					variant_case(defgen::GeneratedSymbolData, gsd) {
						variant_match(gsd.data) {
							variant_case_novalue(defgen::GeneratedSymbolData::ToStringMethod) {
								action();
							}
							variant_default {}
						}
					}
					variant_default {}
				}
			};

			for (const auto& method: methods) {
				// @TODO: #1956 remove this if when ZST refs are supported
				// we fail here, because otherwise we try to lower a self pointer to a ZST type and
				// llvm panics. This check is put inside the for, to only check it if the methods
				// are actually present, and to provide a more specific error location.
				if (not class_type.carriesInformation(ctx)) {
					ctx.logInt(makeBox<dia_int::NotYetImplementedCodeError>(
						"Methods of zero-sized classes are not yet implemented due to ZST not "
						"being properly supported yet.",
						maybeSymbolPst(method.getSymbol()).map([&](auto pst) {
							return pst.unlock(ctx)->getStablePosition();
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

					// If the method is a `toString`, collect its `toString` dependencies too.
					run_if_to_string(method_sym, [&] {
						auto transitive = ctx.query<QueryTransitiveFunctionCalls>(method_sym);
						if (!transitive->hasFailed()) {
							for (SymID dep: transitive->valueOrPanic()) {
								run_if_to_string(dep, [&] {
									auto dep_hout = ctx.query<QueryCodeOfFun>(dep);
									if (dep_hout->hasFailed())
										is_failed = true;
									else
										out_functions.emplace_back(&dep_hout->valueOrPanic());
								});
							}
						}
					});
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

			Ref symbols_in_module_root
				= &ctx.query<QuerySymbolsInScope>(main_file_root_scope)->valueOrThrow();

			HOUTUnit out;

			for (auto sym: *symbols_in_module_root) {
				// grab constants:
				if (kind(sym) == SymbolKind::Const)
					out.glob_data.emplace_back(&ctx.query<QueryHOUTGlobalData>(sym)->valueOrThrow());
				if (kind(sym) == SymbolKind::Variable and isGlobalVar(ctx, sym))
					out.glob_data.emplace_back(&ctx.query<QueryHOUTGlobalData>(sym)->valueOrThrow());
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
