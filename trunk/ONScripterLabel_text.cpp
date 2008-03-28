/* -*- C++ -*-
 *
 *  ONScripterLabel_text.cpp - Text parser of ONScripter
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

// Modified by Mion of Sonozaki Futago-tachi, March 2008, to update from
// Ogapee's 20080121 release source code.

#include "ONScripterLabel.h"

extern unsigned short convSJIS2UTF16( unsigned short in );

#define IS_ROTATION_REQUIRED(x)	\
        ( !IS_TWO_BYTE(*(x)) || \
          *(x) == (char)0x81 && *((x)+1) == (char)0x50 || \
          *(x) == (char)0x81 && *((x)+1) == (char)0x51 || \
          *(x) == (char)0x81 && *((x)+1) >= 0x5b && *((x)+1) <= 0x5d || \
          *(x) == (char)0x81 && *((x)+1) >= 0x60 && *((x)+1) <= 0x64 || \
          *(x) == (char)0x81 && *((x)+1) >= 0x69 && *((x)+1) <= 0x7a || \
          *(x) == (char)0x81 && *((x)+1) == (char)0x80 )

#define IS_TRANSLATION_REQUIRED(x)	\
        ( *(x) == (char)0x81 && *((x)+1) >= 0x41 && *((x)+1) <= 0x44 )

SDL_Surface *ONScripterLabel::renderGlyph(TTF_Font *font, Uint16 text)
{
    GlyphCache *gc = root_glyph_cache;
    GlyphCache *pre_gc = gc;
    while(1){
        if (gc->text == text &&
            gc->font == font){
            if (gc != pre_gc){
                pre_gc->next = gc->next;
                gc->next = root_glyph_cache;
                root_glyph_cache = gc;
            }
            return gc->surface;
        }
        if (gc->next == NULL) break;
        pre_gc = gc;
        gc = gc->next;
    }

    pre_gc->next = NULL;
    gc->next = root_glyph_cache;
    root_glyph_cache = gc;

    gc->text = text;
    gc->font = font;
    if (gc->surface) SDL_FreeSurface(gc->surface);

    static SDL_Color fcol={0xff, 0xff, 0xff}, bcol={0, 0, 0};
    gc->surface = TTF_RenderGlyph_Shaded( font, text, fcol, bcol );

    return gc->surface;
}

void ONScripterLabel::drawGlyph( SDL_Surface *dst_surface, FontInfo *info, SDL_Color &color, char* text, int xy[2], bool shadow_flag, AnimationInfo *cache_info, SDL_Rect *clip, SDL_Rect &dst_rect )
{
    unsigned short unicode;
    if (IS_TWO_BYTE(text[0])){
        unsigned index = ((unsigned char*)text)[0];
        index = index << 8 | ((unsigned char*)text)[1];
        unicode = convSJIS2UTF16( index );
    }
    else{
        if ((text[0] & 0xe0) == 0xa0 || (text[0] & 0xe0) == 0xc0) unicode = ((unsigned char*)text)[0] - 0xa0 + 0xff60;
        else unicode = text[0];
    }

    int minx, maxx, miny, maxy, advanced;
#if 0
    if (TTF_GetFontStyle( (TTF_Font*)info->ttf_font ) !=
        (info->is_bold?TTF_STYLE_BOLD:TTF_STYLE_NORMAL) )
        TTF_SetFontStyle( (TTF_Font*)info->ttf_font, (info->is_bold?TTF_STYLE_BOLD:TTF_STYLE_NORMAL));
#endif
    TTF_GlyphMetrics( (TTF_Font*)info->ttf_font, unicode,
                      &minx, &maxx, &miny, &maxy, &advanced );
    //printf("min %d %d %d %d %d %d\n", minx, maxx, miny, maxy, advanced,TTF_FontAscent((TTF_Font*)info->ttf_font)  );

    SDL_Surface *tmp_surface = renderGlyph( (TTF_Font*)info->ttf_font, unicode );

    bool rotate_flag = false;
    if ( info->getTateyokoMode() == FontInfo::TATE_MODE && IS_ROTATION_REQUIRED(text) ) rotate_flag = true;

    dst_rect.x = xy[0] + minx;
    dst_rect.y = xy[1] + TTF_FontAscent((TTF_Font*)info->ttf_font) - maxy;
    if ( rotate_flag ) dst_rect.x += miny - minx;

    if ( info->getTateyokoMode() == FontInfo::TATE_MODE && IS_TRANSLATION_REQUIRED(text) ){
        dst_rect.x += info->font_size_xy[0]/2;
        dst_rect.y -= info->font_size_xy[0]/2;
    }

    if ( shadow_flag ){
        dst_rect.x += shade_distance[0];
        dst_rect.y += shade_distance[1];
    }

    if ( tmp_surface ){
        if (rotate_flag){
            dst_rect.w = tmp_surface->h;
            dst_rect.h = tmp_surface->w;
        }
        else{
            dst_rect.w = tmp_surface->w;
            dst_rect.h = tmp_surface->h;
        }

        if (cache_info)
            cache_info->blendBySurface( tmp_surface, dst_rect.x, dst_rect.y, color, clip, rotate_flag );

        if (dst_surface)
            alphaBlend32( dst_surface, dst_rect, tmp_surface, color, clip, rotate_flag );
    }
}

void ONScripterLabel::drawChar( char* text, FontInfo *info, bool flush_flag, bool lookback_flag, SDL_Surface *surface, AnimationInfo *cache_info, SDL_Rect *clip )
{
    //printf("draw %x-%x[%s] %d, %d\n", text[0], text[1], text, info->xy[0], info->xy[1] );

    if ( info->ttf_font == NULL ){
        if ( info->openFont( font_file, screen_ratio1, screen_ratio2 ) == NULL ){
            fprintf( stderr, "can't open font file: %s\n", font_file );
            quit();
            exit(-1);
        }
    }

    if ( info->isEndOfLine() ){
        info->newLine();
        for (int i=0 ; i<indent_offset ; i++)
            sentence_font.advanceCharInHankaku(2);

        if ( lookback_flag ){
            for (int i=0 ; i<indent_offset ; i++){
                current_page->add(0x81);
                current_page->add(0x40);
            }
        }
    }

    int xy[2];
    xy[0] = info->x() * screen_ratio1 / screen_ratio2;
    xy[1] = info->y() * screen_ratio1 / screen_ratio2;

    SDL_Color color;
    SDL_Rect dst_rect;
    if ( info->is_shadow ){
        color.r = color.g = color.b = 0;
        drawGlyph(surface, info, color, text, xy, true, cache_info, clip, dst_rect);
    }
    color.r = info->color[0];
    color.g = info->color[1];
    color.b = info->color[2];
    drawGlyph( surface, info, color, text, xy, false, cache_info, clip, dst_rect );

    info->addShadeArea(dst_rect, shade_distance);
    if ( surface == accumulation_surface &&
         !flush_flag &&
         (!clip || AnimationInfo::doClipping( &dst_rect, clip ) == 0) ){
        dirty_rect.add( dst_rect );
    }
    else if ( flush_flag ){
        flushDirect( dst_rect, REFRESH_NONE_MODE );
    }

    /* ---------------------------------------- */
    /* Update text buffer */
    if (IS_TWO_BYTE(text[0]))
        info->advanceCharInHankaku(2);
    else
        info->advanceCharInHankaku(1);

    if ( lookback_flag ){
        current_page->add( text[0] );
        if (IS_TWO_BYTE(text[0]))
            current_page->add( text[1] );
    }
}

