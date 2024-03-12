import { exec } from 'child_process';

const BINARY_PATH = __dirname + "/../../../build/bin/";

export async function getPST(filePath: string): Promise<string> {
	const command = BINARY_PATH + "lsptree_test" + " " + uriToPath(filePath);

	return new Promise((resolve, _) => {
        exec(command, (error, stdout, stderr) => {
            if (error) {
                console.error(`error: ${error.message}`);
            }
            if (stderr) {
                console.error(`stderr: ${stderr}`);
            }
            resolve(stdout);
        });
    });
}

function uriToPath(uri: string): string {
	return uri.replace("file://", "");
}
