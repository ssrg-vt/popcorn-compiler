POPCORN     := /usr/local/popcorn
BINARY      := pyalign
TMP_ZIPFILE := align.zip
BOOTSTRAP   := align.script
TEMPLATES   := templates/*
PYTHON_MAIN := "__main__.py"

TEST_2_NODES=tests/2-nodes
TEST_3_NODES=tests/3-nodes

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

check_dead_code:
	vulture .

tests: bin
	# 2 nodes: x86-ppc
	make -C $(TEST_2_NODES) clean
	make -C $(TEST_2_NODES) MODE=ppc check
	# 2 nodes: x86-arm
	make -C $(TEST_2_NODES) clean
	make -C $(TEST_2_NODES) MODE=arm check
	# TODO 3 nodes test

clean:
	rm -rf *.pyc tags $(TMP_ZIPFILE) $(BINARY)

