#!/usr/bin/env bash

# This scripts enables imitating, on a local shell, the run of the BuildHTMLFromJSONFiles 
# GitHub workflow (residing under .github/workflows/build_html_from_json_files.yml)
# The output directory will reside under ../Pages

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")

ROOT_DIR="${SCRIPT_DIR}/.."

"${SCRIPT_DIR}"/BuildHTMLFromJSONFiles.py -i "${ROOT_DIR}"/PluginFiles -o "${ROOT_DIR}"/Pages/index.html
cp -r "${ROOT_DIR}"/PluginFiles/Images "${ROOT_DIR}"/Pages
