#!/usr/bin/env bash
# Smoke test: build mp-render, render samples/demo.md, and verify that
# every expected token appears in the output. Exits 0 on success.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

if [[ ! -x build/mp-render ]]; then
    echo "==> building mp-render"
    cmake -B build >/dev/null
    cmake --build build --target mp-render -j >/dev/null
fi

OUT=$(mktemp /tmp/mp-render-test.XXXXXX.html)
trap 'rm -f "$OUT"' EXIT

./build/mp-render samples/demo.md > "$OUT" 2>/dev/null

pass=0
fail=0
expect() {
    local label="$1"; local pattern="$2"; local min="${3:-1}"
    local count
    count=$(grep -c -F "$pattern" "$OUT" || true)
    if [[ "$count" -ge "$min" ]]; then
        printf "  ok  %-35s (%d)\n" "$label" "$count"
        pass=$((pass+1))
    else
        printf "  FAIL %-35s want>=%d got %d\n" "$label" "$min" "$count"
        fail=$((fail+1))
    fi
}

forbid() {
    local label="$1"; local pattern="$2"
    if ! grep -q -F "$pattern" "$OUT"; then
        printf "  ok  %-35s (absent)\n" "$label"
        pass=$((pass+1))
    else
        printf "  FAIL %-35s should NOT appear\n" "$label"
        fail=$((fail+1))
    fi
}

echo "== rendering features =="
expect "markdown body wrapper"  'class="markdown-body"'
expect "h1 heading"             '<h1>mark-preview demo</h1>'
expect "GFM table"              '<table>'
expect "task-list checkbox"     '<input type="checkbox"'
expect "strikethrough"          '<del>strike</del>'
expect "autolink"               '<a href="https://github.com">'
expect "code block ruby"        'class="language-ruby"'
expect "code block go"          'class="language-go"'
expect "code block mermaid"     'class="language-mermaid"'
expect "math display dollar"    '$$'
expect "math inline dollar"     '$E = mc^2$'

echo "== bundled asset URLs =="
expect "github-light.css link"  'github-light.css'
expect "katex css link"         'katex/katex.min.css'
expect "highlight css link"     'highlight/github.min.css'
expect "katex js"               'katex/katex.min.js'
expect "auto-render js"         'katex/auto-render.min.js'
expect "mermaid js"             'mermaid/mermaid.min.js'
expect "highlight js"           'highlight/highlight.min.js'
expect "app.js"                 'app.js'

echo "== base url =="
expect "base href points at md dir"  '<base href="file://'
expect "base ends with samples/"     'samples/">'

echo "== safety =="
forbid "raw <script>alert escaped" "<script>alert"

echo
echo "Result: pass=$pass fail=$fail"
exit "$fail"
