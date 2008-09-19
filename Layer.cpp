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

	// Create a new scratch.
void Scratch::init(int &count)
{
	// If this scratch was visible, decrement the counter.
	if (offs) --count;
	// Each scratch object is reinitialised every 3-9 frames.
	time = rand() % 7 + 3;
	// Possibly create a visible scratch, if there aren't already 4.
	if (count > 3 ? false : rand() % 3 == 1) {
		++count;
		offs = rand() % 2 ? 64 : -64;
		x1 = rand() % (width - 20) + 10;
		dx = rand() % 12 - 6;
		x2 = x1 - dx; // The angle of the line is determined by the speed of motion.
	}
	else offs = 0;
}

	
// Called each frame.
void Scratch::update(int &count)
{
	if (--time == 0) 
		init(count);
	else if (offs) {
		x1 += dx;
		x2 += dx;
	}
}
	
// Called each time the screen is refreshed.  Draws a simple line, without antialiasing.
void Scratch::draw(SDL_Surface* surface, SDL_Rect clip) 
{
	// If this scratch is visible and likely to pass through the updated rectangle, draw it.
	if (offs && (x1 >= clip.x || x2 >= clip.x) && (x1 < clip.x + clip.w || x2 < clip.x + clip.w)) {
		const int sp = surface->pitch;
		float dx = (float)(x2 - x1) / width;
		float realx = (float) x1;
		int y = 0;
		while (y != clip.y) { ++y; realx += dx; } // Skip all scanlines above the clipping rectangle.
		while (y < clip.y + clip.h) {
			int lx = (int) floor(realx + 0.5);
			if (lx >= clip.x && lx < clip.x + clip.w) { // Only draw within the clipping rectangle.
				// Get pixel...
				Uint32* p = (Uint32*)((char*)surface->pixels + y * sp + lx * 4);
				const Uint32 c = *p;
				// ...add to or subtract from its colour...
				int c1 = (c & 0xff) + offs, c2 = ((c >> 8) & 0xff) + offs, c3 = ((c >> 16) & 0xff) + offs;
				if (c1 < 0) c1 = 0; else if (c1 > 255) c1 = 255;
				if (c2 < 0) c2 = 0; else if (c2 > 255) c2 = 255;
				if (c3 < 0) c3 = 0; else if (c3 > 255) c3 = 255;
				// ...and put it back.
				*p = c1 | c2 << 8 | c3 << 16;
			}
			++y;
			realx += dx;
		}
	}
}

OldMovieLayer::OldMovieLayer( int w, int h )
{
    width = w;
    height = h;
    
    init();
}

void OldMovieLayer::init()
{
    gv = 0;
    go = 1;

    // set up scratches
    for (int i = 0; i < max_scratch_count; i++)
        scratches[i].setwindow(width, height);
    
	// Generate 10 screens of random noise.
	for (int i = 0; i < 10; ++i) {
		NoiseSurface[i] = AnimationInfo::allocSurface(width, height);
		SDL_LockSurface(NoiseSurface[i]);
		char* px = (char*) NoiseSurface[i]->pixels;
		const int pt = NoiseSurface[i]->pitch;
		for (int y = 0; y < height; ++y, px += pt) {
			Uint32* row = (Uint32*) px;
			for (int x = 0; x < width; ++x, ++row) {
				const int rm = (rand() % 6) * 6;
				*row = 0 | (rm << 16) | (rm << 8) | rm;
			}
		}	
		SDL_UnlockSurface(NoiseSurface[i]);
	}
	
	// Generate 5 scanlines of solid greyscale, used for the glow effect.
	GlowSurface = AnimationInfo::allocSurface(width, max_glow);
	for (SDL_Rect r = { 0, 0, width, 1 }; r.y < max_glow; ++r.y) {
		const int ry = r.y * (26 / max_glow) + 4;
		SDL_FillRect(GlowSurface, &r, SDL_MapRGB(GlowSurface->format, ry, ry, ry));
	}

    initialized = true;
}

// Called once each frame.  Updates effect parameters.
void OldMovieLayer::update()
{
	const int last_x = rx, last_y = ry, last_n = ns;
	// Generate blur offset and noise screen randomly.
	// Ensure neither setting is the same two frames running.
	do {
		rx = rand() % 4 - 2;
		ry = rand() % 2;
	} while (rx == last_x && ry == last_y);
	do {
		ns = rand() % 10;
	} while (ns == last_n);
	// Increment glow; reverse direction if we've reached either limit.
	gv += go;
	if (gv == max_glow) { gv = max_glow - 2; go = -1; }
	if (gv == -1) { gv = 1; go = 1; }
	// Update scratches.
	for (int i = 0; i < max_scratch_count; i++) scratches[i].update(scratch_count);
}

void OldMovieLayer::message( char *message )
{
}

// We need something to use for those SDL_gfx image filter routines, for now
void imageFilterMean(unsigned char *src1, unsigned char *src2, unsigned char *dst, int length)
{
    unsigned char *s1 = src1, *s2 = src2, *d = dst;
    int i = length;

    while (i--) {
        *(d++) = (*(s1++) / 2) + (*(s2++) / 2);
    }
}

