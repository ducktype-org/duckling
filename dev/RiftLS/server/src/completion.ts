import { CompletionItem, CompletionItemKind, TextDocumentPositionParams } from "vscode-languageserver";
import { getKeywords } from './compilerInterface';

interface LSPKeywordData {
	keywords: string[];
	operators: string[];
	specials: string[];
}

let cachedKeywords: CompletionItem[] = [];
let cachedOperators: String[] = [];
let cachedSpecials: String[] = [];

export async function preloadKeywords() {
	try {
		const { keywords, operators, specials }: LSPKeywordData = await getKeywords();
		console.log(keywords);
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

// This handler provides the initial list of the completion items.
export function onCompletion(_textDocumentPosition: TextDocumentPositionParams): CompletionItem[] {
	// The pass parameter contains the position of the text document in
	// which code complete got requested. For the example we ignore this
	// info and always provide the same completion items.
	return [
		...cachedKeywords,
		{
			label: "Rift",
			kind: CompletionItemKind.Text,
			data: 1
		},
		{
			label: "JavaScript",
			kind: CompletionItemKind.Text,
			data: 2
		}
	];
}

// This handler resolves additional information for the item selected in
// the completion list.
export function onCompletionResolve(item: CompletionItem): CompletionItem {
	if (item.data === 1) {
		item.detail = "Rift - najlepszy język programowania";
		item.documentation = "Kocham piwo";
	} else if (item.data === 2) {
		item.detail = "JavaScript details";
		item.documentation = "JavaScript documentation";
	} else {
		item.detail = "Keyword";
		item.documentation = "Documentation";
	}
	return item;
}