import { SemanticToken } from "./common";
import { RiftElement, Stmt, stmtFactory } from "./elements";
import { Identifier, identifierFactory } from "./identifier";
import { SemanticTokenTypes } from "vscode-languageserver";


export class Import extends Stmt {
	names: Identifier[];

	constructor(json: any) {
		super(json);
		this.names = [];
		for (const name_params of json["names"]) {
			const name = identifierFactory.create(name_params);
			if (name) this.names.push(name);
		}
	}

	getElements(): RiftElement[] {
		return this.names;
	}

	getSemanticTokens(): SemanticToken[] {
		const tokens = [SemanticToken.fromPosition(this.source_position, SemanticTokenTypes.keyword, [])];
		this.names.forEach(
			name => tokens.push(...name.getSemanticTokens())
		);
		return tokens;
	}
}

stmtFactory.register("Import", Import);