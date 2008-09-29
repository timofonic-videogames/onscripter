/* -*- C++ -*-
 *
 *  ONScripterLabel_effect.cpp - Effect executer of ONScripter
 *
 *  Copyright (c) 2001-2008 Ogapee. All rights reserved.
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

#include "ONScripterLabel.h"

#define EFFECT_STRIPE_WIDTH (16 * screen_ratio1 / screen_ratio2)
#define EFFECT_STRIPE_CURTAIN_WIDTH (24 * screen_ratio1 / screen_ratio2)
#define EFFECT_QUAKE_AMP (12 * screen_ratio1 / screen_ratio2)

int ONScripterLabel::setEffect( EffectLink *effect, int effect_dst, bool update_backup_surface )
{
    if ( effect->effect == 0 ) return RET_CONTINUE;

    if (update_backup_surface)
        refreshSurface(backup_surface, &dirty_rect.bounding_box, REFRESH_NORMAL_MODE);
    
    int effect_no = effect->effect;
    if ( effect_cut_flag && skip_mode & SKIP_NORMAL ) effect_no = 1;

    SDL_BlitSurface( accumulation_surface, NULL, effect_src_surface, NULL );
        
    switch( effect_dst ){
      case EFFECT_DST_GIVEN:
        break;
            
      case EFFECT_DST_GENERATE:
        int refresh_mode = refreshMode();
        if (update_backup_surface && refresh_mode == REFRESH_NORMAL_MODE){
            SDL_BlitSurface( backup_surface, &dirty_rect.bounding_box, effect_dst_surface, &dirty_rect.bounding_box );
        }
        else{
            if (effect_no == 1)
                refreshSurface( effect_dst_surface, &dirty_rect.bounding_box, refresh_mode );
            else
                refreshSurface( effect_dst_surface, NULL, refresh_mode );
        }
        break;
    }
    
    /* Load mask image */
    if ( effect_no == 15 || effect_no == 18 ){
        if ( !effect->anim.image_surface ){
            parseTaggedString( &effect->anim );
            setupAnimationInfo( &effect->anim );
        }
    }
    if ( effect_no == 11 || effect_no == 12 || effect_no == 13 || effect_no == 14 ||
         effect_no == 16 || effect_no == 17 )
        dirty_rect.fill( screen_width, screen_height );

    effect_counter = 0;
    event_mode = EFFECT_EVENT_MODE;
    advancePhase();

    return RET_WAIT | RET_REREAD;
}

