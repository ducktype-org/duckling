import { DucklingElement, Stmt, stmtFactory } from "./elements";
import { SemanticToken } from "./common";
import { SemanticTokenTypes } from "vscode-languageserver";
import { DottedName } from "./dotted_name";

export class Alias extends Stmt {
	name: string;
	points_to?: DottedName;

	constructor(json: any) {
		super(json);
		this.name = json["position"];
		if (json["points_to"]) {
			this.points_to = new DottedName(json["points_to"]);
		}
	}

	getElements(): DucklingElement[] {
		return this.points_to !== undefined ? [this.points_to] : [];
	}

	getSemanticTokens(): SemanticToken[] {
		const tokens: SemanticToken[] = [SemanticToken.fromPosition(this.source_position, SemanticTokenTypes.variable, [])];
		if (this.points_to !== undefined) {
			tokens.push(...this.points_to.getSemanticTokens());
		}
		return tokens;
	}
}

stmtFactory.register("Alias", Alias);