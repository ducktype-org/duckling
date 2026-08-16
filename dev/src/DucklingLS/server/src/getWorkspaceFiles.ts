import { Connection } from 'vscode-languageserver/node';
import { fileURLToPath } from 'url';
import * as fs from 'fs';
import * as path from 'path';

export interface FileEntry {
  path: string;
  content: string;
}

/**
 * Reads every file under the workspace folders known to the connection.
 * Returns a list of file paths and their contents.
 */
export async function getWorkspaceFiles(connection: Connection): Promise<FileEntry[]> {
  const workspaceFolders = await connection.workspace.getWorkspaceFolders();
  const workspaceUris = workspaceFolders?.map(folder => folder.uri) ?? [];
  const excludes = new Set(['node_modules', '.git', 'dist', 'build', '.vscode', '.idea', 'out']);
  const results: FileEntry[] = [];

  if (workspaceFolders === null || workspaceFolders.length === 0) {
    return results;
  }

  async function* walk(dir: string): AsyncGenerator<string> {
    for (const entry of await fs.promises.readdir(dir, { withFileTypes: true })) {
      const fullPath = path.join(dir, entry.name);
      if (entry.isDirectory() && !excludes.has(entry.name)) {
        yield* walk(fullPath);
      } else if (entry.isFile()) {
        yield fullPath;
      }
    }
  }

  for (const uri of workspaceUris) {
    if (!uri.startsWith('file:')) continue;
    const rootPath = fileURLToPath(uri);

    for await (const filePath of walk(rootPath)) {
      try {
        const content = await fs.promises.readFile(filePath, 'utf8');
        results.push({ path: filePath, content });
      } catch {
        // Ignore files that cannot be read
      }
    }
  }

  return results;
}

/**
 * Filter the given list of files to include only Duckling-related files.
 * Returns a list of Duckling file paths and their contents.
 */
export async function filterDucklingFiles(files: FileEntry[]): Promise<FileEntry[]> {
  const extensions = new Set(["rift", "dl", "duckling", "dm"]);
  const results: FileEntry[] = [];

  for (const file of files) {
    const ext = path.extname(file.path).toLowerCase().slice(1); // remove the dot
    if (extensions.has(ext)) {
      results.push(file);
    }
  }

  return results;
}

export async function getWorkspaceFoldersUris(connection: Connection): Promise<string[]> {
  const workspaceFolders = await connection.workspace.getWorkspaceFolders();
  return workspaceFolders?.map(folder => folder.uri) ?? [];
}
