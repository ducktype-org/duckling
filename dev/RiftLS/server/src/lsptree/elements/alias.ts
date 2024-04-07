import { RiftElement, Stmt, stmtFactory } from "./elements";
import { Identifier, identifierFactory } from "./identifier";
import { SemanticToken } from "./common";
import { SemanticTokenTypes } from "vscode-languageserver";
import { identifier } from ".";

export class Alias extends Stmt {
	name: string;
	points_to: Identifier[];

	constructor(json: any) {
		super(json);
		this.name = json["position"];
		this.points_to = [];
		for (const id of json["points_to"]) {
			const iden = identifierFactory.create(id);
			if (iden) this.points_to.push(iden);
		}
	}

	getElements(): RiftElement[] {
		return this.points_to;
	}

	getSemanticTokens(): SemanticToken[] {
		const tokens: SemanticToken[] = [SemanticToken.fromPosition(this.source_position, SemanticTokenTypes.variable, [])];
		this.points_to.forEach(element => {
			tokens.push(...element.getSemanticTokens());
		});
		return tokens;
	}
}

stmtFactory.register("Alias", Alias);