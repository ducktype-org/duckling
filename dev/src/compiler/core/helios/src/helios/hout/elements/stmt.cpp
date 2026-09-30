#include "stmt.hpp"

#include "../visitors.hpp"

#include <base/except/exceptions.hpp>

namespace compiler::helios::code {
#define STMT_VISITOR(type) \
	void type::acceptVisitor(HoutStmtVisitor& visitor) const { visitor.visit##type(*this); }

	STMT_VISITOR(ReturnStmt)
	STMT_VISITOR(VoidReturnStmt)
	STMT_VISITOR(ExprStmt)
	STMT_VISITOR(IfStmt)
	STMT_VISITOR(WhileStmt)
	STMT_VISITOR(BreakStmt)
	STMT_VISITOR(ContinueStmt)
	STMT_VISITOR(VariableStmt)
	STMT_VISITOR(AssignmentStmt)
	STMT_VISITOR(BlockStmt)

	namespace {
		constexpr usize INDENT_SIZE = 4;

		void addIndent(std::ostream& out, usize indent) {
			out << std::string().append(indent * INDENT_SIZE, ' ');
		}

		const char* controlFlowKindName(ControlFlowKind kind) {
			switch (kind) {
			case ControlFlowKind::If:
				return "if";
			case ControlFlowKind::While:
				return "while";
			case ControlFlowKind::For:
				return "for";
			case ControlFlowKind::Block:
				return "block";
			}
			return "unknown";
		}

		void printControlFlowTargetSelector(std::ostream& out, const ControlFlowTargetSelector& selector) {
			if (const auto* named = std::get_if<NamedTarget>(&selector))
				out << " " << name(named->id).strView();
			else if (const auto* kind = std::get_if<KindTarget>(&selector))
				out << " " << controlFlowKindName(kind->kind);
		}
	}

	Box<CodeBlock> CodeBlock::clone() const {
		std::vector<Box<Stmt>> cloned;
		cloned.reserve(statements.size());
		for (const auto& stmt: statements) cloned.emplace_back(stmt->clone());
		return makeBox<CodeBlock>(std::move(cloned));
	}

	Box<Parameter> Parameter::clone() const {
		return makeBox<Parameter>(
			name,
			type,
			initial_value.map([](auto& value) -> BoxOrCRef<Expr> { return value->clone(); }),
			helios_symbol,
			origin
		);
	}

	void ReturnStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "return ";
		this->value->debugPrint(out);
		out << ";\n";
	}

	Box<Stmt> ReturnStmt::clone() const { return makeBox<ReturnStmt>(origin, value->clone()); }

	void VoidReturnStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "void return;\n";
	}

	Box<Stmt> VoidReturnStmt::clone() const { return makeBox<VoidReturnStmt>(origin); }

	void ExprStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "do ";
		expr->debugPrint(out);
		out << "\n";
	}

	Box<Stmt> ExprStmt::clone() const { return makeBox<ExprStmt>(origin, expr->clone()); }

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

	Box<Stmt> IfStmt::clone() const {
		return makeBox<IfStmt>(
			origin,
			condition->clone(),
			std::move(*then_body.clone()),
			std::move(*else_body.clone()),
			control_flow_id
		);
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

	Box<Stmt> WhileStmt::clone() const {
		return makeBox<WhileStmt>(
			origin, condition->clone(), std::move(*body.clone()), control_flow_id, control_flow_kind
		);
	}

	void BreakStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "break";
		printControlFlowTargetSelector(out, target_selector);
		out << ";\n";
	}

	Box<Stmt> BreakStmt::clone() const { return makeBox<BreakStmt>(origin, target_selector); }

	void ContinueStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "continue";
		printControlFlowTargetSelector(out, target_selector);
		out << ";\n";
	}

	Box<Stmt> ContinueStmt::clone() const {
		return makeBox<ContinueStmt>(origin, target_selector);
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

	Box<Stmt> VariableStmt::clone() const {
		return makeBox<VariableStmt>(origin, initial_value->clone(), type, helios_symbol);
	}

	void AssignmentStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		location_expr->debugPrint(out);
		out << " = ";
		new_value_expr->debugPrint(out);
		out << ";\n";
	}

	Box<Stmt> AssignmentStmt::clone() const {
		return makeBox<AssignmentStmt>(origin, location_expr->clone(), new_value_expr->clone());
	}

	void BlockStmt::debugPrint(std::ostream& out, usize indent) const {
		addIndent(out, indent);
		out << "{\n";
		for (const auto& stmt: body.statements) stmt->debugPrint(out, indent + 1);
		addIndent(out, indent);
		out << "}\n";
	}

	Box<Stmt> BlockStmt::clone() const {
		return makeBox<BlockStmt>(
			origin, std::move(*body.clone()), control_flow_id, control_flow_kind
		);
	}

}
