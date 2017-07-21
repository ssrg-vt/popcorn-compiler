POPCORN_INSTALL=/usr/local/popcorn
BINARY=align-python
TMP_ZIPFILE=align.zip
BOOTSTRAP=align.script
TEMPLATES=templates/
PYTHON_MAIN="__main__.py"

all: test

test:
	@python $(PYTHON_MAIN) --x86-bin test_dir/x86exe_musl \
		--arm-bin test_dir/armexe_musl \
		--x86-map test_dir/map_x86.txt \
		--arm-map test_dir/map_aarch.txt \
		--x86-object-files test_dir/pierre_x86_64.o test_dir/test_x86_64.o \
		--arm-object-files test_dir/pierre_arm.o test_dir/test_arm.o

tags: *.py
	ctags -R

bin:
	zip $(TMP_ZIPFILE) *.py
	cat $(BOOTSTRAP) $(TMP_ZIPFILE) > $(BINARY)
	chmod +x $(BINARY)

install: bin
	cp $(BINARY) $(POPCORN_INSTALL)/bin
	cp -r $(TEMPLATES) $(POPCORN_INSTALL)/share/align-script-templates

clean:
	rm -rf *.pyc tags $(TMP_ZIPFILE) $(BINARY)
