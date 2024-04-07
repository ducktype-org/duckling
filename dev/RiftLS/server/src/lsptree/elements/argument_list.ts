import { NotStmt, notStmtFactory, NotStmtFactory, RiftElement } from "./elements";
import { ElementFactory } from "./element_factory";
import { SemanticToken } from "./common";

export class ArgList extends NotStmt {
	arguments: NotStmt[];

	constructor(json: any) {
		super(json);
		this.arguments = [];
		if (json.has("arguments")) // TODO
			for (const arg_data of json.get("arguments")){
				const arg = notStmtFactory.create(arg_data);
				if (arg) this.arguments.push(arg);
			}
	}
    
	getElements(): RiftElement[] {
		return this.arguments;
	}

	getSemanticTokens(): SemanticToken[] {
		return this.arguments.flatMap(arg => arg.getSemanticTokens());
	}
}

type ArgListFactory = ElementFactory<ArgList, NotStmtFactory>;

export const argListFactory = new ElementFactory<ArgList, NotStmtFactory>(notStmtFactory);

argListFactory.register("ArgList", ArgList);

