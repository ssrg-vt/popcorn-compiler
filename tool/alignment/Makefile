POPCORN     := /usr/local/popcorn
BINARY      := pyalign
TMP_ZIPFILE := align.zip
BOOTSTRAP   := align.script
TEMPLATES   := templates/*
PYTHON_MAIN := "__main__.py"

TEST_2_ARCHS=tests/2-archs
TEST_3_ARCHS=tests/3-archs

all: bin

.PHONY: tags
tags:
	ctags -R
	cscope -Rb

bin:
	rm -rf $(BINARY)
	zip $(TMP_ZIPFILE) *.py
	cat $(BOOTSTRAP) $(TMP_ZIPFILE) > $(BINARY)
	chmod +x $(BINARY)

install: bin
	mkdir -p $(POPCORN)/bin &> /dev/null
	cp $(BINARY) $(POPCORN)/bin
	mkdir -p $(POPCORN)/share/align-script-templates &> /dev/null
	cp -rf $(TEMPLATES) $(POPCORN)/share/align-script-templates

tests: bin
	@# 2 archs: x86-arm
	make -C $(TEST_2_ARCHS) clean
	make -C $(TEST_2_ARCHS) MODE=arm check ALIGN=$(PWD)/$(BINARY)
	@# 2 archs: x86-ppc - Enable when ppc is merged for the compiler
	@# make -C $(TEST_2_ARCHS) clean
	@# make -C $(TEST_2_ARCHS) MODE=ppc check ALIGN=$(PWD)/$(BINARY)
	@# 3 archs - Enable when ppc is merged for the compiler
	@# make -C $(TEST_3_ARCHS) clean
	@# make -C $(TEST_3_ARCHS) check ALIGN=$(PWD)/$(BINARY)

clean:
	rm -rf *.pyc tags cscope.out $(TMP_ZIPFILE) $(BINARY)

