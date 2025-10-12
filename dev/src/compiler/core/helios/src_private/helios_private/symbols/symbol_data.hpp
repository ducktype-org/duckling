#pragma once

#include <helios/scope_symbol_id.hpp>
// #include <helios/symbols/query_type_from_definition.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <pst_parser/access.hpp>
#include <pst_parser/elements/includes/basic.hpp>
#include <typesystem/higher/queries/types.hpp>
#include <typesystem/higher/type_interface.hpp>
#include <typesystem/higher/types.hpp>

#include <base/string_id.hpp>
#include <base/variant.hpp>

#include <hashing/hash.hpp>
#include <query_framework/context.hpp>

namespace compiler::helios {
	struct QueryTypeOfSymbol;
	struct QueryTypeFromDefinition;

	/**
	 * Symbol data shared by all symbols.
	 */
	struct CommonSymbolData final {
		/**
		 * Symbol name.
		 *
		 * @note In the future there might also be anonymous symbols (symbols with no name), like:
		 * `let _ = 5;`, lambdas, `using a.*`, etc.
		 * For now, we work around it, as all things that could be anonymous are also a wildcard.
		 * Those symbols will also need to have mangled name.
		 */
		base::StrID name;

		/**
		 * Symbol kind, determines what kind of symbol it is.
		 */
		SymbolKind kind;

		/**
		 * Whether the symbol is a wildcard symbol.
		 * When lookup encounter a wildcard symbol it
		 * looks-up into that symbol instead of considering the symbol itself.
		 * e.g.: `using a.*`
		 */
		bool is_wildcard = false;

		/**
		 * Whether the symbol is an alias.
		 * Aliases are symbols that are not "real" symbols, but are just a reference to another
		 * symbol. e.g.: `using a = b;`
		 */
		bool is_alias = false;

		/** Whether the symbol is a dependent symbol.
		 * Dependent symbols are symbols that can't be used in actual execution without some
		 * context, e.g. class fields.
		 */
		bool dependent = false;
	};

	/**
	 * @brief Symbol data for all symbols that are created from PST elements.
	 */
	struct PstSymbolData final {
		/**
		 * Scope in which the symbol was defined.
		 */
		ScopeID scope;

		/**
		 * PST element that the symbol was created from.
		 */
		pst::AccessLocked<pst::LangElement> pst_element;
	};

	namespace builtin {
		struct BuiltinFunctionData final {
			tsh::FunctionAbstractType type;

			explicit BuiltinFunctionData(const tsh::FunctionAbstractType type): type(type) {}
		};
	}

	namespace houtgen {
		struct GeneratedSymbolData final {
			// Data for a compiler-generated implicit constructor.
			struct ImplicitConstructor final {
				SymID class_symbol;  // The symbol of the class this constructor belongs to.

				[[nodiscard]]
				u64 queryUnstablePerfectHash() const {
					return hashing::justHash(class_symbol.ref.get());
				}
			};

			// Data for a compiler-generated parameter of a function.
			struct Parameter final {
				SymID function_symbol;  // The symbol of the function this parameter belongs to.
				u64   parameter_index;  // The index of the parameter in the function's signature.

				[[nodiscard]]
				u64 queryUnstablePerfectHash() const {
					return hashing::justHash(function_symbol.ref.get(), parameter_index);
				}
			};

			// Data for a compiler-generated variable (not parameter) in a function.
			struct Variable final {
				SymID function_symbol;   // The symbol of the function this variable belongs to.
				u64   variable_index;    // The index of the variable in the function's body.
				tsh::SymbolType<> type;  // The type of the variable.

				[[nodiscard]]
				u64 queryUnstablePerfectHash() const {
					return hashing::justHash(function_symbol.ref.get(), variable_index);
				}
			};

			std::variant<ImplicitConstructor, Parameter, Variable> data;

			explicit GeneratedSymbolData(
				const std::variant<ImplicitConstructor, Parameter, Variable>& data
			):
				  data(data) {}

			[[nodiscard]]
			u64 queryUnstablePerfectHash() const {
				return hashing::justHash(
					data.index(), VISIT(data, d, return d.queryUnstablePerfectHash();)
				);
			}

			tsh::SymbolType<> getType(query::Context& ctx) const;
		};
	}

	/**
	 * @brief Stores generic symbol data.
	 * @note Symbols and their associated SymbolData are created by HELIOS via queries.
	 * SymbolData is by design a "read-only" structure.
	 */
	struct SymbolData final {
		using OtherData
			= std::variant<PstSymbolData, builtin::BuiltinFunctionData, houtgen::GeneratedSymbolData>;

		CommonSymbolData common;
		OtherData        other;

		static auto makePSTSymbolData(const CommonSymbolData common_data, PstSymbolData pst_data) {
			return SymbolData{
				.common = common_data,
				.other  = pst_data,
			};
		}

		static auto makeBuiltinFunction(
			const base::StrID name, builtin::BuiltinFunctionData builtin_data
		) {
			return SymbolData{
				.common = {
					.name = name,
					.kind = SymbolKind::BuiltinFunction,
				},
				.other  = builtin_data,
			};
		}

		static auto makeGeneratedSymbol(
			const base::StrID name, houtgen::GeneratedSymbolData generated_data
		) {
			SymbolKind kind{};
			variant_match(generated_data.data) {
				variant_case_novalue(houtgen::GeneratedSymbolData::ImplicitConstructor) {
					kind = SymbolKind::Function;
				}
				variant_case_novalue(houtgen::GeneratedSymbolData::Parameter) {
					kind = SymbolKind::Parameter;
				}
				variant_case_novalue(houtgen::GeneratedSymbolData::Variable) {
					kind = SymbolKind::Variable;
				}
				variant_default { CORE_UNREACHABLE(); }
			}
			return SymbolData{
				.common = {
					.name = name,
					.kind = kind,
				},
				.other  = generated_data,
			};
		}

		template<class T>
		[[nodiscard]]
		CRef<T> getData() const {
			return &std::get<T>(other);
		}

		[[nodiscard]]
		CRef<PstSymbolData> getPSTData() const {
			return getData<PstSymbolData>();
		}

		/**
		 * Return associated pst_element cast to Stmt.
		 * Panics if element is not a statement or if symbol is not associated with PST element.
		 */
		[[nodiscard]]
		base::Optional<pst::Access<pst::Stmt>> stmtCast(query::Context& ctx) const {
			return getPSTData()->pst_element.unlock(ctx).dynamicCast<pst::Stmt>();
		}
	};

	/**
	 * @brief Helper struct used to access private SymID data.
	 * It is used by HELIOS only. It is a struct so SymID can friend it.
	 */
	struct GetSymRef_Functor final {
		static auto get(const SymID id) { return id.ref; }

		static SymID make(const CRef<SymbolData> ref) { return SymID{ ref }; }
	};

	inline auto getSymRef(const SymID id) { return GetSymRef_Functor::get(id); }

}
