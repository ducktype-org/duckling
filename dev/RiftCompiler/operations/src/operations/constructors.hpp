/**
 * @file constructors.hpp
 * @brief Created constructors for all types in semantic way
 */

#pragma once

#include "operation.hpp"
#include <exec/helpers.hpp>
#include <symtable/scope_symbol_id.hpp>
#include <typesystem/type_info.hpp>
#include <base/exceptions.hpp>
#include <base/maps.hpp>

/*
 *	@TODO:
 *	This whole file is experimental.
 *	Implementation is not finished.
 */

namespace operation {

	struct TypedSymbol {
		symtable::SymbolId symbol;
		ts::TypeInfo       type_info;

		auto operator<=>(const TypedSymbol& other) const = default;
	};

	// data Typed a = T TypeInfo a
	// TypedSymbol = Typed Symbol


	struct Constructor;

	struct ConstructorCall {
		Constructor*             constructor;
		ts::MemberInfo           member;
		std::vector<TypedSymbol> args;
	};

	struct OperationCall {
		TypedOperation           operation;
		ts::MemberInfo           member;
		std::vector<TypedSymbol> args;
	};

	struct Constructor {
		std::vector<TypedSymbol>     input;
		std::vector<ConstructorCall> virtual_parent_cons;
		std::vector<OperationCall>   operations;
	};

	using Env = base::Map<TypedSymbol, exec::CTV>;

	exec::CTV execConstructor(
		const Constructor& cons, const std::vector<exec::CTV>& args, exec::CTV ctv
	) {
		RIFT_ASSERT(
			args.size() == cons.input.size(), "Arguments don't match parameters in constructor."
		);

		Env env{};


		for (i32 i = 0; i < cons.input.size(); i++) env.put(cons.input[i], args[i]);

		for (const auto& con: cons.virtual_parent_cons) {
			exec::CTV member_ctv = exec::getMember(ctv, con.member, ctv.type.getType());
			std::vector<exec::CTV> args2;
			for (auto s: con.args) args2.push_back(env[s]);
			execConstructor(*con.constructor, args2, member_ctv);
		}

		for (const auto& op: cons.operations) {
			auto ctv_member = exec::getMember(ctv, op.member, (ts::ClassInfo) ctv.type.getType());

			std::vector<exec::CTV> tmp_args{ ctv_member };

			for (auto s: op.args) tmp_args.push_back(env[s]);
			op.operation(tmp_args);
		}

		return ctv;
	}

	std::map<ts::TypeInfo, Constructor> full_constructors;

	void addConstructor(ts::ClassInfo info, Constructor cons) {
		full_constructors.try_emplace(info, cons);
	}

	Constructor getConstructor(ts::ClassInfo info) { return full_constructors.at(info); }

	// @TODO: obecnie tylko bezpośrednie składowe są konstruowane.
	Constructor makeConstructorClass(ts::ClassInfo info) {
		Constructor res;


		for (const auto& [x, y, off_, virtual_ancestor_]: info.members()) {
			res.input.push_back({ x, y.getType() });
			ts::TypeInfo             type_arg    = y.getType();
			auto                     member_info = info.getMemberInfo(x);
			std::vector<TypedSymbol> type_s      = { { x, type_arg } };
			TypedOperation           operation   = getDefault(Defaultable::Assign, type_arg);
			OperationCall            op{ operation, member_info, type_s };
			res.operations.push_back(op);
		}

		return res;
	}

	Constructor makeAddConstructorClass(ts::ClassInfo info) {
		auto cons = makeConstructorClass(info);
		addConstructor(info, cons);
		return cons;
	}

	// Constructor mergeConstructors(Constructor con1, Constructor con2) {
	// 	Constructor res;


	// }


	// lista TypedSymbol - parametry konstruktora
	// lista wywołań konstruktorów wirtualnych (który kontruktor, gdzie ma działać, jakei argumenty)
	// lista: variant spośród
	//		* inicjalizacja jakiegoś pola (A.a = 4, a.b {3, 45}, ...)
	//		* wywołanie jakiejś funkcji użytkownika
	//		- potencjalnie grupujemy te operacje wg kontruktorów z których pochodzą
	//			- z tego można korzytsać by wplatać wypełnianie vtable
	//		* kiedyś będzie jeszcze wyliczanie zmiennych lokalnych (żeby argumenty mogły być postaci
	//		  x+y itd)


	// jak to zrobić bardziej defaultowo:
	// 1. tylko pełne i puste konstruktory
	//     usuwamy użycie symboli. to gdzie co idzie się teraz wylicza jakoooś...
	//	   nie ma też zmiennych lokalnych (ale i tak ich mieliśmy na razie nie mieć)


	// w ten sposób konstruktory byłyby jednolite
	// natomiast ich tworzenie także od strony użytkownika byłoby nieco bardziej skomoplikowane
	// bo trzeba by dostarczać takiej struktury
	// natomiast ta struktura dałaby się wprost pzretłumaczyć z np składni konstruktora jaką ma cpp

	/* zaletą takiego podejścia jest to że w ten sposób konstruktory
	 * są czymś co potencjalnie daje się kompilować
	 *
	 */
}