void ONScripterLabel::drawString( const char *str, uchar3 color, FontInfo *info, bool flush_flag, SDL_Surface *surface, SDL_Rect *rect, AnimationInfo *cache_info )
{
    int i;

    int start_xy[2];
    start_xy[0] = info->xy[0];
    start_xy[1] = info->xy[1];

    /* ---------------------------------------- */
    /* Draw selected characters */
    uchar3 org_color;
    for ( i=0 ; i<3 ; i++ ) org_color[i] = info->color[i];
    for ( i=0 ; i<3 ; i++ ) info->color[i] = color[i];

    bool skip_whitespace_flag = true;
    char text[3] = { '\0', '\0', '\0' };
    while( *str ){
        while (*str == ' ' && skip_whitespace_flag) str++;

#ifdef ENABLE_1BYTE_CHAR
        if ( *str == '`' ){
            str++;
            skip_whitespace_flag = false;
            continue;
        }
#endif
        if ( IS_TWO_BYTE(*str) ){
            /* Kinsoku process */
            if (info->isEndOfLine(2) && isStartKinsoku( str+2 )){
                info->newLine();
                for (int i=0 ; i<indent_offset ; i++){
                    sentence_font.advanceCharInHankaku(2);
                }
            }
            text[0] = *str++;
            text[1] = *str++;
            drawChar( text, info, false, false, surface, cache_info );
        }
        else if (*str == 0x0a || *str == '\\' && info->is_newline_accepted){
            info->newLine();
            str++;
        }
        else{
            text[0] = *str++;
            text[1] = '\0';
            drawChar( text, info, false, false, surface, cache_info );
//Mion: fix for allowing mixed 1 & 2 byte chars
//            if (*str && *str != 0x0a){//                text[0] = *str++;
//                drawChar( text, info, false, false, surface, cache_info );
//            }
        }
    }
    for ( i=0 ; i<3 ; i++ ) info->color[i] = org_color[i];

    /* ---------------------------------------- */
    /* Calculate the area of selection */
    SDL_Rect clipped_rect = info->calcUpdatedArea(start_xy, screen_ratio1, screen_ratio2);
    info->addShadeArea(clipped_rect, shade_distance);

    if ( flush_flag )
        flush( refresh_shadow_text_mode, &clipped_rect );

    if ( rect ) *rect = clipped_rect;
}

