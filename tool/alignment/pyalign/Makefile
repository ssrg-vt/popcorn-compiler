POPCORN     ?= $(HOME)/hermit-popcorn/
BINARY      := pyalign
TMP_ZIPFILE := align.zip
BOOTSTRAP   := align.script
TEMPLATES   := templates/*
PYTHON_MAIN := "__main__.py"

POPCORN_HOST_PREFIX=$(POPCORN)/x86_64-host/

all: bin

.PHONY: tags
tags:
	ctags -R

bin:
	zip $(TMP_ZIPFILE) *.py
	cat $(BOOTSTRAP) $(TMP_ZIPFILE) > $(BINARY)
	chmod +x $(BINARY)

install: bin
	mkdir -p $(POPCORN_HOST_PREFIX)/bin
	cp $(BINARY) $(POPCORN_HOST_PREFIX)/bin
	mkdir -p $(POPCORN_HOST_PREFIX)/share/align-script-templates &> /dev/null
	cp -rf $(TEMPLATES) $(POPCORN_HOST_PREFIX)/share/align-script-templates

clean:
	rm -rf *.pyc tags $(TMP_ZIPFILE) $(BINARY)

