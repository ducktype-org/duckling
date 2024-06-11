import { SemanticToken } from "./common";
import { DucklingElement } from "./elements";
import { ExprElem, exprElemFactory } from "./expression";
import { SemanticTokenTypes } from "vscode-languageserver";
class Operator extends ExprElem {
	operator: string;

	constructor(json: any) {
		super(json);
		this.operator = json["value"];
	}

	getElements(): DucklingElement[] {
		return [];
	}

	getSemanticTokens() {
		return [SemanticToken.fromPosition(this.source_position, SemanticTokenTypes.operator, [])];
	}
}

exprElemFactory.register("Operator", Operator);