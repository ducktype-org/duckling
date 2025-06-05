# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

import sys
import os

# This is only for logging/debugging purposes: 
print("Python version")
print(sys.version)

import time

project = 'Duckling the docs'
copyright = '%s, Ducktype' % time.strftime('%Y')
author = 'Duckling team'
release = '0.0.1'

needs_sphinx = '3.8'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

sys.path.append(os.path.abspath("./doc-config/extensions"))
sys.path.append(os.path.abspath("./doc-config/"))

extensions = [
    'duckling_doc_extension',
    'sphinx.ext.autosectionlabel',
    'myst_parser',
    'sphinx_copybutton',
    'sphinx_reredirects'
]

myst_enable_extensions = [
    "dollarmath",
    "amsmath",
]

myst_heading_anchors = 3

# sphinx.ext.autosectionlabel setup
autosectionlabel_prefix_document = True
autosectionlabel_maxdepth = 3
suppress_warnings = ['autosectionlabel.*']

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store', '*/libs/*', '*/.venv/*', '.venv/*',
                    '**/README.md', 'README.md']

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = "pydata_sphinx_theme"
html_static_path = ['_static']
html_theme_options = {
    "navigation_depth": -1
}

from sphinx.highlighting import lexers
from duck_lexer import DucklingLexer

lexers['duckling'] = DucklingLexer(startinline=True)

print("INNER CONFIG")

# Configuration meant for docs while they are WIP.

redirects = {
    # 'index': 'duckling/introduction/index.html',
}

rst_prolog = """
.. admonition:: We want your feedback!
    :class: note

    Our team is great, but we make mistakes, we make poor decisions, and we succumb to biases and tunnel vision. This documentation will change, and that might be thanks to you.
    
    If you read something that you think makes little sense, we want to hear from you at `contact@ducktype.org <mailto:contact@ducktype.org>`_. Whether you discovered a theoretical soundness problem, simply think that some of Duckling's design is stupid, or even don't like the way something's explained, please (please!) let us know.
"""

