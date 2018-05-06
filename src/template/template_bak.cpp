// Template, major revision 6, update for INFOMOV
// IGAD/NHTV/UU - Jacco Bikker - 2006-2016

// Note:
// this version of the template uses SDL2 for all frame buffer interaction
// see: https://www.libsdl.org
// for glm (OpenGL mathematics) see http://glm.g-truc.net

#pragma warning (disable : 4530) // complaint about exception handler
#pragma warning (disable : 4273)
#pragma warning (disable : 4311) // pointer truncation from HANDLE to long

#include <vector>

#include "template.h"
#include <SDL2/SDL.h>
#include "../game.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_sdl_gl3.h"

#ifdef _WIN32
#include <io.h>
#include <Windows.h>
#include <fcntl.h>
#endif

using namespace Tmpl8;

namespace Tmpl8 { 

void NotifyUser( char* s )
{
#ifdef _WIN32
	HWND hApp = FindWindow( NULL, "Template" );
	MessageBox( hApp, s, "ERROR", MB_OK );
	//exit( 0 );
#else
    std::cout << "ERROR: " << s << std::endl;
#endif
}
}

static int SCRPITCH = 0;
int ACTWIDTH, ACTHEIGHT;
static bool FULLSCREEN = false, firstframe = true;

Surface* surface = 0;
Game* game = 0;
float lastftime = 0;
//LARGE_INTEGER lasttime, ticksPS;
Timer delayTimer;

void delay()
{
	delayTimer.reset();
	while (delayTimer.elapsed() < 0.005f) {};
}

void redirectIO()
{
// Windows specific code that is not needed anyway on OS X (and Linux?)
#ifdef _WIN32
	static const WORD MAX_CONSOLE_LINES = 500;
	int hConHandle;
	HANDLE lStdHandle;
	CONSOLE_SCREEN_BUFFER_INFO coninfo;
	FILE *fp;
	AllocConsole();
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),
	&coninfo);
	coninfo.dwSize.Y = MAX_CONSOLE_LINES;
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE),
	coninfo.dwSize);
	lStdHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	hConHandle = _open_osfhandle((intptr_t)lStdHandle, _O_TEXT);
	fp = _fdopen( hConHandle, "w" );
	*stdout = *fp;
	setvbuf( stdout, NULL, _IONBF, 0 );
	lStdHandle = GetStdHandle(STD_INPUT_HANDLE);
	hConHandle = _open_osfhandle((intptr_t)lStdHandle, _O_TEXT);
	fp = _fdopen( hConHandle, "r" );
	*stdin = *fp;
	setvbuf( stdin, NULL, _IONBF, 0 );
	lStdHandle = GetStdHandle(STD_ERROR_HANDLE);
	hConHandle = _open_osfhandle((intptr_t)lStdHandle, _O_TEXT);
	fp = _fdopen( hConHandle, "w" );
	*stderr = *fp;
	setvbuf( stderr, NULL, _IONBF, 0 );
	std::ios::sync_with_stdio();
	freopen( "CON", "w", stdout );
	freopen( "CON", "w", stderr );
#endif
}

int main( int argc, char *argv[] ) 
{
#ifdef OPENCL
	//redirectIO();
	printf("application started.\n");
	SDL_Init(SDL_INIT_VIDEO);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_Window* window = SDL_CreateWindow("Template", 100, 100, SCRWIDTH, SCRHEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);

	SDL_GLContext context = SDL_GL_CreateContext(window);
	if (context == NULL)
	{
		std::cout << "Could not create OpenGL context!" << std::endl;
		exit(EXIT_FAILURE);
	}
	glewInit();

	ImGui_ImplSdlGL3_Init(window);

	int exitapp = 0;
	auto timer = Timer();
	game = new Game();
	bool relativeMouse = true;
	SDL_SetRelativeMouseMode(relativeMouse ? SDL_TRUE : SDL_FALSE);
	while (!exitapp)
	{
		if (firstframe)
		{
			game->Init();
			firstframe = false;
		}

		game->KeysUpdate(SDL_GetKeyboardState(NULL));

		// event loop
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:
				exitapp = 1;
				break;
			case SDL_CONTROLLERAXISMOTION:
				game->AxisEvent(event.caxis.axis, (float)event.caxis.value / 32767);
				break;
			case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_ESCAPE)
				{
					exitapp = 1;
					// find other keys here: http://sdl.beuc.net/sdl.wiki/SDLKey
				}
				else if (event.key.keysym.sym == SDLK_LALT)
				{
					relativeMouse = !relativeMouse;
					SDL_SetRelativeMouseMode(relativeMouse ? SDL_TRUE : SDL_FALSE);
				}
				game->KeyDown(event.key.keysym.scancode);
				break;
			case SDL_KEYUP:
				game->KeyUp(event.key.keysym.scancode);
				break;
			case SDL_MOUSEMOTION:
				if (relativeMouse)
					game->MouseMove(event.motion.xrel, event.motion.yrel);
				break;
			case SDL_MOUSEBUTTONUP:
				game->MouseUp(event.button.button);
				break;
			case SDL_MOUSEBUTTONDOWN:
				game->MouseDown(event.button.button);
				break;
			default:
				break;
			}
		}

		ImGui_ImplSdlGL3_NewFrame(window);

		// calculate frame time and pass it to game->Tick
		game->Tick(lastftime);
		game->UpdateGui();
		lastftime = timer.elapsed();
		timer.reset();

		ImGui::Render();

		//std::cout << "Frame time: " << lastftime * 1000 << "ms" << std::endl;
		SDL_GL_SwapWindow(window);
