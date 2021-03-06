
#include "gdb-dataplugins.h"
#include "SDL.h"

static void view_SDL_Surface(const void *ptr, const GDB_dataplugin_funcs *funcs)
{
    SDL_Surface surface;
    SDL_Palette palette;
    SDL_PixelFormat fmt;
    void *pixels = NULL;
    SDL_Color *colors = NULL;

    if (funcs->readmem(ptr, &surface, sizeof (SDL_Surface)) != 0) return;
    if (funcs->readmem(surface.format, &fmt, sizeof (SDL_PixelFormat)) != 0) return;

    pixels = funcs->allocmem(surface.pitch * surface.h);
    if (funcs->readmem(surface.pixels, pixels, surface.pitch * surface.h) != 0) return;

    if (fmt.palette)
    {
        if (funcs->readmem(fmt.palette, &palette, sizeof (SDL_Palette)) != 0) return;
        colors = funcs->allocmem(sizeof (SDL_Color) * palette.ncolors);
        funcs->readmem(palette.colors, colors, sizeof (SDL_Color) * palette.ncolors);
    }

    if (SDL_Init(SDL_INIT_VIDEO) == -1)
        funcs->warning("SDL_Init(SDL_INIT_VIDEO) failed: %s", SDL_GetError());
    else if (SDL_SetVideoMode(surface.w, surface.h, 0, 0) == 0)
    {
        funcs->warning("SDL_SetVideoMode() failed: %s", SDL_GetError());
        SDL_Quit();
    }
    else
    {
        SDL_WM_SetCaption("GDB visualization of SDL_Surface", "gdb-sdl-surface");
        SDL_Surface *img = SDL_CreateRGBSurfaceFrom(pixels, surface.w,
                                surface.h, fmt.BitsPerPixel, surface.pitch,
		                        fmt.Rmask, fmt.Gmask, fmt.Bmask, fmt.Amask);

        if (img == NULL)
            funcs->warning("SDL_CreateRGBSurfaceFrom() failed: %s", SDL_GetError());
        else
        {
            SDL_Event e;
            int keep_going = 1;

            if (fmt.palette)
                SDL_SetColors(img, colors, 0, palette.ncolors);

            SDL_BlitSurface(img, NULL, SDL_GetVideoSurface(), NULL);
            SDL_Flip(SDL_GetVideoSurface());

            while (keep_going)
            {
                while (SDL_PollEvent(&e))
                {
                    if (e.type == SDL_QUIT)
                        keep_going = 0;
                    else if ((e.type == SDL_KEYDOWN) && (e.key.keysym.sym == SDLK_ESCAPE))
                        keep_going = 0;
                    else if (e.type == SDL_VIDEOEXPOSE)
                    {
                        SDL_BlitSurface(img, NULL, SDL_GetVideoSurface(), NULL);
                        SDL_Flip(SDL_GetVideoSurface());
                    }
                }

                if (keep_going)
                    SDL_Delay(10);
            }

            SDL_FreeSurface(img);
        }

        SDL_Quit();
    }

    funcs->freemem(colors);
    funcs->freemem(pixels);
}


void GDB_DATAPLUGIN_ENTRY(const GDB_dataplugin_entry_funcs *funcs)
{
    funcs->register_viewer("SDL_Surface", view_SDL_Surface);
}

/* end of viewsdl.c ... */

