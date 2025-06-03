import {
    FoldingRange,
    FoldingRangeKind,
    FoldingRangeParams,
    TextDocuments
} from "vscode-languageserver";
import { TextDocument } from "vscode-languageserver-textdocument";

// Folding Range request handler
export function handleFoldingRanges(
    params: FoldingRangeParams,
    documents: TextDocuments<TextDocument>
): FoldingRange[] {
    const document = documents.get(params.textDocument.uri);
    if (!document) {
        return [];
    }
    let text = document.getText();
    const lines = text.split(/\r?\n/);
    let foldingRanges: FoldingRange[] = [];

    // 1. Fold multi-line comment blocks (/* ... */) and remove them from the text
    for (let lineIndex = 0; lineIndex < lines.length; lineIndex++) {
        const line = lines[lineIndex];
        const startPos = line.indexOf("/*");
        if (startPos !== -1) {
            if (line.indexOf("*/", startPos + 2) !== -1) {
                continue;
            }
            for (let j = lineIndex + 1; j < lines.length; j++) {
                if (lines[j].includes("*/")) {
                    foldingRanges.push({
                        startLine: lineIndex,
                        endLine: j,
                        kind: FoldingRangeKind.Comment
                    });
                    // Remove the comment block from the text
                    for (let k = lineIndex; k <= j; k++) {
                        lines[k] = "";
                    }
                    lineIndex = j;
                    break;
                }
            }
        }
    }

    // 2. Fold consecutive single-line comments (// ...) and remove them from the text
    let i = 0;
    while (i < lines.length) {
        if (lines[i].trim().startsWith("//")) {
            const startLine = i;
            while (i < lines.length && lines[i].trim().startsWith("//")) {
                i++;
            }
            const endLine = i - 1;
            if (endLine > startLine) {
                foldingRanges.push({
                    startLine: startLine,
                    endLine: endLine,
                    kind: FoldingRangeKind.Comment
                });
                // Remove the single-line comments from the text
                for (let k = startLine; k <= endLine; k++) {
                    lines[k] = "";
                }
            }
            continue;
        }
        i++;
    }

    // Update the text after removing comments
    text = lines.join("\n");

    // 3. Fold consecutive import lines into a single range
    i = 0;
    while (i < lines.length) {
        if (lines[i].trim().startsWith("import")) {
            const startLine = i;
            while (i < lines.length && lines[i].trim().startsWith("import")) {
                i++;
            }
            const endLine = i - 1;
            if (endLine > startLine) {
                foldingRanges.push({
                    startLine: startLine,
                    endLine: endLine,
                    kind: FoldingRangeKind.Imports
                });
            }
            continue;
        }
        i++;
    }

    // 4. Fold regions enclosed in curly braces
    const stack: number[] = [];
    lines.forEach((line, lineIndex) => {
        for (let j = 0; j < line.length; j++) {
            if (line[j] === "{") {
                stack.push(lineIndex);
            } else if (line[j] === "}") {
                if (stack.length > 0) {
                    const startLine = stack.pop()!;
                    if (startLine < lineIndex - 1) {
                        foldingRanges.push({
                            startLine: startLine,
                            endLine: lineIndex - 1,
                            kind: FoldingRangeKind.Region
                        });
                    }
                }
            }
        }
    });
    


    // Sort folding ranges by start line
    foldingRanges.sort((a, b) => a.startLine - b.startLine);

    return foldingRanges;
}