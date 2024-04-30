#pragma once

#include "../rift_parser_base.hpp"

#include <token_parser_core/token_stream.hpp>
#include <token_parser_core/parser_state.hpp>
#include <token_parser_core/base_element.hpp>
#include <token_parser_core/common_elements.hpp>
#include <token_parser_core/parser_ref.hpp>


#include <base/string_id.hpp>

#include <base/variant.hpp>
#include <iostream>
#include <span>
#include <utility>

namespace lsp {
	using dia::SourcePosition;
	using std::string;
	using std::vector;
	using tpc::makeRef;
	using tpc::ParserRef;

	inline tpc::Identifier default_tpc_identifier() { return tpc::Identifier{ base::StrId() }; }


	class LSPExpr;

	enum class StmtKind {
		Attribute,
		Import,
		Using,
		Alias,
		Fun,
		Namespace,
		CodeDecl,
		Action,
		Expr,
		Struct,
		TopLevel,
		Const,

		Decl,  // @TODO: this isn't a StmtKind in PST
		EagerLookup,
	};

	class LSPElement {
	public:
		SourcePosition position = SourcePosition::getBadPosition();
		LSPElement()            = default;
		virtual ~LSPElement()   = default;
		virtual void lsp_print(std::ostream& out);
	};

	class LSPStmt: public LSPElement {
	public:
		StmtKind kind      = StmtKind::Attribute;
		LSPStmt()          = default;
		virtual ~LSPStmt() = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPNotStmt: public LSPElement {
	public:
		LSPNotStmt()                               = default;
		virtual ~LSPNotStmt()                      = default;
		void lsp_print(std::ostream& out) override = 0;
	};

	template<typename T>
	concept LSPElementSubclass = std::is_base_of_v<LSPElement, T>;

	template<LSPElementSubclass T>
	class LSPList: public LSPNotStmt {
	public:
		std::vector<ParserRef<T>> elements = {};
		LSPList()                          = default;
		virtual ~LSPList()                 = default;

		void lsp_print(std::ostream& out) override;

		void lsp_list_parametrized_print(std::ostream& out, const string& name);
	};

	void positionPrint(std::ostream&, const dia::SourcePosition&);

	class LSPDottedName final: public LSPNotStmt {
	public:
		bool                    star = false;
		vector<tpc::Identifier> names;
		LSPDottedName()          = default;
		virtual ~LSPDottedName() = default;
		void lsp_print(std::ostream& out);
	};