int ONScripterLabel::doEffect( EffectLink *effect, bool clear_dirty_region )
{
#ifdef INSANI
    int prevduration = effect->duration;
    if ( ctrl_pressed_status || skip_mode & SKIP_TO_WAIT ) effect->duration = 1;
#endif
    effect_start_time = SDL_GetTicks();
    if ( effect_counter == 0 ) effect_start_time_old = effect_start_time - 1;

    effect_timer_resolution = effect_start_time - effect_start_time_old;
    effect_start_time_old = effect_start_time;

    int effect_no = effect->effect;
    if ( effect_cut_flag && skip_mode & SKIP_NORMAL ) effect_no = 1;

    int i;
    int width, width2;
    int height, height2;
    SDL_Rect src_rect={0, 0, screen_width, screen_height};
    SDL_Rect dst_rect={0, 0, screen_width, screen_height};

    /* ---------------------------------------- */
    /* Execute effect */
    //printf("Effect number %d %d\n", effect_no, effect->duration );

    switch ( effect_no ){
      case 0: // Instant display
      case 1: // Instant display
        //drawEffect( &src_rect, &src_rect, effect_dst_surface );
        break;

      case 2: // Left shutter
        width = EFFECT_STRIPE_WIDTH * effect_counter / effect->duration;
        for ( i=0 ; i<screen_width/EFFECT_STRIPE_WIDTH ; i++ ){
            src_rect.x = i * EFFECT_STRIPE_WIDTH;
            src_rect.y = 0;
            src_rect.w = width;
            src_rect.h = screen_height;
            drawEffect(&src_rect, &src_rect, effect_dst_surface);
        }
        break;

      case 3: // Right shutter
        width = EFFECT_STRIPE_WIDTH * effect_counter / effect->duration;
        for ( i=1 ; i<=screen_width/EFFECT_STRIPE_WIDTH ; i++ ){
            src_rect.x = i * EFFECT_STRIPE_WIDTH - width - 1;
            src_rect.y = 0;
            src_rect.w = width;
            src_rect.h = screen_height;
            drawEffect(&src_rect, &src_rect, effect_dst_surface);
        }
        break;

      case 4: // Top shutter
        height = EFFECT_STRIPE_WIDTH * effect_counter / effect->duration;
        for ( i=0 ; i<screen_height/EFFECT_STRIPE_WIDTH ; i++ ){
            src_rect.x = 0;
            src_rect.y = i * EFFECT_STRIPE_WIDTH;
            src_rect.w = screen_width;
            src_rect.h = height;
            drawEffect(&src_rect, &src_rect, effect_dst_surface);
        }
        break;

      case 5: // Bottom shutter
        height = EFFECT_STRIPE_WIDTH * effect_counter / effect->duration;
        for ( i=1 ; i<=screen_height/EFFECT_STRIPE_WIDTH ; i++ ){
            src_rect.x = 0;
            src_rect.y = i * EFFECT_STRIPE_WIDTH - height - 1;
            src_rect.w = screen_width;
            src_rect.h = height;
            drawEffect(&src_rect, &src_rect, effect_dst_surface);
        }
        break;

      case 6: // Left curtain
        width = EFFECT_STRIPE_CURTAIN_WIDTH * effect_counter * 2 / effect->duration;
        for ( i=0 ; i<=screen_width/EFFECT_STRIPE_CURTAIN_WIDTH ; i++ ){
            width2 = width - EFFECT_STRIPE_CURTAIN_WIDTH * EFFECT_STRIPE_CURTAIN_WIDTH * i / screen_width;
            if ( width2 >= 0 ){
                src_rect.x = i * EFFECT_STRIPE_CURTAIN_WIDTH;
                src_rect.y = 0;
                src_rect.w = width2;
                src_rect.h = screen_height;
                drawEffect(&src_rect, &src_rect, effect_dst_surface);
            }
        }
        break;

      case 7: // Right curtain
        width = EFFECT_STRIPE_CURTAIN_WIDTH * effect_counter * 2 / effect->duration;
        for ( i=0 ; i<=screen_width/EFFECT_STRIPE_CURTAIN_WIDTH ; i++ ){
            width2 = width - EFFECT_STRIPE_CURTAIN_WIDTH * EFFECT_STRIPE_CURTAIN_WIDTH * i / screen_width;
            if ( width2 >= 0 ){
                if ( width2 > EFFECT_STRIPE_CURTAIN_WIDTH ) width2 = EFFECT_STRIPE_CURTAIN_WIDTH;
                src_rect.x = screen_width - i * EFFECT_STRIPE_CURTAIN_WIDTH - width2;
                src_rect.y = 0;
                src_rect.w = width2;
                src_rect.h = screen_height;
                drawEffect(&src_rect, &src_rect, effect_dst_surface);
            }
        }
        break;

      case 8: // Top curtain
        height = EFFECT_STRIPE_CURTAIN_WIDTH * effect_counter * 2 / effect->duration;
        for ( i=0 ; i<=screen_height/EFFECT_STRIPE_CURTAIN_WIDTH ; i++ ){
            height2 = height - EFFECT_STRIPE_CURTAIN_WIDTH * EFFECT_STRIPE_CURTAIN_WIDTH * i / screen_height;
            if ( height2 >= 0 ){
                src_rect.x = 0;
                src_rect.y = i * EFFECT_STRIPE_CURTAIN_WIDTH;
                src_rect.w = screen_width;
                src_rect.h = height2;
                drawEffect(&src_rect, &src_rect, effect_dst_surface);
            }
        }
        break;

      case 9: // Bottom curtain
        height = EFFECT_STRIPE_CURTAIN_WIDTH * effect_counter * 2 / effect->duration;
        for ( i=0 ; i<=screen_height/EFFECT_STRIPE_CURTAIN_WIDTH ; i++ ){
            height2 = height - EFFECT_STRIPE_CURTAIN_WIDTH * EFFECT_STRIPE_CURTAIN_WIDTH * i / screen_height;
            if ( height2 >= 0 ){
                src_rect.x = 0;
                src_rect.y = screen_height - i * EFFECT_STRIPE_CURTAIN_WIDTH - height2;
                src_rect.w = screen_width;
                src_rect.h = height2;
                drawEffect(&src_rect, &src_rect, effect_dst_surface);
            }
        }
        break;

      default:
        //printf("effect No. %d is not implemented. Crossfade is substituted for that.\n",effect_no);

      case 10: // Cross fade
        height = 256 * effect_counter / effect->duration;
        alphaBlend( NULL, ALPHA_BLEND_CONST, height, &dirty_rect.bounding_box );
        break;

      case 11: // Left scroll
        width = screen_width * effect_counter / effect->duration;
        src_rect.x = 0;
        dst_rect.x = width;
        src_rect.y = dst_rect.y = 0;
        src_rect.w = dst_rect.w = screen_width - width;
        src_rect.h = dst_rect.h = screen_height;
        drawEffect(&dst_rect, &src_rect, effect_src_surface);

        src_rect.x = screen_width - width - 1;
        dst_rect.x = 0;
        src_rect.y = dst_rect.y = 0;
        src_rect.w = dst_rect.w = width;
        src_rect.h = dst_rect.h = screen_height;
        drawEffect(&dst_rect, &src_rect, effect_dst_surface);
        break;

      case 12: // Right scroll
        width = screen_width * effect_counter / effect->duration;
        src_rect.x = width;
        dst_rect.x = 0;
        src_rect.y = dst_rect.y = 0;
        src_rect.w = dst_rect.w = screen_width - width;
        src_rect.h = dst_rect.h = screen_height;
        drawEffect(&dst_rect, &src_rect, effect_src_surface);

        src_rect.x = 0;
        dst_rect.x = screen_width - width - 1;
        src_rect.y = dst_rect.y = 0;
        src_rect.w = dst_rect.w = width;
        src_rect.h = dst_rect.h = screen_height;
        drawEffect(&dst_rect, &src_rect, effect_dst_surface);
        break;

      case 13: // Top scroll
        width = screen_height * effect_counter / effect->duration;
        src_rect.x = dst_rect.x = 0;
        src_rect.y = 0;
        dst_rect.y = width;
        src_rect.w = dst_rect.w = screen_width;
        src_rect.h = dst_rect.h = screen_height - width;
        drawEffect(&dst_rect, &src_rect, effect_src_surface);

        src_rect.x = dst_rect.x = 0;
        src_rect.y = screen_height - width - 1;
        dst_rect.y = 0;
        src_rect.w = dst_rect.w = screen_width;
        src_rect.h = dst_rect.h = width;
        drawEffect(&dst_rect, &src_rect, effect_dst_surface);
        break;

      case 14: // Bottom scroll
        width = screen_height * effect_counter / effect->duration;
        src_rect.x = dst_rect.x = 0;
        src_rect.y = width;
        dst_rect.y = 0;
        src_rect.w = dst_rect.w = screen_width;
        src_rect.h = dst_rect.h = screen_height - width;
        drawEffect(&dst_rect, &src_rect, effect_src_surface);

        src_rect.x = dst_rect.x = 0;
        src_rect.y = 0;
        dst_rect.y = screen_height - width - 1;
        src_rect.w = dst_rect.w = screen_width;
        src_rect.h = dst_rect.h = width;
        drawEffect(&dst_rect, &src_rect, effect_dst_surface);
        break;

      case 15: // Fade with mask
        alphaBlend( effect->anim.image_surface, ALPHA_BLEND_FADE_MASK, 256 * effect_counter / effect->duration, &dirty_rect.bounding_box );
        break;

      case 16: // Mosaic out
        generateMosaic( effect_src_surface, 5 - 6 * effect_counter / effect->duration );
        break;

      case 17: // Mosaic in
        generateMosaic( effect_dst_surface, 6 * effect_counter / effect->duration );
        break;

      case 18: // Cross fade with mask
        alphaBlend( effect->anim.image_surface, ALPHA_BLEND_CROSSFADE_MASK, 256 * effect_counter * 2 / effect->duration, &dirty_rect.bounding_box );
        break;

      case (CUSTOM_EFFECT_NO + 0 ): // quakey
        if ( effect_timer_resolution > effect->duration / 4 / effect->no )
            effect_timer_resolution = effect->duration / 4 / effect->no;
        dst_rect.x = 0;
        dst_rect.y = (Sint16)(sin(M_PI * 2.0 * effect->no * effect_counter / effect->duration) *
                              EFFECT_QUAKE_AMP * effect->no * (effect->duration -  effect_counter) / effect->duration);
        SDL_FillRect( accumulation_surface, NULL, SDL_MapRGBA( accumulation_surface->format, 0, 0, 0, 0xff ) );
        drawEffect(&dst_rect, &src_rect, effect_dst_surface);
        break;

      case (CUSTOM_EFFECT_NO + 1 ): // quakex
        if ( effect_timer_resolution > effect->duration / 4 / effect->no )
            effect_timer_resolution = effect->duration / 4 / effect->no;
        dst_rect.x = (Sint16)(sin(M_PI * 2.0 * effect->no * effect_counter / effect->duration) *
                              EFFECT_QUAKE_AMP * effect->no * (effect->duration -  effect_counter) / effect->duration);
        dst_rect.y = 0;
        drawEffect(&dst_rect, &src_rect, effect_dst_surface);
        break;

      case (CUSTOM_EFFECT_NO + 2 ): // quake
        dst_rect.x = effect->no*((int)(3.0*rand()/(RAND_MAX+1.0)) - 1) * 2;
        dst_rect.y = effect->no*((int)(3.0*rand()/(RAND_MAX+1.0)) - 1) * 2;
        SDL_FillRect( accumulation_surface, NULL, SDL_MapRGBA( accumulation_surface->format, 0, 0, 0, 0xff ) );
        drawEffect(&dst_rect, &src_rect, effect_dst_surface);
        break;

      case (CUSTOM_EFFECT_NO - 1): // dll-based
        char *dll = effect->anim.image_name;
        char *params = dll;
        while (*params != 0) {
            if (*params == '/') {
                *params++ = 0;
                break;
            }
            params++;
        }
        //printf("dll effect: Got dll '%s', params '%s'\n", dll, params);
        if (!strcmp(dll, "cascade.dll")) {
            effectCascade(params, effect->duration);
        } else if (!strcmp(dll, "trvswave.dll")) {
            effectTrvswave(params, effect->duration);
        } else if (!strcmp(dll, "whirl.dll")) {
            effectWhirl(params, effect->duration);
        } else {
            // do crossfade
            height = 256 * effect_counter / effect->duration;
            alphaBlend( NULL, ALPHA_BLEND_CONST, height, &dirty_rect.bounding_box );
        }
        break;
    }

    //printf("effect conut %d / dur %d\n", effect_counter, effect->duration);

    int drawduration = SDL_GetTicks() - effect_start_time;
    if (drawduration < 17)
        SDL_Delay(17 - drawduration);
    effect_counter += effect_timer_resolution;
    if ( effect_counter < effect->duration && effect_no != 1 ){
        if ( effect_no != 0 ) flush( REFRESH_NONE_MODE, NULL, false );
#ifdef INSANI
	effect->duration = prevduration;
#endif
        return RET_WAIT | RET_REREAD;
    }
    else {
        SDL_BlitSurface(effect_dst_surface,   &dirty_rect.bounding_box,
		        accumulation_surface, &dirty_rect.bounding_box);
	
        if (effect_no)
	    flush(REFRESH_NONE_MODE, NULL, clear_dirty_region);
        if (effect_no == 1)
	    effect_counter = 0;
#ifdef INSANI
	effect->duration = prevduration;
#endif
        event_mode = IDLE_EVENT_MODE;
        display_mode &= ~DISPLAY_MODE_UPDATED;

        return RET_CONTINUE;
    }
}

