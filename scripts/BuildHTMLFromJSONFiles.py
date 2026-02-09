#!/usr/bin/env python3
"""
Inputs a directory containing Nsight Systems plugins JSON files,.
parses the JSON files to create an HTML site listing all plugins.
"""

import sys
import argparse
import json
from pathlib import Path


REQUIRED_KEYS = ("SchemaVersion", "Name", "Description", "Company", "SiteURL", "Architectures", "OperatingSystems")
VALID_ARCHITECTURES = {"x64", "aarch64"}
VALID_OPERATING_SYSTEMS = {"Windows", "Linux"}


def validate_plugin_json(data: dict) -> list[str]:
    """
    Validate plugin JSON format. Returns a list of error messages (empty if valid).
    """
    errors = []
    if not isinstance(data, dict):
        return [f"root must be a JSON object"]

    for key in REQUIRED_KEYS:
        if key not in data:
            errors.append(f"missing required key: {key!r}")

    if "SchemaVersion" in data:
        v = data["SchemaVersion"]
        if not isinstance(v, int) or v != 1:
            errors.append("SchemaVersion must be the integer 1")

    for key in ("Name", "Description", "Company", "SiteURL"):
        if key in data and data[key] is not None and not isinstance(data[key], str):
            errors.append(f"{key!r} must be a string")

    for key in ("Architectures", "OperatingSystems"):
        if key in data:
            val = data[key]
            if not isinstance(val, list):
                errors.append(f"{key!r} must be an array")
            else:
                for i, item in enumerate(val):
                    if not isinstance(item, str):
                        errors.append(f"{key!r}[{i}] must be a string")
                if key == "Architectures":
                    invalid = set(val) - VALID_ARCHITECTURES
                    if invalid:
                        errors.append(f"Architectures must only contain {sorted(VALID_ARCHITECTURES)}; invalid: {sorted(invalid)}")
                elif key == "OperatingSystems":
                    invalid = set(val) - VALID_OPERATING_SYSTEMS
                    if invalid:
                        errors.append(f"OperatingSystems must only contain {sorted(VALID_OPERATING_SYSTEMS)}; invalid: {sorted(invalid)}")

    if "Images" in data:
        img = data["Images"]
        if not isinstance(img, list):
            errors.append("Images must be an array")
        else:
            for i, entry in enumerate(img):
                if not isinstance(entry, dict):
                    errors.append(f"Images[{i}] must be an object")
                else:
                    if "Path" in entry and entry["Path"] is not None and not isinstance(entry["Path"], str):
                        errors.append(f"Images[{i}].Path must be a string")
                    if "Description" in entry and entry["Description"] is not None and not isinstance(entry["Description"], str):
                        errors.append(f"Images[{i}].Description must be a string")

    for key in ("MinNsightSystemsVersion", "SetupNotes"):
        if key in data and data[key] is not None and not isinstance(data[key], str):
            errors.append(f"{key!r} must be a string")

    return errors


def load_plugin_json(path: Path) -> dict | None:
    """Load and parse a single plugin JSON file. Returns None on failure."""
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, json.JSONDecodeError) as e:
        print(f"Error: failed to parse {path}: {e}", file=sys.stderr)
        return None


def collect_plugins(input_dir: Path) -> list[dict]:
    """Collect all valid plugin objects from JSON files in input_dir."""
    plugins = []
    for path in sorted(input_dir.glob("*.json")):
        data = load_plugin_json(path)
        if data is None:
            print(f"Error: failed to load {path}:", file=sys.stderr)
            continue
        validation_errors = validate_plugin_json(data)
        if validation_errors:
            print(f"Error: invalid format in {path}:", file=sys.stderr)
            for err in validation_errors:
                print(f"  - {err}", file=sys.stderr)
            continue
        plugins.append(data)
    return plugins


def escape(s: str) -> str:
    """Escape HTML special characters."""
    return (
        s.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
    )


