#include <stdio.h>
#include <stdint.h>
#include <root.h>
#include <X11/Xlib.h>

#define SDL_DEF_WIDTH 			640
#define SDL_DEF_HEIGHT 			480 //420
#define SDL_BPP 					4
#define SDL_DEPTH 				24

#define GUI_PANEL_HEIGHT		60

#define NUM_BUTTONS 				25
#define PREV_BUTTON 				0
#define NEXT_BUTTON 				1
#define DEFSCREEN_BUTTON 			2
#define FULLSCREEN_BUTTON 			3
#define LEVEL0_BUTTON 				4
#define LEVEL1_BUTTON 				5
#define LEVEL2_BUTTON 				6
#define LEVEL3_BUTTON 				7
#define LEVEL4_BUTTON 				8
#define LEVEL5_BUTTON 				9
#define LEVEL6_BUTTON 				10
#define LEVEL7_BUTTON 				11
#define LEVEL8_BUTTON 				12
#define MINUS_BUTTON 				13
#define PLUS_BUTTON 				14
#define AUTO_UNSELECT_BUTTON 		15
#define AUTO_SELECT_BUTTON 		16
#define LOW_QUALITY_BUTTON 		17
#define MED_QUALITY_BUTTON 		18
#define GOOD_QUALITY_BUTTON 		19
#define HIGH_QUALITY_BUTTON 		20
#define LOW_QUALITY_SEL_BUTTON 	21
#define MED_QUALITY_SEL_BUTTON 	22
#define GOOD_QUALITY_SEL_BUTTON 	23
#define HIGH_QUALITY_SEL_BUTTON 	24

#define NUM_FONTS 				5
#define MAIN_FONT 				0
#define COMIC_SANS_FONT 		1
#define COMIC_SANS_FONT_2 		2
#define COMIC_SANS_FONT_3 		3
#define COMIC_SANS_FONT_4 		4

#define BUTTON_NORM_STATE   	0
#define BUTTON_HOVER_STATE  	1
#define BUTTON_PRESS_STATE  	2

#define FORM_FIELD_NORM_STATE   	0
#define FORM_FIELD_HOVER_STATE  	1
#define FORM_FIELD_EDIT_STATE  	2


typedef struct buttonStruct
{
	int 			show;
	int			btnstatus;
	SDL_Surface* btn;
	SDL_Surface* btnHover;
	SDL_Surface* btnPress;
	SDL_Rect 	coordinates;
	char			tooltip[1024];
}buttonStruct_t, *pbuttonStruct_t;

int					guiSettingsWindow = 0;

static SDL_mutex *		DisplayMutex;
static int 				max_screen_width, max_screen_height;
static int 				screen_width, screen_height;

static buttonStruct_t 	Buttons[NUM_BUTTONS];
static TTF_Font *		MainFont[NUM_FONTS];
static SDL_Event 		saved_event;

int loadButtons(pbuttonStruct_t btnStruct, char * img, char * imgHover, char * imgPress, char * tooltip)
{
	SDL_Surface *	temp;
	
	if(img != NULL)
	{
		temp = IMG_Load(img);
		if (temp == NULL) {
			fprintf(stderr, "loadButtons: Error loading image %s: %s\n", img, SDL_GetError());
			exit(-1);
		}
		btnStruct->btn = SDL_DisplayFormatAlpha(temp);
		SDL_FreeSurface(temp);
	}
	else
	{
		btnStruct->btn = NULL;
	}

	if(imgHover != NULL)
	{
		temp = IMG_Load(imgHover);
		if (temp == NULL) {
			fprintf(stderr, "loadButtons: Error loading image %s: %s\n", img, SDL_GetError());
			exit(-1);
		}
		btnStruct->btnHover = SDL_DisplayFormatAlpha(temp);
		SDL_FreeSurface(temp);
	}
	else
	{
		btnStruct->btnHover = NULL;
	}

	if(imgPress != NULL)
	{
		temp = IMG_Load(imgPress);
		if (temp == NULL) {
			fprintf(stderr, "loadButtons: Error loading image %s: %s\n", img, SDL_GetError());
			exit(-1);
		}
		btnStruct->btnPress= SDL_DisplayFormatAlpha(temp);
		SDL_FreeSurface(temp);
	}
	else
	{
		btnStruct->btnPress = NULL;
	}

	btnStruct->show 			= 1;	
	btnStruct->coordinates.x 	= 0;	
	btnStruct->coordinates.y 	= 0;	
	if(btnStruct->btn != NULL)
	{
		btnStruct->coordinates.w 	= btnStruct->btn->w;
		btnStruct->coordinates.h 	= btnStruct->btn->h;
	}
	else
	{
		btnStruct->coordinates.w 	= 20;
		btnStruct->coordinates.h 	= 34;
	}
	
	if(tooltip != NULL)
		sprintf(btnStruct->tooltip, "%s", tooltip);
	else
		sprintf(btnStruct->tooltip, "");
	
	btnStruct->btnstatus		= BUTTON_NORM_STATE;

	return 0;
}

int updateButtonCoordinates(pbuttonStruct_t btnStruct, int x, int y)
{
	btnStruct->coordinates.x 	= x;
	btnStruct->coordinates.y 	= y;

	return 0;
}


int updateButtonVisibility(pbuttonStruct_t btnStruct, int show)
{
	btnStruct->show 			= show;

	return 0;
}

int verifyButtonCoordinates(pbuttonStruct_t btnStruct, int x, int y)
{
	int ret = ( x > btnStruct->coordinates.x ) && ( x < btnStruct->coordinates.x + btnStruct->coordinates.w )
			&& ( y > btnStruct->coordinates.y ) && ( y < btnStruct->coordinates.y + btnStruct->coordinates.h );
	
	return ret;
}

int drawButton(SDL_Surface *screen, pbuttonStruct_t btnStruct, int state, int mutex)
{
	SDL_Surface * btnTemp;

	btnStruct->btnstatus = state;
	
	if(btnStruct->show == 0)
		return 0;

	if(state == BUTTON_NORM_STATE)
		btnTemp = btnStruct->btn;
	else if(state == BUTTON_HOVER_STATE)
		btnTemp = btnStruct->btnHover;
	else if(state == BUTTON_PRESS_STATE)
		btnTemp = btnStruct->btnPress;

	if(btnTemp == NULL)
		return 0;
	
	if(mutex)
	SDL_LockMutex(DisplayMutex);
		
	SDL_BlitSurface(btnTemp, NULL, screen, &btnStruct->coordinates);
	SDL_UpdateRects(screen, 1, &btnStruct->coordinates);

	if(mutex)
	SDL_UnlockMutex(DisplayMutex);

	return 0;
}

int drawFont(SDL_Surface *screen, TTF_Font * font, char * text, int x, int y, int tr, int tg, int tb, int br, int bg, int bb, int mutex)
{
	SDL_Surface *	textSurface;
	SDL_Color 		ChannelTitleColor = { tr, tg, tb };
	SDL_Color 		ChannelTitleBgColor = { br, bg, bb};
	SDL_Rect 		ChannelTitleRect;

	textSurface = TTF_RenderText_Shaded( font, text, ChannelTitleColor, ChannelTitleBgColor );
	if(textSurface == NULL)
	{
		printf("WARNING: CANNOT RENDER CHANNEL TITLE\n");
	}

	if(mutex)
	SDL_LockMutex(DisplayMutex);

	ChannelTitleRect.x = x;
	ChannelTitleRect.y = y;
	//SDL_FillRect( screen, &ChannelTitleRect, SDL_MapRGB(screen->format, br, bg, bb) );
	//SDL_UpdateRects(screen, 1, &ChannelTitleRect);

	SDL_BlitSurface(textSurface, NULL, screen, &ChannelTitleRect);
	SDL_UpdateRects(screen, 1, &ChannelTitleRect);

	if(mutex)
	SDL_UnlockMutex(DisplayMutex);

	SDL_FreeSurface(textSurface);

	return 0;
}

int fillRectangleColor(SDL_Surface *screen, int x, int y, int width, int height, int r, int g, int b, int mutex)
{
	SDL_Rect 	screenRect;
	Uint32 		clearColor;	

	if(mutex)
	SDL_LockMutex(DisplayMutex);	

	screenRect.x 	= x;
	screenRect.y 	= y;
	screenRect.w 	= width;
	screenRect.h 	= height;
	
	clearColor 	= SDL_MapRGB(screen->format, r, g, b);

	SDL_FillRect(screen, &screenRect, clearColor);
	SDL_UpdateRects(screen, 1, &screenRect);

	if(mutex)
	SDL_UnlockMutex(DisplayMutex);

	return 0;
}

