import { SemanticTokenTypes } from "vscode-languageserver";
import { keyword } from ".";
import { SemanticToken } from "./common";
import { Stmt, stmtFactory } from "./elements";
import { exprFactory } from "./expression";
import { Identifier, identifierFactory } from "./identifier";


export class Using extends Stmt {
	names: Identifier[] = [];

	constructor(json: any){
		super(json);
        
		for (const name_params of json["names"]){
			const name = identifierFactory.create(name_params);
			if (name) this.names.push(name);
		}
	}


	getSemanticTokens(): SemanticToken[] {
		const tokens = [SemanticToken.fromPosition(this.source_position, SemanticTokenTypes.keyword, [])];
		this.names.forEach(name => 
			tokens.push(...name.getSemanticTokens())
		);
		return tokens;
	}
}

stmtFactory.register("Using", Using);