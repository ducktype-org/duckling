import { Identifier, OptionalIdentifier, optionalIdentifierFactory } from "./identifier";
import { CodeDecl, codeDeclFactory, notStmtFactory } from "./elements";
import { RoundGroupExpr } from "./round_group_expr";
import { CodeBlockOrStmt, codeBlockOrStmtFactory } from "./code_block_or_statement";
import { SemanticToken } from "./common";
import { SemanticTokenTypes } from "vscode-languageserver";

export class If extends CodeDecl {
	condition?: RoundGroupExpr;
	body?: CodeBlockOrStmt;
	name?: Identifier;
	constructor(json: any) {
		super(json);
		this.name = OptionalIdentifier.create(json["name"]);
		if (json["condition"] !== "<nullptr>")
			this.condition = notStmtFactory.create(json["condition"]);
		if (json["body"] !== "<nullptr>")
			this.body = codeBlockOrStmtFactory.create(json["body"]);
	}
    
	getSemanticTokens() {
		const tokens = [SemanticToken.fromPosition(this.source_position, SemanticTokenTypes.keyword, [])];
		if (this.name)
			tokens.push(...this.name.getSemanticTokens());
		if (this.condition)
			tokens.push(...this.condition.getSemanticTokens());
		if (this.body)
			tokens.push(...this.body.getSemanticTokens());
		return tokens;
	}
}

codeDeclFactory.register("If", If);