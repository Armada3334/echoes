#!/bin/sh
make distclean
qmake echoes.pro -r -spec linux-g++
make

