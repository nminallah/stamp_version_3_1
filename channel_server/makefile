###########################################

CFLAGS = -Wall -g

UNAME := $(shell uname)

ifeq ($(findstring MINGW64,$(UNAME)),MINGW64)
NAME = channel_server.exe
else
NAME = channel_server.bin
endif

SRCS = 	    src/server.o																															\
					

LIBS =			

PLIB =			-l:os/libos.a																														\


INCLUDES = 	-Isrc                                           	                       \
						-Iinc                                          	                         \
					  -I../../lib/common/inc																									 \
						-I../../lib/os/inc														                           \

						
###########################################
ifeq ($(findstring MINGW64,$(UNAME)),MINGW64)
	include ../../tools/makefileWin
else
	include ../../tools/makefileLinux
endif
