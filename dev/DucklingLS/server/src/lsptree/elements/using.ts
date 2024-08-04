import { SemanticTokenTypes } from "vscode-languageserver";
import { SemanticToken } from "./common";
import { DucklingElement, Stmt, stmtFactory } from "./elements";
import { DottedName, dottedNameFactory } from "./dotted_name";


export class Using extends Stmt {
	names?: DottedName;

	constructor(json: any) {
		super(json);
		if (json["names"]) {
			this.names = dottedNameFactory.createDefined(json["names"]);
		}
	}

	getElements(): DucklingElement[] {
		return this.names !== undefined ? [this.names] : [];
	}

	getSemanticTokens(): SemanticToken[] {
		const tokens = [SemanticToken.fromPosition(this.source_position, SemanticTokenTypes.keyword, [])];
		if (this.names !== undefined) {
			tokens.push(...this.names.getSemanticTokens());
		}
		return tokens;
	}
}

stmtFactory.register("Using", Using);