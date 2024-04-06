import { TextDocument } from 'vscode-languageserver-textdocument';
import { FoldingRange, FoldingRangeKind, TextDocuments, FoldingRangeParams } from 'vscode-languageserver';
import { RiftElement } from "./lsptree/elements/elements";

export function getFoldingRanges(params: FoldingRangeParams,
    documents: TextDocuments<TextDocument>,
    pstCache: Map<string, RiftElement | null>): FoldingRange[] {
    const document = documents.get(params.textDocument.uri);
    if (!document) return [];

    let LSPTree = pstCache.get(document.uri);
    if (!LSPTree) {
        return []
    }

    const text = document.getText();
    const foldingRanges: FoldingRange[] = [];

    return foldingRanges;
}
