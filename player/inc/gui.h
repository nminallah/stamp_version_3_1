#ifndef GUI_H
#define GUI_H

#include <SDL.h>
#include <SDL_thread.h>
#include <SDL_mutex.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include "params.h"

typedef struct GUISettings
{
	pSDLParams_t	pgui_vdec_struct;
	int				maximize_window;
	int				volume;
	int *				auto_quality;
	int *				quality_level;
	
	int 				(*quitAppcbk)(void);
	int 				(*switchChannelCbk)(SDL_Surface *,char *, int);
	int 				(*changeVolumeCbk)(int);
	int 				(*channelServerCbk)(void);
	
	/* internal variables */
	int				threadStatus;

}GUISettings_t, *pGUISettings_t;

int GUIThread(void * arg);

#endif //GUI_H

