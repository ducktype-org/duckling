import { Stmt, stmtFactory, StmtFactory } from "./elements";
import { ElementFactory } from "./element_factory";
import { unpackJSONObject } from "./utils";
import { exprElemFactory } from "./expression";
import { SemanticToken } from "./common";
import { SemanticTokenTypes } from "vscode-languageserver";

export class Identifier extends Stmt {
	name: string;

	constructor(json: any) {
		super(json);
		this.name = json["value"];

	}

	getElements(): Stmt[] {
		return [];
	}

	getSemanticTokens(): SemanticToken[]{
		return [SemanticToken.fromPosition(this.source_position, SemanticTokenTypes.variable, [])];
	}
}

export class OptionalIdentifier extends Identifier {
	constructor(json: any) {
		super(json);
	}

	static create = (json: any): OptionalIdentifier | undefined => {
		console.log(json);
		if (json === "<ANONYMOUS>")
			return undefined;
		return new OptionalIdentifier(unpackJSONObject(json, "Name"));
	};
}


// ORDER OF THESE INITIALIZATION POTENTIALLY MATTERS

export type OptionalIdentifierFactory = ElementFactory<OptionalIdentifier, StmtFactory>;

OptionalIdentifier.create = (json: any): OptionalIdentifier | undefined => {
	if (json === "<ANONYMOUS>")
		return undefined;
	return optionalIdentifierFactory.create(json);
};

export const optionalIdentifierFactory = new ElementFactory<OptionalIdentifier, StmtFactory>(stmtFactory);

optionalIdentifierFactory.register("Identifier", OptionalIdentifier);
optionalIdentifierFactory.register("Name", OptionalIdentifier);

export type IdentifierFactory = ElementFactory<Identifier, StmtFactory>;
export const identifierFactory = new ElementFactory<Identifier, StmtFactory>(stmtFactory);

identifierFactory.register("Identifier", Identifier);
identifierFactory.register("Name", Identifier);

exprElemFactory.register("Identifier", Identifier);
exprElemFactory.register("Name", Identifier);
