import { Decl, DeclFactory, declFactory, RiftElement } from "./elements";
import { Identifier, identifierFactory } from "./identifier";
import { CodeBlockOrStmt, codeBlockOrStmtFactory } from "./code_block_or_statement";
import { ElementFactory } from "./element_factory";
import { SemanticToken } from "./common";
import { SemanticTokenTypes } from "vscode-languageserver";
import { exprListFactory, List } from "./list";
import { Expr } from "./expression";
import { getTokenTypeIndex } from "../../semanticTokensDeclarations";


export class Fun extends Decl {
	name: Identifier;
	params?: List<Expr>;
	rets?: List<Expr>;
	body?: CodeBlockOrStmt;

	constructor(json: any) {
		super(json);
		this.name = identifierFactory.createDefined(json["name"]);
		if (json["params"] !== "<nullptr>") {
			this.params = exprListFactory.create(json["params"]);
		}
		if (json["rets"] !== "<nullptr>") {
			this.rets = exprListFactory.create(json["rets"]);
		}
		if (json["body"] !== "<nullptr>") {
			this.body = codeBlockOrStmtFactory.create(json["body"]);
		}
	}

	getElements(): RiftElement[] {
		const elements: RiftElement[] = [this.name];
		if (this.params)
			elements.push(this.params);
		if (this.rets)
			elements.push(this.rets);
		if (this.body)
			elements.push(this.body);
		return elements;
	}

	getSemanticTokens() {
		const tokens: SemanticToken[] = [SemanticToken.fromPosition(this.source_position, SemanticTokenTypes.keyword, [])];
		const nameToken : SemanticToken[] = this.name.getSemanticTokens();
		if (nameToken) {
			nameToken[0].tokenType = getTokenTypeIndex(SemanticTokenTypes.decorator);
			tokens.push(...nameToken);
		}
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