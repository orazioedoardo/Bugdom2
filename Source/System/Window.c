/****************************/
/*        WINDOW            */
/* By Brian Greenstone      */
/* (c)2002 Pangea Software  */
/* (c)2023 Iliyas Jorio     */
/****************************/


/***************/
/* EXTERNALS   */
/***************/

#include	"game.h"

#if __EMSCRIPTEN__
#include <emscripten.h>
#endif


/****************************/
/*    PROTOTYPES            */
/****************************/


/****************************/
/*    CONSTANTS             */
/****************************/


/**********************/
/*     VARIABLES      */
/**********************/

int				gGameWindowWidth, gGameWindowHeight;


/****************  INIT WINDOW STUFF *******************/

void InitWindowStuff(void)
{
	gGameWindowWidth = 640;
	gGameWindowHeight = 480;
	SDL_GetWindowSizeInPixels(gSDLWindow, &gGameWindowWidth, &gGameWindowHeight);
}



#pragma mark -


/************************** ENTER 2D *************************/

void Enter2D(void)
{
	GrabMouse(false);
	SDL_ShowCursor();
	MyFlushEvents();
}


/************************** EXIT 2D *************************/

void Exit2D(void)
{
}



#pragma mark -


/******************** GET DEFAULT WINDOW SIZE *******************/

void GetDefaultWindowSize(SDL_DisplayID display, int* width, int* height)
{
	const float aspectRatio = 16.0 / 9.0f;
	const float screenCoverage = .8f;

	SDL_Rect displayBounds = { .x = 0, .y = 0, .w = 640, .h = 480 };
	SDL_GetDisplayUsableBounds(display, &displayBounds);

	if (displayBounds.w > displayBounds.h)
	{
		*width = displayBounds.h * screenCoverage * aspectRatio;
		*height = displayBounds.h * screenCoverage;
	}
	else
	{
		*width = displayBounds.w * screenCoverage;
		*height = displayBounds.w * screenCoverage / aspectRatio;
	}
}

/******************** GET NUM DISPLAYS *******************/

int GetNumDisplays(void)
{
	int numDisplays = 0;
	SDL_DisplayID* displays = SDL_GetDisplays(&numDisplays);
	SDL_free(displays);
	return numDisplays;
}

/******************** MOVE WINDOW TO PREFERRED DISPLAY *******************/
//
// This works best in windowed mode.
// Turn off fullscreen before calling this!
//

void MoveToPreferredDisplay(void)
{
#if __EMSCRIPTEN__
	return;		// the web page sizes the canvas
#endif

	if (gGamePrefs.displayNumMinus1 >= GetNumDisplays())
	{
		gGamePrefs.displayNumMinus1 = 0;
	}

	SDL_DisplayID display = gGamePrefs.displayNumMinus1 + 1;

	int w = 640;
	int h = 480;
	GetDefaultWindowSize(display, &w, &h);
	SDL_SetWindowSize(gSDLWindow, w, h);
	SDL_SyncWindow(gSDLWindow);

	int centered = SDL_WINDOWPOS_CENTERED_DISPLAY(display);
	SDL_SetWindowPosition(gSDLWindow, centered, centered);
	SDL_SyncWindow(gSDLWindow);
}

/*********************** SET FULLSCREEN MODE **********************/

void SetFullscreenMode(bool enforceDisplayPref)
{
#if __EMSCRIPTEN__
	// The web page handles fullscreen (Cmd+F); SDL's fullscreen would take over the whole page
	(void) enforceDisplayPref;
	return;
#endif

	if (!gGamePrefs.fullscreen)
	{
		SDL_SetWindowFullscreen(gSDLWindow, 0);
		SDL_SyncWindow(gSDLWindow);

		if (enforceDisplayPref)
		{
			MoveToPreferredDisplay();
		}
	}
	else
	{
		if (enforceDisplayPref)
		{
			SDL_DisplayID currentDisplay = SDL_GetDisplayForWindow(gSDLWindow);
			SDL_DisplayID desiredDisplay = gGamePrefs.displayNumMinus1 + 1;

			if (currentDisplay != desiredDisplay)
			{
				// We must switch back to windowed mode for the preferred monitor to take effect
				SDL_SetWindowFullscreen(gSDLWindow, false);
				SDL_SyncWindow(gSDLWindow);
				MoveToPreferredDisplay();
			}
		}

		// Enter fullscreen mode
		SDL_SetWindowFullscreen(gSDLWindow, true);
		SDL_SyncWindow(gSDLWindow);
	}

	SDL_GL_SetSwapInterval(gGamePrefs.vsync);
}


/******************** SWAP GAME WINDOW *********************/
//
// Presents the frame that was just drawn.
//

#if __EMSCRIPTEN__
double		gAnimationFrameTime = 0;		// timestamp (ms) of the latest animation frame
uint32_t	gAnimationFrameCount = 0;		// number of animation frames waited for so far

// Resumes on the browser's next animation frame (requires asyncify).
// Returns that frame's timestamp, which is aligned to the display's refresh.
EM_ASYNC_JS(double, WaitForAnimationFrame, (void), {
	return await new Promise(requestAnimationFrame);
});
#endif

void SwapGameWindow(void)
{
#if __EMSCRIPTEN__
	// The browser presents WebGL frames by itself once we yield to it. Yield until the next
	// animation frame rather than calling SDL_GL_SwapWindow (which yields with a 0-ms timeout),
	// so that the game runs in lockstep with the display, like it does natively with vsync.
	gAnimationFrameTime = WaitForAnimationFrame();
	gAnimationFrameCount++;
#else
	SDL_GL_SwapWindow(gSDLWindow);
#endif
}
