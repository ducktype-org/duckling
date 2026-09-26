#include "symbol_data.hpp"

#include <helios/scope_id.hpp>
#include <helios/symbols/query_type_of_symbol.hpp>
#include <helios/symbols/symbol_kind.hpp>
#include <helios/tsh/queries/types.hpp>
#include <helios/tsh/symbol_type.hpp>
#include <helios/tsh/types.hpp>
#include <helios_private/symbols/pst_symbol_data.hpp>

#include <base/except/exceptions.hpp>
#include <base/str/str_utils.hpp>

#include <utility>

namespace compiler::helios {
	namespace defgen {
		base::Bit256 Constructor::queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(
				type.queryUnstablePerfectHash(), static_cast<u64>(kind)
			);
		}

		base::Bit256 Method::queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(
				owner_type.queryUnstablePerfectHash(), static_cast<u64>(kind)
			);
		}

		base::Bit256 BuiltinOperator::queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(
				operator_type.queryUnstablePerfectHash(), static_cast<u64>(operatoriness)
			);
		}

		base::Bit256 BuiltinTemplatedSymbol::queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(
				type.queryUnstablePerfectHash(), static_cast<u64>(kind)
			);
		}

		base::Bit256 Parameter::queryUnstablePerfectHash() const {
			return { function_symbol.queryUnstablePerfectHash(), parameter_index };
		}

		base::Bit256 SelfParameter::queryUnstablePerfectHash() const {
			return { method_symbol.queryUnstablePerfectHash(), scope.queryUnstablePerfectHash() };
		}

		base::Bit256 Field::queryUnstablePerfectHash() const {
			return { parent_type.queryUnstablePerfectHash(), index };
		}

		base::Bit256 GeneratedFunctionVariable::queryUnstablePerfectHash() const {
			return { function_symbol.queryUnstablePerfectHash(), variable_index };
		}

		base::Bit256 ControlFlowLocal::queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(owning_scope.queryUnstablePerfectHash(), role);
		}

		// Hash includes the return type, the wrapped PST element and the kind of the input, next to
		// the counter. The mangled name (used for linker symbols) is based only on the counter.
		base::Bit256 ReplInputWrapper::queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(
				return_type, counter, pst_element_hash, static_cast<u64>(type)
			);
		}

		base::Bit256 ReplEmptyVariable::queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(original_variable.queryUnstablePerfectHash());
		}

		base::Bit256 ScriptMainWrapper::queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(script_id, scope.queryUnstablePerfectHash());
		}

		base::Bit256 generatedSymbolUnstablePerfectHash(const GeneratedSymbolDataVariant& data) {
			return hashing::justHash<hashing::SHA256>(
				data.index(), VISIT(data, d, return d.queryUnstablePerfectHash();)
			);
		}

		GeneratedConstant::GeneratedConstant(ctv::CompileTimeValue value, ScopeID scope):
			  value(std::move(value)),
			  scope(scope) {}

		base::Bit256 Module::queryUnstablePerfectHash() const {
			return hashing::justHash<hashing::SHA256>(module_id.queryUnstablePerfectHash());
		}

		base::Bit256 GeneratedConstant::queryUnstablePerfectHash() const {
			hashing::SHA256 hasher;
			hashing::addToHash(hasher, value.queryUnstablePerfectHash());
			hashing::addToHash(hasher, scope.queryUnstablePerfectHash());
			return hasher.finalize();
		}
	}

	SymbolData::SymbolData(CommonSymbolData common, SymbolSemantics other):
		  common(std::move(common)),
		  other(std::move(other)),
		  id(SymbolDataID::next()) {}

	SymbolData SymbolData::makeBuiltinSymbolData(
		CommonSymbolData common_data, BuiltinSemantics builtin_data
	) {
		return { std::move(common_data), builtin_data };
	}

	SymbolData SymbolData::makePSTSymbolData(
		CommonSymbolData common_data, PstImplementedSemantics pst_data
	) {
		return { std::move(common_data), pst_data };
	}

	SymbolData SymbolData::makeClassMemberSymbolData(
		CommonSymbolData common_data, ClassMemberSemantics class_member_data
	) {
		return { std::move(common_data), class_member_data };
	}

	SymbolData SymbolData::makeGeneratedSymbolData(
		const base::StrID name, defgen::GeneratedSymbolDataVariant generated_data
	) {
		SymbolKind kind{};
		bool       ignored_by_lookup = false;
		variant_match(generated_data) {
			variant_case_novalue(defgen::BuiltinOperator) {
				kind = SymbolKind::FunctionDeclaration;
			}
			variant_case_novalue(defgen::BuiltinTemplatedSymbol) {
				// `MoveIn` is a LIR builtin: its call is replaced by an instruction, so it never
				// gets a body.
				kind = SymbolKind::FunctionDeclaration;
			}
			variant_case_novalue(
				defgen::Constructor,
				defgen::Method,
				defgen::ReplInputWrapper,
				defgen::ScriptMainWrapper
			) {
				kind = SymbolKind::Function;
			}
			variant_case_novalue(defgen::Parameter, defgen::SelfParameter) {
				kind = SymbolKind::Parameter;
			}
			variant_case_novalue(defgen::Field) { kind = SymbolKind::Field; }
			variant_case_novalue(defgen::ControlFlowLocal) {
				kind              = SymbolKind::Variable;
				ignored_by_lookup = true;
			}
			variant_case_novalue(defgen::GeneratedFunctionVariable, defgen::ReplEmptyVariable) {
				kind = SymbolKind::Variable;
			}
			variant_case_novalue(defgen::GeneratedConstant) { kind = SymbolKind::Const; }
			variant_case_novalue(defgen::Module) { kind = SymbolKind::Module; }
			variant_default { CORE_UNREACHABLE(); }
		}

		return {
			{
				.name                 = name,
				.kind                 = kind,
				.is_ignored_by_lookup = ignored_by_lookup,
			},
			std::visit(
				[](auto&& x) -> SymbolData::SymbolSemantics { return std::forward<decltype(x)>(x); },
				generated_data
			),
		};
	}

	base::Optional<ScopeID> SymbolData::getScope() const {
		// @TODO: #3099 a lot of scopes could be removed from generated symbols.
		variant_match(other) {
			variant_case(PstImplementedSemantics, pst_data) { return pst_data.scope; }
			variant_case(ClassMemberSemantics, member_data) { return member_data.scope; }
			variant_case(BuiltinSemantics, data) { return data.scope; }
			variant_case(defgen::SelfParameter, param) { return param.scope; }
			variant_case(defgen::ControlFlowLocal, local) { return local.owning_scope; }
			variant_case(defgen::ScriptMainWrapper, script) { return script.scope; }
			variant_case(defgen::GeneratedConstant, gen_const) { return gen_const.scope; }
			variant_default { return {}; }
		}
		CORE_UNREACHABLE();
	}

	bool SymbolData::isPstImplemented() const {
		return std::holds_alternative<PstImplementedSemantics>(other)
		    or std::holds_alternative<ClassMemberSemantics>(other);
	}

	base::Optional<pst::AccessLocked<pst::LangElement>> SymbolData::maybePstElement() const {
		variant_match(other) {
			variant_case(PstImplementedSemantics, data) { return data.getElement(); }
			variant_case(ClassMemberSemantics, data) { return data.getElement(); }
			variant_case(BuiltinSemantics, data) { return data.getElement(); }
			variant_default { return {}; }
		}
	}

	base::Optional<pst::Access<pst::Stmt>> SymbolData::stmtCast(query::Context& ctx) const {
		return maybePstElement().value().unlock(ctx).dynamicCast<pst::Stmt>();
	}
}
