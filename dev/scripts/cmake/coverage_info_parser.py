#!/usr/bin/python3
import sys

assert len(sys.argv) == 2

coverage_html_path = sys.argv[1]
assert coverage_html_path.endswith("index.html")

contents = None
with open(coverage_html_path, "r") as file:
    contents = file.read()

contents = contents.split('<td class="headerItem">Lines:</td>\n')
good_lines = contents[1].split("\n")[:3]

info = []
for line in good_lines:
    data = line.split(">")[1].split("<")[0]
    if "%" not in data:
        info.append(int(data))

info.sort()
hit = int(info[0])
total = int(info[1])
print(round(hit / total * 100, 2))
