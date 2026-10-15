// Copyright 2026 DuckType LLC
//
// This file is part of the Duckling project, licensed under the DuckType
// Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
// of this repository or https://ducktype.org/licenses/DTCL-1.0

#include "hout.hpp"

#include "elements.hpp"

#include <frontend/pst_parser/elements/includes/basic.hpp>

#include <query_framework/context/context.hpp>

#include <memory>

namespace compiler::helios {
	void HOUTUnit::debugPrint(query::Context& ctx, std::ostream& out) const {
		out << "HOUT UNIT:\n\n";

		out << "Constants:\n";
		for (auto& const_gd: glob_data) const_gd->debugPrint(ctx, out);

		out << "\nFunctions:\n";
		for (auto& func: functions) {
			func->debugPrint(out);
			out << "\n";
		}
	}

	HOUTFunctionDeclaration::HOUTFunctionDeclaration(
		const SymID                  symbol,
		const Operatoriness          operatoriness,
		const tsh::SymbolType<>      ret_type,
		std::vector<code::Parameter> parameters,
		SymbolABI                    abi,
		code::ElementOrigin          origin
	):
		  original_symbol(symbol),
		  original_name(name(original_symbol)),
		  operatoriness(operatoriness),
		  abi(abi),
		  return_type(ret_type),
		  parameters(std::move(parameters)),
		  origin(origin) {
		CORE_ASSERT(
			isFunctionLike(kind(symbol)),
			"Symbol is not a function, function declaration, method, constructor nor destructor"
		);
	}

	u64 HOUTFunctionDeclaration::queryUnstablePerfectHash() const {
		return original_symbol.queryUnstablePerfectHash();
	}

	void HOUTFunctionDeclaration::debugPrint(std::ostream& out) const {
		out << "fun ";
		out << original_name.strView() << " (" << original_symbol.queryUnstablePerfectHash() << ")";
		out << " : " << "Return type: ";
		out << this->return_type.toString() << "\n";
		out << "Parameters: \n";
		if (parameters.empty()) out << "  none\n";
		for (auto& param: parameters) {
			out << "  " << param.name.strView() << " : ";
			out << param.type.toString();
			if (param.initial_value.has_value()) {
				out << " = ";
				param.initial_value.value()->debugPrint(out);
			}
			out << "\n";
		}
	}

	u64 HOUTFunction::queryUnstablePerfectHash() const {
		// @note for now it doesn't depend on body
		return declaration->queryUnstablePerfectHash();
	}

	void HOUTFunction::debugPrint(std::ostream& out) const {
		declaration->debugPrint(out);
		out << "{\n";
		for (auto& stmt: body->statements) stmt->debugPrint(out, 1);
		out << "}\n";
	}

	HOUTFunction::HOUTFunction(

		code::ElementOrigin                           origin,
		const CRef<HOUTFunctionDeclaration>           other,
		const std::shared_ptr<const code::CodeBlock>& body
	):
		  origin(origin),
		  declaration(other),
		  body(body) {}

	void HOUTGlobalData::debugPrint(query::Context& ctx, std::ostream& out) const {
		variant_match(value) {
			variant_case(HOUTGlobalConst, const_value) {
				out << "const " << prettyDebugPrint(helios_symbol, ctx) << " : " << type.toString()
					<< " = " << const_value.value.toString() << '\n';
			}
			variant_case(HOUTGlobalVariable, val) {
				out << (type.getMutability() == tsh::Mutability::Mutable ? "var   " : "let   ");
				out << prettyDebugPrint(helios_symbol, ctx) << " : " << type.toString() << " = ";
				val.initial_value.ref()->debugPrint(out);
				out << '\n';
			}
		}
	}

}
