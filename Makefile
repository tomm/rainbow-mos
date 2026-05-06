# ----------------------------
# Makefile Options
# ----------------------------

VERSION_GITREF=$(shell git log -1 --date=format:"%Y%m%d" --format="%ad")-$(shell git rev-parse --short HEAD)
NAME=mos
DEBUG=1

include makefile.inc

format:
	clang-format-16 -i src/*.c src/*.h --style=file:./clang-format.conf