void ONScripterLabel::restoreTextBuffer()
{
    text_info.fill( 0, 0, 0, 0 );

    char out_text[3] = { '\0','\0','\0' };
    FontInfo f_info = sentence_font;
    f_info.clear();
    for ( int i=0 ; i<current_page->text_count ; i++ ){
        if ( current_page->text[i] == 0x0a ){
            f_info.newLine();
        }
        else{
            out_text[0] = current_page->text[i];
	    if ( script_h.preferred_script == ScriptHandler::JAPANESE_SCRIPT ){
		if (out_text[0] == '('){
		    startRuby(current_page->text + i + 1, f_info);
		    continue;
		}
		else if (out_text[0] == '/' && ruby_struct.stage == RubyStruct::BODY ){
		    f_info.addLineOffset(ruby_struct.margin);
		    i = ruby_struct.ruby_end - current_page->text - 1;
		    if (*ruby_struct.ruby_end == ')'){
			endRuby(false, false, NULL);
			i++;
		    }
		    continue;
		}
		else if (out_text[0] == ')' && ruby_struct.stage == RubyStruct::BODY ){
		    ruby_struct.stage = RubyStruct::NONE;
		    continue;
		}
	    }
            if ( IS_TWO_BYTE(out_text[0]) ){
                out_text[1] = current_page->text[i+1];
                if (isStartKinsoku( current_page->text+i+2 )){
                    int i = 2;
                    while (!f_info.isEndOfLine(i) &&
                           isStartKinsoku( current_page->text+i+2 )){
                        i += 2;
                    }
                    if (f_info.isEndOfLine(i)) f_info.newLine();
                }
                i++;
            }
            else{
                out_text[1] = '\0';
//Mion: fix for allowing mixed 1 & 2 byte chars
//                drawChar( out_text, &f_info, false, false, NULL, &text_info );
//
//                if (i+1 == current_page->text_count) break;
//                out_text[0] = current_page->text[i+1];
//                if (out_text[0] == 0x0a) continue;
            }
            drawChar( out_text, &f_info, false, false, NULL, &text_info );
        }
    }
}

int ONScripterLabel::enterTextDisplayMode(bool text_flag)
{
    if (line_enter_status <= 1 && saveon_flag && internal_saveon_flag && text_flag){
        saveSaveFile( -1 );
        internal_saveon_flag = false;
    }

    if ( !(display_mode & TEXT_DISPLAY_MODE) ){
        if ( event_mode & EFFECT_EVENT_MODE ){
            if ( doEffect( &window_effect, NULL, DIRECT_EFFECT_IMAGE, false ) == RET_CONTINUE ){
                display_mode = TEXT_DISPLAY_MODE;
                text_on_flag = true;
                return RET_CONTINUE | RET_NOREAD;
            }
            return RET_WAIT | RET_REREAD;
        }
        else{
            dirty_rect.clear();
            dirty_rect.add( sentence_font_info.pos );
	    SDL_BlitSurface( accumulation_comp_surface, NULL, effect_dst_surface, NULL );
            SDL_BlitSurface( accumulation_surface, NULL, accumulation_comp_surface, NULL );

            return setEffect( &window_effect );
        }
    }

    return RET_NOMATCH;
}

int ONScripterLabel::leaveTextDisplayMode()
{
    if ( display_mode & TEXT_DISPLAY_MODE &&
         erase_text_window_mode != 0 ){

        if ( event_mode & EFFECT_EVENT_MODE ){
            if ( doEffect( &window_effect,  NULL, DIRECT_EFFECT_IMAGE, false ) == RET_CONTINUE ){
                display_mode = NORMAL_DISPLAY_MODE;
                return RET_CONTINUE | RET_NOREAD;
            }
            return RET_WAIT | RET_REREAD;
        }
        else{
            SDL_BlitSurface( accumulation_comp_surface, NULL, effect_dst_surface, NULL );
            SDL_BlitSurface( accumulation_surface, NULL, accumulation_comp_surface, NULL );
            dirty_rect.add( sentence_font_info.pos );

            return setEffect( &window_effect );
        }
    }

    return RET_NOMATCH;
}