int updateVolumeLevel(SDL_Surface *screen, int volume_level, int mutex)
{
	if(volume_level == 0)
	{
		updateButtonVisibility(&Buttons[LEVEL0_BUTTON], 1);
		updateButtonVisibility(&Buttons[LEVEL1_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL2_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL3_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL4_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL5_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL6_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL7_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL8_BUTTON], 0);
	}
	else if(volume_level == 1)
	{
		updateButtonVisibility(&Buttons[LEVEL0_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL1_BUTTON], 1);
		updateButtonVisibility(&Buttons[LEVEL2_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL3_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL4_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL5_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL6_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL7_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL8_BUTTON], 0);
	}
	else if(volume_level == 2)
	{
		updateButtonVisibility(&Buttons[LEVEL0_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL1_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL2_BUTTON], 1);
		updateButtonVisibility(&Buttons[LEVEL3_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL4_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL5_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL6_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL7_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL8_BUTTON], 0);
	}
	else if(volume_level == 3)
	{
		updateButtonVisibility(&Buttons[LEVEL0_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL1_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL2_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL3_BUTTON], 1);
		updateButtonVisibility(&Buttons[LEVEL4_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL5_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL6_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL7_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL8_BUTTON], 0);
	}
	else if(volume_level == 4)
	{
		updateButtonVisibility(&Buttons[LEVEL0_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL1_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL2_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL3_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL4_BUTTON], 1);
		updateButtonVisibility(&Buttons[LEVEL5_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL6_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL7_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL8_BUTTON], 0);
	}
	else if(volume_level == 5)
	{
		updateButtonVisibility(&Buttons[LEVEL0_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL1_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL2_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL3_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL4_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL5_BUTTON], 1);
		updateButtonVisibility(&Buttons[LEVEL6_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL7_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL8_BUTTON], 0);
	}
	else if(volume_level == 6)
	{
		updateButtonVisibility(&Buttons[LEVEL0_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL1_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL2_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL3_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL4_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL5_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL6_BUTTON], 1);
		updateButtonVisibility(&Buttons[LEVEL7_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL8_BUTTON], 0);
	}
	else if(volume_level == 7)
	{
		updateButtonVisibility(&Buttons[LEVEL0_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL1_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL2_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL3_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL4_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL5_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL6_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL7_BUTTON], 1);
		updateButtonVisibility(&Buttons[LEVEL8_BUTTON], 0);
	}
	else
	{
		updateButtonVisibility(&Buttons[LEVEL0_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL1_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL2_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL3_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL4_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL5_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL6_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL7_BUTTON], 0);
		updateButtonVisibility(&Buttons[LEVEL8_BUTTON], 1);
	}

	drawButton(screen, &Buttons[MINUS_BUTTON], BUTTON_NORM_STATE, mutex);
	drawButton(screen, &Buttons[LEVEL0_BUTTON], BUTTON_NORM_STATE, mutex);
	drawButton(screen, &Buttons[LEVEL1_BUTTON], BUTTON_NORM_STATE, mutex);
	drawButton(screen, &Buttons[LEVEL2_BUTTON], BUTTON_NORM_STATE, mutex);
	drawButton(screen, &Buttons[LEVEL3_BUTTON], BUTTON_NORM_STATE, mutex);
	drawButton(screen, &Buttons[LEVEL4_BUTTON], BUTTON_NORM_STATE, mutex);
	drawButton(screen, &Buttons[LEVEL5_BUTTON], BUTTON_NORM_STATE, mutex);
	drawButton(screen, &Buttons[LEVEL6_BUTTON], BUTTON_NORM_STATE, mutex);
	drawButton(screen, &Buttons[LEVEL7_BUTTON], BUTTON_NORM_STATE, mutex);
	drawButton(screen, &Buttons[LEVEL8_BUTTON], BUTTON_NORM_STATE, mutex);
	drawButton(screen, &Buttons[PLUS_BUTTON], BUTTON_NORM_STATE, mutex);

	return 0;
}

int updateQualityLevel(SDL_Surface *screen, int quality_level, int auto_mode, int mutex)
{
	if(auto_mode == 0)
	{
		updateButtonVisibility(&Buttons[AUTO_UNSELECT_BUTTON], 1);
		updateButtonVisibility(&Buttons[AUTO_SELECT_BUTTON], 0);

		updateButtonVisibility(&Buttons[HIGH_QUALITY_SEL_BUTTON], 0);
		updateButtonVisibility(&Buttons[GOOD_QUALITY_SEL_BUTTON], 0);
		updateButtonVisibility(&Buttons[MED_QUALITY_SEL_BUTTON], 0);
		updateButtonVisibility(&Buttons[LOW_QUALITY_SEL_BUTTON], 0);

		if(quality_level == 0)
		{
			updateButtonVisibility(&Buttons[HIGH_QUALITY_BUTTON], 1);
			updateButtonVisibility(&Buttons[GOOD_QUALITY_BUTTON], 0);
			updateButtonVisibility(&Buttons[MED_QUALITY_BUTTON], 0);
			updateButtonVisibility(&Buttons[LOW_QUALITY_BUTTON], 0);
		}
		else if(quality_level == 1)
		{
			updateButtonVisibility(&Buttons[HIGH_QUALITY_BUTTON], 0);
			updateButtonVisibility(&Buttons[GOOD_QUALITY_BUTTON], 1);
			updateButtonVisibility(&Buttons[MED_QUALITY_BUTTON], 0);
			updateButtonVisibility(&Buttons[LOW_QUALITY_BUTTON], 0);
		}
		else if(quality_level == 2)
		{
			updateButtonVisibility(&Buttons[HIGH_QUALITY_BUTTON], 0);
			updateButtonVisibility(&Buttons[GOOD_QUALITY_BUTTON], 0);
			updateButtonVisibility(&Buttons[MED_QUALITY_BUTTON], 1);
			updateButtonVisibility(&Buttons[LOW_QUALITY_BUTTON], 0);
		}
		else
		{
			updateButtonVisibility(&Buttons[HIGH_QUALITY_BUTTON], 0);
			updateButtonVisibility(&Buttons[GOOD_QUALITY_BUTTON], 0);
			updateButtonVisibility(&Buttons[MED_QUALITY_BUTTON], 0);
			updateButtonVisibility(&Buttons[LOW_QUALITY_BUTTON], 1);
		}
	}
	else
	{
		updateButtonVisibility(&Buttons[AUTO_UNSELECT_BUTTON], 0);
		updateButtonVisibility(&Buttons[AUTO_SELECT_BUTTON], 1);

		updateButtonVisibility(&Buttons[HIGH_QUALITY_BUTTON], 0);
		updateButtonVisibility(&Buttons[GOOD_QUALITY_BUTTON], 0);
		updateButtonVisibility(&Buttons[MED_QUALITY_BUTTON], 0);
		updateButtonVisibility(&Buttons[LOW_QUALITY_BUTTON], 0);

		if(quality_level == 0)
		{
			updateButtonVisibility(&Buttons[HIGH_QUALITY_SEL_BUTTON], 1);
			updateButtonVisibility(&Buttons[GOOD_QUALITY_SEL_BUTTON], 0);
			updateButtonVisibility(&Buttons[MED_QUALITY_SEL_BUTTON], 0);
			updateButtonVisibility(&Buttons[LOW_QUALITY_SEL_BUTTON], 0);
		}
		else if(quality_level == 1)
		{
			updateButtonVisibility(&Buttons[HIGH_QUALITY_SEL_BUTTON], 0);
			updateButtonVisibility(&Buttons[GOOD_QUALITY_SEL_BUTTON], 1);
			updateButtonVisibility(&Buttons[MED_QUALITY_SEL_BUTTON], 0);
			updateButtonVisibility(&Buttons[LOW_QUALITY_SEL_BUTTON], 0);
		}
		else if(quality_level == 2)
		{
			updateButtonVisibility(&Buttons[HIGH_QUALITY_SEL_BUTTON], 0);
			updateButtonVisibility(&Buttons[GOOD_QUALITY_SEL_BUTTON], 0);
			updateButtonVisibility(&Buttons[MED_QUALITY_SEL_BUTTON], 1);
			updateButtonVisibility(&Buttons[LOW_QUALITY_SEL_BUTTON], 0);
		}
		else
		{
			updateButtonVisibility(&Buttons[HIGH_QUALITY_SEL_BUTTON], 0);
			updateButtonVisibility(&Buttons[GOOD_QUALITY_SEL_BUTTON], 0);
			updateButtonVisibility(&Buttons[MED_QUALITY_SEL_BUTTON], 0);
			updateButtonVisibility(&Buttons[LOW_QUALITY_SEL_BUTTON], 1);
		}
	}
	

	drawButton(screen, &Buttons[AUTO_UNSELECT_BUTTON], BUTTON_NORM_STATE, mutex);
	drawButton(screen, &Buttons[AUTO_SELECT_BUTTON], BUTTON_NORM_STATE, mutex);

	drawButton(screen, &Buttons[LOW_QUALITY_BUTTON], BUTTON_NORM_STATE, mutex);
	drawButton(screen, &Buttons[MED_QUALITY_BUTTON], BUTTON_NORM_STATE, mutex);
	drawButton(screen, &Buttons[GOOD_QUALITY_BUTTON], BUTTON_NORM_STATE, mutex);
	drawButton(screen, &Buttons[HIGH_QUALITY_BUTTON], BUTTON_NORM_STATE, mutex);

	drawButton(screen, &Buttons[LOW_QUALITY_SEL_BUTTON], BUTTON_NORM_STATE, mutex);
	drawButton(screen, &Buttons[MED_QUALITY_SEL_BUTTON], BUTTON_NORM_STATE, mutex);
	drawButton(screen, &Buttons[GOOD_QUALITY_SEL_BUTTON], BUTTON_NORM_STATE, mutex);
	drawButton(screen, &Buttons[HIGH_QUALITY_SEL_BUTTON], BUTTON_NORM_STATE, mutex);

	return 0;
}

int drawToolTip(SDL_Surface *screen, int i, int mutex)
{
	int tltip_x, tltip_y;
	
	tltip_x = Buttons[i].coordinates.x;
	tltip_y = Buttons[i].coordinates.y;//-10;

	if( (tltip_x+strlen(Buttons[i].tooltip)*9) > screen_width)
		tltip_x = screen_width - strlen(Buttons[i].tooltip)*9;
	
	if( tltip_y < 0)
		tltip_y = 40;

	if(Buttons[i].show == 1)
	{
		if(strlen(Buttons[i].tooltip) > 0)
			drawFont(screen, MainFont[MAIN_FONT], Buttons[i].tooltip, tltip_x, tltip_y, 255, 255, 255, 43, 43, 23, mutex);
	}

	return 0;
}

