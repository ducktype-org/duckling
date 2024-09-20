import { spawn, ChildProcess } from "child_process";
import { DucklingElement, ducklingElementFactory } from "./lsptree/elements/elements";
import { DucklingParserError, toErrors } from "./errors";
import { Connection } from "vscode-languageserver";
import { SemanticToken } from "./lsptree/elements/common";
//import { parseTree } from "./jsonToSemTokens";

// For the compiler daemon client to work, daemon's binary should be in DucklingLS/bin/ directory
const BINARY_PATH = __dirname + "/../../bin/";
const DAEMON_PORT = "42069";
const DAEMON_ADRESS = "http://localhost:" + DAEMON_PORT;

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
		this.waitForReady(connection);

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

	// // This function is called to get the LSPTree from the daemon for a file
	// public async getLSPT(filePath: string, connection: Connection): Promise<DucklingElement | null> {
	// 	this.waitForReady(connection);

	// 	const base64FilePath: string = Buffer.from(uriToFilePath(filePath)).toString('base64');

	// 	const response = fetch(`${DAEMON_ADRESS}/get_lsptree/${base64FilePath}`);

	// 	function handleResponse(res: Response) {
	// 		return res.json();
	// 	}

	// 	function handleJSON(json: any): DucklingElement | null {
	// 		return ducklingElementFactory.createDefined(json); // LSPT is created here from JSON
	// 	}

	// 	function handleCatch(error: any) : null {
	// 		console.error(error);
	// 		return null;
	// 	}

	// 	return response.then(handleResponse).then(handleJSON).catch(handleCatch);
	// }

	// This function is called to get the LSPTree from the daemon for a file
	public async getLSPT(filePath: string, connection: Connection): Promise<JSON | null> {
		this.waitForReady(connection);

		const base64FilePath: string = Buffer.from(uriToFilePath(filePath)).toString('base64');

		const response = fetch(`${DAEMON_ADRESS}/get_lsptree/${base64FilePath}`);

		function handleResponse(res: Response) {
			return res.json();
		}

		function handleCatch(error: any) : null {
			console.error(error);
			return null;
		}

		//return response.then(handleResponse).catch(handleCatch);
		return response.then(handleResponse).catch(handleCatch);
	}

	// This function is called to get the errors from the daemon for a file
	public async getErrors(filePath: string, connection: Connection): Promise<DucklingParserError[]> {
		this.waitForReady(connection);

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
		this.waitForReady(connection);

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
