#!/usr/bin/env bash
# usage: render-formula.sh VERSION TARBALL > k10s.rb
set -euo pipefail
version="$1"
sha="$(shasum -a 256 "$2" | awk '{print $1}')"
dir="$(cd "$(dirname "$0")" && pwd)"
sed -e "s/@VERSION@/${version}/g" -e "s/@SHA256@/${sha}/g" "${dir}/k10s.rb.in"
