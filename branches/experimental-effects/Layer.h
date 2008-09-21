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
    AnimationInfo *sprite_info, *sprite;
    int width, height;

    virtual ~Layer(){};
    
    void setSpriteInfo( AnimationInfo *sinfo, AnimationInfo *anim ){
        sprite_info = sinfo;
        sprite = anim;
    };
    virtual void init() = 0;
    virtual void update( ) = 0;
    virtual void message( const char *message ) = 0;
    virtual void refresh( SDL_Surface* surface, SDL_Rect clip ) = 0;
};

class OldMovieLayer : public Layer
{
public:
    OldMovieLayer( int w, int h );
    ~OldMovieLayer();
    void init();
    void update();
    void message( const char *message );
    void refresh( SDL_Surface* surface, SDL_Rect clip );

private:

    // message parameters
    int blur_level;
    int noise_level;
    int glow_level;
    int scratch_level;
    int dust_level;
    AnimationInfo *dust_sprite;
    AnimationInfo *dust;

    int rx, ry, // Offset of blur (second copy of background image)
        ns;     // Current noise surface
    bool initialized;
    int gv, // Current glow level
        go; // Glow delta: flips between 1 and -1 to fade the glow in and out.

    void om_init();
    void BlendOnSurface(SDL_Surface* src, SDL_Surface* dst, SDL_Rect clip);
};

#endif // __LAYER_H__
