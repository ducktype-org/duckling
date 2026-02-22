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