from docutils import nodes
from docutils.statemachine import ViewList
from docutils.parsers.rst import Directive
from sphinx.util.nodes import nested_parse_with_titles

def static_macro(directive, text):
	rst = ViewList()
	rst.append(text, "fakefile.rst", 0)

	node = nodes.section()
	node.document = directive.state.document

	nested_parse_with_titles(directive.state, rst, node)
	
	return node.children


class NotImportant(Directive):
	def run(self):
		return static_macro(self, ".. attention:: This section is not the most important for Duckling, and is currently left out.")

class TemporarySyntax(Directive):
	def run(self):
		return static_macro(self, ".. attention:: This section uses temporary syntax, that will most likely change.")
	

class NotYetFiguredOut(Directive):
	def run(self):
		return static_macro(self, ".. attention:: This section is „not yet figured out”.")

class JustSimpleDescription(Directive):
	def run(self):
		return static_macro(self, ".. attention:: This section is just a simple description, full documentation is yet to be done.")
	

def setup(app):
	app.add_directive("not_important", NotImportant)
	app.add_directive("temporary_syntax", TemporarySyntax)
	app.add_directive("not-figured-out", NotYetFiguredOut)
	app.add_directive("simple-description", JustSimpleDescription)
	return {
		'version': '0.1',
		'parallel_read_safe': True,
		'parallel_write_safe': True,
	}
