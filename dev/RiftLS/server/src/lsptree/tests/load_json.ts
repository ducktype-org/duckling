import * as index from "../elements/index";
import * as fs from 'fs';

const TEST_FILE_DIR = "dev/RiftLS/server/src/lsptree/tests/input/";

function main() {
    fs.readdirSync(TEST_FILE_DIR).forEach(file => console.log(file));

    for (let json_file of fs.readdirSync(TEST_FILE_DIR)) {
        console.log("Reading file: " + json_file);
        let json = JSON.parse(fs.readFileSync(TEST_FILE_DIR + json_file, "utf8"));
        let riftElement = index.elements.riftElementFactory.createDefined(json);
        let semanticTokens = riftElement.getSemanticTokens();
    }
}

main();