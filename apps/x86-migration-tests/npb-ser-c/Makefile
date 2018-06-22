all:
	for W in bt cg dc ep ft is lu mg sp ua; do \
		make -C $$W; \
	done

.PHONY: clean
clean:
	for W in bt cg dc ep ft is lu mg sp ua; do \
		make -C $$W clean; \
		rm -f $$W/npbparams.h; \
	done

.PHONY: S A B C
S: clean
	./setclass.sh S
	make all

A: clean
	./setclass.sh A
	make all

B: clean
	./setclass.sh B
	make all

C: clean
	./setclass.sh C
	make all

.PHONY: help
help:
	@echo "make {all | S | A | B | C | clean}"
