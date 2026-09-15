EMXX ?= em++
CXX ?= c++
PYTHON ?= python3
# emsdk adds a directory containing a `node/` subdirectory to PATH. Resolve
# through the shell so GNU make cannot mistake that directory for an executable.
NODE ?= $(shell command -v node)
GAME = ../usr/src/game
PORTABLE = ../usr/src/portable
CPPFLAGS += -DBT_PORTABLE=1 -I$(PORTABLE) -I$(GAME) -I../usr/src/stdlib -I. -Ibuild
CXXFLAGS = -std=c++11 -O2
ENGINE = $(GAME)/BTPiece.C $(GAME)/BTPieceManager.C $(GAME)/BTBoardManager.C \
         $(GAME)/BTBoard.C $(GAME)/BTRingNode.C $(GAME)/BTCBoard.C \
         $(PORTABLE)/BTBoxPortable.C $(PORTABLE)/BTPlanner.C
SESSION = Game.C Match.C Catalog.C Recon.C
TESTS = tests/core.C tests/planner.C tests/effects.C tests/combat.C tests/swaps.C tests/peers.C tests/structures.C tests/landing.C tests/motion.C tests/upside.C tests/upside_planner.C tests/recon.C tests/recon_match.C tests/humans.C
HEADERS = $(wildcard $(GAME)/*.H $(PORTABLE)/*.H ../usr/src/stdlib/*.H) \
          Game.H Match.H Catalog.H Recon.H build/WeaponData.H

.PHONY: all assets test test-wasm serve dist archive check-dist clean
all: build/battletris.js build/index.html build/app.js build/online.html build/online.js assets

assets: | build
	rm -rf build/assets
	@if test -d assets; then cp -R assets build/assets; fi

build:
	mkdir -p $@

build/WeaponData.H: generate_catalog.py ../usr/src/share/btweapons.db ../usr/src/share/btweaponsp.db | build
	$(PYTHON) generate_catalog.py ../usr/src/share/btweapons.db ../usr/src/share/btweaponsp.db $@

build/battletris.js: $(ENGINE) $(SESSION) exports.C $(HEADERS) Makefile | build
	$(EMXX) $(CPPFLAGS) $(CXXFLAGS) $(ENGINE) $(SESSION) exports.C --no-entry \
	  -sMODULARIZE=1 -sEXPORT_NAME=createBattleTris -sENVIRONMENT=web,node \
	  -sEXPORTED_RUNTIME_METHODS=HEAP32,UTF8ToString \
	  -sALLOW_MEMORY_GROWTH=1 -sASSERTIONS=1 -o $@

build/index.html: index.html | build
	cp $< $@
build/online.html: online.html | build
	cp $< $@
build/online.js: online.js | build
	cp $< $@
build/app.js: app.js | build
	cp $< $@

build/core-test: $(ENGINE) $(SESSION) $(TESTS) $(HEADERS) Makefile | build
	$(CXX) $(CPPFLAGS) -std=c++11 -O1 -g -fsanitize=address,undefined \
	  -fno-omit-frame-pointer $(ENGINE) $(SESSION) $(TESTS) -o $@

test: build/core-test
	./build/core-test

build/core-test.js: $(ENGINE) $(SESSION) $(TESTS) $(HEADERS) Makefile | build
	$(EMXX) $(CPPFLAGS) $(CXXFLAGS) $(ENGINE) $(SESSION) $(TESTS) \
	  -sENVIRONMENT=node -sASSERTIONS=2 -sSAFE_HEAP=1 -sSTACK_OVERFLOW_CHECK=2 -o $@

test-wasm: all build/core-test.js build/core-test
	"$(NODE)" build/core-test.js
	"$(NODE)" tests/wasm.cjs
	"$(NODE)" tests/compare.cjs

serve: all
	$(PYTHON) -m http.server 8000 --bind 127.0.0.1 --directory build

dist: all
	$(PYTHON) package.py

archive: dist
	$(PYTHON) package.py --archive

check-dist: dist
	$(PYTHON) tests/package.py

clean:
	rm -rf build dist
