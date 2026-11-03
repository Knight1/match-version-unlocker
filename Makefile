THIS_MAKEFILE := $(abspath $(lastword $(MAKEFILE_LIST)))
CURRENT_DIRECTORY := $(abspath $(dir $(THIS_MAKEFILE)))

PROJECT_NAME := $(shell basename $(CURRENT_DIRECTORY))
# The program ID is defined once, in config.json ("title_id"). The contents folder must match the
# NPDM, or Atmosphere can't launch the module and ns waits for it forever.
TITLE_ID := $(shell sed -n 's/^[[:space:]]*"title_id":[[:space:]]*"0x\([0-9A-Fa-f]\{16\}\)".*/\1/p' $(CURRENT_DIRECTORY)/config.json)
ifeq ($(strip $(TITLE_ID)),)
$(error could not read "title_id" from config.json)
endif
VERSION := $(shell git describe --tags --always 2>/dev/null || echo unknown)
LIBS_PATCH := $(CURRENT_DIRECTORY)/patches/libstratosphere-allow-unknown-firmware.patch

.PHONY: all dist release module patch_libs clean

all: dist

# Applies the patch once; does nothing if it is already applied.
patch_libs:
	@git -C Atmosphere-libs apply --reverse --check $(LIBS_PATCH) 2>/dev/null || git -C Atmosphere-libs apply $(LIBS_PATCH)

module: patch_libs
	$(MAKE) -f system_module.mk

dist: module
	rm -rf dist
	mkdir -p dist/atmosphere/contents/$(TITLE_ID)/flags
	cp out/nintendo_nx_arm64_armv8a/release/$(PROJECT_NAME).nsp dist/atmosphere/contents/$(TITLE_ID)/exefs.nsp
	cp mitm.lst dist/atmosphere/contents/$(TITLE_ID)/mitm.lst
	touch dist/atmosphere/contents/$(TITLE_ID)/flags/boot2.flag

release: dist
	rm -f $(PROJECT_NAME)-*.zip
	cd dist && zip -r ../$(PROJECT_NAME)-$(VERSION).zip atmosphere

clean:
	$(MAKE) -f system_module.mk clean
	$(MAKE) -C Atmosphere-libs/libstratosphere clean
	rm -rf dist build out $(PROJECT_NAME)-*.zip
