import {Stmt, stmtFactory} from "./elements";
import {Identifier, identifierFactory} from "./identifier";
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

	getSemanticTokens(): SemanticToken[] {
		const tokens: SemanticToken[] = [];
		this.points_to.forEach(element => {
			tokens.push(...element.getSemanticTokens());
		});
		return tokens;
	}
}

stmtFactory.register("Alias", Alias);