void imageFilterAdd(unsigned char *src1, unsigned char *src2, unsigned char *dst, int length)
{
    unsigned char *s1 = src1, *s2 = src2, *d = dst;
    int i = length;
    while (i--) {
        *d = (*(s1++) / 2) + (*(s2++) / 2);
        if (*d >= 128)
            *d++ = 255;
        else
            *d++ *= 2;
    }
}

void imageFilterSub(unsigned char *src1, unsigned char *src2, unsigned char *dst, int length)
{
    unsigned char *s1 = src1, *s2 = src2, *d = dst;
    int i = length;
    while (i--) {
        if (*s1 < *s2)
            *d++ = 0;
        else
            *d++ = *s1 - *s2;
        ++s1; ++s2;
    }
}

// Apply blur effect by averaging two offset copies of a source surface together.
void OldMovieLayer::BlendOnSurface(SDL_Surface* src, SDL_Surface* dst, SDL_Rect clip)
{
	// Calculate clipping bounds to avoid reading outside the source surface.
	const int srcx = clip.x - rx;
	const int srcy = clip.y - ry;
	const int length = (srcx + clip.w > width ? (width - srcx) : clip.w) * 4;
	int rows = clip.h;
	const bool skipfirstrow = srcy < 0;
	const int srcp = src->pitch;
	const int dstp = dst->pitch;
	
	SDL_LockSurface(src);
	SDL_LockSurface(dst);
	unsigned char* src1px = ((unsigned char*) src->pixels) + srcx * 4 + srcy * srcp;
	unsigned char* src2px = ((unsigned char*) src->pixels) + clip.x * 4 + clip.y * srcp;
	unsigned char* dstpx = ((unsigned char*) dst->pixels) + clip.x * 4 + clip.y * dstp;
	
	// If the vertical offset is 1, we are reading one copy from (x, -1), so we need to
	// skip the first scanline to avoid reading outside the source surface.
	if (skipfirstrow) {
		--rows;
		src1px += srcp;
		src2px += srcp;
		dstpx += dstp;
	}
	
	// Blend the remaining scanlines.
	while (rows--) {
		imageFilterMean(src1px, src2px, dstpx, length);
		src1px += srcp;
		src2px += srcp;
		dstpx += dstp;
	}
	
	// If the horizontal offset is -1, the rightmost column has not been written to.
	// Rectify that by copying it directly from the source image.
	if (rx && clip.x + clip.w >= width) {
		Uint32* r = ((Uint32*) src->pixels) + (width - 1) + clip.y * width;
		Uint32* d = ((Uint32*) dst->pixels) + (width - 1) + clip.y * width;
		while (clip.h--) {
			*d = *r;
			d += width;
			r += width;
		}
	}
	
	SDL_UnlockSurface(src);
	SDL_UnlockSurface(dst);
	
	// If we skipped the first scanline, rectify that by copying it directly from the source image.
	if (skipfirstrow) {
		clip.h = 1;
		SDL_BlitSurface(src, &clip, dst, &clip);
	}
}

// Called every time the screen is refreshed.
// Draws the background image with the old-movie effect applied, using the settings adopted at the
// last call to updateOldMovie().
void OldMovieLayer::refresh(SDL_Surface *surface, SDL_Rect clip)
{
	SDL_BlitSurface(surface, &clip, sprite->image_surface, &clip);

	// Blur background.
	// If no offset is applied, we can just copy the given surface directly.
	// If an offset is present, we average the given surface with an offset version using BlendOnSurface() above.
	if (rx != 0 || ry != 0) {
		BlendOnSurface(sprite->image_surface, surface, clip);
        }

	// Add noise and glow.
	SDL_LockSurface(surface);
	SDL_LockSurface(NoiseSurface[ns]);
	SDL_LockSurface(GlowSurface);
	unsigned char* g = ((unsigned char*) GlowSurface->pixels) + gv * GlowSurface->pitch;
	const int sp = surface->pitch;
	if (clip.x == 0 && clip.y == 0 && clip.w == width && clip.h == height) {	
		// If no clipping rectangle is defined, we can apply the noise in one go.
		unsigned char* s = (unsigned char*) surface->pixels;
		imageFilterSub(s, (unsigned char*) NoiseSurface[ns]->pixels, s, sp * surface->h);
		// Since the glow is stored as a single scanline for each level, we always apply
		// the glow scanline by scanline.
		for (int i = 0; i < height; ++i, s += sp) imageFilterAdd(s, g, s, width * 4);
	}
	else {
		// Otherwise we do everything scanline by scanline.
		const int length = clip.w * 4;
		const int np = NoiseSurface[ns]->pitch;
		unsigned char* s = ((unsigned char*) surface->pixels) + clip.x * 4 + clip.y * sp;
		unsigned char* n = ((unsigned char*) NoiseSurface[ns]->pixels) + clip.x * 4 + clip.y * np;
		for (int i = clip.h; i; --i, s += sp, n += np) {
			imageFilterSub(s, n, s, length); // subtract noise
			imageFilterAdd(s, g, s, length); // add glow
		}
	}
	SDL_UnlockSurface(NoiseSurface[ns]);
	SDL_UnlockSurface(GlowSurface);

	// Add scratches.
	for (int i = 0; i < max_scratch_count; i++) scratches[i].draw(surface, clip);

	// And we're done.
	SDL_UnlockSurface(surface);
}
