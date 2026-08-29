# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

import importlib.util
import os
import sys

docs_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(docs_dir, "sphinx"))

kerneldoc_srctree = os.path.join(docs_dir, "..")


project = 'RAS Daemon'
copyright = '2013-2026, Mauro Carvalho Chehab'
author = 'Mauro Carvalho Chehab'
version = release = 'development'

extensions = ['kerneldoc']

exclude_patterns = []

primary_domain = "c"
highlight_language = "bash"

pygments_style = 'sphinx'

if importlib.util.find_spec('furo') is None:
    html_theme = 'alabaster'
    html_static_path = ['static/alabaster']
    html_theme_options = {
        "page_width": "120em",
        "sidebar_width": "15em",
        "fixed_sidebar": "true",
        "font_size": "inherit",
        "font_family": "serif",
    }
else:
    html_theme = 'furo'
    html_static_path = ['static/furo']
    html_css_files = ['custom.css']
    html_theme_options = {
        "navigation_with_keys": True,
        "light_css_variables": {
            "color-brand-primary": "#8f1d1d",
            "color-brand-content": "#8f1d1d",
            "color-api-background": "#f3f5f7",
        },
        "dark_css_variables": {
            "color-brand-primary": "#ff8a80",
            "color-brand-content": "#ff8a80",
            "color-api-background": "#20242b",
        },
    }
    pygments_dark_style = 'monokai'
