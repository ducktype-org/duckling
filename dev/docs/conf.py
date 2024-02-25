import importlib
# "Imports" config from submodule
with open("./doc-config/conf.py") as conf:
	exec(conf.read())

rst_prolog = """
.. |HIR| replace:: :doc:`HIR </source-doc/dev-handbook/compiler/hir>`
"""
