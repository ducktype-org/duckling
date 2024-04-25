import { SemanticToken } from "./common";
import { RiftElement, Stmt, stmtFactory } from "./elements";
import { Identifier, identifierFactory } from "./identifier";
import { SemanticTokenTypes } from "vscode-languageserver";
import { DottedName, dottedNameFactory } from "./dotted_name";

export class Import extends Stmt {
	names?: DottedName;
	alias?: Identifier;
	star: boolean = false;

	constructor(json: any) {
		super(json);
		this.star = json["star"];
		this.alias = identifierFactory.create(json["alias"]);
		if (json["names"]) {
			this.names = dottedNameFactory.createDefined(json["names"]);
		}
	}

	getElements(): RiftElement[] {
		const elements = [];
		if (this.alias !== undefined) {
			elements.push(this.alias);
		}
		if (this.names !== undefined) {
			elements.push(this.names);
		}
		return elements;
	}

	getSemanticTokens(): SemanticToken[] {
		const tokens = [SemanticToken.fromPosition(this.source_position, SemanticTokenTypes.keyword, [])];
		if (this.alias !== undefined) {
			tokens.push(...this.alias.getSemanticTokens());
		}
		if (this.names !== undefined) {
			tokens.push(...this.names.getSemanticTokens());
		}
		return tokens;
	}
}

stmtFactory.register("Import", Import);