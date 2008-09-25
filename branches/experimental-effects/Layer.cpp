/* -*- C++ -*-
 * 
 *  Layer.cpp - Code for effect layers
 *
 *  Added by Mion (Sonozaki Futago-tachi) Sep 2008
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
#ifndef NO_LAYER_EFFECTS
#ifndef BPP16

#include "Layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define RMASK 0x00ff0000
#define GMASK 0x0000ff00
#define BMASK 0x000000ff
#define AMASK 0xff000000
#else
#define RMASK 0x0000ff00
#define GMASK 0x00ff0000
#define BMASK 0xff000000
#define AMASK 0x000000ff
#endif

#define MAX_SPRITE_NUM 1000

/*
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
 *  Modified extensively by Mion (Sonozaki Futago-tachi) 
 */

const int max_scratch_count = 6; // Number of scratches visible.
const int max_glow = 25; // Number of glow levels.
int scratch_count; // Number of scratches visible.

class Scratch {
private:
	int offs;   // Tint of the line: 64 for light, -64 for dark, 0 for no scratch.
	int x1, x2; // Horizontal position of top and bottom of the line.
	int dx;     // Distance by which the line moves each frame.
	int time;   // Number of frames remaining before reinitialisation.
	int width, height;
	void init(int &level);
public:
	Scratch() : offs(0), time(1) {}
	void setwindow(int w, int h){ width = w; height = h; }
	void update(int &level);
	void draw(SDL_Surface* surface, SDL_Rect clip);
};

	// Create a new scratch.
void Scratch::init(int &level)
{
	// If this scratch was visible, decrement the counter.
	if (offs) --scratch_count;
	// Each scratch object is reinitialised every 3-9 frames.
	time = rand() % 7 + 3;
	// Possibly create a visible scratch, if there aren't already 4.
//	if (scratch_count > 3 ? false : rand() % 3 == 1) {
	if (rand() % 600 < level) {
		++scratch_count;
		offs = rand() % 2 ? 64 : -64;
		x1 = rand() % (width - 20) + 10;
		dx = rand() % 12 - 6;
		x2 = x1 - dx; // The angle of the line is determined by the speed of motion.
	}
	else offs = 0;
}

// Called each frame.
void Scratch::update(int &level)
{
	if (--time == 0) 
		init(level);
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

Scratch scratches[max_scratch_count];
SDL_Surface* NoiseSurface[10]; // We store 10 screens of random noise, and flip between them at random.
SDL_Surface* GlowSurface;      // For the glow effect, we store a single surface with a scanline for each glow level.
int om_count = 0;
bool initialized_om_surfaces = false;

OldMovieLayer::OldMovieLayer( int w, int h )
{
    width = w;
    height = h;

    blur_level = noise_level = glow_level = scratch_level = dust_level = 0;
    dust_sprite = dust = NULL;
    dust_pts = NULL;

    initialized = false;
}

OldMovieLayer::~OldMovieLayer() {
    if (initialized) {
        --om_count;
        if (om_count == 0) {
            for (int i=0; i<10; i++)
                SDL_FreeSurface(NoiseSurface[i]);
            SDL_FreeSurface(GlowSurface);
            initialized_om_surfaces = false;
            if (dust) delete dust;
            if (dust_pts) delete[] dust_pts;
        }
    }
}

void OldMovieLayer::om_init()
{
    ++om_count;

    gv = 0;
    go = 1;
    rx = ry = 0;
    ns = 0;

    if (! initialized_om_surfaces ) {

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
				const int rm = (rand() % (noise_level + 1)) * 2;
				*row = 0 | (rm << 16) | (rm << 8) | rm;
			}
		}	
		SDL_UnlockSurface(NoiseSurface[i]);
	}
	
	// Generate max_glow scanlines of solid greyscale, used for the glow effect.
	GlowSurface = AnimationInfo::allocSurface(width, max_glow);
	for (SDL_Rect r = { 0, 0, width, 1 }; r.y < max_glow; r.y++) {
		const int ry = (r.y * 30 / max_glow) + 4;
		SDL_FillRect(GlowSurface, &r, SDL_MapRGB(GlowSurface->format, ry, ry, ry));
	}

        if (dust_sprite) {
            // Copy dust sprite to dust
            dust = new AnimationInfo(*dust_sprite);
            dust->visible = true;
        }
        if (dust_pts) delete[] dust_pts;
        dust_pts = new Pt[10];
    }

    initialized = true;
}

