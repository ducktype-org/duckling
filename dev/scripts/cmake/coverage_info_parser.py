#!/usr/bin/python3
import sys

assert len(sys.argv) == 2

coverage_html_path = sys.argv[1]
assert coverage_html_path.endswith("index.html")

contents = None
with open(sys.argv[1], "r") as file:
    contents = file.read()

contents = contents.split('<td class="headerItem">Lines:</td>\n')
good_lines = contents[1].split("\n")[:3]

info = []
for line in good_lines:
    info.append(line.split(">")[1].split("<")[0])

print(good_lines)
print(info)
total = int(info[1])
hit = int(info[2])
print(hit / total * 100)
