#include "queries.hpp"

#include <diagnostic_interactive/placeholder.hpp>
#include <frontend/module_tree/queries.hpp>
#include <helios/hout/elements.hpp>
#include <helios/hout/hout.hpp>
#include <helios/mangler/mangler.hpp>
#include <helios/queries/function_queries.hpp>
#include <helios/queries/global_data_queries.hpp>
#include <helios/symbols/attributes.hpp>
#include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/query_type_symbol_data.hpp>
#include <helios/symbols/symbol_id.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/type_interface.hpp>
#include <helios/utils/hout_walkers.hpp>
#include <helios_private/errors/dia_interactive_elements.hpp>
#include <helios_private/errors/duplicated_definition.hpp>
#include <helios_private/scopes/scopes.hpp>
#include <helios_private/symbols/symbol_data.hpp>
#include <helios_private/symbols/symbols.hpp>

#include "base/collections/maps.hpp"
#include <base/except/exceptions.hpp>
#include <base/extend_cpp/variant_match.hpp>
#include <base/extend_cpp/vector_utils.hpp>
#include <base/types/ok_bad.hpp>

#include <query_framework/query_errors.hpp>
#include <query_framework/standard_query/query_impl.hpp>
#include <query_framework/utils/query_failed_try.hpp>

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

			std::vector<query::TaskHandle> scheduled_function_code_tasks;
			std::vector<query::TaskHandle> scheduled_global_data_tasks;

			// Classes are not stored in the HOUTUnit, so we collect their symbols here to be able
			// to check them for duplicates later.
			std::vector<SymID> class_symbols;

			for (auto scope: *scopes_to_process) {
				Ref symbols_in_scope = ctx.query<QuerySymbolsInScope>(scope);
				if (symbols_in_scope->hasFailed()) {
					is_failed = true;
					continue;
				}

				for (auto sym: symbols_in_scope->valueOrPanic()) {
					// Register default constructors for all symbols that need them.
					const auto sym_kind = kind(sym);
					switch (sym_kind) {
					case SymbolKind::Function:
						scheduled_function_code_tasks.emplace_back(ctx.schedule<QueryCodeOfFun>(sym)
						);
						break;
					case SymbolKind::Const:
					case SymbolKind::Variable:
						if (sym_kind == SymbolKind::Variable && not isGlobalVar(ctx, sym)) break;
						scheduled_global_data_tasks.emplace_back(
							ctx.schedule<QueryHOUTGlobalData>(sym)
						);
						break;
					case SymbolKind::Class: {
						class_symbols.push_back(sym);
						auto result = appendClassTasks(
							scheduled_function_code_tasks, scheduled_global_data_tasks, sym, ctx
						);
						if (result.isBad()) is_failed = true;
						break;
					}
					default:
						break;
					}
				}
			}


			for (auto handler: scheduled_function_code_tasks) {
				// we "catch" failure here to continue gathering other functions:
				auto hout_function = ctx.await<QueryCodeOfFun>(handler);
				if (hout_function->hasFailed()) {
					is_failed = true;
					continue;
				} else {
					auto& func = hout_function->valueOrPanic();
					out.functions.emplace_back(&func);
				}
			}
			for (auto handler: scheduled_global_data_tasks) {
				auto global_data = ctx.await<QueryHOUTGlobalData>(handler);
				if (global_data->hasFailed()) {
					is_failed = true;
					continue;
				} else {
					auto& func = global_data->valueOrPanic();
					out.glob_data.emplace_back(&func);
				}
			}

			if (duplicatesCheck(ctx, out, class_symbols).isBad()) is_failed = true;
			if (duplicatedFieldsCheck(ctx, class_symbols).isBad()) is_failed = true;
			if (collectReplicatedSymbols(ctx, out).isBad()) is_failed = true;


			// This is the place where we would check for all the functions and if they return some
			// type, like class or tuple then we can append the destructor.


			if (is_failed) return query::Failed();

			return out;
		}

		/**
		 * @brief This function collects all used symbols from the hout unit module
		 * that should be appended to the module, for example some compiler generated symbols
		 * or template instantiations in the future.
		 */
		static base::OkBad collectReplicatedSymbols(query::Context& ctx, HOUTUnit& out_unit) {
			// We perform a DFS traversal of the HOUT Unit. We keep track of the visited functions.
			std::unordered_set<SymID> added_symbols;
			std::vector<SymID>        symbol_stack;
			base::OkBad               result = base::OK;

			for (auto f: out_unit.functions) {
				added_symbols.insert(f->declaration->original_symbol);
				symbol_stack.push_back(f->declaration->original_symbol);
			}
			for (auto g: out_unit.glob_data) {
				added_symbols.insert(g->helios_symbol);
				symbol_stack.push_back(g->helios_symbol);
			}

			while (not symbol_stack.empty()) {
				auto current_sym = symbol_stack.back();
				symbol_stack.pop_back();

				auto qresult = ctx.query<QueryDirectUsedSymbols>(current_sym);
				if (qresult->hasFailed()) {
					result = base::BAD;
					continue;
				}
				auto& used_symbols = qresult->valueOrPanic();

				for (auto used_fun: used_symbols.used_functions) {
					if (added_symbols.contains(used_fun)) continue;
					if (emissionPolicy(ctx, used_fun) != EmissionPolicy::Replicated) continue;

					added_symbols.insert(used_fun);
					if (implementsQueryCodeOfFun(used_fun))
						out_unit.functions.emplace_back(
							&ctx.query<QueryCodeOfFun>(used_fun)->valueOrThrow()
						);
					symbol_stack.push_back(used_fun);
				}
				for (auto used_global: used_symbols.used_globals) {
					if (added_symbols.contains(used_global)) continue;
					if (emissionPolicy(ctx, used_global) != EmissionPolicy::Replicated) continue;

					added_symbols.insert(used_global);
					out_unit.glob_data.emplace_back(
						&ctx.query<QueryHOUTGlobalData>(used_global)->valueOrThrow()
					);
					symbol_stack.push_back(used_global);
				}
			}
			return result;
		}

		/**
		 * @brief Reports a duplicated definition diagnostic if a `SymID`s mangled name collides
		 * with a previously seen definition.
		 *
		 * @param seen_declarations Mangled names seen so far, mapped to the source position of the
		 * first definition that used them.
		 * @param sym_id The new SymID being inserted.
		 * @param stable_pos Source position of the definition.
		 * @return `true` if `sym_id` duplicates an earlier definition.
		 */
		static bool reportIfDuplicate(
			query::Context&                                      ctx,
			base::HashMap<base::StrID, dia_int::StablePosition>& seen_declarations,
			SymID                                                sym_id,
			base::StrID                                          original_name,
			dia_int::StablePosition                              stable_pos
		) {
			base::StrID mangled_name = compiler::helios::mangler::getSimpleMangledName(ctx, sym_id);
			if (mangled_name.isBad()) return false;

			// Allow duplicate mangled names for symbols with backend-dependent implementations.
			if (hasAttribute<attributes::DVMOnlyImpl>(sym_id)
			    or hasAttribute<attributes::NativeOnlyImpl>(sym_id)) {
				return false;
			}

			return reportIfNameTaken(
				ctx, seen_declarations, mangled_name, original_name, stable_pos
			);
		}

		/**
		 * @brief Reports a duplicated definition diagnostic if @p key_name was already registered
		 * in @p seen_declarations, and registers it otherwise.
		 *
		 * @param seen_declarations Names seen so far, mapped to the source position of the first
		 * definition that used them.
		 * @param key_name Name the definition is registered under (mangled for module level
		 * symbols, the plain name for class fields).
		 * @param original_name Name of the definition as written in the source code.
		 * @param stable_pos Source position of the definition.
		 * @return `true` if an earlier definition already used @p key_name.
		 */
		static bool reportIfNameTaken(
			query::Context&                                      ctx,
			base::HashMap<base::StrID, dia_int::StablePosition>& seen_declarations,
			base::StrID                                          key_name,
			base::StrID                                          original_name,
			dia_int::StablePosition                              stable_pos
		) {
			auto [entry, inserted] = seen_declarations.try_emplace(key_name, stable_pos);
			if (inserted) return false;

			auto error
				= makeBox<dia_int::DuplicatedDefinitionError>(original_name.str(), stable_pos);

			// Point the user at the previous declaration.
			error->addAttachedMessage(
				makeBox<dia_int::PlaceholderNote>("Previous declaration here.", entry->second)
			);

			ctx.logInt(std::move(error));

			return true;
		}

		/**
		 * @brief Reports fields declared more than once inside the same class.
		 *
		 * @param class_symbols Class symbols of the unit.
		 * @return `base::BAD` if at least one class declares the same field name twice. The caller
		 * is responsible for failing the query gracefully; this must not throw, as
		 * `QueryModuleHOUT` does not catch query-failure exceptions thrown from `provide`.
		 */
		static base::OkBad duplicatedFieldsCheck(
			query::Context& ctx, const std::vector<SymID>& class_symbols
		) {
			bool found_duplicate = false;

			for (SymID class_sym: class_symbols) {
				Ref class_data = ctx.query<QueryClassSymbolData>(class_sym);
				// A class we could not resolve is already diagnosed elsewhere.
				if (class_data->hasFailed()) continue;

				base::HashMap<base::StrID, dia_int::StablePosition> seen_fields;

				for (SymID field_sym: class_data->valueOrPanic().members) {
					if_opt_some(maybeSymbolPst(field_sym), pst) {
						if (reportIfNameTaken(
								ctx,
								seen_fields,
								name(field_sym),
								name(field_sym),
								pst.unlock(ctx)->getStablePosition()
							))
							found_duplicate = true;
					}
				}
			}
			return found_duplicate ? base::BAD : base::OK;
		}

		/**
		 * @brief Reports duplicated definitions of functions, globals and classes sharing a
		 * mangled name.
		 *
		 * @param class_symbols Class symbols of the unit.
		 * @return `base::BAD` if at least one duplicate was found. The caller is responsible for
		 * failing the query gracefully; this must not throw, as `QueryModuleHOUT` does not catch
		 * query-failure exceptions thrown from `provide`.
		 */
		static base::OkBad duplicatesCheck(
			query::Context& ctx, const HOUTUnit& unit, const std::vector<SymID>& class_symbols
		) {
			base::HashMap<base::StrID, dia_int::StablePosition> seen_declarations;
			bool                                                found_duplicate = false;

			for (const auto& func: unit.functions) {
				if_opt_some(func->origin.getStablePosition(), stable_pos) {
					if (reportIfDuplicate(
							ctx,
							seen_declarations,
							func->declaration->original_symbol,
							func->declaration->original_name,
							stable_pos
						))
						found_duplicate = true;
				}
			}

			for (const auto& global: unit.glob_data) {
				if_opt_some(global->origin.getStablePosition(), stable_pos) {
					if (reportIfDuplicate(
							ctx,
							seen_declarations,
							global->helios_symbol,
							global->original_name,
							stable_pos
						))
						found_duplicate = true;
				}
			}

			for (SymID class_sym: class_symbols) {
				if_opt_some(maybeSymbolPst(class_sym), pst) {
					auto stable_pos = pst.unlock(ctx)->getStablePosition();
					if (reportIfDuplicate(
							ctx, seen_declarations, class_sym, name(class_sym), stable_pos
						))
						found_duplicate = true;
				}
			}
			return found_duplicate ? base::BAD : base::OK;
		}

		/**
		 * Append the methods and static variables of a class.
		 */
		static base::OkBad appendClassTasks(
			std::vector<query::TaskHandle>&                  out_function_code_tasks,
			[[maybe_unused]] std::vector<query::TaskHandle>& out_global_data_tasks,
			const SymID                                      class_sym,
			Context&                                         ctx
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

			for (const auto& method: methods) {
				auto method_sym = method.getSymbol();

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
					return base::BAD;
				}
				// We only here add the methods that are owner only.
				if (emissionPolicy(ctx, method_sym) != EmissionPolicy::OwnerOnly) continue;

				out_function_code_tasks.push_back(ctx.schedule<QueryCodeOfFun>(method_sym));
			}
			return base::OK;
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
