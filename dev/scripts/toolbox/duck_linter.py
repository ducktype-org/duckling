from typing import List, Callable
import re
import os
from pathlib import Path

from scripts.toolbox.helpers import (
	bash_command_get_output,
	log_info,
	log_warning,
	log_new_line
)

RELATIVE_IMPORT_REGEX = re.compile(r'#include "(.*?)"')

class SourceFile:
	def __init__(self, path: str):
		self.path = Path(path)
		self.dir = self.path.parent
		self.errors = []
		with open(path, 'r') as file:
			self.content = file.read()

	def relativeImportChecks(self):
		imports = re.findall(RELATIVE_IMPORT_REGEX, self.content)
		for imp in imports:
			import_path = self.dir / imp
			if not import_path.exists():
				self.errors.append(f"Relative import `{imp}` does not exist.")

	def runAllChecks(self):
		self.relativeImportChecks()

		if len(self.errors) > 0:
			log_warning(f"{self.path}: ERRORS FOUND")
			for error in self.errors:
				log_warning(" * " + error)
		else:
			log_info(f"{self.path}: OK")

		log_new_line()

def get_modified_file(all, relative_to):
	ls_out = None
	if all:
		ls_out = bash_command_get_output("git ls-files")[0]
	else:
		ls_out = bash_command_get_output(f"git diff --name-only --relative {relative_to}")[0]
	files = ls_out.splitlines()
	return files

def get_source_files(all, relative_to) -> List[SourceFile]:
	files = get_modified_file(all, relative_to)
	source_files = []
	for file in files:
		if file.endswith(".cpp") or file.endswith(".hpp"):
			source_files.append(SourceFile(file))
	return source_files


def duck_linter_impl(all, branch):
	# Simple implementation for now
	
	file = get_source_files(all, branch)
	for f in file:
		f.runAllChecks()
	
