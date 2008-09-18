/* -*- C++ -*-
 * 
 *  Layer.cpp
 *  Emulation of Takashi Toyama's "oldmovie.dll" NScripter filter for ONScripter.
 *
 *  To get frequent enough updates, rather than implement this as a sprite, we
 *  modify the event loop to update the old-movie effect every 120ms, and modify
 *  the screen refresh function to use these routines rather than the regular
 *  background drawing system when the old-movie effect is active.
 *
 *  The old-movie effect itself is created in a rather inefficient way, compared
 *  to Toyama-san's tight ASM original, but we have to run on PowerPC as well. ^^
 *  Using SDL_gfx's MMX-optimised composition routines should claw back some of
 *  the lost performance on x86 systems.
 *
 *  Copyright (c) 2006 Peter Jolly.
 *
 *  haeleth@haeleth.net
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef BPP16
#error "The old movie effect is only defined for 32BPP builds."
#endif

#define RMASK 0x00ff0000
#define GMASK 0x0000ff00
#define BMASK 0x000000ff
#define AMASK 0xff000000

OldMovieLayer::OldMovieLayer( int w, int h )
{
    width = w;
    height = h;
    init();
}

void OldMovieLayer::init()
{
    initialized = true;
}

// Called once each frame.  Updates effect parameters.
void OldMovieLayer::update()
{
	// Create surfaces if necessary.
	if (!initialized) init();
	const int last_x = rx, last_y = ry;
	// Generate blur offset and noise screen randomly.
	// Ensure neither setting is the same two frames running.
	do {
		rx = rand() % 2 - 1;
		ry = rand() % 2;
	} while (rx == last_x && ry == last_y);
}

void OldMovieLayer::message( char *message )
{
}

// Apply blur effect by averaging two offset copies of a source surface together.
void OldMovieLayer::BlendOnSurface(SDL_Surface* src, SDL_Surface* dst, SDL_Rect clip)
{
        SDL_Rect clip2 = clip, clip3 = clip;

	//SDL_BlitSurface(src, &clip, dst, &clip);

        //SDL_SetAlpha(src, SDL_SRCALPHA, 128);
	clip2.x += rx;
	clip2.w -= rx;
	clip3.w -= rx;
	clip2.y += ry;
	clip2.h -= ry;
	clip3.h -= ry;
	SDL_BlitSurface(src, &clip2, dst, &clip3);
	//SDL_SetAlpha(src, SDL_SRCALPHA, 255);
}

// Called every time the screen is refreshed.
// Draws the background image with the old-movie effect applied, using the settings adopted at the
// last call to updateOldMovie().
void OldMovieLayer::refresh(SDL_Surface *surface, SDL_Rect clip)
{
        SDL_Surface *tmp = surface;
        surface = sprite->image_surface;
        sprite->image_surface = tmp;

	// Blur background.
	// If no offset is applied, we can just copy the given surface directly.
	// If an offset is present, we average the given surface with an offset version using BlendOnSurface() above.
	if (rx != 0 || ry != 0) {
  	        //SDL_BlitSurface(surface, &clip, sprite->image_surface, &clip);
		BlendOnSurface(sprite->image_surface, surface, clip);
        }
}