// Called once each frame.  Updates effect parameters.
void OldMovieLayer::update()
{
    if (initialized) {

	const int last_x = rx, last_y = ry, last_n = ns;
	// Generate blur offset and noise screen randomly.
	// Ensure neither setting is the same two frames running.
 	if (blur_level > 0) {
	    do {
		rx = rand() % (blur_level + 1) - 1;
		ry = rand() % (blur_level + 1);
	    } while (rx == last_x && ry == last_y);
	}
	do {
		ns = rand() % 10;
	} while (ns == last_n);
	// Increment glow; reverse direction if we've reached either limit.
	gv += go;
	if (gv >= 5) { gv = 3; go = -1; }
	if (gv < 0) { gv = 1; go = 1; }
	// Update scratches.
	for (int i = max_scratch_count-1; i >= 0; --i) scratches[i].update(scratch_level);

        // Update dust
        for (int i=9; i>=0; --i) {
            dust_pts[i].cell = rand() % (dust->num_of_cells);
            dust_pts[i].x = rand() % (width + 10) - 5;
            dust_pts[i].y = rand() % (height + 10) - 5;
        }
    }
}

char *OldMovieLayer::message( const char *message, int &ret_int )
{
    int sprite_no = 0;
    ret_int = 0;
    if (!sprite_info)
        return NULL;

    printf("OldMovieLayer: got message '%s'\n", message);
    if (sscanf(message, "s|%d,%d,%d,%d,%d,%d", 
               &blur_level, &noise_level, &glow_level, 
               &scratch_level, &dust_level, &sprite_no)) {
        if (blur_level < 0) blur_level = 0;
        else if (blur_level > 3) blur_level = 3;
        if (noise_level < 0) noise_level = 0;
        else if (noise_level > 24) noise_level = 24;
        if (glow_level < 0) glow_level = 0;
        else if (glow_level > 24) glow_level = 24;
        if (scratch_level < 0) scratch_level = 0;
        else if (scratch_level > 400) scratch_level = 400;
        if (dust_level < 0) dust_level = 0;
        else if (dust_level > 400) dust_level = 400;
        if ((sprite_no >= 0) && (sprite_no < MAX_SPRITE_NUM))
            dust_sprite = &sprite_info[sprite_no];
        om_init();
    }
    return NULL;
}

// We need something to use for those SDL_gfx image filter routines, for now
static void imageFilterMean(unsigned char *src1, unsigned char *src2, unsigned char *dst, int length)
{
    unsigned char *s1 = src1, *s2 = src2, *d = dst;
    int i = length;
    while (i--) {
        int result = ((int) *(s1++) + (int) *(s2++)) / 2;
        *(d++) = (unsigned char) result;
    }
}

static void imageFilterAdd(unsigned char *src1, unsigned char *src2, unsigned char *dst, int length)
{
    unsigned char *s1 = src1, *s2 = src2, *d = dst;
    int i = length;
    while (i--) {
        int result = (int) *(s1++) + (int) *(s2++);
        if (result & 0x0100)
            result = 255;
        *(d++) = (unsigned char) result;
    }
}

static void imageFilterSub(unsigned char *src1, unsigned char *src2, unsigned char *dst, int length)
{
    unsigned char *s1 = src1, *s2 = src2, *d = dst;
    int i = length;
    while (i--) {
        int result = (int) *(s1++) - (int) *(s2++);
        if (result < 0)
            result = 0;
        *(d++) = (unsigned char) result;
    }
}