def build_html(plugins: list[dict], output_file_path: Path) -> None:
    """Generate an HTML page listing all plugins and write it to output_file_path."""
    title = "Nsight Systems Plugins"
    toc_rows = []
    body_rows = []
    for i, p in enumerate(plugins):
        company = escape(p.get("Company", "Unnamed"))
        name = escape(p.get("Name", "Unnamed"))
        anchor_id = f"plugin-{i}"
        toc_rows.append(f'        <li><a href="#{anchor_id}">{name}</a></li>')
        desc = escape(p.get("Description", ""))
        site_url = p.get("SiteURL", "")
        site_esc = escape(site_url)
        archs = ", ".join(p.get("Architectures", []))
        oses = ", ".join(p.get("OperatingSystems", []))
        min_ver = escape(str(p.get("MinNsightSystemsVersion", "")))
        setup_notes = escape(p.get("SetupNotes", ""))
        images = p.get("Images") or []
        img_html = ""
        if images:
            first = images[0]
            path = first.get("Path", "")
            alt = escape(first.get("Description", name))
            if path:
                path_esc = escape(path)
                img_html = f'<a href="{path_esc}"><img src="{path_esc}" alt="{alt}" class="plugin-img" /></a>'
        site_link = f'<a href="{site_esc}" rel="noopener noreferrer">{site_esc}</a>' if site_url else ""
        body_rows.append(
            f"""
        <article class="plugin-card" id="{anchor_id}">
            <div class="plugin-header">
                <h2>{name}</h2>
            </div>
            <p class="plugin-desc">{desc}</p>
            {f'<div class="plugin-thumb">{img_html}</div>' if img_html else ''}
            <dl class="plugin-meta">
                <dt>Architectures</dt><dd>{escape(archs) or "-"}</dd>
                <dt>Operating systems</dt><dd>{escape(oses) or "-"}</dd>
                {f'<dt>Minimal Nsight Systems version</dt><dd>{min_ver}</dd>' if min_ver else ''}
                {f'<dt>Setup Notes</dt><dd>{setup_notes}</dd>' if setup_notes else ''}
                <dt>Site URL</dt><dd>{f'<p class="plugin-link">{site_link}</p>' if site_link else ''}</dd>
                <dt>Company</dt><dd>{company}</dd>
            </dl>
        </article>"""
        )
    toc = "\n".join(toc_rows)
    body = "\n".join(body_rows)
    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>{title}</title>
    <link rel="icon" type="image/x-icon" href="./Images/nvidia-favicon.ico">
    <style>
        :root {{ font-family: system-ui, sans-serif; line-height: 1.5; color: #1a1a1a; background: #f5f5f5; }}
        body {{ max-width: 720px; margin: 0 auto; padding: 1.5rem; }}
        .page-header {{ display: flex; align-items: center; gap: 1rem; margin-bottom: 0.5rem; }}
        .page-header img {{ height: 5rem; width: auto; display: block; margin-left: -1.5rem; }}
        h1 {{ margin: 0; }}
        .toc {{ background: #fff; border-radius: 8px; padding: 1rem 1.25rem; margin-bottom: 1.5rem; box-shadow: 0 1px 3px rgba(0,0,0,0.08); }}
        .toc ul {{ margin: 0; padding-left: 1.5rem; }}
        .toc li {{ margin: 0.35rem 0; }}
        .toc a {{ color: #0066cc; text-decoration: none; }}
        .toc a:hover {{ text-decoration: underline; }}
        .plugin-card {{ background: #fff; border-radius: 8px; padding: 1.25rem; margin-bottom: 1.5rem; box-shadow: 0 1px 3px rgba(0,0,0,0.08); }}
        .plugin-header {{ display: flex; align-items: flex-start; gap: 1rem; flex-wrap: wrap; }}
        .plugin-header h2 {{ margin: 0 0 0.5rem 0; font-size: 1.25rem; }}
        .plugin-thumb {{ flex-shrink: 0; }}
        .plugin-thumb a {{ text-decoration: none; display: inline-block; }}
        .plugin-img {{ max-width: 700px; max-height: 300px; object-fit: contain; border-radius: 4px; }}
        .plugin-desc {{ margin: 0.5rem 0; color: #333; }}
        .plugin-meta {{ margin: 0.75rem 0; font-size: 0.9rem; }}
        .plugin-meta dt {{ font-weight: 600; margin-top: 0.25rem; }}
        .plugin-meta dd {{ margin: 0 0 0 1rem; }}
        .plugin-link {{ margin: 0.5rem 0 0 0; }}
        .plugin-link a {{ color: #0066cc; }}
    </style>
</head>
<body>
    <header class="page-header">
        <img src="./Images/nvidia-logo-horiz-rgb-blk-for-screen.svg" alt="NVIDIA" />
    </header>
    <h1>{title}</h1>
    <p>This site lists Nsight Systems third-party plugins.<p>
    <ul>
        <li>For information about Nsight Systems, visit the <a href="https://developer.nvidia.com/nsight-systems" target="_blank" rel="noopener noreferrer">Nsight Systems website</a>.</li>
        <li>To add a third-party plugin to this list, refer to the instructions provided in the <a href="https://github.com/NVIDIA/NsightSystemsPlugins/blob/main/ADD_PLUGIN.md" target="_blank" rel="noopener noreferrer">ADD_PLUGIN.md file.</a> file.</li>
    </ul>
    <nav class="toc" aria-label="Plugin list">
    <p><b>Table of Contents</b></p>
        <ul>
            {toc}
        </ul>
        <p><a href="#legal-disclaimer">Legal Disclaimer</a></p>
    </nav>
{body}
<article class="plugin-card" id="legal-disclaimer">
<p><b>Legal Disclaimer</b></p>
    <p>The plugins made available on this site are third-party projects provided solely as a convenience and resource for developers. These plugins are not developed, reviewed, tested, modified, or endorsed by us.</p>
    <p>Third-party plugins may contain errors, security vulnerabilities, or functionality that is inaccurate, incomplete, or otherwise undesirable. By downloading or using any plugin, you acknowledge and agree that you do so at your own risk. We disclaim all responsibility and liability for any harm, damage, or loss arising from the use of these plugins.</p>
    <p>Use of any plugin is subject to the applicable third-party license terms, and you are solely responsible for complying with those terms.</p>
    <p>We do not provide support, updates, or security fixes for these plugins.</p>
</article>
</body>
</html>
"""
    output_file_path.parent.mkdir(parents=True, exist_ok=True)
    output_file_path.write_text(html, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Build HTML from Nsight Systems plugin JSON files.")
    parser.add_argument(
        "-i",
        "--input-dir",
        default=None,
        type=Path,
        help="Directory containing Nsight Systems plugins JSON files",
    )
    parser.add_argument(
        "-o",
        "--output-file",
        default=None,
        type=Path,
        help="The output HTML file path",
    )
   
    args = parser.parse_args()
    input_dir = args.input_dir.resolve()
    if not input_dir.is_dir():
        print(f"Error: not a directory: {input_dir}", file=sys.stderr)
        return 1
    if args.output_file is not None:
        output_file_path = args.output_file.resolve()
    else:
        print(f"Error: --output-file was not specified")
        return 1
    
    plugins = collect_plugins(input_dir)
    if not plugins:
        print("No plugin JSON files found.", file=sys.stderr)
        return 1
    
    build_html(plugins, output_file_path)
    print(f"Wrote {output_file_path} ({len(plugins)} plugin(s))")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
