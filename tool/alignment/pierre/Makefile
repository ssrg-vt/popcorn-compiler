all: test

test:
	@python Main.py --x86-bin test_dir/x86exe_musl \
		--arm-bin test_dir/armexe_musl \
		--x86-map test_dir/map_x86.txt \
		--arm-map test_dir/map_aarch.txt

tags: *.py
	ctags -R

clean:
	rm *.pyc tags
