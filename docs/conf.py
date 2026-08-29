# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html


project = 'RAS Daemon'
copyright = '2013-2026, Mauro Carvalho Chehab'
author = 'Mauro Carvalho Chehab'

extensions = []

exclude_patterns = []

primary_domain = "c"
highlight_language = "bash"

#templates_path = ['templates']
html_static_path = ['static']

html_theme = 'alabaster'
html_theme_options = {
    "page_width": "65em",
    "sidebar_width": "15em",
    "fixed_sidebar": "true",
    "font_size": "inherit",
    "font_family": "serif",
}