void ONScripterLabel::drawEffect(SDL_Rect *dst_rect, SDL_Rect *src_rect, SDL_Surface *surface)
{
    SDL_Rect clipped_rect;
    if (AnimationInfo::doClipping(dst_rect, &dirty_rect.bounding_box, &clipped_rect)) return;
    if (src_rect != dst_rect){
        src_rect->x += clipped_rect.x;
        src_rect->y += clipped_rect.y;
        src_rect->w = clipped_rect.w;
        src_rect->h = clipped_rect.h;
    }

    SDL_BlitSurface(surface, src_rect, accumulation_surface, dst_rect);
}

void ONScripterLabel::generateMosaic( SDL_Surface *src_surface, int level )
{
    int i, j, ii, jj;
    int width = 160;
    for ( i=0 ; i<level ; i++ ) width >>= 1;

#ifdef BPP16
    int total_width = accumulation_surface->pitch / 2;
#else
    int total_width = accumulation_surface->pitch / 4;
#endif
    SDL_LockSurface( src_surface );
    SDL_LockSurface( accumulation_surface );
    ONSBuf *src_buffer = (ONSBuf *)src_surface->pixels;

    for ( i=screen_height-1 ; i>=0 ; i-=width ){
        for ( j=0 ; j<screen_width ; j+=width ){
            ONSBuf p = src_buffer[ i*total_width+j ];
            ONSBuf *dst_buffer = (ONSBuf *)accumulation_surface->pixels + i*total_width + j;

            int height2 = width;
            if (i+1-width < 0) height2 = i+1;
            int width2 = width;
            if (j+width > screen_width) width2 = screen_width - j;
            for ( ii=0 ; ii<height2 ; ii++ ){
                for ( jj=0 ; jj<width2 ; jj++ ){
                    *dst_buffer++ = p;
                }
                dst_buffer -= total_width + width2;
            }
        }
    }

    SDL_UnlockSurface( accumulation_surface );
    SDL_UnlockSurface( src_surface );
}

