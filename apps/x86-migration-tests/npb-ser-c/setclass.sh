#!/bin/bash

if [ $# -ne 1 ]; then
	echo "Usage: $0 {S | A | B | C}"
	exit 1
fi

if [[ $1 != "S" && $1 != "A" && $1 != "B" && $1 != "C" ]]; then
	echo "Usage: $0 {S | A | B | C}"
	exit 1
fi

for W in bt cg dc ep ft is lu mg sp ua; do
	cd $W
	rm -f npbparams.h

	if [[ $W == "dc" && $1 == "C" ]]; then
		echo "Warning: DC has no C class, falling back on B (for DC only)"
		ln -s npbparams-B.h npbparams.h
	else
		ln -s npbparams-$1.h npbparams.h
	fi
	cd ..
done
