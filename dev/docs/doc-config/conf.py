# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

import sys

# This is only for logging/debugging purposes: 
print("Python version")
print (sys.version)

import time

project = 'Rift the docs'
copyright = '%s, Ducktype' % time.strftime('%Y')
author = 'Rift team'
release = '0.0.1'

needs_sphinx = '3.8'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = ['breathe']
breathe_default_project = "Rift"

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']


# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = "pydata_sphinx_theme"
html_static_path = ['_static']




# custom Pygments highlighting:
from pygments.lexer import RegexLexer
from pygments import token
from sphinx.highlighting import lexers

# This a a placeholder, and does not work as indented:
class RiftLexer(RegexLexer):
    name = 'rift'

    tokens = {
        'root': [
            (r'i32', token.Keyword),
            (r'[a-zA-Z]', token.Name),
        ],
        'comment': [
            (r'/\*', token.Comment.Multiline, '#push'),
            (r'\*/', token.Comment.Multiline, '#pop'),
        ]
    }

lexers['rift'] = RiftLexer(startinline=True)

print("INNER CONFIG")