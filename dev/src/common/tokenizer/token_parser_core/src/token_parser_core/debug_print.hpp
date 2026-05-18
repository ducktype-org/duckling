#pragma once

#include "common_elements.hpp"

#include <base/pointers/box.hpp>

#include <lang_definitions/key_spec_op.hpp>

namespace tpc {
	using lang_def::Keyword;
	using lang_def::NamedOperator;
	using lang_def::Special;
	using lexer::Operator;

	void nullAwareDprint(Identifier, std::ostream& out);
	void nullAwareDprint(Keyword, std::ostream& out);
	void nullAwareDprint(Operator, std::ostream& out);
	void nullAwareDprint(Special, std::ostream& out);

	template<typename T>
	void nullAwareDprint(const Box<T>& ref, std::ostream& out) {
		// This templates's logic is very weird...
		// It implies that if not bool(*ref) then <nullptr> else dprint...
		if (!ref)
			out << "\"<nullptr>\"";
		else
			ref->debugPrint(out);
	}

	template<typename T>
	void nullAwareDprint(const MBox<T>& ref, std::ostream& out) {
		if (!ref)
			out << "\"<nullptr>\"";
		else
			ref->debugPrint(out);
	}

	template<typename T>
	void nullAwareDprint(MCRef<T> ref, std::ostream& out) {
		if (!ref)
			out << "\"<nullptr>\"";
		else
			ref->debugPrint(out);
	}
}