int initGUI(SDL_Surface **	screen, SDL_mutex ** mutex, int * width, int * height, int fullScreenMode)
{
	int sdl_video_flags;

#ifndef WIN32
	XInitThreads();
#endif
	
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0 ) 
	{
		printf("Could not initialize SDL - %s\n", SDL_GetError());
		return -1;
	}

	max_screen_width 	= SDL_GetVideoInfo()->current_w & (0xFFFFFFC0);
	max_screen_height 	= SDL_GetVideoInfo()->current_h-60;

	printf("MAX: width: %d height: %d\n", screen_width, screen_height);

	if(fullScreenMode == 0)
	{
		screen_width 	= SDL_DEF_WIDTH;
		screen_height 	= SDL_DEF_HEIGHT;
		sdl_video_flags 	= SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL;
	}
	else
	{
		screen_width 	= max_screen_width;
		screen_height 	= max_screen_height;
		sdl_video_flags 	= SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_FULLSCREEN;
	}

	printf("width: %d height: %d\n", screen_width, screen_height);

	if (!(*screen = SDL_SetVideoMode(screen_width, screen_height, SDL_DEPTH, sdl_video_flags)))
	{
		SDL_Quit();
		return 1;
	}

	*width = screen_width;
	*height = screen_height-GUI_PANEL_HEIGHT;

	*mutex = SDL_CreateMutex();
	
	return sdl_video_flags;
}

int loadGUI(SDL_Surface *screen)
{
	int ret=IMG_Init(IMG_INIT_PNG);
	if((ret&IMG_INIT_PNG) != IMG_INIT_PNG)
	{
		printf("IMG_Init error: %s !!!\n", IMG_GetError());
		exit(-1);
	}

	if( TTF_Init() == -1 )
	{
		printf("TTF_Init error: %s\n", TTF_GetError());
		exit(-11);
	}

	loadButtons(&Buttons[PREV_BUTTON], 
				"images/left.png",
				"images/leftHover.png",
				"images/leftPress.png",
				"Previous Channel");
	
	loadButtons(&Buttons[NEXT_BUTTON], 
				"images/right.png",
				"images/rightHover.png",
				"images/rightPress.png",
				"Next Channel");

	loadButtons(&Buttons[DEFSCREEN_BUTTON], 
				"images/down.png",
				"images/downHover.png",
				"images/downPress.png",
				"Minimize");

	loadButtons(&Buttons[FULLSCREEN_BUTTON], 
				"images/up.png",
				"images/upHover.png",
				"images/upPress.png",
				"Maximize");

	loadButtons(&Buttons[MINUS_BUTTON], 
				"images/minus.png",
				"images/minusHover.png",
				"images/minusPress.png",
				"Decrease Volume");
	loadButtons(&Buttons[PLUS_BUTTON], 
				"images/plus.png",
				"images/plusHover.png",
				"images/plusPress.png",
				"Increase Volume");

	loadButtons(&Buttons[LEVEL0_BUTTON], 
				"images/slider0.png",
				NULL,
				NULL,
				NULL);
	loadButtons(&Buttons[LEVEL1_BUTTON], 
				"images/slider1.png",
				NULL,
				NULL,
				NULL);
	loadButtons(&Buttons[LEVEL2_BUTTON], 
				"images/slider2.png",
				NULL,
				NULL,
				NULL);
	loadButtons(&Buttons[LEVEL3_BUTTON], 
				"images/slider3.png",
				NULL,
				NULL,
				NULL);
	loadButtons(&Buttons[LEVEL4_BUTTON], 
				"images/slider4.png",
				NULL,
				NULL,
				NULL);
	loadButtons(&Buttons[LEVEL5_BUTTON], 
				"images/slider5.png",
				NULL,
				NULL,
				NULL);
	loadButtons(&Buttons[LEVEL6_BUTTON], 
				"images/slider6.png",
				NULL,
				NULL,
				NULL);
	loadButtons(&Buttons[LEVEL7_BUTTON], 
				"images/slider7.png",
				NULL,
				NULL,
				NULL);
	loadButtons(&Buttons[LEVEL8_BUTTON], 
				"images/slider8.png",
				NULL,
				NULL,
				NULL);

	loadButtons(&Buttons[AUTO_UNSELECT_BUTTON], 
				"images/radio_button.png",
				NULL,
				NULL,
				NULL);
	loadButtons(&Buttons[AUTO_SELECT_BUTTON], 
				"images/radio_button_selected.png",
				NULL,
				NULL,
				NULL);

	loadButtons(&Buttons[LOW_QUALITY_BUTTON], 
				"images/quality_low.png",
				NULL,
				NULL,
				NULL);
	loadButtons(&Buttons[MED_QUALITY_BUTTON], 
				"images/quality_medium.png",
				NULL,
				NULL,
				NULL);
	loadButtons(&Buttons[GOOD_QUALITY_BUTTON], 
				"images/quality_good.png",
				NULL,
				NULL,
				NULL);
	loadButtons(&Buttons[HIGH_QUALITY_BUTTON], 
				"images/quality_high.png",
				NULL,
				NULL,
				NULL);
	
	loadButtons(&Buttons[LOW_QUALITY_SEL_BUTTON], 
				"images/quality_low_selected.png",
				NULL,
				NULL,
				NULL);
	loadButtons(&Buttons[MED_QUALITY_SEL_BUTTON], 
				"images/quality_medium_selected.png",
				NULL,
				NULL,
				NULL);
	loadButtons(&Buttons[GOOD_QUALITY_SEL_BUTTON], 
				"images/quality_good_selected.png",
				NULL,
				NULL,
				NULL);
	loadButtons(&Buttons[HIGH_QUALITY_SEL_BUTTON], 
				"images/quality_high_selected.png",
				NULL,
				NULL,
				NULL);

	//Open the font
	MainFont[MAIN_FONT] = TTF_OpenFont("font/mainfont.ttf" , 14);
	if( MainFont[MAIN_FONT] == NULL)
	{
		printf("OpenFont error, %s file not found\n", "mainfont.ttf");
		exit(-1);
	}
	//TTF_SetFontStyle(MainFont[MAIN_FONT], TTF_STYLE_BOLD);

	MainFont[COMIC_SANS_FONT] = TTF_OpenFont("font/comic.ttf" , 16 );
	if( MainFont[COMIC_SANS_FONT] == NULL)
	{
		printf("OpenFont error, %s file not found\n", "comic.ttf");
		exit(-1);
	}
	TTF_SetFontStyle(MainFont[COMIC_SANS_FONT], TTF_STYLE_BOLD);

	MainFont[COMIC_SANS_FONT_2] = TTF_OpenFont("font/comic.ttf" , 14 );
	if( MainFont[COMIC_SANS_FONT_2] == NULL)
	{
		printf("OpenFont error, %s file not found\n", "comic.ttf");
		exit(-1);
	}
	TTF_SetFontStyle(MainFont[COMIC_SANS_FONT_2], TTF_STYLE_BOLD);

	MainFont[COMIC_SANS_FONT_3] = TTF_OpenFont("font/comic.ttf" , 12 );
	if( MainFont[COMIC_SANS_FONT_3] == NULL)
	{
		printf("OpenFont error, %s file not found\n", "comic.ttf");
		exit(-1);
	}
	TTF_SetFontStyle(MainFont[COMIC_SANS_FONT_3], TTF_STYLE_BOLD);

	MainFont[COMIC_SANS_FONT_4] = TTF_OpenFont("font/comic.ttf" , 10 );
	if( MainFont[COMIC_SANS_FONT_4] == NULL)
	{
		printf("OpenFont error, %s file not found\n", "comic.ttf");
		exit(-1);
	}
	TTF_SetFontStyle(MainFont[COMIC_SANS_FONT_4], TTF_STYLE_BOLD);

	SDL_WM_SetCaption("STAMP Player", NULL);

	return 0;	
}

