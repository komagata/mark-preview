#!/usr/bin/env bash
# Download third-party assets that get embedded into the binary at build time.
# Idempotent: skips files that already exist.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ASSETS="${ROOT_DIR}/assets"

KATEX_VERSION="0.16.11"
MERMAID_VERSION="10.9.1"
HLJS_VERSION="11.10.0"
GH_MD_CSS_VERSION="5.6.1"

mkdir -p \
    "${ASSETS}" \
    "${ASSETS}/katex" \
    "${ASSETS}/katex/fonts" \
    "${ASSETS}/mermaid" \
    "${ASSETS}/highlight"

dl() {
    local url="$1"
    local dest="$2"
    if [[ -s "${dest}" ]]; then
        echo "  exists: ${dest#${ROOT_DIR}/}"
        return
    fi
    echo "  fetch:  ${dest#${ROOT_DIR}/}"
    curl --fail --silent --show-error --location --output "${dest}" "${url}"
}

echo "==> KaTeX ${KATEX_VERSION}"
KATEX_BASE="https://cdn.jsdelivr.net/npm/katex@${KATEX_VERSION}/dist"
dl "${KATEX_BASE}/katex.min.css"               "${ASSETS}/katex/katex.min.css"
dl "${KATEX_BASE}/katex.min.js"                "${ASSETS}/katex/katex.min.js"
dl "${KATEX_BASE}/contrib/auto-render.min.js"  "${ASSETS}/katex/auto-render.min.js"

# KaTeX bundles around 60 font files; we only need the woff2 set that
# katex.min.css references. Pull the ones used by the default math fonts.
KATEX_FONTS=(
    KaTeX_AMS-Regular
    KaTeX_Caligraphic-Bold
    KaTeX_Caligraphic-Regular
    KaTeX_Fraktur-Bold
    KaTeX_Fraktur-Regular
    KaTeX_Main-Bold
    KaTeX_Main-BoldItalic
    KaTeX_Main-Italic
    KaTeX_Main-Regular
    KaTeX_Math-BoldItalic
    KaTeX_Math-Italic
    KaTeX_SansSerif-Bold
    KaTeX_SansSerif-Italic
    KaTeX_SansSerif-Regular
    KaTeX_Script-Regular
    KaTeX_Size1-Regular
    KaTeX_Size2-Regular
    KaTeX_Size3-Regular
    KaTeX_Size4-Regular
    KaTeX_Typewriter-Regular
)
for f in "${KATEX_FONTS[@]}"; do
    dl "${KATEX_BASE}/fonts/${f}.woff2" "${ASSETS}/katex/fonts/${f}.woff2"
done

echo "==> Mermaid ${MERMAID_VERSION}"
dl "https://cdn.jsdelivr.net/npm/mermaid@${MERMAID_VERSION}/dist/mermaid.min.js" \
   "${ASSETS}/mermaid/mermaid.min.js"

echo "==> highlight.js ${HLJS_VERSION}"
HLJS_BASE="https://cdn.jsdelivr.net/gh/highlightjs/cdn-release@${HLJS_VERSION}/build"
dl "${HLJS_BASE}/highlight.min.js"             "${ASSETS}/highlight/highlight.min.js"
dl "${HLJS_BASE}/styles/github.min.css"        "${ASSETS}/highlight/github.min.css"
dl "${HLJS_BASE}/styles/github-dark.min.css"   "${ASSETS}/highlight/github-dark.min.css"

echo "==> github-markdown-css ${GH_MD_CSS_VERSION}"
GH_MD_BASE="https://cdn.jsdelivr.net/npm/github-markdown-css@${GH_MD_CSS_VERSION}"
dl "${GH_MD_BASE}/github-markdown-light.css"   "${ASSETS}/github-light.css"
dl "${GH_MD_BASE}/github-markdown-dark.css"    "${ASSETS}/github-dark.css"

echo
echo "All assets present under: ${ASSETS}/"
