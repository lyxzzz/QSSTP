CC=g++
RM=rm -f

INCLUDE_PATH=../utils/include
LIB_FILES=../utils/libutils.a
LD_FLAGS=-L../utils -lutils
APP_FLAGS=-g -std=c++11 -I$(INCLUDE_PATH)