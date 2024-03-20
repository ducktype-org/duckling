import {Decl, DeclFactory, declFactory} from "./elements";
import { Identifier, identifierFactory } from "./identifier";
import { ParamList, paramListFactory } from "./param_list";
import { RetList, retListFactory } from "./ret_list";
import { CodeBlockOrStmt, codeBlockOrStmtFactory } from "./code_block_or_statement";
import { ElementFactory } from "./element_factory";
import { SemanticToken } from "./common";
import { SemanticTokenTypes } from "vscode-languageserver";


export class Fun extends Decl {
	name: Identifier;
	params?: ParamList;
	rets?: RetList;
	body?: CodeBlockOrStmt;

	constructor(json: any) {
		super(json);
		this.name = identifierFactory.createDefined(json["name"]);
		if (json["params"] !== "<nullptr>") {
			this.params = paramListFactory.create(json["params"]);
		}
		if (json["rets"] !== "<nullptr>") {
			this.rets = retListFactory.create(json["rets"]);
		}
		if (json["body"] !== "<nullptr>") {
			this.body = codeBlockOrStmtFactory.create(json["body"]);
		}
	}

	getSemanticTokens() {
		const tokens = [SemanticToken.fromPosition(this.source_position, SemanticTokenTypes.keyword, [])];
		tokens.push(...this.name.getSemanticTokens());
		if (this.params)
			tokens.push(...this.params.getSemanticTokens());
		if (this.rets)
			tokens.push(...this.rets.getSemanticTokens());
		if (this.body)
			tokens.push(...this.body.getSemanticTokens());
		return tokens;
	}
}

export type FunctionFactory = ElementFactory<Fun, DeclFactory>;
export const functionFactory = new ElementFactory<Fun, DeclFactory>(declFactory);

functionFactory.register("Fun", Fun);