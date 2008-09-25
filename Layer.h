/* -*- C++ -*-
 *
 *  Layer.h - Base class for effect layers
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

#ifndef __LAYER_H__
#define __LAYER_H__

#include "AnimationInfo.h"
#include "BaseReader.h"

struct Pt { int x; int y; int type; int cell; };

struct Layer
{
    BaseReader *reader;
    AnimationInfo *sprite_info, *sprite;
    int width, height;

    virtual ~Layer(){};
    
    void setSpriteInfo( AnimationInfo *sinfo, AnimationInfo *anim ){
        sprite_info = sinfo;
        sprite = anim;
    };
    virtual void update( ) = 0;
    virtual char* message( const char *message, int &ret_int ) = 0;
    virtual void refresh( SDL_Surface* surface, SDL_Rect clip ) = 0;
};

class OldMovieLayer : public Layer
{
public:
    OldMovieLayer( int w, int h );
    ~OldMovieLayer();
    void update();
    char* message( const char *message, int &ret_int );
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

    Pt *dust_pts;
    int rx, ry, // Offset of blur (second copy of background image)
        ns;     // Current noise surface
    int gv, // Current glow level
        go; // Glow delta: flips between 1 and -1 to fade the glow in and out.
    bool initialized;

    void om_init();
    //void BlendOnSurface(SDL_Surface* src, SDL_Surface* dst, SDL_Rect clip);
};

class FuruLayer : public Layer
{
public:
    FuruLayer( int w, int h, bool animated, BaseReader *br=NULL );
    ~FuruLayer();
    void update();
    char* message( const char *message, int &ret_int );
    void refresh( SDL_Surface* surface, SDL_Rect clip );

private:
    bool tumbling; // true (hana) or false (snow)

    // message parameters
    int interval; // 1 ~ 10000; # frames between a new element release
    int fall_velocity; // 1 ~ screen_height; pix/frame
    int wind; // -screen_width/2 ~ screen_width/2; pix/frame 
    int amplitude; // 0 ~ screen_width/2; pix/frame
    int period; // 0 ~ 359; degree/frame
    AnimationInfo *elements[3];
    bool paused, halted;

    // rolling buffer
    struct Pt3 { Pt elem[3]; };
    Pt3 *points;
    int pstart, pend;

    int window_x, window_w;
    int frame_count;
    bool initialized;

    void furu_init();
    void validate_params();
};

#endif // __LAYER_H__
