###########################################

CFLAGS = -Wall -g

UNAME := $(shell uname)

ifeq ($(findstring MINGW64,$(UNAME)),MINGW64)
NAME = p2p_streamer.exe
else
NAME = p2p_streamer.bin
endif

SRCS = 	    src/root.o																															\
						src/payload_headers.o																										\
						src/recvfromsrc.o																												\
						src/peer_sampling.o																											\
						src/peer_messages.o																											\
						src/chunk_trading.o																											\
						src/chunk_sending.o																											\
						src/chunk_schedular.o																										\
						src/topology.o																													\

ifeq ($(findstring MINGW64,$(UNAME)),MINGW64)
SRCS +=		src/net_helper-udp-win32.o
else
SRCS +=		src/net_helper-udp.o
endif


LIBS =					-l:GRAPES/libgrapes.a																										\
						-l:os/libos.a																														\


INCLUDES = 	-Isrc                                           	                       \
						-Iinc                                          	                         \
						-I../../lib/ffmpeg-install/include/						                           \
						-I../../lib/x264-install/include							                           \
						-I../../lib/lame-install/include/lame			                           		 \
						-I../../lib/GRAPES/include/					                           		 			 \
						-I../../lib/GRAPES/src/							                           		 			 \
					  -I../../lib/common/inc																									 \
						-I../../lib/os/inc														                           \

						
###########################################
ifeq ($(findstring MINGW64,$(UNAME)),MINGW64)
	include ../../tools/makefileWin
else
	include ../../tools/makefileLinux
endif
