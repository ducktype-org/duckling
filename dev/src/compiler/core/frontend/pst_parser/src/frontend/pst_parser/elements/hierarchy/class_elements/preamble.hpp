#pragma once

#define CLASS_STMT_CHILD_CONSTRUCTOR(class_name, kind)                                 \
	class_name(const LangParserState& state): ClassStmt(StmtKind::class_name, state) { \
		this->element_kind = kind;                                                     \
	}

#define CLASS_STMT_PASS_CONSTRUCTOR(class_name) \
	class_name(StmtKind kind, const LangParserState& state): ClassStmt(kind, state) {}

#define CLASS_STMT_SPEC_CONSTRUCTOR(class_name)                                           \
	class_name(const LangParserState& state): ClassSpecial(StmtKind::class_name, state) { \
		this->element_kind = ElementKind::ClassSpecial;                                   \
	}

#define CLASS_STMT_PARSE(class_name) static MBox<class_name> parse(LangParserState& state)

#define PARSE_DECL() static MBox<ThisClass> parse(LangParserState& state)
