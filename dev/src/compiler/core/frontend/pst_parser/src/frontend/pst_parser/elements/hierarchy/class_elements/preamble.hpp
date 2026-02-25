#pragma once

#define CLASS_STMT_CHILD_CONSTRUCTOR(class_name, kind)                                           \
	class_name(const dia::SourcePosition& position): ClassStmt(StmtKind::class_name, position) { \
		this->element_kind = kind;                                                               \
	}

#define CLASS_STMT_PASS_CONSTRUCTOR(class_name) \
	class_name(StmtKind kind, const dia::SourcePosition& position): ClassStmt(kind, position) {}

#define CLASS_STMT_SPEC_CONSTRUCTOR(class_name)          \
	class_name(const dia::SourcePosition& position):     \
		  ClassSpecial(StmtKind::class_name, position) { \
		this->element_kind = ElementKind::ClassSpecial;  \
	}

#define CLASS_STMT_PARSE(class_name) static MBox<class_name> parse(LangParserState& state)
