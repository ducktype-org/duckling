import { TextDocument } from 'vscode-languageserver-textdocument';
import { FoldingRange, FoldingRangeKind } from 'vscode-languageserver';

export function getFoldingRanges(document: TextDocument): FoldingRange[] {
	const text = document.getText();
	const foldingRanges: FoldingRange[] = [];
	const stack: { startLine: number, startCharacter: number }[] = [];

	for (let i = 0, line = 0, character = 0; i < text.length; i++, character++) {
		const char = text[i];

		if (char === '\n') {
			line++;
			character = -1;
			continue;
		}

		if (char === '{') {
			stack.push({ startLine: line, startCharacter: character });
		} else if (char === '}') {
			const start = stack.pop();
			if (start) {
				if (start.startLine < line) {
					const newFold = FoldingRange.create(start.startLine, line, start.startCharacter, 
														character, FoldingRangeKind.Region);
					foldingRanges.push(newFold);
				}
			}
		}
	}

	return foldingRanges;
}