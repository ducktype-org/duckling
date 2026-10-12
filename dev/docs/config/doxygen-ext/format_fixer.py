# Copyright 2026 DuckType LLC
#
# This file is part of the Duckling project, licensed under the DuckType
# Compiler License, Version 1.0. See the LICENSE or LICENSE.md file in the root
# of this repository or https://ducktype.org/licenses/DTCL-1.0

import argparse
import re

# This is currently unused but I'm leaving it as an example and future starting point

parser = argparse.ArgumentParser("Doxygen format fixer")
parser.add_argument("filename")

args = parser.parse_args()

filename = args.filename

with open(filename, "r") as file:
	contents = file.read()

print(re.sub("\n^[ \t]*:", ":\n", contents, flags=re.MULTILINE))