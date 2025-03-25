import { spawn, ChildProcess } from "child_process";
import { DucklingParserError, toErrors } from "./errors";
import { Connection, CompletionItem, TextDocumentPositionParams } from "vscode-languageserver";
import { Location } from "vscode-languageserver/node";

// For the compiler daemon client to work, daemon's binary should be in DucklingLS/bin/ directory
const BINARY_PATH = __dirname + "/../../bin/";
const DAEMON_PORT = "14369";
const DAEMON_ADRESS = "http://localhost:" + DAEMON_PORT;

export interface Token {
	line: number;
	startCharacter: number;
	length: number;
	tokenType: number;
	tokenModifiers: number;
}


/**
 * @brief A client for the compiler daemon.
 * This class should be instantiated once and closed when the server is closed.
 * It contains methods for interacting with the compiler daemon.
 * 
 * Each method sends a request to the daemon, the response is later parsed and appropriate objects are created.
 * Each request is sent to the daemon using the fetch API to the appropriate endpoint.
 * The endpoints are defined in the compiler daemon.
 */
export class CompilerDaemonClient {
	private process: ChildProcess;

	constructor() {
		this.process = spawn(
			BINARY_PATH + "lsp_daemon", 
			["-p", DAEMON_PORT], 
			{stdio: 'inherit'} // This is necessary for the server to remain responsive
		);
	}

	// This function is called when the server is closed
	public async exit(): Promise<void> {
		this.process.kill();
	}

	// This function is called to make sure the daemon is ready
	private async waitForReady(connection: Connection): Promise<void> {
		for (let i = 0; i < 10; i++){  
			const response = await fetch(`${DAEMON_ADRESS}/status`);

			if (response.status == 200) {
				return;
			} else {
				await new Promise(resolve => setTimeout(resolve, 1000));
			}
		}

		connection.sendNotification('window/showMessage', {type: 1, message: 'Could not connect to compiler daemon'});
	}

	// This function is called to update the file in the daemon
	public async putFile(filePath: string, fileContent: string, connection: Connection): Promise<void> {
		await this.waitForReady(connection);

		const base64FilePath: string = Buffer.from(uriToFilePath(filePath)).toString('base64');
		const base64FileContent: string = Buffer.from(fileContent).toString('base64');

		const response = fetch(`${DAEMON_ADRESS}/put_file/${base64FilePath}/${base64FileContent}`);

		function handleResponse(res: Response) {
			if (res.status != 200) {
				throw new Error(`Error: ${res.status}`);
			}
		}

		function handleCatch(error: any) {
			console.error(error);
		}

		return response.then(handleResponse).catch(handleCatch);
	}

	// This function is called to get the semantic tokens from the daemon for a file
	public async getSemanticTokens(filePath: string, connection: Connection): Promise<Token[]> {
		await this.waitForReady(connection);

		const base64FilePath: string = Buffer.from(uriToFilePath(filePath)).toString('base64');

		try {
			const response = await fetch(`${DAEMON_ADRESS}/get_semantic_tokens/${base64FilePath}`);
			
			// Print the response status and headers
			console.log(`Response status: ${response.status}`);
			const headers: { [key: string]: string } = {};
			response.headers.forEach((value, key) => {
				headers[key] = value;
			});
			console.log(`Response headers: ${JSON.stringify(headers)}`);
	
			if (!response.ok) {
				console.log("Response not ok!!!!!!!");
				throw new Error(`Error: ${response.status} ${response.statusText}`);
			}

			// // Read and print the response body as text
			// const responseBody = await response.text();
			// console.log(`Response body: ${responseBody}`);
	

			const jsonResponse = await response.json();
			console.log(`getSemanticTokens response: ${JSON.stringify(jsonResponse)}\n`);

			// Assuming the response is a JSON array of Token elements
			const tokens: Token[] = jsonResponse.map((token: any) => ({
				line: token.line,
				startCharacter: token.startCharacter,
				length: token.length,
				tokenType: token.tokenType,
				tokenModifiers: token.tokenModifiers
			}));

			return tokens;
		} catch (error) {
			if (error instanceof Error) {
				console.error(`getSemanticTokens error: ${error.message}`);
			} else {
				console.error(`getSemanticTokens error: ${String(error)}`);
			}
			return [];
		}
	}

