###########################################

CFLAGS = -Wall -g

UNAME := $(shell uname)

ifeq ($(findstring MINGW64,$(UNAME)),MINGW64)
NAME = source.exe
else
NAME = source.bin
endif

SRCS = 	    src/root.o																															\
						src/demux.o																															\
						src/videoDecoder.o																													\
						src/videoEncoder.o																											\
						src/payload_headers.o																										\
						src/gchunkiser.o																												\
						src/fecenc.o																												\
						src/fileWrite.o																													\
						src/copyIO.o																													\
						src/audioDecoder.o																													\
						src/audioEncoder.o																													\
		src/webInterface.o									\
		src/yuv2bmp.o									\


LIBS =			-l:ffmpeg-install/lib/libavformat.a																			\
						-l:ffmpeg-install/lib/libavcodec.a																			\
						-l:ffmpeg-install/lib/libavutil.a																				\
						-l:ffmpeg-install/lib/libswscale.a																			\
						-l:ffmpeg-install/lib/libswresample.a																		\
						-l:crc32_fast/libcrc32_fast.a																						\
						-l:fec/libfec.a																						\
						-l:x264-install/lib/libx264.a																						\
						-l:lame-install/lib/libmp3lame.a																				\
		-l:webserver/libwebserver.a							\
						-l:GRAPES/libgrapes.a																										\
						-l:os/libos.a																														\

PLIB =

INCLUDES = 	-Isrc                                           	                       \
						-Iinc                                          	                         \
						-I../../lib/ffmpeg-install/include/						                           \
						-I../../lib/x264-install/include							                           \
						-I../../lib/lame-install/include/lame			                           		 \
						-I../../lib/GRAPES/include/					                           		 			 \
						-I../../lib/GRAPES/src/							                           		 			 \
						-I../../lib/crc32_fast/inc/						                           \
						-I../../lib/fec/inc/						                           \
					  -I../../lib/common/inc																									 \
						-I../../lib/os/inc														                           \
		-I../../lib/webserver/inc			                           \

						
###########################################
ifeq ($(findstring MINGW64,$(UNAME)),MINGW64)
	include ../../tools/makefileWin
else
	include ../../tools/makefileLinux
endif
