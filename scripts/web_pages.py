"""Web portal page table and product name, shared by build_web.py and preview_web.py.

The product name comes from [crossink] product_name in platformio.ini, the
same value the firmware build passes to C++ as CROSSINK_PRODUCT_NAME."""
import os
import re


def product_name(root):
    """Return [crossink] product_name from platformio.ini."""
    section = None
    with open(os.path.join(root, "platformio.ini"), encoding="utf-8") as f:
        for line in f:
            stripped = line.strip()
            if stripped.startswith("["):
                section = stripped
            elif section == "[crossink]":
                match = re.match(r"product_name\s*=\s*([^;#]+)", stripped)
                if match:
                    return match.group(1).strip()
    raise SystemExit("web_pages: [crossink] product_name is missing from platformio.ini")


def pages(product):
    """Pages the firmware serves: slug -> (generated identifier, <title>, active nav, extra <head> markup)."""
    return {
        "runner":   ("RunnerPageHtml",   f"Plugins - {product}",         "settings", '<script src="/plugin-host.js"></script>'),
        "home":     ("HomePageHtml",     product,                        "home",     ""),
        "files":    ("FilesPageHtml",    f"Files - {product}",           "files",    '  <script src="/js/jszip.min.js"></script><script src="/plugin-host.js"></script>'),
        "settings": ("SettingsPageHtml", f"Settings - {product} Reader", "settings", '<script src="/plugin-host.js"></script>'),
        "fonts":    ("FontsPageHtml",    f"Fonts - {product}",           "fonts",    ""),
    }
