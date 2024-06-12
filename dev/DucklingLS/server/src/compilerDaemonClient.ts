import { spawn, ChildProcess } from "child_process";
import { DucklingElement, ducklingElementFactory } from "./lsptree/elements/elements";
import { DucklingParserError, toErrors } from "./errors";
import { Connection } from "vscode-languageserver";

const BINARY_PATH = __dirname + "/../../bin/";
const DAEMON_PORT = "42069";
const DAEMON_ADRESS = "http://localhost:" + DAEMON_PORT;

export class CompilerDaemonClient {
	private process: ChildProcess;

	constructor() {
		this.process = spawn(BINARY_PATH + "lsp_daemon", ["-p", DAEMON_PORT], {stdio: 'inherit'});
	}

	public async exit(): Promise<void> {
		this.process.kill();
	}

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

	public async getLSPT(filePath: string, connection: Connection): Promise<DucklingElement | null> {
		this.waitForReady(connection);

		const base64FilePath: string = Buffer.from(uriToFilePath(filePath)).toString('base64');

		const response = fetch(`${DAEMON_ADRESS}/get_lsptree/${base64FilePath}`);

		function handleResponse(res: Response) {
			return res.json();
		}

		function handleJSON(json: any): DucklingElement | null {
			return ducklingElementFactory.createDefined(json);
		}

		function handleCatch(error: any) : null {
			console.error(error);
			return null;
		}

		return response.then(handleResponse).then(handleJSON).catch(handleCatch);
	}

	public async getErrors(filePath: string, connection: Connection): Promise<DucklingParserError[]> {
		this.waitForReady(connection);

		const base64FilePath: string = Buffer.from(uriToFilePath(filePath)).toString('base64');

		const response = fetch(`${DAEMON_ADRESS}/get_errors/${base64FilePath}`);

		function handleResponse(res: Response) {
			return res.text();
		}

		function handleJSON(text: string): DucklingParserError[] {
			return toErrors(text);
		}

		function handleCatch(error: any): DucklingParserError[] {
			console.error(error);
			return [];
		}

		return response.then(handleResponse).then(handleJSON).catch(handleCatch);
	}

	public async getKeywords(connection: Connection): Promise<LSPKeywordData> {
		this.waitForReady(connection);

		const response = fetch(`${DAEMON_ADRESS}/export_keywords`);

		function handleResponse(res: Response) {
			return res.text();
		}

		function handleJSON(text: string): LSPKeywordData {
			return JSON.parse(text);
		}

		function handleCatch(error: any): LSPKeywordData {
			console.error(error);
			return {keywords: [], operators: [], specials: []};
		}

		return response.then(handleResponse).then(handleJSON).catch(handleCatch);
	}
}

export interface LSPKeywordData {
	keywords: string[];
	operators: string[];
	specials: string[];
}

function uriToFilePath(uri: string): string {
	return uri.split(":")[1];
}
