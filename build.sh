#!/bin/bash
set -eo pipefail
cd "$(dirname "$0")"
mkdir -p build
musl-gcc -O2 -Wall -pedantic-errors -s -static recur.c -o build/recur