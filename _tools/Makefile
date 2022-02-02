all: mkdectape dtr2dta dta2dtr

mkdectape: dectape.c pdp6common.c
	cc -o $@ $^

dtr2dta: dtr2dta.c
	cc -o $@ $^
dta2dtr: dtr2dta.c
	cc -o $@ $^