void ONScripterLabel::effectCascade( char *params, int duration )
{
#define CASCADE_DIR   1
#define CASCADE_LR    2
#define CASCADE_UP    0
#define CASCADE_DOWN  1
#define CASCADE_LEFT  2
#define CASCADE_RIGHT 3
#define CASCADE_CROSS 4
#define CASCADE_IN    8

    SDL_Surface *src_surface, *dst_surface;
    SDL_Rect src_rect={0, 0, screen_width, screen_height};
    SDL_Rect dst_rect={0, 0, screen_width, screen_height};
    int mode, width, start, end;
 
    if (params[0] == 'u')
        mode = CASCADE_UP;
    else if (params[0] == 'd')
        mode = CASCADE_DOWN;
    else if (params[0] == 'r')
        mode = CASCADE_RIGHT;
    else
        mode = CASCADE_LEFT;

    if (params[1] == 'i')
        mode |= CASCADE_IN;
    else if (params[1] == 'x')
        mode |= CASCADE_IN | CASCADE_CROSS;
    
    if (mode & CASCADE_IN)
        src_surface = effect_dst_surface;
    else
        src_surface = effect_src_surface;
    if (mode & CASCADE_CROSS)
        dst_surface = effect_tmp_surface;
    else
        dst_surface = accumulation_surface;

    if (effect_counter == 0)
        effect_tmp = 0;

    if (mode & CASCADE_LR) {
        // moves left-right
        width = screen_width * effect_counter / duration;
        if (!(mode & CASCADE_IN))
            width = screen_width - width;
            
        src_rect.y = dst_rect.y = 0;
        src_rect.h = dst_rect.h = screen_height;
        src_rect.w = dst_rect.w = 1;
        if ((mode & CASCADE_CROSS) && (width > 0)) {
            // need to cascade-out the src
            if (mode & CASCADE_DIR) {
                // moves right
                start = 0;
                end = width;
                dst_rect.x = end;
            } else {
                // moves left
                start = screen_width - width;
                end = screen_width;
                dst_rect.x = start;
            }
            src_rect.x = 0;
            SDL_BlitSurface(effect_src_surface, &dst_rect, accumulation_surface, &src_rect);
            for (int i=start; i<end; i++) {
                dst_rect.x = i;
                SDL_BlitSurface(accumulation_surface, &src_rect, effect_src_surface, &dst_rect);
            }
        }
        if (mode & CASCADE_DIR) {
            // moves right
            start = width;
            end = screen_width;
            src_rect.x = start;
        } else {
            // moves left
            start = 0;
            end = screen_width - width;
            src_rect.x = end;
        }
        for (int i=start; i<end; i++) {
            dst_rect.x = i;
            SDL_BlitSurface(src_surface, &src_rect, dst_surface, &dst_rect);
        }
        if ((mode & CASCADE_IN) && (width > 0)) {
            if (mode & CASCADE_DIR)
                src_rect.x = effect_tmp;
            else
                src_rect.x = screen_width - width;
            dst_rect.x = src_rect.x;
            src_rect.w = dst_rect.w = width - effect_tmp;
            SDL_BlitSurface(src_surface, &src_rect, dst_surface, &dst_rect);
            effect_tmp = width;
        }
    } else {
        // moves up-down
        width = screen_height * effect_counter / duration;
        if (!(mode & CASCADE_IN))
            width = screen_height - width;

        src_rect.x = dst_rect.x = 0;
        src_rect.h = dst_rect.h = 1;
        src_rect.w = dst_rect.w = screen_width;
        if ((mode & CASCADE_CROSS) && (width > 0)) {
            // need to cascade-out the src
            if (mode & CASCADE_DIR) {
                // moves down
                start = 0;
                end = width;
                dst_rect.y = end;
            } else {
                // moves up
                start = screen_height - width;
                end = screen_height;
                dst_rect.y = start;
            }
            src_rect.y = 0;
            SDL_BlitSurface(effect_src_surface, &dst_rect, accumulation_surface, &src_rect);
            for (int i=start; i<end; i++) {
                dst_rect.y = i;
                SDL_BlitSurface(accumulation_surface, &src_rect, effect_src_surface, &dst_rect);
            }
        }
        if (mode & CASCADE_DIR) {
            // moves down
            start = width;
            end = screen_height;
            src_rect.y = start;
        } else {
            // moves up
            start = 0;
            end = screen_height - width;
            src_rect.y = end;
        }
        for (int i=start; i<end; i++) {
            dst_rect.y = i;
            SDL_BlitSurface(src_surface, &src_rect, dst_surface, &dst_rect);
        }
        if ((mode & CASCADE_IN) && (width > 0)) {
            if (mode & CASCADE_DIR)
                src_rect.y = effect_tmp;
            else
                src_rect.y = screen_height - width;
            dst_rect.y = src_rect.y;
            src_rect.h = dst_rect.h = width - effect_tmp;
            SDL_BlitSurface(src_surface, &src_rect, dst_surface, &dst_rect);
            effect_tmp = width;
        }
    }
    if (mode & CASCADE_CROSS) {
        // do crossfade
        width = 256 * effect_counter / duration;
        alphaBlend( NULL, ALPHA_BLEND_CONST, width, &dirty_rect.bounding_box, NULL, dst_surface );
    }
}

