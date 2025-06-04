import os
import sys

sys.path.insert(0, os.path.abspath("../src/quackpack"))

# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = "quackpack"
copyright = "2025, Ducktype"
author = "Ignacy Gębuś, Tymoteusz Kucharek, Stanisław Wiśniewski, Artur Wojtuszkiewicz"

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = ["sphinx.ext.autodoc", "sphinx.ext.autosummary", "sphinx_autodoc_typehints"]

templates_path = ["templates"]
exclude_patterns = ["build", "Thumbs.db", ".DS_Store"]

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = "alabaster"
html_static_path = ["static"]
