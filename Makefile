# Malwarium — repo-root convenience targets.
#
# Deliberately NOT wired into platformio.ini (no extra_scripts pre-hook): running python
# on every device build would be a surprise. These are developer-invoked, first-cut helpers.

.PHONY: pedia pedia-check pedia-sd pedia-tar dump-content

# Regenerate web/data/pedia_data.js from the live firmware content tables
# (src/core/content/embedded_content.cpp, src/core/model/combat.cpp, include/tunables.h),
# and copy every asset that data references into web/assets/ so the served bundle
# can never point at art the card doesn't carry. Run this + commit whenever a
# creature/item/move/mod table changes, or whenever an asset it references is redrawn.
pedia: dump-content
	python3 tools/gen_pedia_data.py --repo . --out web/data/pedia_data.js

# The generator reads the content tables through the firmware's OWN code (see the
# banner in tools/dump_content.cpp): descriptions are templates the device expands
# from each row's magnitudes, so running the device's expander is what keeps the
# site's numbers from disagreeing with the panel's. The tool needs only the content
# tables + the expander — no framebuffer, no assets, no engine — so it compiles
# directly here instead of linking malcore, and works without cmake.
dump-content: build/dump_content
build/dump_content: tools/dump_content.cpp src/core/content/effect_text.cpp \
                    $(wildcard src/core/content/content_*.cpp) \
                    $(wildcard src/core/content/areas/*/area.cpp) \
                    $(wildcard src/core/content/creatures/*/line.h) \
                    $(wildcard src/core/content/creatures/*.h) \
                    $(wildcard src/core/content/*.h) src/core/content/effect_text.h \
                    src/core/app/game_achievements.h src/core/app/game_rig_shop.h
	@mkdir -p build
	$(CXX) -std=c++17 -O1 -I src -I include -o $@ \
	    tools/dump_content.cpp src/core/content/content_*.cpp \
	    src/core/content/areas/*/area.cpp \
	    src/core/content/effect_text.cpp

# CI-staleness guard: regenerate to a temp file and diff against the committed copy.
# Fails (non-zero) if they differ, so a content change without a re-gen gets caught.
# Data only — the asset sync is `pedia`'s job, and a check shouldn't edit the tree.
pedia-check: dump-content
	@tmp=$$(mktemp); \
	python3 tools/gen_pedia_data.py --repo . --out $$tmp --no-sync-assets; \
	if diff -q $$tmp web/data/pedia_data.js > /dev/null; then \
		echo "pedia data in sync"; \
		rm -f $$tmp; \
	else \
		echo "web/data/pedia_data.js is STALE — run 'make pedia' and commit the result"; \
		diff -u web/data/pedia_data.js $$tmp || true; \
		rm -f $$tmp; \
		exit 1; \
	fi

# Stage the served web/ bundle onto an SD card (or any mounted destination) so the
# on-device AP server (src/platform/esp32/ap_server.h) finds it at /sdcard/web/<path>.
# Regenerates pedia_data.js first so the staged copy is never stale. Excludes the
# dev-only web/fixtures/ (browser-preview fixture, never served on-device) and
# web/README.md (not part of the served bundle).
#
# web/VERSION rides along: it is what an update check reads back to decide whether
# the card's bundle is older than the published one, so a hand-staged card must
# carry the same stamp an OTA install would write (src/platform/esp32/web_bundle.h).
#
# Usage: make pedia-sd DEST=/Volumes/MALWARIUM
pedia-sd: pedia
	@if [ -z "$(DEST)" ]; then \
		echo "error: DEST is required, e.g.:"; \
		echo "  make pedia-sd DEST=/Volumes/MALWARIUM"; \
		exit 1; \
	fi
	@mkdir -p "$(DEST)/web/assets" "$(DEST)/web/data" "$(DEST)/web/fonts"
	@cp web/index.html web/style.css web/app.js web/VERSION "$(DEST)/web/"
	@cp -R web/assets/. "$(DEST)/web/assets/"
	@cp -R web/data/. "$(DEST)/web/data/"
	@cp -R web/fonts/. "$(DEST)/web/fonts/"
	@# macOS cp writes AppleDouble ._* sidecars (+ .DS_Store) when copying xattrs
	@# onto a FAT/exFAT card — harmless to the device but dead-weight litter. Strip
	@# them so the staged bundle is exactly the served files.
	@find "$(DEST)/web" \( -name '._*' -o -name '.DS_Store' \) -delete 2>/dev/null || true
	@echo "staged web/ bundle -> $(DEST)/web/"
	@echo "  $(DEST)/web/index.html"
	@echo "  $(DEST)/web/style.css"
	@echo "  $(DEST)/web/app.js"
	@echo "  $(DEST)/web/VERSION  ($$(grep '^code=' web/VERSION))"
	@echo "  $(DEST)/web/assets/  ($$(find web/assets -type f | wc -l | tr -d ' ') files)"
	@echo "  $(DEST)/web/data/    ($$(find web/data -type f | wc -l | tr -d ' ') files)"
	@echo "  $(DEST)/web/fonts/   ($$(find web/fonts -type f | wc -l | tr -d ' ') files)"

# Build the `web` artifact an on-device update downloads, and print the manifest
# row that describes it. The device unpacks this over /sdcard/web with no
# decompressor, so the shape is fixed — tools/make_web_tar.py is its reference
# producer, and that file's header states the format the reader requires.
#
# The archive is written by a script rather than `tar` because it has to be
# BYTE-REPRODUCIBLE: the manifest publishes a SHA-256 over it, so identical
# content must give an identical hash whether it was built here or on a runner.
# `tar` records mtimes and the building user's uid/gid, which makes every build
# a different archive and turns the digest check into a race against the next
# publish. The flags that would fix that differ between BSD and GNU tar, so the
# portable answer is to write the headers directly.
#
# Usage: make pedia-tar [OUT=dist/web-0.1.0.tar]
pedia-tar: pedia
	@out="$(OUT)"; [ -n "$$out" ] || out="dist/web-$$(sed -n 's/^version=//p' web/VERSION).tar"; \
	python3 tools/make_web_tar.py web "$$out"; \
	echo; \
	echo "manifest row:"; \
	printf '  {"id":"web","version":"%s","code":%s,\n' \
	  "$$(sed -n 's/^version=//p' web/VERSION)" "$$(sed -n 's/^code=//p' web/VERSION)"; \
	printf '   "url":"https://YOUR-HOST/%s","size":%s,\n' \
	  "$$(basename "$$out")" "$$(wc -c < "$$out" | tr -d ' ')"; \
	printf '   "sha256":"%s"}\n' "$$(shasum -a 256 "$$out" | cut -d' ' -f1)"

# --- Publishing ------------------------------------------------------------
#
# The targets below build everything the publish host serves, into dist/:
# `manifest` is what a DEVICE downloads (the artifacts and the list describing
# them), `pages` adds what a PERSON opens (pages/, plus the boot images the USB
# flasher writes). `pages` is the whole publish and what CI runs; it is also one
# you can serve off a laptop:
#
#   make pages BASE=http://192.168.1.50:8000
#   (cd dist && python3 -m http.server 8000)
#   PLATFORMIO_BUILD_FLAGS='-DUPDATE_MANIFEST_URL=\"http://192.168.1.50:8000/manifest.json\"' \
#     pio run -e waveshare_s3_154 -t upload
#
# BASE is whatever the DEVICE can reach — a laptop's LAN address, not localhost.
# `pio run` has no -D of its own; PLATFORMIO_BUILD_FLAGS is what appends to the
# environment's build_flags, and the escaped quotes make the macro a string literal.
#
# BASE does not reach the pages: both resolve every path against their own origin,
# so they work under any host without being told which one (pages/README.md).

.PHONY: firmware-artifact manifest manifest-check boot-images site pages

DIST ?= dist
FW_ENV ?= waveshare_s3_154
# V=1 streams the compiler's output instead of swallowing it. Publishing is a
# build, and a build you can't see is one you can only wait on — tools/
# serve_updates.sh turns this on by default for exactly that reason.
V ?= 0
PIO_OUT = $(if $(filter-out 0,$(V)),,>/dev/null)

# Build the firmware image and stage it under its version, so two builds of
# different versions can sit in dist/ at once and a device can be moved between
# them. Version comes from include/version.h — the same numbers the running build
# reports, so a staged image can never claim a version the firmware won't.
firmware-artifact:
	@pio run -e $(FW_ENV) $(PIO_OUT)
	@mkdir -p "$(DIST)"
	@maj=$$(sed -n 's/^#  define FW_VERSION_MAJOR *//p' include/version.h); \
	min=$$(sed -n 's/^#  define FW_VERSION_MINOR *//p' include/version.h); \
	pat=$$(sed -n 's/^#  define FW_VERSION_PATCH *//p' include/version.h); \
	cp ".pio/build/$(FW_ENV)/firmware.bin" "$(DIST)/mal-$$maj.$$min.$$pat.bin"; \
	echo "built $(DIST)/mal-$$maj.$$min.$$pat.bin  ($$(wc -c < "$(DIST)/mal-$$maj.$$min.$$pat.bin" | tr -d ' ') bytes)"

# Build both artifacts and the manifest that lists them, then validate the result
# with the DEVICE's parser — a manifest the device would reject looks exactly like
# a dead network from the operator's side, so the publish fails here instead.
#
# The firmware `code` below repacks MAJOR/MINOR/PATCH the way include/version.h
# does (MINOR two digits, PATCH three). The two must agree: the published code is
# what a device compares its OWN compiled code against, so a mismatch either
# offers an update the device already has or hides one it doesn't.
manifest: firmware-artifact
	@$(MAKE) --no-print-directory pedia-tar OUT="$(DIST)/web-$$(sed -n 's/^version=//p' web/VERSION).tar" >/dev/null
	@base="$(BASE)"; [ -n "$$base" ] || { echo "error: BASE is required, e.g."; echo "  make manifest BASE=http://192.168.1.50:8000"; exit 1; }; \
	maj=$$(sed -n 's/^#  define FW_VERSION_MAJOR *//p' include/version.h); \
	min=$$(sed -n 's/^#  define FW_VERSION_MINOR *//p' include/version.h); \
	pat=$$(sed -n 's/^#  define FW_VERSION_PATCH *//p' include/version.h); \
	fw="mal-$$maj.$$min.$$pat.bin"; \
	web="web-$$(sed -n 's/^version=//p' web/VERSION).tar"; \
	{ \
	  printf '{"artifacts":[\n'; \
	  printf '  {"id":"firmware","version":"%s.%s.%s","code":%s,\n' "$$maj" "$$min" "$$pat" \
	    "$$((maj * 100000 + min * 1000 + pat))"; \
	  printf '   "url":"%s/%s","size":%s,\n' "$$base" "$$fw" "$$(wc -c < "$(DIST)/$$fw" | tr -d ' ')"; \
	  printf '   "sha256":"%s"},\n' "$$(shasum -a 256 "$(DIST)/$$fw" | cut -d' ' -f1)"; \
	  printf '  {"id":"web","version":"%s","code":%s,\n' \
	    "$$(sed -n 's/^version=//p' web/VERSION)" "$$(sed -n 's/^code=//p' web/VERSION)"; \
	  printf '   "url":"%s/%s","size":%s,\n' "$$base" "$$web" "$$(wc -c < "$(DIST)/$$web" | tr -d ' ')"; \
	  printf '   "sha256":"%s"}\n' "$$(shasum -a 256 "$(DIST)/$$web" | cut -d' ' -f1)"; \
	  printf ']}\n'; \
	} > "$(DIST)/manifest.json"
	@$(MAKE) --no-print-directory manifest-check

# Re-validate whatever is in dist/ without rebuilding it. Also the thing to run
# against a manifest fetched back off a real host, to prove what is SERVED parses
# — not just what was generated.
manifest-check: build/check_manifest
	@./build/check_manifest "$(DIST)/manifest.json" "$(DIST)"

# The validator is the DEVICE's own manifest parser wrapped in a main(), and that
# parser is self-contained — so it compiles from its two files here rather than
# linking malcore, and a publish works anywhere a C++17 compiler does (same
# reasoning as build/dump_content above).
build/check_manifest: tools/check_manifest.cpp src/core/net/update_manifest.cpp \
                      src/core/net/update_manifest.h
	@mkdir -p build
	$(CXX) -std=c++17 -O1 -I src -I include -o $@ \
	    tools/check_manifest.cpp src/core/net/update_manifest.cpp

# The three boot images the USB flasher writes in front of the app, staged under
# fixed names because — unlike the app and the bundle — they do not carry a
# version: the flasher reads the app's filename out of the manifest and knows
# these three by convention (pages/flash/flash.js names the addresses).
#
# They are NOT manifest rows. The manifest is a list of things a device downloads
# for ITSELF, and an over-the-air update can install none of these three: it
# writes into an app slot the partition table already defines, which is exactly
# why a bootloader or table change needs the cable in the first place.
#
# bootloader.bin and partitions.bin are build output; boot_app0.bin ships with the
# Arduino framework, so it is copied out of the PlatformIO package tree. Both
# lookups fail loudly — a publish missing one of these serves a flasher that
# 404s halfway through, which is worse than a publish that didn't happen.
PIO_CORE  ?= $(HOME)/.platformio
BOOT_APP0 ?= $(PIO_CORE)/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin

boot-images: firmware-artifact
	@mkdir -p "$(DIST)"
	@for f in bootloader.bin partitions.bin; do \
	    src=".pio/build/$(FW_ENV)/$$f"; \
	    [ -f "$$src" ] || { echo "error: the firmware build produced no $$src"; exit 1; }; \
	    cp "$$src" "$(DIST)/$$f"; \
	done
	@[ -f "$(BOOT_APP0)" ] || { \
	    echo "error: boot_app0.bin not found at $(BOOT_APP0)"; \
	    echo "  it ships with framework-arduinoespressif32 — set PIO_CORE if your"; \
	    echo "  PlatformIO core directory is not $(PIO_CORE)"; exit 1; }
	@cp "$(BOOT_APP0)" "$(DIST)/boot_app0.bin"
	@echo "staged boot images -> $(DIST)/ (bootloader.bin, partitions.bin, boot_app0.bin)"

# The served pages themselves. pages/README.md documents the directory for someone
# reading the repo and is not part of the site, so it is the one file dropped.
site:
	@mkdir -p "$(DIST)"
	@cp -R pages/. "$(DIST)/"
	@rm -f "$(DIST)/README.md"
	@find "$(DIST)" \( -name '._*' -o -name '.DS_Store' \) -delete 2>/dev/null || true
	@echo "staged pages/ -> $(DIST)/ (index.html, flash/, vendor/)"

# The whole publish, in the order the pieces depend on each other: the artifacts
# and their manifest first (which is also what validates them), then the boot
# images beside them, then the pages that read both. This is what CI runs.
pages: manifest boot-images site
	@echo "publish ready in $(DIST)/ — serve it, or upload it as a Pages artifact"
