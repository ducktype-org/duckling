#!/usr/bin/env bash
# Builds main.pdf from main.tex.
# Runs pdflatex, then bibtex, then pdflatex twice more so that references,
# citations and the table of figures all resolve.
set -euo pipefail

cd "$(dirname "$0")"

JOB=main
OUT=build
export BIBINPUTS="${BIBINPUTS-}"

command -v pdflatex >/dev/null || { echo "pdflatex not found" >&2; exit 1; }
command -v bibtex   >/dev/null || { echo "bibtex not found" >&2; exit 1; }

mkdir -p "$OUT"

run_latex() {
	pdflatex -interaction=nonstopmode -halt-on-error -file-line-error \
		-output-directory="$OUT" "$JOB.tex" >/dev/null
}

echo "[1/4] pdflatex"
run_latex
echo "[2/4] bibtex"
(cd "$OUT" && BIBINPUTS="..:$BIBINPUTS" bibtex "$JOB" >/dev/null) \
	|| echo "  bibtex reported warnings, continuing"
echo "[3/4] pdflatex"
run_latex
echo "[4/4] pdflatex"
run_latex

cp "$OUT/$JOB.pdf" "./$JOB.pdf"
echo
echo "Done: $(pwd)/$JOB.pdf"
if command -v pdfinfo >/dev/null; then
	pdfinfo "$JOB.pdf" | grep -E '^(Pages|Page size)'
fi