	class LSPImport: public LSPStmt {
	public:
		ParserRef<LSPDottedName> names;
		tpc::Identifier          alias = default_tpc_identifier();
		LSPImport()                    = default;
		virtual ~LSPImport()           = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPUsing: public LSPStmt {
	public:
		ParserRef<LSPDottedName> names;
		LSPUsing()          = default;
		virtual ~LSPUsing() = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPAlias: public LSPStmt {
	public:
		tpc::Identifier          name = default_tpc_identifier();
		ParserRef<LSPDottedName> points_to;
		LSPAlias()          = default;
		virtual ~LSPAlias() = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPCodeBlock: public LSPNotStmt {
	public:
		vector<ParserRef<LSPStmt>> statements = {};
		LSPCodeBlock()                        = default;
		virtual ~LSPCodeBlock()               = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPCodeBlockOrStmt: public LSPNotStmt {
	public:
		std::variant<ParserRef<LSPStmt>, ParserRef<LSPCodeBlock>> content
			= static_cast<ParserRef<LSPStmt>>(nullptr);
		LSPCodeBlockOrStmt() = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPRoundGroupExpr: public LSPNotStmt {
	public:
		ParserRef<LSPExpr> expr      = nullptr;
		LSPRoundGroupExpr()          = default;
		virtual ~LSPRoundGroupExpr() = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPExpr: public LSPStmt {
	public:
		enum class GroupType {
			RoundGroup  = 0,
			SquareGroup = 1,
			CurlyGroup  = 2,
			AngleGroup  = 3,
		};

		struct KeywordValue {
			rift_def::Keyword keyword  = rift_def::Keyword::NotAKeyword;
			SourcePosition    position = SourcePosition::getBadPosition();
			void              lsp_print(std::ostream&);
		};

		struct Group {
			GroupType          type     = GroupType::RoundGroup;
			ParserRef<LSPExpr> expr     = nullptr;
			SourcePosition     position = SourcePosition::getBadPosition();
			void               lsp_print(std::ostream&);
		};

		struct Operator {
			base::StrId    oper_id  = base::StrId();
			SourcePosition position = SourcePosition::getBadPosition();
			void           lsp_print(std::ostream&);
		};

		struct Identifier {
			base::StrId    indent_id = base::StrId();
			SourcePosition position  = SourcePosition::getBadPosition();
			void           lsp_print(std::ostream&);
		};

		struct NumLiteral {
			base::StrId    num_id   = base::StrId();
			SourcePosition position = SourcePosition::getBadPosition();
			void           lsp_print(std::ostream&);
		};

		using LSPExprElem = std::variant<Operator, Identifier, NumLiteral, Group, KeywordValue>;

		vector<LSPExprElem> elements = {};
		LSPExpr()                    = default;
		virtual ~LSPExpr()           = default;
		void lsp_print(std::ostream& out);
	};

	class LSPAction: public LSPStmt {
	public:
		std::optional<ParserRef<LSPExpr>> expr = std::nullopt;
		LSPAction()                            = default;
		void parametrizedLSPPrint(
			std::ostream&                            out,
			const std::optional<ParserRef<LSPExpr>>& action,
			std::string_view                         name,
			const std::string&                       preposition
		) const;
		virtual ~LSPAction() = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPReturn: public LSPAction {
	public:
		LSPReturn()          = default;
		virtual ~LSPReturn() = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPBreak: public LSPAction {
	public:
		LSPBreak()          = default;
		virtual ~LSPBreak() = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPContinue: public LSPAction {
	public:
		LSPContinue()          = default;
		virtual ~LSPContinue() = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPRedo: public LSPAction {
	public:
		LSPRedo()          = default;
		virtual ~LSPRedo() = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPRestart: public LSPAction {
	public:
		LSPRestart()          = default;
		virtual ~LSPRestart() = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPDefer: public LSPAction {
	public:
		LSPDefer()          = default;
		virtual ~LSPDefer() = default;
		void lsp_print(std::ostream& out) override;
	};

	// @TODO: should it be an action
	class LSPThrow: public LSPAction {
	public:
		LSPThrow()          = default;
		virtual ~LSPThrow() = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPConst: public LSPStmt {
	public:
		tpc::Identifier    name  = default_tpc_identifier();
		ParserRef<LSPExpr> type  = nullptr;
		ParserRef<LSPExpr> value = nullptr;
		LSPConst()               = default;
		virtual ~LSPConst()      = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPDecl: public LSPStmt {
	public:
		LSPDecl()          = default;
		virtual ~LSPDecl() = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPCodeDecl: public LSPDecl {
	public:
		LSPCodeDecl()          = default;
		virtual ~LSPCodeDecl() = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPTopLevel: public LSPDecl {
	public:
		vector<ParserRef<LSPStmt>> statements = {};
		LSPTopLevel()                         = default;
		virtual ~LSPTopLevel()                = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPNamespace: public LSPDecl {
	public:
		tpc::Identifier         name = default_tpc_identifier();
		ParserRef<LSPCodeBlock> body = nullptr;
		LSPNamespace()               = default;
		virtual ~LSPNamespace()      = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPStruct: public LSPDecl {
	public:
		tpc::Identifier            name  = default_tpc_identifier();
		vector<ParserRef<LSPExpr>> bases = {};
		ParserRef<LSPCodeBlock>    body  = nullptr;
		LSPStruct()                      = default;
		virtual ~LSPStruct()             = default;
		void lsp_print(std::ostream& out) override;
	};

	using LSPParamList = LSPList<LSPExpr>;
	using LSPRetList   = LSPList<LSPExpr>;

	class LSPFun: public LSPDecl {
	public:
		tpc::Identifier               name   = default_tpc_identifier();
		ParserRef<LSPParamList>       params = nullptr;
		ParserRef<LSPRetList>         rets   = nullptr;
		ParserRef<LSPCodeBlockOrStmt> body   = nullptr;
		LSPFun()                             = default;

		virtual ~LSPFun() = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPBlock: public LSPCodeDecl {
	public:
		tpc::OptionalIdentifier optional_name = tpc::OptionalIdentifier();
		ParserRef<LSPCodeBlock> code_block    = nullptr;
		LSPBlock()                            = default;
		virtual ~LSPBlock()                   = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPIf: public LSPCodeDecl {
	public:
		ParserRef<LSPRoundGroupExpr>  condition     = nullptr;
		tpc::OptionalIdentifier       optional_name = tpc::OptionalIdentifier();
		ParserRef<LSPCodeBlockOrStmt> body          = nullptr;
		ParserRef<LSPCodeBlockOrStmt> else_body     = nullptr;
		LSPIf()                                     = default;
		virtual ~LSPIf()                            = default;
		void lsp_print(std::ostream& out) override;
	};

	class LSPWhile: public LSPCodeDecl {
	public:
		ParserRef<LSPRoundGroupExpr>  condition     = nullptr;
		tpc::OptionalIdentifier       optional_name = tpc::OptionalIdentifier();
		ParserRef<LSPCodeBlockOrStmt> body          = nullptr;
		LSPWhile()                                  = default;
		virtual ~LSPWhile()                         = default;
		void lsp_print(std::ostream& out) override;
	};

	using LSPArgList = LSPList<LSPExpr>;

	class LSPAttribute: public LSPStmt {
	public:
		tpc::Identifier       name = default_tpc_identifier();
		ParserRef<LSPArgList> args = nullptr;
		LSPAttribute()             = default;
		virtual ~LSPAttribute()    = default;
		void lsp_print(std::ostream& out) override;
	};
}
