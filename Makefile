O=-O0 -g -Werror -Wno-pointer-sign
all:
	@cc $O -oa tokeniser.c a.c

t:
	@tcc $O -oa a.c
