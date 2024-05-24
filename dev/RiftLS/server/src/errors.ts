import { DiagnosticSeverity } from "vscode-languageserver";

const ERR_DELIMITER = /(ERR|WARN|INFO)/g;
const SEVERITY_MAP: { [key: string]: DiagnosticSeverity } = {
	"ERR": DiagnosticSeverity.Error,
	"WARN": DiagnosticSeverity.Warning,
	"INFO": DiagnosticSeverity.Information,
};

export class RiftParserError {
	public readonly line: number;
	public readonly column: number;
	public readonly message: string;
	public readonly severity: DiagnosticSeverity;
	public readonly type: string;

	constructor(line: number, column: number, message: string, severity: DiagnosticSeverity, type: string) {
		this.line = line;
		this.column = column;
		this.message = message;
		this.severity = severity;
		this.type = type;
	}
}

export function toErrors(output: string): RiftParserError[] {
	const errors: RiftParserError[] = [];
	const errorsDesc = concatenatePairs(removeANSIEscapeCodes(output).split(ERR_DELIMITER));
	
	for (const errorDesc of errorsDesc) {
		if (errorDesc.length === 0) {
			continue;
		}
		
		const parts = errorDesc.split(":");
		if (parts.length < 5) {
			continue;
		}
		
		const line = parseInt(parts[3]);
		const column = parseInt(parts[4].split("\n")[0]);
		const message = parts[4].split("\n").slice(1).join("\n").trimEnd();
		const severity = SEVERITY_MAP[parts[0].split(" ")[0]] || DiagnosticSeverity.Error;
		const type = parts[0].split(" ")[1];
		errors.push(new RiftParserError(line, column, message, severity, type));
	}
	
	return errors;
}

function removeANSIEscapeCodes(input: string): string {
	const ansiEscapePattern = /\u001b\[[0-9;]*m/g;
	return input.replace(ansiEscapePattern, '');
}

function concatenatePairs(array: string[]): string[] {
	const result: string[] = [];

	for (let i = 1; i < array.length; i += 2) {
		if (i + 1 < array.length) {
			result.push(array[i] + array[i + 1]);
		} else {
			result.push(array[i]);
		}
	}

	return result;
}
