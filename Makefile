GAME_SRCS := $(shell find $(CURDIR)/src -name '*.c' -or -name '*.S')

build:
	make -C ../time4riscv \
		SOURCES="boot.S dtekv-lib.c $(GAME_SRCS)" \
		CFLAGS="-Wall -nostdlib -O3 -mabi=ilp32 -march=rv32imzicsr -fno-builtin -I. -I$(CURDIR)/src"

run: build
	cd ../dtekv-tools && ./dtekv-run ../time4riscv/main.bin