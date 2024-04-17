import { spawn } from "child_process";
import { DiagnosticSeverity } from "vscode-languageserver";
import { elements } from "./lsptree/elements/index";
import { RiftElement } from "./lsptree/elements/elements";

const BINARY_PATH = __dirname + "/../../../build/bin/";

export class RiftParserError {
	public readonly line: number;
	public readonly column: number;
	public readonly message: string;
	public readonly severity: DiagnosticSeverity;

	constructor(line: number, column: number, message: string, severity: DiagnosticSeverity) {
		this.line = line;
		this.column = column;
		this.message = message;
		this.severity = severity;
	}
}

export function parseFile(filePath: string): Promise<[RiftElement | null, RiftParserError[]]> {
	const command: string = BINARY_PATH + "lsptree_interface";
	const args: string[] = [uriToPath(filePath)];

	return new Promise((resolve, reject) => {
		const childProcess = spawn(command, args);
		let treeOutput: string = '';
		let errorsOutput: string = '';

		childProcess.stdout.on('data', (data: Buffer) => {
			treeOutput += data.toString();
		});

		childProcess.stderr.on('data', (data: Buffer) => {
			errorsOutput += data.toString();
		});

		childProcess.on('error', (error: Error) => {
			console.error(`error: ${error.message}`);
			reject(error);
		});

		childProcess.on('close', (code: number) => {
			if (code === 0) {
				const errors = toErrors(errorsOutput);
				try {
					const LSPTreeJson = JSON.parse(treeOutput);
					const LSPTree = elements.riftElementFactory.createDefined(LSPTreeJson);
					resolve([LSPTree, errors]);
				} catch (error) {
					console.error(`error: ${error}`);
					resolve([null, errors]);
				}
			} else {
				reject(new Error(`Process exited with code ${code}`));
			}
		});
	});
}

function uriToPath(uri: string): string {
	return uri.replace("file://", "");
}

function toErrors(output: string): RiftParserError[] {
	const errors: RiftParserError[] = [];
	const errorsDesc = removeANSIEscapeCodes(output).split("In file");

	for (let i = 0; i < errorsDesc.length; i++) {
		const errorDesc = errorsDesc[i];
		if (errorDesc.length === 0) {
			continue;
		}

		const parts = errorDesc.split(":");
		if (parts.length < 5) {
			continue;
		}

		const line = parseInt(parts[2]);
		const column = parseInt(parts[3].split("\n")[0]);
		const message = parts[4];
		const severity = parts[3].indexOf("error") !== -1 ? DiagnosticSeverity.Error : DiagnosticSeverity.Warning;
		errors.push(new RiftParserError(line, column, message, severity));
	}

	return errors;
}

function removeANSIEscapeCodes(input: string): string {
	const ansiEscapePattern = /\u001b\[[0-9;]*m/g;
	return input.replace(ansiEscapePattern, '');
}

export function getKeywords(): Promise<any> {
	const command = BINARY_PATH + "lsp_test";

	return new Promise((resolve, reject) => {
		const childProcess = spawn(command);
		let output: string = '';

		childProcess.stdout.on('data', (data: Buffer) => {
			output += data.toString();
		});

		childProcess.on('error', (error: Error) => {
			console.error(`error: ${error.message}`);
			reject(error);
		});

		childProcess.on('close', (code: number) => {
			if (code === 0) {
				const parsedOutput = JSON.parse(output);
				resolve(parsedOutput);
			} else {
				reject(new Error(`Process exited with code ${code}`));
			}
		});
	});
}