int drawGUI(SDL_Surface *screen, int fullScreenMode, int volume_level, int * quality_level, int * auto_quality, char * channel_name, int mutex)
{
	int i;
		
	updateButtonCoordinates(&Buttons[PREV_BUTTON], (screen_width - 10) - (Buttons[PREV_BUTTON].coordinates.w*2), screen_height - (Buttons[PREV_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2));
	updateButtonCoordinates(&Buttons[NEXT_BUTTON], (screen_width - 10) - (Buttons[NEXT_BUTTON].coordinates.w), screen_height - (Buttons[NEXT_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2));
	updateButtonCoordinates(&Buttons[FULLSCREEN_BUTTON], 10, screen_height - (Buttons[FULLSCREEN_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2));
	updateButtonCoordinates(&Buttons[DEFSCREEN_BUTTON], 10, screen_height - (Buttons[DEFSCREEN_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2));
	updateButtonCoordinates(&Buttons[MINUS_BUTTON], 54, screen_height - (Buttons[MINUS_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2));
	updateButtonCoordinates(&Buttons[LEVEL0_BUTTON], 87, screen_height - (Buttons[LEVEL0_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2));
	updateButtonCoordinates(&Buttons[LEVEL1_BUTTON], 87, screen_height - (Buttons[LEVEL1_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2));
	updateButtonCoordinates(&Buttons[LEVEL2_BUTTON], 87, screen_height - (Buttons[LEVEL2_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2));
	updateButtonCoordinates(&Buttons[LEVEL3_BUTTON], 87, screen_height - (Buttons[LEVEL3_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2));
	updateButtonCoordinates(&Buttons[LEVEL4_BUTTON], 87, screen_height - (Buttons[LEVEL4_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2));
	updateButtonCoordinates(&Buttons[LEVEL5_BUTTON], 87, screen_height - (Buttons[LEVEL5_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2));
	updateButtonCoordinates(&Buttons[LEVEL6_BUTTON], 87, screen_height - (Buttons[LEVEL6_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2));
	updateButtonCoordinates(&Buttons[LEVEL7_BUTTON], 87, screen_height - (Buttons[LEVEL7_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2));
	updateButtonCoordinates(&Buttons[LEVEL8_BUTTON], 87, screen_height - (Buttons[LEVEL8_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2));
	updateButtonCoordinates(&Buttons[PLUS_BUTTON], 212, screen_height - (Buttons[PLUS_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2));
	updateButtonCoordinates(&Buttons[AUTO_UNSELECT_BUTTON], (screen_width-180)/2+80, screen_height - (Buttons[AUTO_UNSELECT_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2) + 12);
	updateButtonCoordinates(&Buttons[AUTO_SELECT_BUTTON], (screen_width-180)/2+80, screen_height - (Buttons[AUTO_SELECT_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2) + 12);
	updateButtonCoordinates(&Buttons[LOW_QUALITY_BUTTON], (screen_width-140)/2+80, screen_height - (Buttons[LOW_QUALITY_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2) + 12);
	updateButtonCoordinates(&Buttons[MED_QUALITY_BUTTON], (screen_width-140)/2+80, screen_height - (Buttons[MED_QUALITY_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2) + 12);
	updateButtonCoordinates(&Buttons[GOOD_QUALITY_BUTTON], (screen_width-140)/2+80, screen_height - (Buttons[GOOD_QUALITY_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2) + 12);
	updateButtonCoordinates(&Buttons[HIGH_QUALITY_BUTTON], (screen_width-140)/2+80, screen_height - (Buttons[HIGH_QUALITY_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2) + 12);	
	updateButtonCoordinates(&Buttons[LOW_QUALITY_SEL_BUTTON], (screen_width-140)/2+80, screen_height - (Buttons[LOW_QUALITY_SEL_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2) + 12);
	updateButtonCoordinates(&Buttons[MED_QUALITY_SEL_BUTTON], (screen_width-140)/2+80, screen_height - (Buttons[MED_QUALITY_SEL_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2) + 12);
	updateButtonCoordinates(&Buttons[GOOD_QUALITY_SEL_BUTTON], (screen_width-140)/2+80, screen_height - (Buttons[GOOD_QUALITY_SEL_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2) + 12);
	updateButtonCoordinates(&Buttons[HIGH_QUALITY_SEL_BUTTON], (screen_width-140)/2+80, screen_height - (Buttons[HIGH_QUALITY_SEL_BUTTON].coordinates.h/2) - (GUI_PANEL_HEIGHT/2) + 12);	

	if(fullScreenMode == 0)
	{
		updateButtonVisibility(&Buttons[FULLSCREEN_BUTTON], 1);
		updateButtonVisibility(&Buttons[DEFSCREEN_BUTTON], 0);
	}
	else
	{
		updateButtonVisibility(&Buttons[FULLSCREEN_BUTTON], 0);
		updateButtonVisibility(&Buttons[DEFSCREEN_BUTTON], 1);
	}

	//Video Display area
	fillRectangleColor(screen, 
					  0, 0, screen_width, (screen_height - GUI_PANEL_HEIGHT),
					  0, 0, 0,
					  mutex);	// Dark-Blue color

	//GUI Panel area
//*
	fillRectangleColor(screen, 
					  0, (screen_height - GUI_PANEL_HEIGHT), screen_width, (5),
					  0, 0, 0,
					  mutex);
//*/
	fillRectangleColor(screen, 
					  0, (screen_height - GUI_PANEL_HEIGHT+5), screen_width, GUI_PANEL_HEIGHT-5*2,
					  0, 0, 0,
					  mutex);

	fillRectangleColor(screen, 
					  0, (screen_height - 5), screen_width, (5),
					  0, 0, 0,
					  mutex);

	for(i=0;i<=NUM_BUTTONS-1;i++)
		drawButton(screen, &Buttons[i], BUTTON_NORM_STATE, mutex);

	updateVolumeLevel(screen, volume_level, mutex);

	drawFont(screen, MainFont[COMIC_SANS_FONT_3], "(Quality) ",  (screen_width+140)/2+20, screen_height-GUI_PANEL_HEIGHT/2+2, 120, 120, 120, 0, 0, 0, mutex);

	drawFont(screen, MainFont[COMIC_SANS_FONT_4], "Auto ",  (screen_width-115)/2+20, screen_height-GUI_PANEL_HEIGHT/2+4, 180, 180, 180, 0, 0, 0, mutex);

	updateQualityLevel(screen, *quality_level, *auto_quality, mutex);

	drawFont(screen, MainFont[COMIC_SANS_FONT], channel_name,  (screen_width-100)/2+80, screen_height-GUI_PANEL_HEIGHT/2 - 45/2, 232, 232, 232, 0, 0, 0, mutex);

	return 0;
}

int guiStatus = IDLE_THREAD;

int get_GUIStatus()
{
	return guiStatus;
}

int fillFormField(SDL_Surface *screen, int option_x, int option_y, char * title, char * value, int state)
{	
	char append_value[128] = {0};
	
	if(state == FORM_FIELD_NORM_STATE)
	{
		drawFont(screen, MainFont[COMIC_SANS_FONT_2], title,  option_x, option_y, 16, 16, 16, 129, 150, 181, 1);
		fillRectangleColor(screen,  option_x + 170, option_y+2, 200, 18, 190, 190, 190, 1);	// Dark-Blue color
		fillRectangleColor(screen,  option_x + 171, option_y+3, 198, 16, 190, 190, 190, 1);	// Dark-Blue color
		drawFont(screen, MainFont[MAIN_FONT], value,  option_x + 180, option_y+3, 16, 16, 16, 190, 190, 190, 1);
	}
	else if(state == FORM_FIELD_HOVER_STATE)
	{
		drawFont(screen, MainFont[COMIC_SANS_FONT_2], title,  option_x, option_y, 16, 16, 16, 129, 150, 181, 1);
		fillRectangleColor(screen,  option_x + 170, option_y+2, 200, 18, 0, 0, 0, 1);	// Dark-Blue color
		fillRectangleColor(screen,  option_x + 171, option_y+3, 198, 16, 150, 150, 150, 1);	// Dark-Blue color
		drawFont(screen, MainFont[MAIN_FONT], value,  option_x + 180, option_y+3, 16, 16, 16, 150, 150, 150, 1);
	}
	else // state = FORM_FIELD_EDIT_STATE
	{
		sprintf(append_value, "%s_", value);
		drawFont(screen, MainFont[COMIC_SANS_FONT_2], title,  option_x, option_y, 16, 16, 16, 129, 150, 181, 1);
		fillRectangleColor(screen,  option_x + 170, option_y+2, 200, 18, 0, 0, 0, 1);	// Dark-Blue color
		fillRectangleColor(screen,  option_x + 171, option_y+3, 198, 16, 232, 232, 232, 1);	// Dark-Blue color
		drawFont(screen, MainFont[MAIN_FONT], append_value,  option_x + 180, option_y+3, 16, 16, 16, 232, 232, 232, 1);
	}
	
	return 0;
	
}

int fillFormButton(SDL_Surface *screen, int option_x, int option_y, int offset, char * value, int state)
{
	if(state == FORM_FIELD_NORM_STATE)
	{
		fillRectangleColor(screen,  option_x + 173, option_y+5, 160, 30, 129, 150, 181, 1);	// Dark-Blue color
		fillRectangleColor(screen,  option_x + 172, option_y+4, 160, 30, 64, 64, 64, 1);	// Dark-Blue color
		fillRectangleColor(screen,  option_x + 171, option_y+3, 160, 30, 190, 190, 190, 1);	// Dark-Blue color
		drawFont(screen, MainFont[COMIC_SANS_FONT_2], value,  option_x + offset, option_y+8, 0, 33, 87, 190, 190, 190, 1);
	}
	else
	{
	
		fillRectangleColor(screen,  option_x + 173, option_y+5, 160, 30, 16, 16, 16, 1);	// Dark-Blue color
		fillRectangleColor(screen,  option_x + 171, option_y+3, 160, 30, 170, 170, 170, 1);	// Dark-Blue color
		drawFont(screen, MainFont[COMIC_SANS_FONT_2], value,  option_x + offset, option_y+8, 0, 33, 87, 170, 170, 170, 1);
	}
	
	return 0;
	
}


int GUISettingsWindow(void * arg)
{
	pGUISettings_t 	pSettings = (pGUISettings_t ) arg;
	pSDLParams_t 	pParams = (pSDLParams_t ) (pSettings->pgui_vdec_struct);

	SDL_Event 		event;
	int 				keypress = 0;

	int form_x = 45;
	int form_y = 60;
	int form_width = screen_width-80;
	int form_height = screen_height-140;

	int option_x;
	int option_y;

	int player_ip = 0;
	int edit_player_ip = 0;
	int player_ip_state = 0;
	char player_ip_str[128] = {0};

	int player_port = 0;
	int edit_player_port = 0;
	int player_port_state = 0;
	char player_port_str[128] = {0};

	int external_ip = 0;
	int edit_external_ip = 0;
	int external_ip_state = 0;
	char external_ip_str[128] = {0};

	int external_port = 0;
	int edit_external_port = 0;
	int external_port_state = 0;
	char external_port_str[128] = {0};

	int ch_server_ip = 0;
	int edit_ch_server_ip = 0;
	int ch_server_ip_state = 0;
	char ch_server_ip_str[128] = {0};

	int ch_server_port = 0;
	int edit_ch_server_port = 0;
	int ch_server_port_state = 0;
	char ch_server_port_str[128] = {0};	

	int save_sel = 0;
	int save_state = 0;

	int temp;
	int temp2;
	int temp3;

	GetConfiguration(player_ip_str, external_ip_str, ch_server_ip_str, &temp, &temp2, &temp3);
	sprintf(ch_server_port_str, "%d", temp);		
	sprintf(player_port_str, "%d", temp2);
	sprintf(external_port_str, "%d", temp3);
		
	//Background area
	fillRectangleColor(pParams->screenHandle, 0, 0, screen_width, screen_height, 0, 33, 87, 1);

	//Palette area
	fillRectangleColor(pParams->screenHandle, form_x, form_y, form_width, form_height, 129, 150, 181, 1);

	drawFont(pParams->screenHandle, MainFont[COMIC_SANS_FONT_2], "Version 3.1 ",  form_x+500, form_y+390, 242, 242, 242, 0, 33, 87, 1);

	/*******************************/
	
	// Player IP
	player_ip_state = FORM_FIELD_NORM_STATE;
	fillFormField(pParams->screenHandle, form_x + 10, form_y + 10, "Player IP:", player_ip_str, player_ip_state);

	// Player Port
	player_port_state = FORM_FIELD_NORM_STATE;
	fillFormField(pParams->screenHandle, form_x + 10, form_y + 40, "Player Port:", player_port_str, player_port_state);

	// External IP
	external_ip_state = FORM_FIELD_NORM_STATE;
	fillFormField(pParams->screenHandle, form_x + 10, form_y + 70, "External IP:", external_ip_str, external_ip_state);

	// External Port
	external_port_state = FORM_FIELD_NORM_STATE;
	fillFormField(pParams->screenHandle, form_x + 10, form_y + 100, "External Port:", external_port_str, external_port_state);

	// Division
	fillRectangleColor(pParams->screenHandle,  form_x + 11, form_y + 140, 540, 2, 0, 0, 0, 1);	// Dark-Blue color

	// Channel Server IP
	ch_server_ip_state = FORM_FIELD_NORM_STATE;
	fillFormField(pParams->screenHandle, form_x + 10, form_y + 160, "Channel Server IP:", ch_server_ip_str, ch_server_ip_state);

	// Channel Server Port
	ch_server_port_state = FORM_FIELD_NORM_STATE;
	fillFormField(pParams->screenHandle, form_x + 10, form_y + 200, "Channel Server Port:", ch_server_port_str, ch_server_port_state);

	/*******************************/

	// Save
	save_state = FORM_FIELD_NORM_STATE;
	fillFormButton(pParams->screenHandle, form_x+30, form_y+280, 234, "OK", save_state);

	while(!keypress)
	{
		//while(SDL_PollEvent(&event)) 
		if(SDL_WaitEvent(&event))
		{
			if(!(edit_player_ip | edit_player_port | edit_external_ip | edit_external_port | edit_ch_server_ip | edit_ch_server_port))
			{
				switch (event.type) 
				{
					case SDL_KEYDOWN:
					{
						if((event.key.keysym.sym == SDLK_RETURN) || (event.key.keysym.sym == SDLK_KP_ENTER) )
						{
							if((player_ip == 1) && (player_ip_state != FORM_FIELD_EDIT_STATE))
							{
								player_ip_state = FORM_FIELD_EDIT_STATE;
								fillFormField(pParams->screenHandle, form_x + 10, form_y + 10, "Player IP:", player_ip_str, player_ip_state);
								edit_player_ip = 1;
							}

							if((player_port == 1) && (player_port_state != FORM_FIELD_EDIT_STATE))
							{
								player_port_state = FORM_FIELD_EDIT_STATE;
								fillFormField(pParams->screenHandle, form_x + 10, form_y + 40, "Player Port:", player_port_str, player_port_state);
								edit_player_port = 1;
							}

							if((external_ip == 1) && (external_ip_state != FORM_FIELD_EDIT_STATE))
							{
								external_ip_state = FORM_FIELD_EDIT_STATE;
								fillFormField(pParams->screenHandle, form_x + 10, form_y + 70, "External IP:", external_ip_str, external_ip_state);
								edit_external_ip = 1;
							}

							if((external_port == 1) && (external_port_state != FORM_FIELD_EDIT_STATE))
							{
								external_port_state = FORM_FIELD_EDIT_STATE;
								fillFormField(pParams->screenHandle, form_x + 10, form_y + 100, "External Port:", external_port_str, external_port_state);
								edit_external_port = 1;
							}

							if((ch_server_ip == 1) && (ch_server_ip_state != FORM_FIELD_EDIT_STATE))
							{
								ch_server_ip_state = FORM_FIELD_EDIT_STATE;
								fillFormField(pParams->screenHandle, form_x + 10, form_y + 160, "Channel Server IP:", ch_server_ip_str, ch_server_ip_state);
								edit_ch_server_ip = 1;
							}

							if((ch_server_port == 1) && (ch_server_port_state != FORM_FIELD_EDIT_STATE))
							{
								ch_server_port_state = FORM_FIELD_EDIT_STATE;
								fillFormField(pParams->screenHandle, form_x + 10, form_y + 200, "Channel Server Port:", ch_server_ip_str, ch_server_port_state);
								edit_ch_server_port = 1;
							}
						}
					}
					break;

					case SDL_MOUSEMOTION:
					{
						option_x = form_x+10;
						option_y = form_y+10;
						player_ip = ( event.motion.x > (option_x + 170) ) && ( event.motion.x < (option_x+370) ) && ( event.motion.y > (option_y+2)) && ( event.motion.y < (option_y+19) );

						option_x = form_x+10;
						option_y = form_y+40;
						player_port = ( event.motion.x > (option_x + 170) ) && ( event.motion.x < (option_x+370) ) && ( event.motion.y > (option_y+2)) && ( event.motion.y < (option_y+19) );

						option_x = form_x+10;
						option_y = form_y+70;
						external_ip = ( event.motion.x > (option_x + 170) ) && ( event.motion.x < (option_x+370) ) && ( event.motion.y > (option_y+2)) && ( event.motion.y < (option_y+19) );

						option_x = form_x+10;
						option_y = form_y+100;
						external_port = ( event.motion.x > (option_x + 170) ) && ( event.motion.x < (option_x+370) ) && ( event.motion.y > (option_y+2)) && ( event.motion.y < (option_y+19) );

						option_x = form_x+10;
						option_y = form_y+160;
						ch_server_ip = ( event.motion.x > (option_x + 170) ) && ( event.motion.x < (option_x+370) ) && ( event.motion.y > (option_y+2)) && ( event.motion.y < (option_y+19) );

						option_x = form_x+10;
						option_y = form_y+200;
						ch_server_port = ( event.motion.x > (option_x + 170) ) && ( event.motion.x < (option_x+370) ) && ( event.motion.y > (option_y+2)) && ( event.motion.y < (option_y+19) );

						option_x = form_x+30;
						option_y = form_y+280;
						save_sel = ( event.motion.x > (option_x + 171) ) && ( event.motion.x < (option_x+331) ) && ( event.motion.y > (option_y+3)) && ( event.motion.y < (option_y+34) );

						if((player_ip == 1) && (player_ip_state != FORM_FIELD_HOVER_STATE))
						{
							player_ip_state = FORM_FIELD_HOVER_STATE;
							fillFormField(pParams->screenHandle, form_x + 10, form_y + 10, "Player IP:", player_ip_str, player_ip_state);
						}
						else if((player_ip == 0) && (player_ip_state != FORM_FIELD_NORM_STATE))
						{
							player_ip_state = FORM_FIELD_NORM_STATE;
							fillFormField(pParams->screenHandle, form_x + 10, form_y + 10, "Player IP:", player_ip_str, player_ip_state);
						}

						if((player_port == 1) && (player_port_state != FORM_FIELD_HOVER_STATE))
						{
							player_port_state = FORM_FIELD_HOVER_STATE;
							fillFormField(pParams->screenHandle, form_x + 10, form_y + 40, "Player Port:", player_port_str, player_port_state);
						}
						else if((player_port == 0) && (player_port_state != FORM_FIELD_NORM_STATE))
						{
							player_port_state = FORM_FIELD_NORM_STATE;
							fillFormField(pParams->screenHandle, form_x + 10, form_y + 40, "Player Port:", player_port_str, player_port_state);
						}

						if((external_ip == 1) && (external_ip_state != FORM_FIELD_HOVER_STATE))
						{
							external_ip_state = FORM_FIELD_HOVER_STATE;
							fillFormField(pParams->screenHandle, form_x + 10, form_y + 70, "External IP:", external_ip_str, external_ip_state);
						}
						else if((external_ip == 0) && (external_ip_state != FORM_FIELD_NORM_STATE))
						{
							external_ip_state = FORM_FIELD_NORM_STATE;
							fillFormField(pParams->screenHandle, form_x + 10, form_y + 70, "External IP:", external_ip_str, external_ip_state);
						}

						if((external_port == 1) && (external_port_state != FORM_FIELD_HOVER_STATE))
						{
							external_port_state = FORM_FIELD_HOVER_STATE;
							fillFormField(pParams->screenHandle, form_x + 10, form_y + 100, "External Port:", external_port_str, external_port_state);
						}
						else if((external_port == 0) && (external_port_state != FORM_FIELD_NORM_STATE))
						{
							external_port_state = FORM_FIELD_NORM_STATE;
							fillFormField(pParams->screenHandle, form_x + 10, form_y + 100, "External Port:", external_port_str, external_port_state);
						}

						if((ch_server_ip == 1) && (ch_server_ip_state != FORM_FIELD_HOVER_STATE))
						{
							ch_server_ip_state = FORM_FIELD_HOVER_STATE;
							fillFormField(pParams->screenHandle, form_x + 10, form_y + 160, "Channel Server IP:", ch_server_ip_str, ch_server_ip_state);
						}
						else if((ch_server_ip == 0) && (ch_server_ip_state != FORM_FIELD_NORM_STATE))
						{
							ch_server_ip_state = FORM_FIELD_NORM_STATE;
							fillFormField(pParams->screenHandle, form_x + 10, form_y + 160, "Channel Server IP:", ch_server_ip_str, ch_server_ip_state);
						}		

						if((ch_server_port == 1) && (ch_server_port_state != FORM_FIELD_HOVER_STATE))
						{
							ch_server_port_state = FORM_FIELD_HOVER_STATE;
							fillFormField(pParams->screenHandle, form_x + 10, form_y + 200, "Channel Server Port:", ch_server_port_str, ch_server_port_state);
						}
						else if((ch_server_port == 0) && (ch_server_port_state != FORM_FIELD_NORM_STATE))
						{
							ch_server_port_state = FORM_FIELD_NORM_STATE;
							fillFormField(pParams->screenHandle, form_x + 10, form_y + 200, "Channel Server Port:", ch_server_port_str, ch_server_port_state);
						}							

						if((save_sel == 1) && (save_state != FORM_FIELD_HOVER_STATE))
						{
							save_state = FORM_FIELD_HOVER_STATE;
							fillFormButton(pParams->screenHandle, form_x+30, form_y+280, 234, "OK", save_state);
							
						}
						else if((save_sel == 0) && (save_state != FORM_FIELD_NORM_STATE))
						{
							save_state = FORM_FIELD_NORM_STATE;
							fillFormButton(pParams->screenHandle, form_x+30, form_y+280, 234, "OK", save_state);
						}
					}
					break;

					case SDL_MOUSEBUTTONUP:
					{
						
						if(player_ip == 1)
						{
							fillFormField(pParams->screenHandle, form_x + 10, form_y + 10, "Player IP:", player_ip_str, FORM_FIELD_EDIT_STATE);
							edit_player_ip = 1;
						}
						else
						if(player_port == 1)
						{
							fillFormField(pParams->screenHandle, form_x + 10, form_y + 40, "Player Port:", player_port_str, FORM_FIELD_EDIT_STATE);
							edit_player_port = 1;
						}
						else
						if(external_ip == 1)
						{
							fillFormField(pParams->screenHandle, form_x + 10, form_y + 70, "External IP:", external_ip_str, FORM_FIELD_EDIT_STATE);
							edit_external_ip = 1;
						}
						else
						if(external_port == 1)
						{
							fillFormField(pParams->screenHandle, form_x + 10, form_y + 100, "External Port:", external_port_str, FORM_FIELD_EDIT_STATE);
							edit_external_port = 1;
						}
						else
						if(ch_server_ip == 1)
						{
							fillFormField(pParams->screenHandle, form_x + 10, form_y + 160, "Channel Server IP:", ch_server_ip_str, FORM_FIELD_EDIT_STATE);
							edit_ch_server_ip = 1;
						}
						else
						if(ch_server_port == 1)
						{
							fillFormField(pParams->screenHandle, form_x + 10, form_y + 200, "Channel Server Port:", ch_server_port_str, FORM_FIELD_EDIT_STATE);
							edit_ch_server_port = 1;
						}						
						else
						if(save_sel == 1)
						{
							printf("keypress: %d\n", keypress);
							keypress = 1;
						}
					}
					break;
				}
			}
			else
			{
				switch (event.type) 
				{
					case SDL_QUIT:
						keypress = 1;
					break;
					case SDL_MOUSEBUTTONUP:
					{
						int is_it_edit_player_ip = (edit_player_ip == 1);
						int is_it_edit_external_ip = (edit_external_ip == 1);
						int is_it_edit_player_port = (edit_player_port == 1);
						int is_it_edit_external_port = (edit_external_port == 1);
						
						int is_it_edit_ch_server_ip = (edit_ch_server_ip == 1);
						int is_it_edit_ch_server_port = (edit_ch_server_port == 1);
						
						
						if((edit_player_ip == 1) || (edit_player_port == 1) || (edit_external_ip == 1) || (edit_external_port == 1)  || (edit_ch_server_ip == 1) || (edit_ch_server_port == 1))
						{
							option_x = form_x+10;
							option_y = form_y+10;
							player_ip = ( event.motion.x > (option_x + 170) ) && ( event.motion.x < (option_x+370) ) && ( event.motion.y > (option_y+2)) && ( event.motion.y < (option_y+19) );

							option_x = form_x+10;
							option_y = form_y+40;
							player_port = ( event.motion.x > (option_x + 170) ) && ( event.motion.x < (option_x+370) ) && ( event.motion.y > (option_y+2)) && ( event.motion.y < (option_y+19) );

							option_x = form_x+10;
							option_y = form_y+70;
							external_ip = ( event.motion.x > (option_x + 170) ) && ( event.motion.x < (option_x+370) ) && ( event.motion.y > (option_y+2)) && ( event.motion.y < (option_y+19) );

							option_x = form_x+10;
							option_y = form_y+100;
							external_port = ( event.motion.x > (option_x + 170) ) && ( event.motion.x < (option_x+370) ) && ( event.motion.y > (option_y+2)) && ( event.motion.y < (option_y+19) );

							option_x = form_x+10;
							option_y = form_y+160;
							ch_server_ip = ( event.motion.x > (option_x + 170) ) && ( event.motion.x < (option_x+370) ) && ( event.motion.y > (option_y+2)) && ( event.motion.y < (option_y+19) );

							option_x = form_x+10;
							option_y = form_y+200;
							ch_server_port = ( event.motion.x > (option_x + 170) ) && ( event.motion.x < (option_x+370) ) && ( event.motion.y > (option_y+2)) && ( event.motion.y < (option_y+19) );

							option_x = form_x+30;
							option_y = form_y+280;
							save_sel = ( event.motion.x > (option_x + 171) ) && ( event.motion.x < (option_x+331) ) && ( event.motion.y > (option_y+3)) && ( event.motion.y < (option_y+34) );

							if(edit_player_ip == 1)
								edit_player_ip = 0;
							else if(edit_player_port == 1)
								edit_player_port = 0;
							else if(edit_external_ip == 1)
								edit_external_ip = 0;
							else if(edit_external_port == 1)
								edit_external_port = 0;
							else if(edit_ch_server_ip == 1)
								edit_ch_server_ip = 0;
							else if(edit_ch_server_port == 1)
								edit_ch_server_port = 0;							

							if(is_it_edit_player_ip == 1)
							{								
							      	if(edit_player_ip == 0)
								{
									fillFormField(pParams->screenHandle, form_x + 10, form_y + 10, "Player IP:", player_ip_str, FORM_FIELD_HOVER_STATE);
								}
							}
							else
							if(is_it_edit_player_port == 1)
							{								
							      	if(edit_player_port == 0)
								{
									fillFormField(pParams->screenHandle, form_x + 10, form_y + 40, "Player Port:", player_port_str, FORM_FIELD_HOVER_STATE);
								}
							}
							else
							if(is_it_edit_external_ip == 1)
							{								
							      	if(edit_external_ip == 0)
								{
									fillFormField(pParams->screenHandle, form_x + 10, form_y + 70, "External IP:", external_ip_str, FORM_FIELD_HOVER_STATE);
								}
							}
							else
							if(is_it_edit_external_port == 1)
							{								
							      	if(edit_external_port == 0)
								{
									fillFormField(pParams->screenHandle, form_x + 10, form_y + 100, "External Port:", external_port_str, FORM_FIELD_HOVER_STATE);
								}
							}
							else
							if(is_it_edit_ch_server_ip == 1)
							{								
							      	if(edit_ch_server_ip == 0)
								{
									fillFormField(pParams->screenHandle, form_x + 10, form_y + 160, "Channel Server IP:", ch_server_ip_str, FORM_FIELD_HOVER_STATE);
								}
							}
							else
							if(is_it_edit_ch_server_port == 1)
							{
							      	if(edit_ch_server_port== 0)
								{
									fillFormField(pParams->screenHandle, form_x + 10, form_y + 200, "Channel Server Port:", ch_server_port_str, FORM_FIELD_HOVER_STATE);
								}
							}							

							if((player_ip == 1) && (player_ip_state != FORM_FIELD_HOVER_STATE))
							{
								player_ip_state = FORM_FIELD_HOVER_STATE;
								fillFormField(pParams->screenHandle, form_x + 10, form_y + 10, "Player IP:", player_ip_str, player_ip_state);
							}
							else if((player_ip == 0) && (player_ip_state != FORM_FIELD_NORM_STATE))
							{
								player_ip_state = FORM_FIELD_NORM_STATE;
								fillFormField(pParams->screenHandle, form_x + 10, form_y + 10, "Player IP:", player_ip_str, player_ip_state);
							}

							if((player_port == 1) && (player_port_state != FORM_FIELD_HOVER_STATE))
							{
								player_port_state = FORM_FIELD_HOVER_STATE;
								fillFormField(pParams->screenHandle, form_x + 10, form_y + 40, "Player Port:", player_port_str, player_port_state);
							}
							else if((player_port == 0) && (player_port_state != FORM_FIELD_NORM_STATE))
							{
								player_port_state = FORM_FIELD_NORM_STATE;
								fillFormField(pParams->screenHandle, form_x + 10, form_y + 40, "Player Port:", player_port_str, player_port_state);
							}

							if((external_ip == 1) && (external_ip_state != FORM_FIELD_HOVER_STATE))
							{
								external_ip_state = FORM_FIELD_HOVER_STATE;
								fillFormField(pParams->screenHandle, form_x + 10, form_y + 70, "External IP:", external_ip_str, external_ip_state);
							}
							else if((external_ip == 0) && (external_ip_state != FORM_FIELD_NORM_STATE))
							{
								external_ip_state = FORM_FIELD_NORM_STATE;
								fillFormField(pParams->screenHandle, form_x + 10, form_y + 70, "External IP:", external_ip_str, external_ip_state);
							}

							if((external_port == 1) && (external_port_state != FORM_FIELD_HOVER_STATE))
							{
								external_port_state = FORM_FIELD_HOVER_STATE;
								fillFormField(pParams->screenHandle, form_x + 10, form_y + 100, "External Port:", external_port_str, external_port_state);
							}
							else if((external_port == 0) && (external_port_state != FORM_FIELD_NORM_STATE))
							{
								external_port_state = FORM_FIELD_NORM_STATE;
								fillFormField(pParams->screenHandle, form_x + 10, form_y + 100, "External Port:", external_port_str, external_port_state);
							}

							if((ch_server_ip == 1) && (ch_server_ip_state != FORM_FIELD_HOVER_STATE))
							{
								ch_server_ip_state = FORM_FIELD_HOVER_STATE;
								fillFormField(pParams->screenHandle, form_x + 10, form_y + 160, "Channel Server IP:", ch_server_ip_str, ch_server_ip_state);
							}
							else if((ch_server_ip == 0) && (ch_server_ip_state != FORM_FIELD_NORM_STATE))
							{
								ch_server_ip_state = FORM_FIELD_NORM_STATE;
								fillFormField(pParams->screenHandle, form_x + 10, form_y + 160, "Channel Server IP:", ch_server_ip_str, ch_server_ip_state);
							}		

							if((ch_server_port == 1) && (ch_server_port_state != FORM_FIELD_HOVER_STATE))
							{
								ch_server_port_state = FORM_FIELD_HOVER_STATE;
								fillFormField(pParams->screenHandle, form_x + 10, form_y + 200, "Channel Server Port:", ch_server_port_str, ch_server_port_state);
							}
							else if((ch_server_port == 0) && (ch_server_port_state != FORM_FIELD_NORM_STATE))
							{
								ch_server_port_state = FORM_FIELD_NORM_STATE;
								fillFormField(pParams->screenHandle, form_x + 10, form_y + 200, "Channel Server Port:", ch_server_port_str, ch_server_port_state);
							}							

							if((save_sel == 1) && (save_state != FORM_FIELD_HOVER_STATE))
							{
								save_state = FORM_FIELD_HOVER_STATE;
								fillFormButton(pParams->screenHandle, form_x+30, form_y+280, 234, "OK", save_state);
								
							}
							else if((save_sel == 0) && (save_state != FORM_FIELD_NORM_STATE))
							{
								save_state = FORM_FIELD_NORM_STATE;
								fillFormButton(pParams->screenHandle, form_x+30, form_y+280, 234, "OK", save_state);
							}

						}
					}
					break;
					case SDL_KEYDOWN:
					{
						int valid = 0;
						int is_it_edit_player_ip = (edit_player_ip == 1);
						int is_it_edit_external_ip = (edit_external_ip == 1);
						int is_it_edit_player_port = (edit_player_port == 1);
						int is_it_edit_external_port = (edit_external_port == 1);
						int is_it_edit_ch_server_ip = (edit_ch_server_ip == 1);
						int is_it_edit_ch_server_port = (edit_ch_server_port == 1);
						char * gui_str;
						int limit;
						
						
						if((edit_player_ip == 1) || (edit_player_port == 1) || (edit_external_ip == 1) || (edit_external_port == 1) || (edit_ch_server_ip == 1) || (edit_ch_server_port == 1))
						{
							if(edit_player_ip == 1)
							{
								gui_str = player_ip_str;
								limit = 16;
							}
							if(edit_player_port == 1)
							{
								gui_str = player_port_str;
								limit = 5;
							}
							if(edit_external_ip == 1)
							{
								gui_str = external_ip_str;
								limit = 16;
							}
							if(edit_external_port == 1)
							{
								gui_str = external_port_str;
								limit = 5;
							}
							if(edit_ch_server_ip == 1)
							{
								gui_str = ch_server_ip_str;
								limit = 16;
							}
							if(edit_ch_server_port== 1)
							{
								gui_str = ch_server_port_str;
								limit = 5;
							}							
								
							if( (event.key.keysym.sym == SDLK_0) || (event.key.keysym.sym == SDLK_KP0) )
							{
								if(strlen(gui_str) < limit)
								{
									gui_str[strlen(gui_str)] ='0';
								      	valid = 1;
								}
							}
							else if( (event.key.keysym.sym == SDLK_1) || (event.key.keysym.sym == SDLK_KP1) )
							{
								if(strlen(gui_str) < limit)
								{
									gui_str[strlen(gui_str)] ='1';
								      	valid = 1;
								}
							}
							else if( (event.key.keysym.sym == SDLK_2) || (event.key.keysym.sym == SDLK_KP2) )
							{
								if(strlen(gui_str) < limit)
								{
									gui_str[strlen(gui_str)] ='2';
								      	valid = 1;
								}
							}
							else if( (event.key.keysym.sym == SDLK_3) || (event.key.keysym.sym == SDLK_KP3) )
							{
								if(strlen(gui_str) < limit)
								{
									gui_str[strlen(gui_str)] ='3';
								      	valid = 1;
								}
							}
							else if( (event.key.keysym.sym == SDLK_4) || (event.key.keysym.sym == SDLK_KP4) )
							{
								if(strlen(gui_str) < limit)
								{
									gui_str[strlen(gui_str)] ='4';
								      	valid = 1;
								}
							}
							else if( (event.key.keysym.sym == SDLK_5) || (event.key.keysym.sym == SDLK_KP5) )
							{
								if(strlen(gui_str) < limit)
								{
									gui_str[strlen(gui_str)] ='5';
								      	valid = 1;
								}
							}
							else if( (event.key.keysym.sym == SDLK_6) || (event.key.keysym.sym == SDLK_KP6) )
							{
								if(strlen(gui_str) < limit)
								{
									gui_str[strlen(gui_str)] ='6';
								      	valid = 1;
								}
							}
							else if( (event.key.keysym.sym == SDLK_7) || (event.key.keysym.sym == SDLK_KP7) )
							{
								if(strlen(gui_str) < limit)
								{
									gui_str[strlen(gui_str)] ='7';
								      	valid = 1;
								}
							}
							else if( (event.key.keysym.sym == SDLK_8) || (event.key.keysym.sym == SDLK_KP8) )
							{
								if(strlen(gui_str) < limit)
								{
									gui_str[strlen(gui_str)] ='8';
								      	valid = 1;
								}
							}
							else if( (event.key.keysym.sym == SDLK_9) || (event.key.keysym.sym == SDLK_KP9) )
							{
								if(strlen(gui_str) < limit)
								{
									gui_str[strlen(gui_str)] ='9';
								      	valid = 1;
								}
							}
							else if(( (event.key.keysym.sym == SDLK_PERIOD) || (event.key.keysym.sym == SDLK_KP_PERIOD) ) && (edit_ch_server_port == 0))
							{
								if(strlen(gui_str) < limit)
								{
									gui_str[strlen(gui_str)] ='.';
								      	valid = 1;
								}
							}
							else if(event.key.keysym.sym == SDLK_BACKSPACE)
							{
								if(strlen(gui_str) > 0)
								{
									gui_str[strlen(gui_str)-1] ='\0';
								      	valid = 1;
								}
							}
							else if((event.key.keysym.sym == SDLK_RETURN) || (event.key.keysym.sym == SDLK_KP_ENTER) )
							{
								if(edit_player_ip == 1)
									edit_player_ip = 0;
								else if(edit_player_port == 1)
									edit_player_port = 0;
								else if(edit_external_ip == 1)
									edit_external_ip = 0;
								else if(edit_external_port == 1)
									edit_external_port = 0;
								else if(edit_ch_server_ip == 1)
									edit_ch_server_ip = 0;
								else if(edit_ch_server_port == 1)
									edit_ch_server_port = 0;							
							}

							if(is_it_edit_player_ip == 1)
							{
							      	if(valid == 1)
								{
									fillFormField(pParams->screenHandle, form_x + 10, form_y + 10, "Player IP:", player_ip_str, FORM_FIELD_EDIT_STATE);
								}
									
							      	if(edit_player_ip == 0)
								{
									fillFormField(pParams->screenHandle, form_x + 10, form_y + 10, "Player IP:", player_ip_str, FORM_FIELD_HOVER_STATE);
								}
							}
							else
							if(is_it_edit_player_port == 1)
							{
							      	if(valid == 1)
								{
									fillFormField(pParams->screenHandle, form_x + 10, form_y + 40, "Player Port:", player_port_str, FORM_FIELD_EDIT_STATE);
								}
									
							      	if(edit_player_port == 0)
								{
									fillFormField(pParams->screenHandle, form_x + 10, form_y + 40, "Player Port:", player_port_str, FORM_FIELD_HOVER_STATE);
								}
							}
							else
							if(is_it_edit_external_ip == 1)
							{
							      	if(valid == 1)
								{
									fillFormField(pParams->screenHandle, form_x + 10, form_y + 70, "External IP:", external_ip_str, FORM_FIELD_EDIT_STATE);
								}
									
							      	if(edit_external_ip == 0)
								{
									fillFormField(pParams->screenHandle, form_x + 10, form_y + 70, "External IP:", external_ip_str, FORM_FIELD_HOVER_STATE);
								}
							}
							else
							if(is_it_edit_external_port == 1)
							{
							      	if(valid == 1)
								{
									fillFormField(pParams->screenHandle, form_x + 10, form_y + 100, "External Port:", external_port_str, FORM_FIELD_EDIT_STATE);
								}
									
							      	if(edit_external_port == 0)
								{
									fillFormField(pParams->screenHandle, form_x + 10, form_y + 100, "External Port:", external_port_str, FORM_FIELD_HOVER_STATE);
								}
							}
							else
							if(is_it_edit_ch_server_ip == 1)
							{
							      	if(valid == 1)
								{
									fillFormField(pParams->screenHandle, form_x + 10, form_y + 160, "Channel Server IP:", ch_server_ip_str, FORM_FIELD_EDIT_STATE);
								}
									
							      	if(edit_ch_server_ip == 0)
								{
									fillFormField(pParams->screenHandle, form_x + 10, form_y + 160, "Channel Server IP:", ch_server_ip_str, FORM_FIELD_HOVER_STATE);
								}
							}
							else
							if(is_it_edit_ch_server_port == 1)
							{
							      	if(valid == 1)
								{
									fillFormField(pParams->screenHandle, form_x + 10, form_y + 200, "Channel Server Port:", ch_server_port_str, FORM_FIELD_EDIT_STATE);
								}
									
							      	if(edit_ch_server_port== 0)
								{
									fillFormField(pParams->screenHandle, form_x + 10, form_y + 200, "Channel Server Port:", ch_server_port_str, FORM_FIELD_HOVER_STATE);
								}
							}							
						}
					}
					break;
				}
			}

		}

		//usleep(1000000);
	}

	temp = atoi(ch_server_port_str);
	temp2 = atoi(player_port_str);
	temp3 = atoi(external_port_str);
	SetConfiguration(player_ip_str, external_ip_str, ch_server_ip_str, temp, temp2, temp3);
	SaveConfiguration();

	pSettings->channelServerCbk();

	//Video Display area
	fillRectangleColor(pParams->screenHandle, 
					  0, 0, screen_width, screen_height,
					  0, 0, 0,
					  1);	// Dark-Blue color
					  
	guiSettingsWindow = 1;

}

int GUIThread(void * arg)
{
	pGUISettings_t 	pSettings = (pGUISettings_t ) arg;
	pSDLParams_t 	pParams = (pSDLParams_t ) (pSettings->pgui_vdec_struct);
	SDL_Event 		event;
	int 				keypress = 0;
	int 				sdl_video_flags;
	int 				i; 

	char				channel_name[1024] = {0};
	int				fullScreenMode = pSettings->maximize_window;
	int				volume_level = (int)((float)(pSettings->volume)*(8.0/100.0));

	guiStatus = IDLE_THREAD;

	//2Initialize Graphics

	/* Initialize GUI */
	pParams->sdl_flags = initGUI(&pParams->screenHandle, &pParams->mutex, &pParams->display_width, &pParams->display_height, fullScreenMode);

	DisplayMutex = pParams->mutex;

	loadGUI(pParams->screenHandle);

	GUISettingsWindow(arg);

	pSettings->switchChannelCbk(pParams->screenHandle, channel_name, 0);

	drawGUI(pParams->screenHandle, fullScreenMode, volume_level, pSettings->quality_level, pSettings->auto_quality, channel_name, 1);

	while(!keypress) 
	{
		pSettings->threadStatus = RUNNING_THREAD;

		guiStatus = RUNNING_THREAD;

		//while(SDL_PollEvent(&event)) 
		if(SDL_WaitEvent(&event))
		{     		
			switch (event.type) 
			{
				case SDL_QUIT:
					keypress = 1;
					pSettings->quitAppcbk();
				break;
				case SDL_KEYDOWN:
					//keypress = 1;
					//pSettings->quitAppcbk();
				break;

				case SDL_MOUSEBUTTONDOWN:

					for(i=0; i<NUM_BUTTONS; i++)
					{
						if(verifyButtonCoordinates(&Buttons[i], event.motion.x, event.motion.y))
						{
							drawButton(pParams->screenHandle, &Buttons[i], BUTTON_PRESS_STATE, 1);
						}
					}

				break;

				case SDL_MOUSEBUTTONUP:

					for(i=0; i<NUM_BUTTONS; i++)
					{
						if(verifyButtonCoordinates(&Buttons[i], event.motion.x, event.motion.y))
						{
							if(i == PREV_BUTTON)
							{
								if(pSettings->switchChannelCbk(pParams->screenHandle, channel_name, -1))
								{
									drawFont(pParams->screenHandle, MainFont[COMIC_SANS_FONT], channel_name,  (screen_width-100)/2+80, screen_height-GUI_PANEL_HEIGHT/2 - 45/2, 232, 232, 232, 0, 0, 0, 1);
								}
							}
							else if(i== NEXT_BUTTON)
							{
								if(pSettings->switchChannelCbk(pParams->screenHandle, channel_name, 1))
								{
									drawFont(pParams->screenHandle, MainFont[COMIC_SANS_FONT], channel_name,  (screen_width-100)/2+80, screen_height-GUI_PANEL_HEIGHT/2 - 45/2, 232, 232, 232, 0, 0, 0, 1);
								}
							}
							else if(i== FULLSCREEN_BUTTON)
							{
								if(fullScreenMode == 0)
									fullScreenMode = 1;
								else
									fullScreenMode = 0;

								SDL_LockMutex(DisplayMutex);	

								if(fullScreenMode == 0)
								{
									screen_width 	= SDL_DEF_WIDTH;
									screen_height 	= SDL_DEF_HEIGHT;
									sdl_video_flags 	= SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL;									
								}
								else
								{
									screen_width 	= max_screen_width;
									screen_height 	= max_screen_height;
									sdl_video_flags 	= SDL_HWSURFACE | SDL_ASYNCBLIT | SDL_HWACCEL | SDL_FULLSCREEN;
								}

								if (!(pParams->screenHandle = SDL_SetVideoMode(screen_width, screen_height, SDL_DEPTH, sdl_video_flags)))
								{
									SDL_Quit();
									return 1;
								}

								pParams->display_width 	= screen_width;
								pParams->display_height 	= screen_height-GUI_PANEL_HEIGHT;
								pParams->sdl_flags 		= sdl_video_flags;
								drawGUI(pParams->screenHandle, fullScreenMode, volume_level, pSettings->quality_level, pSettings->auto_quality, channel_name, 0);

								SDL_UnlockMutex(DisplayMutex);

							}
							else if(i == MINUS_BUTTON)
							{
								volume_level = volume_level-1;
								if(volume_level >= 0)
								{
									updateVolumeLevel(pParams->screenHandle, volume_level, 1);
									pSettings->changeVolumeCbk(volume_level);
								}
								else
								{
									volume_level = 0;
								}
							}
							else if(i == PLUS_BUTTON)
							{
								volume_level = volume_level+1;
								if(volume_level <= 8)
								{
									updateVolumeLevel(pParams->screenHandle, volume_level, 1);
									pSettings->changeVolumeCbk(volume_level);
								}
								else
								{
									volume_level = 8;
								}
							}
							else if(i == AUTO_UNSELECT_BUTTON)
							{
								if(*pSettings->auto_quality == 0)
								{
									*pSettings->quality_level = 3;
									*pSettings->auto_quality = 1;
								}
								else
								{
									*pSettings->quality_level = 3;
									*pSettings->auto_quality = 0;
								}

								updateQualityLevel(pParams->screenHandle, *pSettings->quality_level, *pSettings->auto_quality, 1);
							}
							else if(i == LOW_QUALITY_BUTTON)
							{
								if(*pSettings->auto_quality == 0)
								{
									if( (event.motion.x >= (Buttons[i].coordinates.x + 4)) && (event.motion.x <= (Buttons[i].coordinates.x + 16)) )
									{
										*pSettings->quality_level = 3;
										updateQualityLevel(pParams->screenHandle, *pSettings->quality_level, *pSettings->auto_quality, 1);
									}
									else if( (event.motion.x >= (Buttons[i].coordinates.x + 22)) && (event.motion.x <= (Buttons[i].coordinates.x + 34)) )
									{
										*pSettings->quality_level = 2;
										updateQualityLevel(pParams->screenHandle, *pSettings->quality_level, *pSettings->auto_quality, 1);
									}
									else if( (event.motion.x >= (Buttons[i].coordinates.x + 40)) && (event.motion.x <= (Buttons[i].coordinates.x + 52)) )
									{
										*pSettings->quality_level = 1;
										updateQualityLevel(pParams->screenHandle, *pSettings->quality_level, *pSettings->auto_quality, 1);
									}
									else if( (event.motion.x >= (Buttons[i].coordinates.x + 58)) && (event.motion.x <= (Buttons[i].coordinates.x + 70)) )
									{
										*pSettings->quality_level = 0;
										updateQualityLevel(pParams->screenHandle, *pSettings->quality_level, *pSettings->auto_quality, 1);
									}		
								}
							}

						}
					}
				break;

				case SDL_MOUSEMOTION:

					for(i=0; i<NUM_BUTTONS; i++)
					{
						if(verifyButtonCoordinates(&Buttons[i], event.motion.x, event.motion.y))
						{
							drawButton(pParams->screenHandle, &Buttons[i], BUTTON_HOVER_STATE, 1);

							//drawGUI(*pSettings->screenHandle, *fullScreenMode, *pSettings->volume_level, cnt, 1);
							//drawToolTip(*pSettings->screenHandle, i, 1);
						}
						else
						{
							drawButton(pParams->screenHandle, &Buttons[i], BUTTON_NORM_STATE, 1);
						}
					}

				break;
				
			}
		}

		//usleep(100);
	}

	pSettings->threadStatus = STOPPED_THREAD;

	guiStatus = STOPPED_THREAD;

	return 0;
}