void ONScripterLabel::doClickEnd()
{
#ifdef INSANI
    skip_to_wait = 0;
    skip_in_text = 0;
#endif

    if ( automode_flag ){
        event_mode =  WAIT_TEXT_MODE | WAIT_INPUT_MODE | WAIT_VOICE_MODE;
        if ( automode_time < 0 )
            startTimer( -automode_time * num_chars_in_sentence / 2);
        else
            startTimer( automode_time );
    }
    else if ( autoclick_time > 0 ){
        event_mode = WAIT_SLEEP_MODE;
        startTimer( autoclick_time );
    }
    else{
        event_mode = WAIT_TEXT_MODE | WAIT_INPUT_MODE | WAIT_TIMER_MODE;
        advancePhase();
    }
    draw_cursor_flag = true;
    num_chars_in_sentence = 0;
}

int ONScripterLabel::clickWait( char *out_text )
{
#ifdef INSANI
    skip_to_wait = 0;
    skip_in_text = 0;
#endif

    if ( (skip_flag || draw_one_page_flag || ctrl_pressed_status) && !textgosub_label ){
        clickstr_state = CLICK_NONE;
        flush(refreshMode());
        num_chars_in_sentence = 0;

        return RET_CONTINUE | RET_NOREAD;
    }
    else{
        clickstr_state = CLICK_WAIT;
        key_pressed_flag = false;
        if ( textgosub_label ){
            saveoffCommand();

            textgosub_clickstr_state = CLICK_WAIT;
            if (script_h.getNext()[0] == 0x0a)
                textgosub_clickstr_state = CLICK_WAITEOL;
            gosubReal( textgosub_label, script_h.getNext() );
            indent_offset = 0;
            line_enter_status = 0;
            string_buffer_offset = 0;
            return RET_CONTINUE;
        }

        doClickEnd();

        return RET_WAIT | RET_NOREAD;
    }
}

int ONScripterLabel::clickNewPage( char *out_text )
{
#ifdef INSANI
    skip_to_wait = 0;
    skip_in_text = 0;
#endif

    clickstr_state = CLICK_NEWPAGE;
    if ( skip_flag || draw_one_page_flag || ctrl_pressed_status ) flush( refreshMode() );

    if ( (skip_flag || ctrl_pressed_status) && !textgosub_label ){
        clickstr_state = CLICK_NONE;
        event_mode = WAIT_SLEEP_MODE;
        advancePhase();
        num_chars_in_sentence = 0;
    }
    else{
        key_pressed_flag = false;
        if ( textgosub_label ){
            saveoffCommand();

            textgosub_clickstr_state = CLICK_NEWPAGE;
            gosubReal( textgosub_label, script_h.getNext() );
            indent_offset = 0;
            line_enter_status = 0;
            string_buffer_offset = 0;
            return RET_CONTINUE;
        }

        doClickEnd();
    }
    return RET_WAIT | RET_NOREAD;
}

void ONScripterLabel::startRuby(char *buf, FontInfo &info)
{
    ruby_struct.stage = RubyStruct::BODY;
    ruby_font = info;
    ruby_font.ttf_font = NULL;
    if ( ruby_struct.font_size_xy[0] != -1 )
        ruby_font.font_size_xy[0] = ruby_struct.font_size_xy[0];
    else
        ruby_font.font_size_xy[0] = info.font_size_xy[0]/2;
    if ( ruby_struct.font_size_xy[1] != -1 )
        ruby_font.font_size_xy[1] = ruby_struct.font_size_xy[1];
    else
        ruby_font.font_size_xy[1] = info.font_size_xy[1]/2;

    ruby_struct.body_count = 0;
    ruby_struct.ruby_count = 0;

    while(1){
        if ( *buf == '/' ){
            ruby_struct.stage = RubyStruct::RUBY;
            ruby_struct.ruby_start = buf+1;
        }
        else if ( *buf == ')' || *buf == '\0' ){
            break;
        }
        else{
            if ( ruby_struct.stage == RubyStruct::BODY )
                ruby_struct.body_count++;
            else if ( ruby_struct.stage == RubyStruct::RUBY )
                ruby_struct.ruby_count++;
        }
        buf++;
    }
    ruby_struct.ruby_end = buf;
    ruby_struct.stage = RubyStruct::BODY;
    ruby_struct.margin = ruby_font.initRuby(info, ruby_struct.body_count/2, ruby_struct.ruby_count/2);
}

