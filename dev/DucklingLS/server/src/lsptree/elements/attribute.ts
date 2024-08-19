import { SemanticTokenTypes } from "vscode-languageserver";
import { SemanticToken } from "./common";
import { DucklingElement, Stmt, stmtFactory } from "./elements";
import { Expr } from "./expression";
import { Identifier, identifierFactory } from "./identifier";
import { exprListFactory, List } from "./list";
import { getTokenTypeIndex } from "../../semanticTokensDeclarations";

export class Attribute extends Stmt {
	name: Identifier;
	args?: List<Expr>;
	constructor(json: any) {
		super(json);
		this.name = identifierFactory.createDefined(json["name"]);
		if (json["args"])
			this.args = exprListFactory.createDefined(json["args"]);
	}

	getElements(): DucklingElement[] {
		const elements: DucklingElement[] = [this.name];
		if (this.args)
			elements.push(this.args);
		return elements;
	}

	getSemanticTokens() {
		const tokens: SemanticToken[] = [SemanticToken.fromPosition(this.source_position, SemanticTokenTypes.decorator, [])];
		const nameToken : SemanticToken[] = this.name.getSemanticTokens();
		if (nameToken) {
			nameToken[0].tokenType = getTokenTypeIndex(SemanticTokenTypes.decorator);
			tokens.push(...nameToken);
		}
		if (this.args)
			tokens.push(...this.args.getSemanticTokens());
		return tokens;
	}
}

stmtFactory.register("Attribute", Attribute);