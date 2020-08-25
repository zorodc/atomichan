#!/bin/sh

cc *.c ./tests/demo.c -lpthread -lm -o demo.out
cc *.c ./tests/test.c -lpthread     -o test.out
