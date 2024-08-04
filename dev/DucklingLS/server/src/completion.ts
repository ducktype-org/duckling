import { CompletionItem, CompletionItemKind, Connection, TextDocumentPositionParams, TextDocuments} from "vscode-languageserver";
import { TextDocument } from 'vscode-languageserver-textdocument';
import { CompilerDaemonClient, LSPKeywordData } from './compilerDaemonClient';
import { Identifier, OptionalIdentifier } from "./lsptree/elements/identifier";
import { DucklingElement } from "./lsptree/elements/elements";

// Cached arrays to store keywords, operators, and special keywords for completion
let cachedKeywords: CompletionItem[] = [];
let cachedOperators: String[] = [];
let cachedSpecials: String[] = [];

/**
 * Preloads keywords from the CompilerDaemonClient and caches them.
 * @param client - Instance of CompilerDaemonClient to fetch keywords from.
 * @param connection - The connection to the language server.
 */
export async function preloadKeywords(client: CompilerDaemonClient, connection: Connection) {
	try {
		// Fetch keywords, operators, and special keywords
		const { keywords, operators, specials }: LSPKeywordData = await client.getKeywords(connection);
		cachedOperators = operators;
		cachedSpecials = specials;
		// Map fetched keywords to CompletionItems
		cachedKeywords = keywords.map((keyword, index) => ({
			label: keyword,
			kind: CompletionItemKind.Keyword,
			data: index
		}));
	} catch (error) {
		console.error("Failed to load keyword data:", error);
	}
}

/**
 * Provides completion items based on the current text document position.
 * @param textDocumentPosition - The position in the text document.
 * @param documents - The collection of text documents.
 * @param lsptCache - Cache of Duckling elements.
 * @returns An array of CompletionItems.
 */
export function getCompletionItems(
	textDocumentPosition: TextDocumentPositionParams,
	documents: TextDocuments<TextDocument>,
	lsptCache: Map<string, DucklingElement | null>,
): CompletionItem[] {
	const document = documents.get(textDocumentPosition.textDocument.uri);
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

/**
 * Resolves additional information for a selected completion item.
 * @param item - The completion item to resolve.
 * @returns The resolved completion item.
 */
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

/**
 * Retrieves identifiers from the LSPT cache.
 * @param lsptCache - Cache of Duckling elements.
 * @returns An array of CompletionItems for identifiers.
 */
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

/**
 * Traverses a Duckling element tree to extract identifiers.
 * @param node - The current Duckling element node.
 * @param key - The key associated with the current node.
 * @param acc - Accumulator map for collected identifiers.
 */
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
			// Recursively traverse the tree for other elements
			traverseTree(element, key, acc);
		}
	});
}