	// This function is called to get the errors from the daemon for a file
	public async getErrors(filePath: string, connection: Connection): Promise<DucklingParserError[]> {
		await this.waitForReady(connection);

		const base64FilePath: string = Buffer.from(uriToFilePath(filePath)).toString('base64');

		const response = fetch(`${DAEMON_ADRESS}/get_errors/${base64FilePath}`);

		function handleResponse(res: Response) {
			return res.text();
		}

		function handleText(text: string): DucklingParserError[] {
			return toErrors(text); // Errors are created here from text
		}

		function handleCatch(error: any): DucklingParserError[] {
			console.error(error);
			return [];
		}

		return response.then(handleResponse).then(handleText).catch(handleCatch);
	}

	// This function is called to get all of the keywords from the daemon
	public async getKeywords(connection: Connection): Promise<LSPKeywordData> {
		await this.waitForReady(connection);

		const response = fetch(`${DAEMON_ADRESS}/export_keywords`);

		function handleResponse(res: Response) {
			return res.text();
		}

		function handleJSON(text: string): LSPKeywordData {
			return JSON.parse(text); // Keywords are created here from JSON
		}

		function handleCatch(error: any): LSPKeywordData {
			console.error(error);
			return {keywords: [], operators: [], specials: []};
		}

		return response.then(handleResponse).then(handleJSON).catch(handleCatch);
	}

	public async getCompletionItems(filePath: string, _textDocumentPosition: TextDocumentPositionParams,  connection: Connection): Promise<CompletionItem[]> {
		await this.waitForReady(connection);
		const base64FilePath: string = Buffer.from(uriToFilePath(filePath)).toString('base64');
		const line: string = (_textDocumentPosition.position.line).toString();
		const offset: string = (_textDocumentPosition.position.character).toString();
		const response = fetch(`${DAEMON_ADRESS}/get_completion_items/${base64FilePath}/${line}/${offset}`);

		return [];
	}

	public async getDefinition(_textDocumentPosition: TextDocumentPositionParams, connection: Connection): Promise<Location | Location[] | null> {
		await this.waitForReady(connection);
		const base64FilePath: string = Buffer.from(uriToFilePath(_textDocumentPosition.textDocument.uri)).toString('base64');
		const line: string = (_textDocumentPosition.position.line).toString();
		const offset: string = (_textDocumentPosition.position.character).toString();
		const response = await fetch(`${DAEMON_ADRESS}/get_definition/${base64FilePath}/${line}/${offset}`);

		// Print the response status and headers
		console.log(`Response status: ${response.status}`);
		const headers: { [key: string]: string } = {};
		response.headers.forEach((value, key) => {
			headers[key] = value;
		});
		console.log(`Response headers: ${JSON.stringify(headers)}`);

		if (!response.ok) {
			console.log("Response not ok!!!!!!!");
			throw new Error(`Error: ${response.status} ${response.statusText}`);
		}

		const jsonResponse = await response.json();
		console.log(`get_definition response: ${JSON.stringify(jsonResponse)}\n`);

		// Assuming the response is a JSON array of Token elements
		const definitions: Location[] = jsonResponse.map((definition: any) => ({
			uri: definition.uri,
			range: {
				start: {
					line: definition.range.start.line,
					character: definition.range.start.character
				},
				end: {
					line: definition.range.end.line,
					character: definition.range.end.character
				}
			}
		}));

		return definitions;
	}
}

// Class representing keyword data from the compiler daemon
export interface LSPKeywordData {
	keywords: string[];
	operators: string[];
	specials: string[];
}

// File paths are stored in URIs, this function converts them to file paths
function uriToFilePath(uri: string): string {
	return uri.split(":")[1];
}