void ONScripterLabel::endRuby(bool flush_flag, bool lookback_flag, SDL_Surface *surface)
{
    char out_text[3]= {'\0', '\0', '\0'};
    if ( rubyon_flag ){
        ruby_font.clear();
        char *buf = ruby_struct.ruby_start;
        while( buf < ruby_struct.ruby_end ){
            out_text[0] = *buf;
            if ( IS_TWO_BYTE(*buf) ){
                out_text[1] = *(buf+1);
                drawChar( out_text, &ruby_font, flush_flag, lookback_flag, surface, &text_info );
                buf++;
            }
            else{
                out_text[1] = '\0';
                drawChar( out_text, &ruby_font, flush_flag,  lookback_flag, surface, &text_info );
                if ( *(buf+1) ){
                    out_text[1] = *(buf+1);
                    drawChar( out_text, &ruby_font, flush_flag,  lookback_flag, surface, &text_info );
                    buf++;
                }
            }
            buf++;
        }
    }
    ruby_struct.stage = RubyStruct::NONE;
}

int ONScripterLabel::textCommand()
{
    if (pretextgosub_label &&
        (line_enter_status == 0 ||
         (line_enter_status == 1 &&
          (script_h.getStringBuffer()[string_buffer_offset] == '[' ||
           zenkakko_flag &&
           (unsigned char) script_h.getStringBuffer()[string_buffer_offset] == 0x81
           && (unsigned char) script_h.getStringBuffer()[string_buffer_offset+1] == 0x79))) ){
        gosubReal( pretextgosub_label, script_h.getCurrent() );
        line_enter_status = 1;
        return RET_CONTINUE;
    }

    int ret = enterTextDisplayMode();
    if ( ret != RET_NOMATCH ) return ret;

    line_enter_status = 2;
    ret = processText();
    if (ret == RET_CONTINUE){
        indent_offset = 0;
    }

    return ret;
}

