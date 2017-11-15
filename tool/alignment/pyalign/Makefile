POPCORN     := /usr/local/popcorn
BINARY      := pyalign
TMP_ZIPFILE := align.zip
BOOTSTRAP   := align.script
TEMPLATES   := templates/*
LDSCRIPTS   := ld-scripts/*
PYTHON_MAIN := "__main__.py"

all: bin

.PHONY: tags
tags:
	ctags -R

bin:
	zip $(TMP_ZIPFILE) *.py
	cat $(BOOTSTRAP) $(TMP_ZIPFILE) > $(BINARY)
	chmod +x $(BINARY)

install: bin
	cp $(BINARY) $(POPCORN)/bin
	mkdir -p $(POPCORN)/share/align-script-templates &> /dev/null
	cp -rf $(TEMPLATES) $(POPCORN)/share/align-script-templates
	mkdir -p $(POPCORN)/share/ld-scripts-vanilla &> /dev/null
	cp -rf $(LDSCRIPTS) $(POPCORN)/share/ld-script-vanilla

check_dead_code:
	vulture .

clean:
	rm -rf *.pyc tags $(TMP_ZIPFILE) $(BINARY)

