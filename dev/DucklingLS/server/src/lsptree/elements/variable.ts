import { Decl, DeclFactory, declFactory, DucklingElement } from "./elements";
import { Identifier, identifierFactory } from "./identifier";
import { ElementFactory } from "./element_factory";
import { SemanticToken } from "./common";
import { SemanticTokenTypes } from "vscode-languageserver";
import { exprFactory, Expr } from "./expression";
import { getTokenTypeIndex } from "../../semanticTokensDeclarations";


export class Variable extends Decl {
	name: Identifier;
	type?: Expr;
	value?: Expr;
	is_const: boolean = true;

	constructor(json: any) {
		super(json);
		this.name = identifierFactory.createDefined(json["name"]);
		if (json["type"] !== "<nullptr>") {
			this.type = exprFactory.createDefined(json["type"]);
		}
		if (json["value"] !== "<nullptr>") {
			this.value = exprFactory.createDefined(json["value"]);
		}
		this.is_const = json["is_const"];
	}

	getElements(): DucklingElement[] {
		const elements: DucklingElement[] = [this.name];
		if (this.type)
			elements.push(this.type);
		if (this.value)
			elements.push(this.value);
		return elements;
	}

	getSemanticTokens() {
		const tokens: SemanticToken[] = [SemanticToken.fromPosition(this.source_position, SemanticTokenTypes.keyword, [])];
		const nameToken : SemanticToken[] = this.name.getSemanticTokens();
		if (nameToken) {
			tokens.push(...nameToken);
		}
		if (this.type)
			tokens.push(...this.type.getSemanticTokens().flatMap(t => {
				t.tokenType = getTokenTypeIndex(SemanticTokenTypes.type);
				return t;
			}));
		if (this.value)
			tokens.push(...this.value.getSemanticTokens());
		return tokens;
	}
}

export type VariableFactory = ElementFactory<Variable, DeclFactory>;
export const variableFactory = new ElementFactory<Variable, DeclFactory>(declFactory);

variableFactory.register("Variable", Variable);