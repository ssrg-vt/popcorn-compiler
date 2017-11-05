POPCORN     := /usr/local/popcorn
BINARY      := pyalign
TMP_ZIPFILE := align.zip
BOOTSTRAP   := align.script
TEMPLATES   := templates/*
PYTHON_MAIN := "__main__.py"

TEST_DIR=tests
CHECK_ALIGN=../../../util/scripts/check-align.py

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
	make -C $(TEST_DIR) clean
	make -C $(TEST_DIR) MODE=ppc
	$(CHECK_ALIGN) $(TEST_DIR)/test_x86-64 $(TEST_DIR)/test_powerpc64
	make -C $(TEST_DIR) clean
	make -C $(TEST_DIR) MODE=arm
	$(CHECK_ALIGN) $(TEST_DIR)/test_x86-64 $(TEST_DIR)/test_aarch64

clean:
	rm -rf *.pyc tags $(TMP_ZIPFILE) $(BINARY)

