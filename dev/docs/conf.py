import importlib
# "Imports" config from submodule
with open("./doc-config/conf.py") as conf:
	exec(conf.read())

