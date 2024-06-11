import { CompletionItem, CompletionItemKind, Connection, TextDocumentPositionParams, TextDocuments} from "vscode-languageserver";
import { TextDocument } from 'vscode-languageserver-textdocument';
import { CompilerDaemonClient, LSPKeywordData } from './compilerDaemonClient';
import { Identifier, OptionalIdentifier } from "./lsptree/elements/identifier";
import { DucklingElement } from "./lsptree/elements/elements";

let cachedKeywords: CompletionItem[] = [];
let cachedOperators: String[] = [];
let cachedSpecials: String[] = [];

export async function preloadKeywords(client: CompilerDaemonClient, connection: Connection) {
	try {
		const { keywords, operators, specials }: LSPKeywordData = await client.getKeywords(connection);
		cachedOperators = operators;
		cachedSpecials = specials;
		cachedKeywords = keywords.map((keyword, index) => ({
			label: keyword,
			kind: CompletionItemKind.Keyword,
			data: index
		}));
	} catch (error) {
		console.error("Failed to load keyword data:", error);
	}
}

export function getCompletionItems(
	textDocumentPosition: TextDocumentPositionParams,
	documents: TextDocuments<TextDocument>,
	lsptCache: Map<string, DucklingElement | null>,
): CompletionItem[] {
	const document = documents.get(textDocumentPosition.textDocument.uri);//TODO
	const position = textDocumentPosition.position;
	if (!document) {
		return [];
	}
	const text = document.getText();
	const lineText = text.split('\n')[position.line].substring(0, position.character);
	if (lineText.endsWith('.')) {
		// Fetch and return identifiers if the last character is a dot
		return getIdentifiers(lsptCache);
	} else {
		// Return both keywords and identifiers otherwise
		return cachedKeywords.concat(getIdentifiers(lsptCache));
	}
}


// This handler resolves additional information for the item selected in
// the completion list.
export function onCompletionResolve(item: CompletionItem): CompletionItem {
	if (item.data === 1) {
		item.detail = "Duckling - najlepszy język programowania";
		item.documentation = "Kocham piwo";
	} else if (item.data === 2) {
		item.detail = "JavaScript details";
		item.documentation = "JavaScript documentation";
	}

	return item;
}

export function getIdentifiers(lsptCache: Map<string, DucklingElement | null>) {
	let acc: Map<String, CompletionItem> = new Map();

	lsptCache.forEach((value, key) => {
		// Each value is a LSPT TopLevel node, we want to extract identifiers from those
		if (value) {
			traverseTree(value, key, acc);
		}
	});
	return [...(acc.values())];
}

function traverseTree(node: DucklingElement, key: string, acc: Map<String, CompletionItem>) {
	node?.getElements().forEach((element) => {
		if (element instanceof Identifier || element instanceof OptionalIdentifier) {
			let resElem = element.name;
			if (resElem) {
				let res: CompletionItem = {
					label: resElem,
					kind: CompletionItemKind.Variable, // Variable for now
					detail: key,
					documentation: "STRICTLY SPEAKING DEBUG",
				};
				acc.set(resElem, res);
			}
		} else {
			traverseTree(element, key, acc);
		}
	});
}