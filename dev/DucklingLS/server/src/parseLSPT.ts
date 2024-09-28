import {Token, semanticTokensLegend} from "./semanticTokensDeclarations";
import {FoldingRange} from "vscode-languageserver";
import { CompletionItem, CompletionItemKind, Connection, TextDocumentPositionParams, TextDocuments} from "vscode-languageserver";


type SemanticToken = Token

export function extractObjectsWithTokenKind(obj: any): SemanticToken[] {
    const result: SemanticToken[] = [];
    
    // Check if this object has a position field
    if (obj && typeof obj === 'object' && 'position' in obj && obj.position && 'semanticTokenType' in obj) {
        result.push({
            line: obj.position.startLine,
            startCharacter: obj.position.startColumn,
            length: obj.position.end - obj.position.start,
            tokenType: semanticTokensLegend.tokenTypes.indexOf(obj.tokenType) || 0,
            tokenModifiers: obj.tokenModifiers || 0
        });
    }

    // Recursively search for nested objects
    for (const key in obj) {
        if (obj.hasOwnProperty(key)) {
            const value = obj[key];

            // If the value is an object or array, recurse into it
            if (typeof value === 'object' && value !== null) {
                result.push(...extractObjectsWithTokenKind(value));
            }
        }
    }

    return result;
}


export function extractObjectsWithFoldingRange(obj: any): FoldingRange[] {
    const result: FoldingRange[] = [];

    // Check if this object has a position field
    if (obj && typeof obj === 'object' && 'position' in obj && obj.position && 'foldingRangeKind' in obj) {
        result.push({
            startLine: obj.position.startLine,
            startCharacter: obj.position.startColumn,
            endLine: obj.position.end - obj.position.start,
            endCharacter: obj.position.endColumn,
            kind: obj.foldingRangeKind || 'comment',
        });
    }

    // Recursively search for nested objects
    for (const key in obj) {
        if (obj.hasOwnProperty(key)) {
            const value = obj[key];

            // If the value is an object or array, recurse into it
            if (typeof value === 'object' && value !== null) {
                result.push(...extractObjectsWithFoldingRange(value));
            }
        }
    }

    return result;
}

// For Completion suggestions we will extract all identifiers from the current document
export function extractIdentifiers(obj: any): CompletionItem[] {
    const result: CompletionItem[] = [];

    // Check if this object has a position field
    if (obj && typeof obj === 'object' && 'name' in obj && obj.name !== "<ANONYMOUS>") {
        result.push({label: obj.name});
    }

    // Recursively search for nested objects
    for (const key in obj) {
        if (obj.hasOwnProperty(key)) {
            const value = obj[key];

            // If the value is an object or array, recurse into it
            if (typeof value === 'object' && value !== null) {
                result.push(...extractIdentifiers(value));
            }
        }
    }

    return result;
}

export function updateLSPTInCache(lsptCache: Map<string, any>, uri: string, lspt: any) {
    lsptCache.set(uri, lspt);
}