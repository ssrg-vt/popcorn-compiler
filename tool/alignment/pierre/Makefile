BINARY=align
TMP_ZIPFILE=align.zip
BOOTSTRAP=align.script

all: test

test:
	@python Main.py --x86-bin test_dir/x86exe_musl \
		--arm-bin test_dir/armexe_musl \
		--x86-map test_dir/map_x86.txt \
		--arm-map test_dir/map_aarch.txt

tags: *.py
	ctags -R

bin:
	zip $(TMP_ZIPFILE) *.py
	cat $(BOOTSTRAP) $(TMP_ZIPFILE) > $(BINARY)
	chmod +x $(BINARY)

install: bin
	

clean:
	rm *.pyc tags $(TMP_ZIPFILE) $(BINARY)
