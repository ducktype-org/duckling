import { spawn, ChildProcess } from "child_process";
import * as net from 'net';
import { Connection, CompletionItem, TextDocumentPositionParams, Diagnostic } from "vscode-languageserver";
import { Location } from "vscode-languageserver/node";
import { getWorkspaceFiles, filterDucklingFiles } from './getWorkspaceFiles';
import * as fs from 'fs';
import * as path from 'path';

// For the compiler daemon client to work, daemon's binary should be in DucklingLS/bin/ directory
const BINARY_PATH = __dirname + "/../../bin/";

function findFreePort(): Promise<number> {
	return new Promise((resolve, reject) => {
		const server = net.createServer();
		server.listen(0, '127.0.0.1', () => {
			const port = (server.address() as net.AddressInfo).port;
			server.close(() => resolve(port));
		});
		server.on('error', reject);
	});
}

export interface Token {
	line: number;
	startCharacter: number;
	length: number;
	tokenType: number;
	tokenModifiers: number;
}

async function fetchWithTimeout(
  url: string,
  options: RequestInit = {},
  timeoutMs = 5000
): Promise<Response | undefined> {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), timeoutMs);

  try {
    return await fetch(url, {
      ...options,
      signal: controller.signal
    });
  } catch (err: any) {
    return undefined;
  } finally {
    clearTimeout(timeoutId);
  }
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
	private ls_daemon_process!: ChildProcess;
	private port: number = 0;
	private ls_daemon_address: string = '';
	private startupPromise: Promise<void>;

	constructor() {
		this.startupPromise = this.startup();
	}

	private async startup(): Promise<void> {
		this.port = await findFreePort();
		this.ls_daemon_address = `http://localhost:${this.port}`;
		this.ls_daemon_process = this.startServerBinary();
	}

	private startServerBinary(): ChildProcess {
		const logPath = path.join(__dirname, 'daemon.log');
		const logStream = fs.createWriteStream(logPath);
		const childProcess = spawn(
			BINARY_PATH + "lsp_daemon", 
			["start", "-p", this.port.toString()], 
			{stdio: ["ignore", "pipe", "pipe"], detached: false} // This is necessary for the server to remain responsive
		);
		childProcess.stdout?.on("data", (data) => {
			process.stdout.write(data);
			logStream.write(data);
		});

		// Mirror stderr to console and log file
		childProcess.stderr?.on("data", (data) => {
			process.stderr.write(data);
			logStream.write(data);
		});

		childProcess.on("close", (code) => {
			console.log(`Compiler daemon exited with code ${code}`);
			logStream.end();
		});
		return childProcess;
	}
	
	public async restart(connection: Connection): Promise<void> {
		this.ls_daemon_process.kill();

		// Wait max 2 seconds for the process to exit
		for (let i = 0; i < 20; i++) {
			if (this.ls_daemon_process.exitCode !== null) break;
			await new Promise(resolve => setTimeout(resolve, 100));
		}
		
		this.startupPromise = this.startup();
		await this.waitForReady(connection);
		await this.putWorkspace(connection);
	}

	// This function is called when the server is closed
	public async exit(): Promise<void> {
		this.ls_daemon_process.kill();
	}

	// Fetches a URL, and if the daemon returns 500 it restarts the daemon and retries once
	private async fetchWithRestart(url: string, connection: Connection): Promise<Response> {
		const response = await fetch(url);
		if (response.status === 500) {
			connection.sendNotification('window/showMessage', {type: 1, message: 'DucklingLS daemon error, restarting...'});
			await this.restart(connection);
			return fetch(url);
		}
		return response;
	}

	// This function is called to make sure the daemon is ready
	private async waitForReady(connection: Connection): Promise<void> {
		await this.startupPromise;
		console.log("Checking if compiler daemon is ready...");
		for (let i = 0; i < 30; i++){
			const response = await fetchWithTimeout(`${this.ls_daemon_address}/status`, {}, 100);

			if (response && response.status == 200) {
				console.log("Compiler daemon is ready.");
				return;
			} else {
				console.log("Compiler daemon not ready yet, retrying...");
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

		const response = this.fetchWithRestart(`${this.ls_daemon_address}/put_file/${base64FilePath}/${base64FileContent}`, connection);

		async function handleResponse(res: Response) {
			if (res.status != 200) {
				const text = await res.text();
				throw new Error(`Error: ${res.status} ${text}`);
			}
		}

		function handleCatch(error: any) {
			console.error(error);
		}

		return response.then(handleResponse).catch(handleCatch);
	}

	// Used for debug in various places
	public async callDebugPrint(connection: Connection, arg: string): Promise<void> {
		await this.waitForReady(connection);
		console.log("Debugging in progress...")
		try {
			const base64Arg: string = Buffer.from(arg).toString('base64');
			const response = await fetch(`${this.ls_daemon_address}/debug/${base64Arg}`);
			if (!response.ok) {
				console.log("Call debug failed");
				throw new Error(`Error: ${response.status} ${response.statusText}`);
			}
			const text = await response.text();
			console.log("ERROR CONTENTS:");
			console.log(text);
		} catch (error) {
			if (error instanceof Error) {
				console.error(`cdp error: ${error.message}`);
			} else {
				console.error(`cdp error: ${String(error)}`);
			}
		}
		return;
	}

	// This function is called to update the workspace in the daemon
	public async putWorkspace(connection: Connection): Promise<void> {
		await this.waitForReady(connection);
		
		const folders = (await connection.workspace.getWorkspaceFolders())?.map(folder => folder.uri) ?? [];
		for (const folder of folders) {
			var base64FilePath: string = Buffer.from(uriToFilePath(folder)).toString('base64');
			await this.fetchWithRestart(`${this.ls_daemon_address}/init_directory/${base64FilePath}`, connection);
		}

		return;
	}

	// This function is called to get the semantic tokens from the daemon for a file
	public async getSemanticTokens(filePath: string, connection: Connection): Promise<Token[]> {
		await this.waitForReady(connection);

		const base64FilePath: string = Buffer.from(uriToFilePath(filePath)).toString('base64');

		const response = await this.fetchWithRestart(`${this.ls_daemon_address}/get_semantic_tokens/${base64FilePath}`, connection);
		
		if (!response.ok) {
			console.error(`getSemanticTokens error: ${response.status} ${response.statusText}`);
			return [];
		}

		const jsonResponse = await response.json();

		// Assuming the response is a JSON array of Token elements
		const tokens: Token[] = jsonResponse.map((token: any) => ({
			line: token.line,
			startCharacter: token.startCharacter,
			length: token.length,
			tokenType: token.tokenType,
			tokenModifiers: token.tokenModifiers
		}));
		
		return tokens;
	}

	// This function is called to get the errors from the daemon for a file
	public async getErrors(filePath: string, connection: Connection): Promise<Record<string, Diagnostic[]>> {
		await this.waitForReady(connection);

		const base64FilePath: string = Buffer.from(uriToFilePath(filePath)).toString('base64');

		const response = await this.fetchWithRestart(`${this.ls_daemon_address}/get_errors/${base64FilePath}`, connection);

		if (!response.ok) {
			console.error(`getErrors error: ${response.status} ${response.statusText}`);
			return {};
		}

		return await response.json() as Record<string, Diagnostic[]>;
	}

	// This function is called to get all of the keywords from the daemon
	public async getKeywords(connection: Connection): Promise<LSPKeywordData> {
		await this.waitForReady(connection);

		const response = fetch(`${this.ls_daemon_address}/export_keywords`);

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
		const response = fetch(`${this.ls_daemon_address}/get_completion_items/${base64FilePath}/${line}/${offset}`);

		return [];
	}

	public async getDefinition(_textDocumentPosition: TextDocumentPositionParams, offset: number, connection: Connection): Promise<Location | Location[] | null> {
		return [];
		await this.waitForReady(connection);
		const base64FilePath: string = Buffer.from(uriToFilePath(_textDocumentPosition.textDocument.uri)).toString('base64');
		const response = await this.fetchWithRestart(`${this.ls_daemon_address}/get_definitions/${base64FilePath}/${offset.toString()}`, connection);
		
		if (!response.ok) {
			throw new Error(`Error: ${response.status} ${response.statusText}`);
		}

		const jsonResponse = await response.json();
		
		// Ther response MUST be a JSON object by now, otherwise the request will fail

		const definitions: Location[] = jsonResponse.map((definition: any) => ({
			uri: definition.uri,
			range: {
				start: {
					line: definition.range.start.line-1,
					character: definition.range.start.character-1
				},
				end: {
					line: definition.range.end.line-1,
					character: definition.range.end.character-1
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
	if (uri.startsWith("file:")) {
		const filepath = uri.split(":")[1];
		return filepath.replace("///", "").replace("\\\\\\", "");
	}
	if (uri.startsWith("/") || uri.startsWith("\\")) {
		return uri.slice(1);
	}
	return uri;
}
