import { SemanticTokenTypes } from "vscode-languageserver";
import { SemanticToken } from "./common";
import { ExprElem, exprElemFactory } from "./expression";


export class NumLiteral extends ExprElem {
	value: string;
	constructor(json: any) {
		super(json);
		this.value = json["value"];
	}

	getSemanticTokens() {
		return [SemanticToken.fromPosition(this.source_position, SemanticTokenTypes.number, [])];
	}
}

exprElemFactory.register("NumLiteral", NumLiteral);