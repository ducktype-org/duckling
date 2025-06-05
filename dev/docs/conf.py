import importlib
# "Imports" config from doc-config/conf.py
with open("./doc-config/conf.py") as conf:
	exec(conf.read())

