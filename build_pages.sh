#!/bin/sh
# Builds the GitHub Pages version into docs/
# The C++ analyzer is compiled to WebAssembly with Emscripten, so the web map
# works without running server.exe.
#
# With Emscripten installed:   sh build_pages.sh
# With Docker instead:         docker run --rm -v "$PWD:/src" -w /src emscripten/emsdk:3.1.74 sh build_pages.sh
set -e

mkdir -p docs
cp web/index.html web/style.css web/app.js docs/

emcc -std=c++11 -O2 src/server.cpp src/Network.cpp -o docs/analyzer.js \
  -sMODULARIZE=1 -sEXPORT_NAME=createAnalyzer \
  -sEXPORTED_FUNCTIONS=_handle_api,_main \
  -sEXPORTED_RUNTIME_METHODS=ccall \
  -sALLOW_MEMORY_GROWTH=1 \
  --embed-file data@/data

# load the wasm analyzer before app.js
sed -i 's#<script src="app.js"></script>#<script src="analyzer.js"></script>\n<script>window.NFA_READY = createAnalyzer().then(function (m) { window.NFA_WASM = function (path, query) { return m.ccall("handle_api", "string", ["string", "string"], [path, query]); }; });</script>\n<script src="app.js"></script>#' docs/index.html

touch docs/.nojekyll
echo "docs/ is ready"