void ONScripterLabel::effectTrvswave( char *params, int duration )
{
#define TRVSWAVE_AMPLITUDE   9
#define TRVSWAVE_WVLEN_START 256
#define TRVSWAVE_WVLEN_END   32

    SDL_Rect src_rect={0, 0, screen_width, 1};
    SDL_Rect dst_rect={0, 0, screen_width, 1};
    int ampl, wvlen;
    int width = 256 * effect_counter / duration;
    alphaBlend( NULL, ALPHA_BLEND_CONST, width, &dirty_rect.bounding_box );
    if (effect_counter * 2 < duration) {
        ampl = TRVSWAVE_AMPLITUDE * 2 * effect_counter / duration;
        wvlen = (Sint16)(1.0/(((1.0/TRVSWAVE_WVLEN_END - 1.0/TRVSWAVE_WVLEN_START) * 2 * effect_counter / duration) + (1.0/TRVSWAVE_WVLEN_START)));
    } else {
        ampl = TRVSWAVE_AMPLITUDE * 2 * (duration - effect_counter) / duration;
        wvlen = (Sint16)(1.0/(((1.0/TRVSWAVE_WVLEN_END - 1.0/TRVSWAVE_WVLEN_START) * 2 * (duration - effect_counter) / duration) + (1.0/TRVSWAVE_WVLEN_START)));
    }
    SDL_FillRect( effect_tmp_surface, NULL, SDL_MapRGBA( effect_tmp_surface->format, 0, 0, 0, 0xff ) );
    int y_offset = -screen_height / 2;
    for (int i=0; i<screen_height; i++) {
        src_rect.y = dst_rect.y = i;
        dst_rect.x = (Sint16)(ampl * sin(M_PI * 2.0 * y_offset / wvlen));
        SDL_BlitSurface(accumulation_surface, &src_rect, effect_tmp_surface, &dst_rect);
        ++y_offset;
    }
    SDL_Surface *tmp = effect_tmp_surface;
    effect_tmp_surface = accumulation_surface;
    accumulation_surface = tmp;
}

