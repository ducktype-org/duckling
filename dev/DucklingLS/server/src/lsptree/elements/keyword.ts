import { SemanticTokenTypes } from "vscode-languageserver";
import { SemanticToken } from "./common";
import { ExprElem, exprElemFactory } from "./expression";
import { DucklingElement } from "./elements";

export class KeywordValue extends ExprElem {
	value: string;
	constructor(json: any) {
		super(json);
		this.value = json["value"];
	}

	getElements(): DucklingElement[] {
		return [];
	}

	getSemanticTokens() {
		return [SemanticToken.fromPosition(this.source_position, SemanticTokenTypes.keyword, [])];
	}
}

exprElemFactory.register("KeywordValue", KeywordValue);