// Apply blur effect by averaging two offset copies of a source surface together.
static void BlurOnSurface(SDL_Surface* src, SDL_Surface* dst, SDL_Rect clip, int rx, int ry, int width)
{
	// Calculate clipping bounds to avoid reading outside the source surface.
	const int srcx = clip.x - rx;
	const int srcy = clip.y - ry;
	const int length = (srcx + clip.w > width ? (width - srcx) : clip.w) * 4;
	int rows = clip.h;
	const int skipfirstrows = (srcy < 0) ? -srcy : 0;
	const int srcp = src->pitch;
	const int dstp = dst->pitch;
	
	SDL_LockSurface(src);
	SDL_LockSurface(dst);
	unsigned char* src1px = ((unsigned char*) src->pixels) + srcx * 4 + srcy * srcp;
	unsigned char* src2px = ((unsigned char*) src->pixels) + clip.x * 4 + clip.y * srcp;
	unsigned char* dstpx = ((unsigned char*) dst->pixels) + clip.x * 4 + clip.y * dstp;
	
	// If the vertical offset is positive, we are reading one copy from (x, -1), so we need to
	// skip the first scanline to avoid reading outside the source surface.
	for (int i=skipfirstrows; i; --i) {
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
	if (rx && (clip.x + clip.w >= width)) {
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
	
	// If we skipped the first scanlines, rectify that by copying directly from the source image.
	if (skipfirstrows) {
		clip.h = skipfirstrows;
		SDL_BlitSurface(src, &clip, dst, &clip);
	}
}

static void drawTaggedSurface( SDL_Surface *dst_surface, AnimationInfo *anim, SDL_Rect &clip )
{
        SDL_Rect poly_rect = anim->pos;

        if (!anim->affine_flag)
            anim->blendOnSurface( dst_surface, poly_rect.x, poly_rect.y,
                                  clip, anim->trans );
        else
            anim->blendOnSurface2( dst_surface, poly_rect.x, poly_rect.y,
                                   clip, anim->trans );
}

// Called every time the screen is refreshed.
// Draws the background image with the old-movie effect applied, using the settings adopted at the
// last call to updateOldMovie().
void OldMovieLayer::refresh(SDL_Surface *surface, SDL_Rect clip)
{
    if (initialized) {

	// Blur background.
	// If no offset is applied, we can just copy the given surface directly.
	// If an offset is present, we average the given surface with an offset version

	if (rx != 0 || ry != 0) {
        	SDL_BlitSurface(surface, &clip, sprite->image_surface, &clip);
		BlurOnSurface(sprite->image_surface, surface, clip, rx, ry, width);
        }

	// Add noise and glow.
	SDL_LockSurface(surface);
	SDL_LockSurface(NoiseSurface[ns]);
	SDL_LockSurface(GlowSurface);
	unsigned char* g = ((unsigned char*) GlowSurface->pixels) + (gv * glow_level / 4) * GlowSurface->pitch;
	const int sp = surface->pitch;
	if (clip.x == 0 && clip.y == 0 && clip.w == width && clip.h == height) {	
		// If no clipping rectangle is defined, we can apply the noise in one go.
		unsigned char* s = (unsigned char*) surface->pixels;
		if (noise_level > 0)
			imageFilterSub(s, (unsigned char*) NoiseSurface[ns]->pixels, s, sp * surface->h);
		// Since the glow is stored as a single scanline for each level, we always apply
		// the glow scanline by scanline.
		if (glow_level > 0)
			for (int i = height; i; --i, s += sp) imageFilterAdd(s, g, s, width * 4);
	}
	else {
		// Otherwise we do everything scanline by scanline.
		const int length = clip.w * 4;
		const int np = NoiseSurface[ns]->pitch;
		unsigned char* s = ((unsigned char*) surface->pixels) + clip.x * 4 + clip.y * sp;
		unsigned char* n = ((unsigned char*) NoiseSurface[ns]->pixels) + clip.x * 4 + clip.y * np;
		for (int i = clip.h; i; --i, s += sp, n += np) {
			if (noise_level > 0) imageFilterSub(s, n, s, length); // subtract noise
			if (glow_level > 0) imageFilterAdd(s, g, s, length); // add glow
		}
	}
	SDL_UnlockSurface(NoiseSurface[ns]);
	SDL_UnlockSurface(GlowSurface);

	// Add scratches.
	if (scratch_level > 0)
	    for (int i = 0; i < max_scratch_count; i++) scratches[i].draw(surface, clip);

	// Add dust specks.
	if (dust && dust_level > 0) {
	    for (int i=9; i>0; --i) {
		if ((rand() & 1023) < dust_level) {
		    dust->current_cell = dust_pts[i].cell;
		    dust->pos.x = dust_pts[i].x;
		    dust->pos.y = dust_pts[i].y;
                    drawTaggedSurface( surface, dust, clip );
		}
            }
        }

	// And we're done.
	SDL_UnlockSurface(surface);
    }
}


/* FuruLayer Code: for emulating snow.dll & hana.dll (falling stuff) */

FuruLayer::FuruLayer( int w, int h, bool animated, BaseReader *br )
{
    width = w;
    height = h;
    tumbling = animated;
    reader = br;

    interval = fall_velocity = wind = amplitude = period = 0;
    elements[0] = elements[1] = elements[2] = NULL;
    paused = halted = false;
    points = NULL;
    pstart = pend = 0;
    frame_count = 0;
    
    initialized = false;
}

FuruLayer::~FuruLayer(){
    if (elements[0]) delete elements[0];
    if (elements[1]) delete elements[1];
    if (elements[2]) delete elements[2];
    if (points) delete[] points;
}

void FuruLayer::furu_init()
{
    window_x = 0; window_w = width;
    if (points) delete[] points;
    points = new Pt3[512];
    pstart = pend = 0;
    halted = false;
    paused = false;

    initialized = true;
}

void FuruLayer::update()
{
    if (initialized && !paused) {
        if (tumbling) {
            for (int j=2; j>=0; --j) {
                int i = pstart;
                while (i != pend) {
                    points[i].elem[j].x += wind;
                    points[i].elem[j].y += fall_velocity;
                    points[i].elem[j].cell = (points[i].elem[j].cell + 1) %
                        elements[j]->num_of_cells; 
                    ++i &= 511;
                }
            }
        } else {
            for (int j=2; j>=0; --j) {
                int i = pstart;
                while (i != pend) {
                    points[i].elem[j].x += wind;
                    points[i].elem[j].y += fall_velocity;
                    ++i &= 511;
                }
            }
        }
        if (interval > 1)
            frame_count = (frame_count + 1) % interval;
        if (!halted && (frame_count == 0)) {
            int tmp = (pend + 1) & 511;
            if (tmp != pstart) {
                // add a point for each element
                for (int j=2; j>=0; --j) {
                    points[pend].elem[j].x = (rand() % window_w) + window_x;
                    points[pend].elem[j].y = -(elements[j]->pos.h);
                    points[pend].elem[j].type = j;
                    points[pend].elem[j].cell = 0;
                    pend = tmp;
                }
            }
        }
        if ((pstart != pend) && (points[pstart].elem[0].y > (height + 30)))
            ++pstart &= 511;
    }
}

static void setStr( char **dst, const char *src, int num=-1 )
{
    if ( *dst ) delete[] *dst;
    *dst = NULL;
    
    if ( src ){
        if (num >= 0){
            *dst = new char[ num + 1 ];
            memcpy( *dst, src, num );
            (*dst)[num] = '\0';
        }
        else{
            *dst = new char[ strlen( src ) + 1];
            strcpy( *dst, src );
        }
    }
}

static SDL_Surface *loadImage( char *file_name, bool *has_alpha, SDL_Surface *surface, BaseReader *br )
{
    if ( !file_name ) return NULL;
    unsigned long length = br->getFileLength( file_name );

    if ( length == 0 )
        return NULL;
    unsigned char *buffer = new unsigned char[length];
    int location;
    br->getFile( file_name, buffer, &location );
    SDL_Surface *tmp = IMG_Load_RW(SDL_RWFromMem( buffer, length ), 1);

    char *ext = strrchr(file_name, '.');
    if ( !tmp && ext && (!strcmp( ext+1, "JPG" ) || !strcmp( ext+1, "jpg" ) ) ){
        fprintf( stderr, " *** force-loading a JPG image [%s]\n", file_name );
        SDL_RWops *src = SDL_RWFromMem( buffer, length );
        tmp = IMG_LoadJPG_RW(src);
        SDL_RWclose(src);
    }
    if ( tmp && has_alpha ) *has_alpha = tmp->format->Amask;

    delete[] buffer;
    if ( !tmp ){
        fprintf( stderr, " *** can't load file [%s] ***\n", file_name );
        return NULL;
    }

    SDL_Surface *ret = SDL_ConvertSurface( tmp, surface->format, SDL_SWSURFACE );
    SDL_FreeSurface( tmp );
    return ret;
}

void FuruLayer::validate_params()
{
    int half_wx = width / 2;

    if (interval < 1) interval = 1;
    else if (interval > 10000) interval = 10000;
    if (fall_velocity < 1) fall_velocity = 1;
    else if (fall_velocity > height) fall_velocity = height;
    if (wind < -half_wx) wind = -half_wx;
    else if (wind > half_wx) wind = half_wx;
    if (amplitude < 0) amplitude = 0;
    else if (amplitude > half_wx) amplitude = half_wx;
    if (period < 0) period = 0;
    else if (period > 359) period = 359;

    //adjust the window based on wind & fall velocity
    if (wind < 0) {
        window_x = 0;
        window_w = width - wind * height / fall_velocity;
    } else if (wind == 0) {
        window_x = 0;
        window_w = width;
    } else {
        window_x = -(wind * height / fall_velocity);
        window_w = width + wind * height / fall_velocity;
    }
}


char *FuruLayer::message( const char *message, int &ret_int )
{
    int num_cells[3], tmp[5];
    char buf[3][128];

    char *ret_str = NULL;
    ret_int = 0;

    if (!sprite)
        return NULL;

    //printf("FuruLayer: got message '%s'\n", message);
//Image loading
    if (!strncmp(message, "i|", 2)) {
        if (tumbling) {
        // "Hana"
            if (sscanf(message, "i|%d,%d,%d,%d,%d,%d",
                       &tmp[0], &num_cells[0],
                       &tmp[1], &num_cells[1],
                       &tmp[2], &num_cells[2])) {
                for (int i=2; i>=0; --i) {
                    if (elements[i]) delete elements[i];
                    elements[i] = new AnimationInfo(sprite_info[tmp[i]]);
                    elements[i]->num_of_cells = num_cells[i];
                }
            } else
            if (sscanf(message, "i|%120[^,],%d,%120[^,],%d,%120[^,],%d",
                              &buf[0][0], &num_cells[0],
                              &buf[1][0], &num_cells[1],
                              &buf[2][0], &num_cells[2])) {
                for (int i=2; i>=0; --i) {
                    bool has_alpha = false;
                    SDL_Surface *img = loadImage( &buf[i][0], &has_alpha, sprite->image_surface, reader );
                    AnimationInfo *anim = new AnimationInfo();
                    anim->num_of_cells = num_cells[i];
                    anim->duration_list = new int[ anim->num_of_cells ];
                    for (int j=anim->num_of_cells - 1; j>=0; --j )
                        anim->duration_list[j] = 0;
                    anim->loop_mode = 3; // not animatable
                    anim->trans_mode = AnimationInfo::TRANS_TOPLEFT;
                    setStr( &anim->file_name, &buf[i][0] );
                    anim->setupImage(img, NULL, has_alpha);
                    if ( img ) SDL_FreeSurface(img);
                    if (elements[i]) delete elements[i];
                    elements[i] = anim;
                }
            }
        } else {
        // "Snow"
            if (sscanf(message, "i|%d,%d,%d", 
                       &tmp[0], &tmp[1], &tmp[2])) {
                for (int i=2; i>=0; --i) {
                    if (elements[i]) delete elements[i];
                    elements[i] = new AnimationInfo(sprite_info[tmp[i]]);
                }
            } else if (sscanf(message, "i|%[^,],%[^,],%[^,]",
                              &buf[0][0], &buf[1][0], &buf[2][0])) {
                for (int i=2; i>=0; --i) {
                    Uint32 firstpix = 0;
                    bool has_alpha = false;
                    SDL_Surface *img = loadImage( &buf[i][0], &has_alpha, sprite->image_surface, reader );
                    AnimationInfo *anim = new AnimationInfo();
                    anim->num_of_cells = 1;
                    SDL_LockSurface( img );
                    firstpix = *((Uint32*) img->pixels) & ~AMASK;
                    if (firstpix > 0) {
                        anim->trans_mode = AnimationInfo::TRANS_TOPLEFT;
                    } else {
                        // if first pix is black, this is an "additive" sprite
                        anim->trans_mode = AnimationInfo::TRANS_ADD;
                    }
                    SDL_UnlockSurface( img );
                    setStr( &anim->file_name, &buf[i][0] );
                    anim->setupImage(img, NULL, has_alpha);
                    if ( img ) SDL_FreeSurface(img);
                    if (elements[i]) delete elements[i];
                    elements[i] = anim;
                }
            }
        }
//Set Parameters
    } else if (sscanf(message, "s|%d,%d,%d,%d,%d", 
                      &interval, &fall_velocity, &wind, 
                      &amplitude, &period)) {
        furu_init();
        validate_params();
//Transition (adjust) Parameters
    } else if (sscanf(message, "t|%d,%d,%d,%d,%d", 
                      &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4])) {
        interval += tmp[0];
        fall_velocity += tmp[1];
        wind += tmp[2];
        amplitude += tmp[3];
        period += tmp[4];
        validate_params();
//Get Parameters
    } else if (!strcmp(message, "g")) {
        ret_int = paused ? 1 : 0;
        sprintf(&buf[0][0], "s|%d,%d,%d,%d,%d",
                interval, fall_velocity, wind, amplitude, period);
        setStr( &ret_str, &buf[0][0]);
//Halt adding new elements
    } else if (!strcmp(message, "h")) {
        halted = true;
//Get number of elements displayed
    } else if (!strcmp(message, "n")) {
        ret_int = ((pend - pstart) & 511) * 3;
//Pause
    } else if (!strcmp(message, "p")) {
        paused = true;
//Restart
    } else if (!strcmp(message, "r")) {
        paused = false;
    }
    return ret_str;
}

void FuruLayer::refresh(SDL_Surface *surface, SDL_Rect clip)
{
    if (initialized) {
        for (int i=pstart; i!=pend; ++i &= 511) {
            for (int j=2; j>=0; --j) {
                Pt curpt = points[i].elem[j];
                AnimationInfo *anim = elements[j];
                if (anim) {
                    anim->visible = true;
	            anim->current_cell = curpt.cell;
       	            anim->pos.x = curpt.x;
                    anim->pos.y = curpt.y;
                    drawTaggedSurface( surface, anim, clip );
                }
            }
        }
    }
}

#endif // ndef BPP16
#endif // ndef NO_LAYER_EFFECTS
