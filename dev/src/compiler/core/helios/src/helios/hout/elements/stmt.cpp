#include "stmt.hpp"

#include "../visitors.hpp"

namespace compiler::helios::code {
#define STMT_VISITOR(type) \
	void type::acceptVisitor(HoutStmtVisitor& visitor) const { visitor.visit##type(*this); }

	STMT_VISITOR(ReturnStmt)
	STMT_VISITOR(VoidReturnStmt)
	STMT_VISITOR(ExprStmt)
	STMT_VISITOR(IfStmt)
	STMT_VISITOR(WhileStmt)
	STMT_VISITOR(VariableStmt)
	STMT_VISITOR(AssignmentStmt)
	STMT_VISITOR(BlockStmt)
	STMT_VISITOR(MatchStmt)

	namespace {
		constexpr usize INDENT_SIZE = 4;

		void addIndent(std::ostream& out, usize indent) {
			out << std::string().append(indent * INDENT_SIZE, ' ');
		}
	}

	void ReturnStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "return ";
		this->value->debugPrint(out);
		out << ";\n";
	}

	void VoidReturnStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "void return;\n";
	}

	void ExprStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "do ";
		expr->debugPrint(out);
		out << "\n";
	}

	void IfStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "if (";
		condition->debugPrint(out);
		out << ") {\n";
		for (const auto& stmt: then_body.statements) stmt->debugPrint(out, indent + 1);
		addIndent(out, indent);
		out << "} else {\n";
		for (const auto& stmt: else_body.statements) stmt->debugPrint(out, indent + 1);
		addIndent(out, indent);
		out << "}\n";
	}

	void WhileStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "while (";
		condition->debugPrint(out);
		out << ") {\n";
		for (const auto& stmt: body.statements) stmt->debugPrint(out, indent + 1);
		addIndent(out, indent);
		out << "}\n";
	}

	void VariableStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "var ";
		out << name(this->helios_symbol).strView();
		out << " : ";

		// this might not be correct:?
		out << this->type.toString();
		out << " = ";
		this->initial_value->debugPrint(out);


		out << ";\n";
	}

	void AssignmentStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		location_expr->debugPrint(out);
		out << " = ";
		new_value_expr->debugPrint(out);
		out << ";\n";
	}

	void BlockStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "{\n";
		for (const auto& stmt: body.statements) stmt->debugPrint(out, indent + 1);
		addIndent(out, indent);
		out << "}\n";
	}

	void MatchStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "match (";
		subject->debugPrint(out);
		out << ") {\n";
		for (const auto& match_case: cases) {
			addIndent(out, indent + 1);
			if (match_case.alternative_index.has_value())
				out << "case [alt=" << match_case.alternative_index.value() << "] {\n";
			else
				out << "case [wildcard] {\n";
			for (const auto& stmt: match_case.body.statements) stmt->debugPrint(out, indent + 2);
			addIndent(out, indent + 1);
			out << "}\n";
		}
		addIndent(out, indent);
		out << "}\n";
	}
}