int ONScripterLabel::processText()
{
    printf("textCommand %s %d %d %d\n", script_h.getStringBuffer() + string_buffer_offset, string_buffer_offset, event_mode, line_enter_status);
    char out_text[3]= {'\0', '\0', '\0'};

    if ( event_mode & (WAIT_INPUT_MODE | WAIT_TEXTOUT_MODE ) ){
        draw_cursor_flag = false;
        if ( clickstr_state == CLICK_WAIT ){
            clickstr_state = CLICK_NONE;
        }
        else if ( clickstr_state == CLICK_NEWPAGE ){
            event_mode = IDLE_EVENT_MODE;
            newPage( true );
            clickstr_state = CLICK_NONE;
            return RET_CONTINUE | RET_NOREAD;
        }

        event_mode = IDLE_EVENT_MODE;
    }

    if ( event_mode & WAIT_SLEEP_MODE ){
        draw_cursor_flag = false;
        if ( clickstr_state == CLICK_WAIT ){
            clickstr_state = CLICK_NONE;
        }
        else if ( clickstr_state == CLICK_NEWPAGE ){
            event_mode = IDLE_EVENT_MODE;
            newPage( true );
            clickstr_state = CLICK_NONE;
            return RET_CONTINUE | RET_NOREAD;
        }
        else
            event_mode = IDLE_EVENT_MODE;
    }

    if (string_buffer_offset == 0) {
        if ( script_h.preferred_script == ScriptHandler::JAPANESE_SCRIPT ) {
            processBreaks(new_line_skip_flag, KINSOKU);
        } else {
            processBreaks(new_line_skip_flag, SPACEBREAK);
        }
    }

    if (script_h.getStringBuffer()[string_buffer_offset] == '`') {
        string_buffer_offset++;
        script_h.toggle1ByteEndStatus();
    }

    if (script_h.getStringBuffer()[string_buffer_offset] == 0x0a ||
	script_h.getStringBuffer()[string_buffer_offset] == 0x00){
        if (!click_skip_page_flag) skip_in_text = 0;
        indent_offset = 0; // redundant
        line_enter_status = 3;
        return RET_CONTINUE;
    }

    //printf("*** textCommand %d (%d,%d)\n", string_buffer_offset, sentence_font.xy[0], sentence_font.xy[1]);

    char ch = script_h.getStringBuffer()[string_buffer_offset];

    if (isTextCommand(script_h.getStringBuffer() + string_buffer_offset) == 0) {
        if (sentence_font.isEndOfLine(0) ||
            (IS_TWO_BYTE(ch) && sentence_font.isEndOfLine(1))) {
            // no room for current char on the line
            printf("at end; breaking before %s", script_h.getStringBuffer() + string_buffer_offset);
            ch = doLineBreak();
        }
        else {
            int break_offset;
            break_offset = findNextBreak(string_buffer_offset);
            if (break_offset == 0) {
                // can break before char
                if ( IS_TWO_BYTE(ch) )
                    break_offset += 2;
                else
                   break_offset++;
                break_offset += findNextBreak(break_offset + string_buffer_offset);
                if (sentence_font.isEndOfLine(break_offset-1)) {
                    printf("breaking before %s", script_h.getStringBuffer() + string_buffer_offset);
                    ch = doLineBreak();
                }
            }
        }
    }

    new_line_skip_flag = false;

    if ( IS_TWO_BYTE(ch) ){ // Shift jis

        out_text[0] = script_h.getStringBuffer()[string_buffer_offset];
        out_text[1] = script_h.getStringBuffer()[string_buffer_offset+1];

        if ( skip_flag || draw_one_page_flag || ctrl_pressed_status ){
            drawChar( out_text, &sentence_font, false, true, accumulation_surface, &text_info );
            num_chars_in_sentence += 2;

            string_buffer_offset += 2;
            return RET_CONTINUE | RET_NOREAD;
        }
        else{
            drawChar( out_text, &sentence_font, true, true, accumulation_surface, &text_info );
            num_chars_in_sentence += 2;
            string_buffer_offset += 2;
            event_mode = WAIT_TEXTOUT_MODE;
#ifdef INSANI
	    if ( skip_in_text == 1 || skip_to_wait == 1)
	        advancePhase( 0 );
            else
#endif
	    if ( sentence_font.wait_time == -1 )
                advancePhase( default_text_speed[text_speed_no] );
            else
                advancePhase( sentence_font.wait_time );
            return RET_WAIT | RET_NOREAD;
        }
    }
    else if (script_h.checkClickstr(&script_h.getStringBuffer()[string_buffer_offset]) == -2) {
        // got the special "\@" clickwait-or-page
        string_buffer_offset += 2;
        if (sentence_font.getRemainingLine() <= clickstr_line)
            return clickNewPage( NULL );
        else
            return clickWait( NULL );
    }
    else if ( ch == '@' ){ // wait for click
        string_buffer_offset++;
        return clickWait( NULL );
    }
    else if ( ch == '\\' ){ // new page
        string_buffer_offset++;
        return clickNewPage( NULL );
    }
    else if ( ch == '!' ){
        string_buffer_offset++;
        if ( script_h.getStringBuffer()[ string_buffer_offset ] == 's' ){
            string_buffer_offset++;
            if ( script_h.getStringBuffer()[ string_buffer_offset ] == 'd' ){
                sentence_font.wait_time = -1;
                string_buffer_offset++;
            }
            else{
                int t = 0;
                while( script_h.getStringBuffer()[ string_buffer_offset ] >= '0' &&
                       script_h.getStringBuffer()[ string_buffer_offset ] <= '9' ){
                    t = t*10 + script_h.getStringBuffer()[ string_buffer_offset ] - '0';
                    string_buffer_offset++;
                }
                sentence_font.wait_time = t;
                while (script_h.getStringBuffer()[ string_buffer_offset ] == ' ' ||
                       script_h.getStringBuffer()[ string_buffer_offset ] == '\t') string_buffer_offset++;
            }
        }
        else if ( script_h.getStringBuffer()[ string_buffer_offset ] == 'w' ||
                  script_h.getStringBuffer()[ string_buffer_offset ] == 'd' ){
#ifdef INSANI
            skip_in_text = 0;
            event_mode = WAIT_SLEEP_MODE;
#endif
            bool flag = false;
            if ( script_h.getStringBuffer()[ string_buffer_offset ] == 'd' ) flag = true;
            string_buffer_offset++;
            int t = 0;
            while( script_h.getStringBuffer()[ string_buffer_offset ] >= '0' &&
                   script_h.getStringBuffer()[ string_buffer_offset ] <= '9' ){
                t = t*10 + script_h.getStringBuffer()[ string_buffer_offset ] - '0';
                string_buffer_offset++;
            }
            if ( skip_flag || draw_one_page_flag || ctrl_pressed_status
#ifdef INSANI
                  || skip_to_wait
#endif
            ){
                return RET_CONTINUE | RET_NOREAD;
            }
            else{
                event_mode |= WAIT_TEXTOUT_MODE;
                if ( flag ) event_mode |= WAIT_INPUT_MODE;
                key_pressed_flag = false;
                startTimer( t );
                return RET_WAIT | RET_NOREAD;
            }
        }
#ifdef INSANI
        else{
                string_buffer_offset--;
                goto notacommand;
        }
#endif
        return RET_CONTINUE | RET_NOREAD;
    }
    else if ( ch == '#' ){
         char hexchecker;
         for ( int tmpctr = 0; tmpctr <= 5; tmpctr++) {
             hexchecker = script_h.getStringBuffer()[ string_buffer_offset+tmpctr+1 ];
             if(!((hexchecker >= '0' && hexchecker <= '9') || (hexchecker >= 'a' && hexchecker <= 'f') || (hexchecker >= 'A' && hexchecker <= 'F'))) goto notacommand;
         }
        readColor( &sentence_font.color, script_h.getStringBuffer() + string_buffer_offset );
        readColor( &ruby_font.color, script_h.getStringBuffer() + string_buffer_offset );
        string_buffer_offset += 7;
        return RET_CONTINUE | RET_NOREAD;
    }
    else if ( ch == '(' && !(script_h.getEndStatus() & ScriptHandler::END_1BYTE_CHAR)){
        current_page->add('(');
        startRuby( script_h.getStringBuffer() + string_buffer_offset + 1, sentence_font );

        string_buffer_offset++;
        return RET_CONTINUE | RET_NOREAD;
    }
#ifdef INSANI
    else if ( ch == '/'){
#else
    else if ( ch == '/' && !(script_h.getEndStatus() & ScriptHandler::END_1BYTE_CHAR) ){
#endif
        if ( ruby_struct.stage == RubyStruct::BODY ){
            current_page->add('/');
            sentence_font.addLineOffset(ruby_struct.margin);
            string_buffer_offset = ruby_struct.ruby_end - script_h.getStringBuffer();
            if (*ruby_struct.ruby_end == ')'){
                if ( skip_flag || draw_one_page_flag || ctrl_pressed_status )
                    endRuby(false, true, accumulation_surface);
                else
                    endRuby(true, true, accumulation_surface);
                current_page->add(')');
                string_buffer_offset++;
            }

            return RET_CONTINUE | RET_NOREAD;
        }
#ifdef INSANI
        else if (script_h.getStringBuffer()[string_buffer_offset+1] != 0x0a) goto notacommand;
#endif
        else{ // skip new line
            new_line_skip_flag = true;
            string_buffer_offset++;
            if (script_h.getStringBuffer()[string_buffer_offset] != 0x0a)
                errorAndExit( "'new line' must follow '/'." );
            return RET_CONTINUE; // skip the following eol
        }
    }
    else if ( ch == ')' && !(script_h.getEndStatus() & ScriptHandler::END_1BYTE_CHAR) &&
              ruby_struct.stage == RubyStruct::BODY ){
        current_page->add(')');
        string_buffer_offset++;
        ruby_struct.stage = RubyStruct::NONE;
        return RET_CONTINUE | RET_NOREAD;
    }
    else{
#ifdef INSANI
        notacommand:
#endif
        out_text[0] = ch;

        bool flush_flag = true;
        if ( skip_flag || draw_one_page_flag || ctrl_pressed_status )
            flush_flag = false;
        drawChar( out_text, &sentence_font, flush_flag, true, accumulation_surface, &text_info );
        num_chars_in_sentence++;
        string_buffer_offset++;
        if ( skip_flag || draw_one_page_flag || ctrl_pressed_status ){
            return RET_CONTINUE | RET_NOREAD;
        }
        else{
            event_mode = WAIT_TEXTOUT_MODE;
#ifdef INSANI
	    if ( skip_in_text == 1 || skip_to_wait == 1)
		advancePhase( 0 );
            else
#endif
	    if ( sentence_font.wait_time == -1 )
                advancePhase( default_text_speed[text_speed_no] );
            else
                advancePhase( sentence_font.wait_time );
            return RET_WAIT | RET_NOREAD;
        }
    }

    return RET_NOMATCH;
}

char ONScripterLabel::doLineBreak()
{
    sentence_font.newLine();
    for (int i=0 ; i<indent_offset ; i++){
        current_page->add(0x81);
        current_page->add(0x40);
        sentence_font.advanceCharInHankaku(2);
    }
    char ch = script_h.getStringBuffer()[string_buffer_offset];
    while (ch == ' ') {
        ch = script_h.getStringBuffer()[++string_buffer_offset];
    }
    return ch;
}

int ONScripterLabel::isTextCommand(const char *buf)
{
    int offset = 0;
    if (script_h.checkClickstr(buf) == -2) {
        // got the special "\@" clickwait-or-page
        return 2;
    }
    else if ( *buf == '`' ){
        return 1;
    }
    else if ( *buf == '@' ){ // clickwait
        return 1;
    }
    else if ( *buf == '\\' ){ // new page
        return 1;
    }
    else if ( *buf == '!' ){
        offset++;
        if ( buf[offset] == 's' ){
            offset++;
            if ( buf[offset] == 'd' ){
                offset++;
            }
            else{
                while( buf[offset] >= '0' && buf[offset] <= '9' ){
                    offset++;
                }
            }
        }
        else if ( buf[offset] == 'w' || buf[offset] == 'd' ){
            offset++;
            while( buf[offset] >= '0' && buf[offset] <= '9' ){
                offset++;
            }
        } else
            return 0;
        return offset;
    }
    else if ( *buf == '#' ){
         char hexchecker;
         for ( int tmpctr = 1; tmpctr <= 6; tmpctr++) {
             hexchecker = buf[tmpctr];
             if (!((hexchecker >= '0' && hexchecker <= '9') ||
                 (hexchecker >= 'a' && hexchecker <= 'f') ||
                 (hexchecker >= 'A' && hexchecker <= 'F')))
                 return 0;
         }
         return 7;
    }
    else
        return 0;


}

void ONScripterLabel::processBreaks(bool cont_line, LineBreakType style)
{
    char *string_buffer = script_h.getStringBuffer();
    if (string_buffer_breaks) delete[] string_buffer_breaks;
    string_buffer_breaks = new bool[strlen(string_buffer)+2];
    unsigned int i=0, j=0, cmd=0;

    do {
        cmd = isTextCommand(string_buffer + i);
        i += cmd;
    } while (cmd > 0);
    // don't break before first char
    if (cont_line)
        string_buffer_breaks[i] = true;
    else {
        string_buffer_breaks[i] = false;
        printf("Can't break before %s", string_buffer + i);
    }


    if (style == KINSOKU) {
        // straight kinsoku
        while (i<strlen(string_buffer)) {
            if (IS_TWO_BYTE(string_buffer[i]))
                j = 2;
            else
                j = 1;
            do {
                cmd = isTextCommand(string_buffer + i + j);
                j += cmd;
            } while (cmd > 0);
            if (isEndKinsoku(string_buffer + i) ||
                isStartKinsoku(string_buffer + i + j)) {
                // don't break before s_kinsoku or after e_kinsoku
                string_buffer_breaks[i+j] = false;
                printf("Can't break before %s", string_buffer + i + j);
            } else {
                string_buffer_breaks[i+j] = true;
            }
            i += j;
        }
    }
    else if (style == SPACEBREAK) {
        // straight space-breaking
        while (i<strlen(string_buffer)) {
            if (IS_TWO_BYTE(string_buffer[i]))
                j = 2;
            else
                j = 1;
            do {
                cmd = isTextCommand(string_buffer + i + j);
                j += cmd;
            } while (cmd > 0);
            if (!IS_TWO_BYTE(string_buffer[i+j]) &&
                (string_buffer[i+j] == 0x0a ||
                 string_buffer[i+j] == '/' ||
                 (string_buffer[i+j] == ' ' && string_buffer[i] != ' '))) {
                printf("Can break before %s", string_buffer + i + j);
                string_buffer_breaks[i+j] = true;
            } else {
                string_buffer_breaks[i+j] = false;
            }
            i += j;
        }
    }
}

int ONScripterLabel::findNextBreak(int offset)
{
    // return offset of first break_before
    char *string_buffer = script_h.getStringBuffer();
    int i = 0, len = 0, cmd = 0;
    while (i<offset) {
        cmd = isTextCommand(string_buffer + i);
        if (cmd > 0) {
            i += cmd;
        } else if (IS_TWO_BYTE(string_buffer[i])) {
            i += 2;
        } else {
            i++;
        }
    }
    while (i<(int)(strlen(string_buffer)+2)) {
        do {
            cmd = isTextCommand(string_buffer + i);
            i += cmd;
        } while (cmd > 0);
        if (string_buffer_breaks[i]) {
            printf("i=%d, len=%d\n", i, len);
            return len;
        }
        if (IS_TWO_BYTE(string_buffer[i])) {
            i += 2;
            len += 2;
        } else {
            i++;
            len++;
        }
    }
    return len;
}

