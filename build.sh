#!/bin/bash
set -eo pipefail
cd "$(dirname "$0")"
mkdir -p build
musl-gcc -O2 -fanalyzer -Wall -Wextra -Werror -pedantic-errors -s -static recur.c -o build/recur