import { CompletionItem, CompletionItemKind, Connection} from "vscode-languageserver";
import { CompilerDaemonClient, LSPKeywordData } from './compilerDaemonClient';

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
