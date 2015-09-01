#!/bin/sh

if [ $1 -eq 1 ]; then
	if [ $2 -eq 1 ]; then
		cp ../flag.dat ../../foodology/chinesefood/
	fi
	cp ../input.dat ../../foodology/chinesefood/
	rm ../../foodology/chinesefood/input.dat
fi

if [ $1 -eq 2 ]; then
	if [ $2 -eq 1 ]; then
		cp ../flag.dat ../../foodology/chinesefood1/
	fi
	cp ../input.dat ../../foodology/chinesefood1/
	rm ../../foodology/chinesefood1/input.dat
fi

if [ $1 -eq 3 ]; then
	if [ $2 -eq 1 ]; then
		cp ../flag.dat .
	fi
	cp ../input.dat .
	rm input.dat
fi

if [ $1 -eq 0 ]; then
	rm ../../foodology/chinesefood/flag.dat
	rm ../../foodology/chinesefood1/flag.dat
	rm flag.dat
fi
