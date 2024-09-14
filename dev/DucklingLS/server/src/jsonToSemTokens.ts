import { SourcePosition, SemanticToken } from "./lsptree/elements/common";
import { getTokenTypeIndex, stringToSemanticTokenType } from "./semanticTokensDeclarations";

type Node = {
    [key: string]: any;
};

function parseTree(json: Node): SemanticToken[] {
    const result: SemanticToken[] = [];

    function traverse(node: Node): void {
        const keys = Object.keys(node);
        if (keys.length === 0) return;

        const firstField = keys[0]; // Assume first field is the first key
        const position = new SourcePosition(node.position);
        const semTokenType = node.semTokenType;

        // Push the current node's tuple to the result array
        if (typeof firstField === 'string' && typeof semTokenType === 'string') {
            result.push(SemanticToken.fromPosition(position, stringToSemanticTokenType(semTokenType), []));
        }

        // Recursively traverse the rest of the fields as children nodes
        keys.forEach(key => {
            if (key !== firstField && key !== 'position' && key !== 'semTokenType') {
                const childNode = node[key];
                if (typeof childNode === 'object' && childNode !== null) {
                    traverse(childNode);
                }
            }
        });
    }

    traverse(json);
    return result;
}

