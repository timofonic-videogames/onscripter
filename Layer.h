/* -*- C++ -*-
 *
 *  Layer.h - Base class for effect layers
 *
 *  Added by Mion (Sonozaki Futago-tachi) Sep 2008
 *
 *  ogapee@aqua.dti2.ne.jp
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

#ifndef __LAYER_H__
#define __LAYER_H__

#include "AnimationInfo.h"

struct Layer
{
    AnimationInfo *sprite;
    int width, height;

    virtual ~Layer(){};
    
    void setSprite( AnimationInfo *anim ){ sprite = anim; };
    virtual void init() = 0;
    virtual void update( ) = 0;
    virtual void message( char *message ) = 0;
    virtual void refresh( SDL_Surface* surface, SDL_Rect clip ) = 0;
};

const int max_scratch_count = 6; // Number of scratches visible.
const int max_glow = 5; // Number of glow levels.

class Scratch {
private:
	int offs;   // Tint of the line: 64 for light, -64 for dark, 0 for no scratch.
	int x1, x2; // Horizontal position of top and bottom of the line.
	int dx;     // Distance by which the line moves each frame.
	int time;   // Number of frames remaining before reinitialisation.
	int width, height;
	void init(int &count);
public:
	Scratch() : offs(0), time(1) {}
	void setwindow(int w, int h){ width = w; height = h; }
	void update(int &count);
	void draw(SDL_Surface* surface, SDL_Rect clip);
};

class OldMovieLayer : public Layer
{
public:
    OldMovieLayer( int w, int h );
    void init();
    void update();
    void message( char *message );
    void refresh( SDL_Surface* surface, SDL_Rect clip );

private:
    int scratch_count; // Number of scratches visible.
    Scratch scratches[max_scratch_count];
    int rx, ry, // Offset of blur (second copy of background image)
        ns;     // Current noise surface
    bool initialized;
    SDL_Surface* NoiseSurface[10]; // We store 10 screens of random noise, and flip between them at random.
    SDL_Surface* GlowSurface;      // For the glow effect, we store a single surface with a scanline for each glow level.
    int gv, // Current glow level
        go; // Glow delta: flips between 1 and -1 to fade the glow in and out.

    void BlendOnSurface(SDL_Surface* src, SDL_Surface* dst, SDL_Rect clip);
};

#endif // __LAYER_H__
