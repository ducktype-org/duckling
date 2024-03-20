import { spawn } from "child_process";

const BINARY_PATH = __dirname + "/../../../build/bin/";

export function getPST(filePath: string): Promise<string> {
	const command: string = BINARY_PATH + "lsptree_test";
	const args: string[] = [uriToPath(filePath)];

	return new Promise((resolve, reject) => {
		const childProcess = spawn(command, args);
		let output: string = '';

		childProcess.stdout.on('data', (data: Buffer) => {
			output += data.toString();
		});

		childProcess.stderr.on('data', (data: Buffer) => {
			console.error(`stderr: ${data.toString()}`);
		});

		childProcess.on('error', (error: Error) => {
			console.error(`error: ${error.message}`);
			reject(error);
		});

		childProcess.on('close', (code: number) => {
			if (code === 0) {
				resolve(output);
			} else {
				reject(new Error(`Process exited with code ${code}`));
			}
		});
	});
}


function uriToPath(uri: string): string {
	return uri.replace("file://", "");
}
