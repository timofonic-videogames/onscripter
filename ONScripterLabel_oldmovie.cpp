/* -*- C++ -*-
 * 
 *  ONScripterLabel_oldmovie.cpp
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

#include "ONScripterLabel.h"
#include <SDL/SDL_imageFilter.h>
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

int count = 0; // Number of scratches visible. Range is 0..4.

class Scratch {
	int offs;   // Tint of the line: 64 for light, -64 for dark, 0 for no scratch.
	int x1, x2; // Horizontal position of top and bottom of the line.
	int dx;     // Distance by which the line moves each frame.
	int time;   // Number of frames remaining before reinitialisation.

	// Create a new scratch.
	void init()
	{
		// If this scratch was visible, decrement the counter.
		if (offs) --count;
		// Each scratch object is reinitialised every 3-9 frames.
		time = rand() % 7 + 3;
		// Possibly create a visible scratch, if there aren't already 4.
		if (count > 3 ? false : rand() % 3 == 1) {
			++count;
			offs = rand() % 2 ? 64 : -64;
			x1 = rand() % 620 + 10;
			dx = rand() % 12 - 6;
			x2 = x1 - dx; // The angle of the line is determined by the speed of motion.
		}
		else offs = 0;
	}

public:	
	Scratch() : offs(0), time(1) {}
	
	// Called each frame.
	void update()
	{
		if (--time == 0) 
			init();
		else if (offs) {
			x1 += dx;
			x2 += dx;
		}
	}
	
	// Called each time the screen is refreshed.  Draws a simple line, without antialiasing.
	void draw(SDL_Surface* surface, SDL_Rect clip) 
	{
		// If this scratch is visible and likely to pass through the updated rectangle, draw it.
		if (offs && (x1 >= clip.x || x2 >= clip.x) && (x1 < clip.x + clip.w || x2 < clip.x + clip.w)) {
			const int sp = surface->pitch;
			float dx = (float)(x2 - x1) / 480;
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
};

bool initialised = false;      // Whether the surfaces below have been created.
SDL_Surface* NoiseSurface[10]; // We store 10 screens of random noise, and flip between them at random.
SDL_Surface* GlowSurface;      // For the glow effect, we store a single surface with a scanline for each glow level.

const int scratch_count = 6;
Scratch scratches[scratch_count];

int rx, ry, // Offset of blur (second copy of background image)
    ns;     // Current noise surface
int gv = 0, // Current glow level
    go = 1; // Glow delta: flips between 1 and -1 to fade the glow in and out.

const int max_glow = 5; // Number of glow levels.

// Create surfaces
void om_init()
{
	// Generate 10 screens of random noise.
	for (int i = 0; i < 10; ++i) {
		NoiseSurface[i] = AnimationInfo::allocSurface(640, 480);
		SDL_LockSurface(NoiseSurface[i]);
		char* px = (char*) NoiseSurface[i]->pixels;
		const int pt = NoiseSurface[i]->pitch;
		for (int y = 0; y < 480; ++y, px += pt) {
			Uint32* row = (Uint32*) px;
			for (int x = 0; x < 640; ++x, ++row) {
				const int rm = (rand() % 6) * 6;
				*row = 0 | (rm << 16) | (rm << 8) | rm;
			}
		}	
		SDL_UnlockSurface(NoiseSurface[i]);
	}
	
	// Generate 5 scanlines of solid greyscale, used for the glow effect.
	GlowSurface = AnimationInfo::allocSurface(640, max_glow);
	for (SDL_Rect r = { 0, 0, 640, 1 }; r.y < max_glow; ++r.y) {
		const int ry = r.y * (26 / max_glow) + 4;
		SDL_FillRect(GlowSurface, &r, SDL_MapRGB(GlowSurface->format, ry, ry, ry));
	}
	
	initialised = true;
}

// Called once each frame.  Updates effect parameters.
void ONScripterLabel::updateOldMovie()
{
	SDL_Rect r = { 0, 0, screen_width, screen_height };
	if (!(event_mode & EFFECT_EVENT_MODE)) { // Freeze effect while fading screen in and out.
		// Create surfaces if necessary.
		if (!initialised) om_init();
		// Only update parameters if the old-movie effect is active.
		// In Hallucinate, that's indicated by the existence of sprite 120.
		if (sprite_info[120].image_surface && sprite_info[120].visible) {
			const int last_x = rx, last_y = ry, last_n = ns;
			// Generate blur offset and noise screen randomly.
			// Ensure neither setting is the same two frames running.
			do {
				rx = rand() % 2 - 1;
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
			for (int i = 0; i < scratch_count; ++i) scratches[i].update();
			// Refresh entire screen.
			flushDirect(r, refreshMode() | (draw_cursor_flag ? REFRESH_CURSOR_MODE : 0));
		}
	}
	return;
}

// Apply blur effect by averaging two offset copies of a source surface together.
void BlendOnSurface(SDL_Surface* src, SDL_Surface* dst, SDL_Rect clip)
{
	// Calculate clipping bounds to avoid reading outside the source surface.
	const int srcx = clip.x - rx;
	const int srcy = clip.y - ry;
	const int length = (srcx + clip.w > 640 ? clip.w - 1 : clip.w) * 4;
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
		SDL_imageFilterMean(src1px, src2px, dstpx, length);
		src1px += srcp;
		src2px += srcp;
		dstpx += dstp;
	}
	
	// If the horizontal offset is -1, the rightmost column has not been written to.
	// Rectify that by copying it directly from the source image.
	if (rx && clip.x + clip.w >= 640) {
		Uint32* r = ((Uint32*) src->pixels) + 639 + clip.y * 640;
		Uint32* d = ((Uint32*) dst->pixels) + 639 + clip.y * 640;
		while (clip.h--) {
			*d = *r;
			d += 640;
			r += 640;
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
void ONScripterLabel::refreshOldMovie(SDL_Surface* surface, SDL_Rect clip)
{
	// Blur background.
	// If no offset is applied, we can just copy the background image directly.
	// If an offset is present, we average the image and an offset version using BlendOnSurface() above.
	if (rx == 0 && ry == 0)
		SDL_BlitSurface(bg_info.image_surface, &clip, surface, &clip);
	else
		BlendOnSurface(bg_info.image_surface, surface, clip);
	
	// Add noise and glow.
	SDL_LockSurface(surface);
	SDL_LockSurface(NoiseSurface[ns]);
	SDL_LockSurface(GlowSurface);
	unsigned char* g = ((unsigned char*) GlowSurface->pixels) + gv * GlowSurface->pitch;
	const int sp = surface->pitch;
	if (clip.x == 0 && clip.y == 0 && clip.w == 640 && clip.h == 480) {	
		// If no clipping rectangle is defined, we can apply the noise in one go.
		unsigned char* s = (unsigned char*) surface->pixels;
		SDL_imageFilterSub(s, (unsigned char*) NoiseSurface[ns]->pixels, s, sp * surface->h);
		// Since the glow is stored as a single scanline for each level, we always apply
		// the glow scanline by scanline.
		for (int i = 0; i < 480; ++i, s += sp) SDL_imageFilterAdd(s, g, s, 640 * 4);
	}
	else {
		// Otherwise we do everything scanline by scanline.
		const int length = clip.w * 4;
		const int np = NoiseSurface[ns]->pitch;
		unsigned char* s = ((unsigned char*) surface->pixels) + clip.x * 4 + clip.y * sp;
		unsigned char* n = ((unsigned char*) NoiseSurface[ns]->pixels) + clip.x * 4 + clip.y * np;
		for (int i = clip.h; i; --i, s += sp, n += np) {
			SDL_imageFilterSub(s, n, s, length); // subtract noise
			SDL_imageFilterAdd(s, g, s, length); // add glow
		}
	}
	SDL_UnlockSurface(NoiseSurface[ns]);
	SDL_UnlockSurface(GlowSurface);
	
	// Add scratches.
	for (int i = 0; i < scratch_count; ++i) scratches[i].draw(surface, clip);

	// And we're done.
	SDL_UnlockSurface(surface);
}
