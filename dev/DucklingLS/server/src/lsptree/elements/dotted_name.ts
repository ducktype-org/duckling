import { NotStmt } from "./elements";
import { Identifier, identifierFactory } from "./identifier";
import { ElementFactory } from "./element_factory";
import { NotStmtFactory, notStmtFactory } from "./elements";

class DottedName extends NotStmt {
	star: boolean = false;
	names: Identifier[] = [];

	constructor(json: any) {
		super(json);
		if (json["star"]) this.star = json["star"];
		for (const name of json["names"]) {
			this.names.push(identifierFactory.createDefined(name));
		}
	}

	getElements() {
		return this.names;
	}

	getSemanticTokens() {
		return this.names.flatMap(name => name.getSemanticTokens());
	}
}

type DottedNameFactory = ElementFactory<DottedName, NotStmtFactory>;
export const dottedNameFactory = new ElementFactory<DottedName, NotStmtFactory>(notStmtFactory);

dottedNameFactory.register("DottedName", DottedName);

export { DottedName };
