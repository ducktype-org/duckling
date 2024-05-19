import sys
import os
import subprocess
import json


LSP_TREE_BINARY_PATH = "build/bin/lsptree_interface"
EXAMPLE_FILES_PATH = "RiftCompiler/pst_parser/tests/snippets"
OTHER_FILES_PATH = "RiftCompiler/rift_snippets"
SAVE_FILE_DIR = "RiftLS/test_lsptree/results"
SAVE_FOR_TS_TEST = "RiftLS/server/src/lsptree/tests/input"




def check_json(obj):
    assert isinstance(obj, dict), f"Expected dict, got {type(obj)}"
    assert len(obj) == 1, f"Expected 1 key, got {len(obj)}"

    key, value = list(obj.items())[0]

    assert isinstance(key, str) and key[0].isupper(), "Expected key to be a string starting with an uppercase letter"
    assert isinstance(value, dict), f"Expected value to be a dict, got {value} of {type(value)}"
    # if key != "DottedName":
    #     assert "position" in value, "Expected 'position' key in value"

    for k, v in value.items():
        assert k[0].islower()
        if k == "position":
            check_position(v)
        elif isinstance(v, dict):
            check_json(v)
        elif isinstance(v, list):
            for item in v:
                if isinstance(item, dict):
                    check_json(item)


def check_position(obj):
    assert isinstance(obj, dict), f"Expected dict, got {type(obj)}"
    assert len(obj) == 4, f"Expected 4 keys, got {len(obj)}"
    assert "line" in obj and isinstance(obj["line"], int), f"Expected 'line' to be an int, got {obj['line']}"
    assert "column" in obj and isinstance(obj["column"], int), f"Expected 'column' to be an int, got {obj['column']}"
    assert "start" in obj and isinstance(obj["start"], int), f"Expected 'start' to be an int, got {obj['start']}"
    assert "end" in obj and isinstance(obj["end"], int), f"Expected 'end' to be an int, got {obj['end']}"


def check_file(json_str):
    obj = json.loads(json_str)

    check_json(obj)

def main():


    
    def check_dir(dirpath, save_prefix = ""):
        for filename in os.listdir(dirpath):
            if filename.endswith(".rift") and not filename.endswith("_err.rift"):
                print(f"Checking {filename}")
                proc = subprocess.Popen([LSP_TREE_BINARY_PATH, os.path.join(dirpath, filename)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                out, err = proc.communicate()
                #print(out.decode("utf-8"))
                if proc.returncode != 0:
                    print(f"Error running parser on {filename}")
                    print(err.decode("utf-8"))
                    print(out.decode("utf-8"))

                    continue
                check_file(out.decode("utf-8"))
                

                # save json to file 
                with open(os.path.join(SAVE_FILE_DIR, save_prefix+filename.replace(".rift", ".json")), "w") as f:
                    f.write(out.decode("utf-8"))
                with open(os.path.join(SAVE_FOR_TS_TEST, save_prefix+filename.replace(".rift", ".json")), "w") as f:
                    f.write(out.decode("utf-8"))
                # print(f"{filename} is correct")

    check_dir(EXAMPLE_FILES_PATH, "pst_parser_")
    check_dir(OTHER_FILES_PATH, "RIFT_COMPILER_")

if __name__ == "__main__":
    main()