#ifdef __APPLE__// For some reason SDL_SetRelativeMouseMode doesnt capture mouse correctly on macOS (Sierra)
		SDL_WarpMouseInWindow(window, SCRWIDTH/2, SCRHEIGHT/2);
#endif
	}
	game->Shutdown();
	ImGui_ImplSdlGL3_Shutdown();
	SDL_GL_DeleteContext(context);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 1;
#else
	redirectIO();
	printf( "application started.\n" );
	SDL_Init( SDL_INIT_VIDEO );
	surface = new Surface( SCRWIDTH, SCRHEIGHT );
	surface->Clear( 0 );
	surface->InitCharset();
	SDL_Window* window = SDL_CreateWindow( "Template", 100, 100, SCRWIDTH, SCRHEIGHT, SDL_WINDOW_SHOWN );
	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC );
	SDL_Texture* frameBuffer = SDL_CreateTexture( renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCRWIDTH, SCRHEIGHT );
	int exitapp = 0;
	auto timer = Timer();
	game = new Game();
	game->SetTarget( surface );
	SDL_SetRelativeMouseMode(SDL_TRUE);
	while (!exitapp) 
	{
		void* target = 0;
		int pitch;
		SDL_LockTexture( frameBuffer, NULL, &target, &pitch );
		if (pitch == (surface->GetWidth() * 4))
		{
			memcpy( target, surface->GetBuffer(), SCRWIDTH * SCRHEIGHT * 4 );
		}
		else
		{
			unsigned char* t = (unsigned char*)target;
			for( int i = 0; i < SCRHEIGHT; i++ )
			{
				memcpy( t, surface->GetBuffer() + i * SCRWIDTH, SCRWIDTH * 4 );
				t += pitch;
			}
		}
		SDL_UnlockTexture( frameBuffer );
		SDL_RenderCopy( renderer, frameBuffer, NULL, NULL );
		SDL_RenderPresent( renderer );
		if (firstframe)
		{
			game->Init();
			firstframe = false;
		}

		game->KeysUpdate(SDL_GetKeyboardState(NULL));

		// event loop
		SDL_Event event;
		while (SDL_PollEvent( &event )) 
		{
			switch (event.type)
			{
			case SDL_QUIT:
				exitapp = 1;
				break;
			case SDL_CONTROLLERAXISMOTION:
				game->AxisEvent(event.caxis.axis, (float) event.caxis.value / 32767);
				break;
			case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_ESCAPE) 
				{
					exitapp = 1;
					// find other keys here: http://sdl.beuc.net/sdl.wiki/SDLKey
				}
				game->KeyDown( event.key.keysym.scancode );
				break;
			case SDL_KEYUP:
				game->KeyUp( event.key.keysym.scancode );
				break;
			case SDL_MOUSEMOTION:
				game->MouseMove( event.motion.xrel, event.motion.yrel );
				break;
			case SDL_MOUSEBUTTONUP:
				game->MouseUp( event.button.button );
				break;
			case SDL_MOUSEBUTTONDOWN:
				game->MouseDown( event.button.button );
				break;
			default:
				break;
			}
		}

		// calculate frame time and pass it to game->Tick
		game->Tick( lastftime );
		lastftime = timer.elapsed();
		timer.reset();
		//std::cout << "Frame time: " << lastftime * 1000 << "ms" << std::endl;

#ifdef __APPLE__// For some reason SDL_SetRelativeMouseMode doesnt capture mouse correctly on macOS (Sierra)
		SDL_WarpMouseInWindow(window, SCRWIDTH/2, SCRHEIGHT/2);
#endif
	}
	game->Shutdown();
	SDL_Quit();
	return 1;
#endif
}