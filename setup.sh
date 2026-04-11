#!/bin/bash
# ============================================================================
# setup.sh - Download third-party dependencies
# ============================================================================
# run this after cloning the repo to fetch header-only libraries
# that are too large to commit to git.
#
# usage: ./setup.sh
# ============================================================================

set -e

INCLUDE_DIR="$(dirname "$0")/include"
mkdir -p "$INCLUDE_DIR"

echo "downloading Eigen 3.4.0 (linear algebra)..."
curl -sL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz -o /tmp/eigen.tar.gz
cd /tmp && tar xzf eigen.tar.gz
cp -r /tmp/eigen-3.4.0/Eigen "$INCLUDE_DIR/"
rm -rf /tmp/eigen-3.4.0 /tmp/eigen.tar.gz
echo "  done."

echo "downloading nlohmann/json 3.11.3..."
curl -sL https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp -o "$INCLUDE_DIR/json.hpp"
echo "  done."

echo ""
echo "all dependencies installed. you can now build:"
echo "  mkdir build && cd build && cmake .. && make"
