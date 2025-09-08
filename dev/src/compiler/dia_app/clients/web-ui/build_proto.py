#!/usr/bin/env python3
import shutil, sys
from pathlib import Path
from grpc_tools import protoc

script_dir = Path(__file__).resolve().parent
proto_src = script_dir.parent.parent / "common" / "raw_proto" / "view.proto"

if not proto_src.exists():
    sys.exit(1)

shutil.copy(proto_src, script_dir / "view.proto")
sys.exit(protoc.main([
    "",
    f"-I{script_dir}",
    f"--python_out={script_dir}",
    f"--grpc_python_out={script_dir}",
    str(script_dir / "view.proto")
]))
