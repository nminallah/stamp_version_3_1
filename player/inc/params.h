#ifndef PARAMS_H
#define PARAMS_H

typedef struct SDLParams
{
	SDL_Surface *	screenHandle;
	SDL_mutex * 		mutex;
	int 				sdl_flags;
	int 				display_width;
	int 				display_height;
	
}SDLParams_t, *pSDLParams_t;

#endif
