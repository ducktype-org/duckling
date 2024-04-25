import { RiftElement, Stmt, stmtFactory } from "./elements";
import { Identifier, identifierFactory } from "./identifier";
import { SemanticToken } from "./common";
import { SemanticTokenTypes } from "vscode-languageserver";
import { identifier } from ".";
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

	getElements(): RiftElement[] {
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