"""Sphinx configuration for the flexlib site."""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).parent / "_ext"))

_ver = (ROOT / "arch/fx_version.h").read_text()
version = release = ".".join(
    re.search(rf"define FLEXLIB_VERSION_{p} (\d+)", _ver).group(1)
    for p in ("MAJOR", "MINOR", "PATCH")
)
try:
    commit = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT,
                            capture_output=True, text=True, check=True).stdout.strip()
except (OSError, subprocess.CalledProcessError):
    commit = ""

project = "flexlib"
author = "Antony Kasera"
copyright = "2026 Antony Kasera"

extensions = ["breathe", "myst_parser", "sphinx_design", "sphinx_copybutton",
              "flexlib_site"]
exclude_patterns = ["examples", "_ext", "get-started.md", "results.md",
                    "design.md", "dsp"]
myst_enable_extensions = ["colon_fence", "deflist", "dollarmath", "attrs_inline"]
myst_heading_anchors = 3

breathe_projects = {"flexlib": str(ROOT / "build/doxygen/xml")}
breathe_default_project = "flexlib"
breathe_domain_by_extension = {"h": "c"}
breathe_show_include = False
primary_domain = "c"
toc_object_entries = False
smartquotes_action = "De"  # dashes and ellipses; quotes stay straight
highlight_language = "c"

html_theme = "pydata_sphinx_theme"
html_title = "flexlib"
html_static_path = ["_static"]
html_css_files = ["custom.css"]
templates_path = ["_templates"]
html_context = {"commit": commit, "default_mode": "auto"}
html_show_sourcelink = False
html_copy_source = False
html_sidebars = {"index": [], "**": ["sidebar-nav-bs"]}

html_theme_options = {
    "navbar_start": ["navbar-logo"],
    "navbar_center": ["navbar-nav"],
    "navbar_end": ["version-switcher", "search-button", "navbar-icon-links",
                   "theme-switcher"],
    "navbar_persistent": [],
    "header_links_before_dropdown": 7,
    "switcher": {"json_url": "_static/switcher.json", "version_match": version},
    "check_switcher": False,
    "icon_links": [{"name": "Repository",
                    "url": "https://github.com/antonykasera/flexlib",
                    "icon": "fa-brands fa-github"}],
    "article_header_start": [],
    "secondary_sidebar_items": ["page-toc"],
    "show_toc_level": 1,
    "show_nav_level": 1,
    "navigation_depth": 2,
    "show_prev_next": False,
    "footer_start": ["flexlib-footer"],
    "footer_center": [],
    "footer_end": [],
    "pygments_light_style": "default",
    "pygments_dark_style": "github-dark",
}

copybutton_prompt_text = "$ "
copybutton_only_copy_prompt_lines = False
