###########################################

CFLAGS = -Wall -g

UNAME := $(shell uname)

ifeq ($(findstring MINGW64,$(UNAME)),MINGW64)
NAME = player.exe
else
NAME = player.bin
endif

SRCS = 	    src/root.o																															\
						src/videoDecoderPlayer.o																								\
						src/audioDecoderPlayer.o																								\
						src/payload_headers.o																										\
						src/gdechunkiser.o																											\
						src/fecdec.o																												\
						src/fileWrite.o																													\
						src/gui.o																																\
						src/MuxPlayer.o																																\
						


LIBS =			-l:ffmpeg-install/lib/libavformat.a																			\
						-l:ffmpeg-install/lib/libavcodec.a																			\
						-l:ffmpeg-install/lib/libavutil.a																				\
						-l:ffmpeg-install/lib/libswscale.a																			\
						-l:ffmpeg-install/lib/libswresample.a																		\
						-l:crc32_fast/libcrc32_fast.a																						\
						-l:fec/libfec.a																						\
						-l:x264-install/lib/libx264.a																						\
						-l:lame-install/lib/libmp3lame.a																				\
						-l:GRAPES/libgrapes.a																										\
						-l:os/libos.a																														\
						-l:sdl-install/lib/libSDL.a																							\
						-l:sdl-install/lib/libSDL_image.a																				\
						-l:sdl-install/lib/libSDL_ttf.a																					\
						-l:freetype-install/lib/libfreetype.a																		\

ifeq ($(findstring MINGW64,$(UNAME)),MINGW64)
PLIB =		-lwinmm -lgdi32 -ldxguid -lpng
else
PLIB =			-lX11
endif

INCLUDES = 	-Isrc                                           	                       \
						-Iinc                                          	                         \
						-I../../lib/ffmpeg/														                           \
						-I../../lib/ffmpeg-install/include/						                           \
						-I../../lib/x264-install/include							                           \
						-I../../lib/lame-install/include/lame			                           		 \
						-I../../lib/GRAPES/include/					                           		 			 \
						-I../../lib/GRAPES/src/							                           		 			 \
						-I../../lib/crc32_fast/inc/						                           \
						-I../../lib/fec/inc/						                           \
					  -I../../lib/common/inc																									 \
						-I../../lib/os/inc														                           \
						-I../../lib/sdl-install/include/SDL			      	             						 \
						-I../../lib/freetype-install/include/freetype2 	             						 \

						
###########################################
ifeq ($(findstring MINGW64,$(UNAME)),MINGW64)
	include ../../tools/makefileWin
else
	include ../../tools/makefileLinux
endif