void ONScripterLabel::effectWhirl( char *params, int duration )
{
#define OMEGA (M_PI / 64)
#define CENTER_X ((screen_width-1)/2)
#define CENTER_Y ((screen_height-1)/2)

    int direction = (params[0] == 'r') ? -1 : 1;

    float t = (float) effect_counter * M_PI / (duration * 2);
    float one_minus_cos = 1 - cos(t);
    float rad_amp = M_PI * (sin(t) - one_minus_cos);
    float rad_base = M_PI * 2 * one_minus_cos + rad_amp;

    int width = 256 * effect_counter / duration;
    alphaBlend( NULL, ALPHA_BLEND_CONST, width, &dirty_rect.bounding_box,
                NULL, NULL, effect_tmp_surface );

    SDL_LockSurface( effect_tmp_surface );
    SDL_LockSurface( accumulation_surface );
    ONSBuf *src_buffer = (ONSBuf *)effect_tmp_surface->pixels;
    ONSBuf *dst_buffer = (ONSBuf *)accumulation_surface->pixels;

    //left-side double-quadrant
    for ( int i=0 ; i<screen_height ; ++i ){
        for ( int j=0 ; j<screen_width ; j+=2, dst_buffer+=2 ){
            int ii=i, jj=j;
            float x = j - CENTER_X, y = i - CENTER_Y;
            // convert to polar
            float theta = atan2(y,x);
            float r = x * x + y * y;
            r = sqrt(r);
            //whirl
            theta += direction * (rad_base + rad_amp * sin(r * OMEGA));
            //convert to rectangular
            jj = (int) (r * cos(theta) + CENTER_X);
            ii = (int) (r * sin(theta) + CENTER_Y);
            if (jj < 0) jj = 0;
            if (jj >= screen_width) jj = screen_width-1;
            if (ii < 0) ii = 0;
            if (ii >= screen_height) ii = screen_height-1;

            // change pixel value!
            *dst_buffer = *(src_buffer + screen_width * ii + jj);
            *(dst_buffer+1) = *dst_buffer;
        }
    }

    SDL_UnlockSurface( accumulation_surface );
    SDL_UnlockSurface( effect_tmp_surface );
}
