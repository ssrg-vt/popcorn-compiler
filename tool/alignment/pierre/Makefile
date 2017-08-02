POPCORN_INSTALL=/usr/local/popcorn
BINARY=align-python
TMP_ZIPFILE=align.zip
BOOTSTRAP=align.script
TEMPLATES=templates/*
PYTHON_MAIN="__main__.py"

all: test

test:
	@python $(PYTHON_MAIN) \
		--x86-bin test_dir/x86exe_musl \
		--arm-bin test_dir/armexe_musl \
		--x86-map test_dir/map_x86.txt \
		--arm-map test_dir/map_aarch.txt \
		--x86-object-files test_dir/pierre_x86_64.o test_dir/test_x86_64.o \
		--arm-object-files test_dir/pierre_arm.o test_dir/test_arm.o

tags: *.py
	ctags -R

# TODO remove this later
PIERRE_TEST_DIR=/root/popcorn-compiler/pierre-test/
TOOL_TEST_DIR=/root/popcorn-compiler/tool/alignment/pierre/test_dir/
copy-test-file:
	cp -f $(PIERRE_TEST_DIR)/align/armexe_musl $(TOOL_TEST_DIR)/armexe_musl
	cp -f $(PIERRE_TEST_DIR)/align/x86exe_musl $(TOOL_TEST_DIR)/x86exe_musl
	cp -f $(PIERRE_TEST_DIR)/align/map_aarch.txt $(TOOL_TEST_DIR)/map_arm.txt
	cp -f $(PIERRE_TEST_DIR)/align/map_x86.txt $(TOOL_TEST_DIR)/map_x86.txt
	cp -f $(PIERRE_TEST_DIR)/pierre.o $(TOOL_TEST_DIR)/pierre_arm.o
	cp -f $(PIERRE_TEST_DIR)/test.o $(TOOL_TEST_DIR)/test_arm.o
	cp -f $(PIERRE_TEST_DIR)/pierre_x86_64.o $(TOOL_TEST_DIR)/pierre_x86.o
	cp -f $(PIERRE_TEST_DIR)/test_x86_64.o $(TOOL_TEST_DIR)/test_x86.o



bin:
	zip $(TMP_ZIPFILE) *.py
	cat $(BOOTSTRAP) $(TMP_ZIPFILE) > $(BINARY)
	chmod +x $(BINARY)

install: bin
	cp $(BINARY) $(POPCORN_INSTALL)/bin
	mkdir $(POPCORN_INSTALL)/share/align-script-templates &> /dev/null
	cp -r $(TEMPLATES) $(POPCORN_INSTALL)/share/align-script-templates

check:
	make -C utils

clean:
	rm -rf *.pyc tags $(TMP_ZIPFILE) $(BINARY)

debug:
	python -m pdb $(PYTHON_MAIN) \
		--x86-bin test_dir/x86exe_musl \
		--arm-bin test_dir/armexe_musl \
		--x86-map test_dir/map_x86.txt \
		--arm-map test_dir/map_aarch.txt \
		--x86-object-files test_dir/pierre_x86_64.o test_dir/test_x86_64.o \
		--arm-object-files test_dir/pierre_arm.o test_dir/test_arm.o
 
