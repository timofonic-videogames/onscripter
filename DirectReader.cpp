//$Id:$ -*- C++ -*-
/*
 *  DirectReader.cpp - Reader from independent files
 *
 *  Copyright (c) 2001-2006 Ogapee. All rights reserved.
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

#include "DirectReader.h"
#include <bzlib.h>
#if !defined(WIN32) && !defined(MACOS9) && !defined(PSP) && !defined(__OS2__)
#include <dirent.h>
#endif

#if defined(MACOSX) || defined(LINUX) || defined(UTF8_FILESYSTEM)
#define RECODING_FILENAMES
#ifdef MACOSX
#include <CoreFoundation/CoreFoundation.h>
#else
#include <iconv.h>
static int iconv_ref_count = 0;
static iconv_t iconv_cd = NULL;
#endif
#endif

#ifdef CONST_ICONV
#define ICONVCAST (const char**)
#else
#define ICONVCAST
#endif

#ifndef SEEK_END
#define SEEK_END 2
#endif

#define READ_LENGTH 4096
#define WRITE_LENGTH 5000

#define EI 8
#define EJ 4
#define P   1  /* If match length <= P then output one character */
#define N (1 << EI)  /* buffer size */
#define F ((1 << EJ) + P)  /* lookahead buffer size */

#define IS_TWO_BYTE(x) \
        ( ((x) & 0xe0) == 0xe0 || ((x) & 0xe0) == 0x80 )

DirectReader::DirectReader( DirPaths *path, const unsigned char *key_table )
{
    file_full_path = NULL;
    file_sub_path = NULL;
    file_path_len = 0;

    capital_name = new char[MAX_FILE_NAME_LENGTH*2+1];
    capital_name_tmp = new char[MAX_FILE_NAME_LENGTH*2+1];
#if defined(RECODING_FILENAMES) && !defined(MACOSX)
    if (iconv_cd == NULL) iconv_cd = iconv_open("UTF-8", "SJIS");
    iconv_ref_count++;
#endif

    setArchivePath(path);

    int i;
    if (key_table){
        key_table_flag = true;
        for (i=0 ; i<256 ; i++) this->key_table[i] = key_table[i];
    }
    else{
        key_table_flag = false;
        for (i=0 ; i<256 ; i++) this->key_table[i] = (unsigned char) i;
    }

    read_buf = new unsigned char[READ_LENGTH];
    decomp_buffer = new unsigned char[N*2];
    decomp_buffer_len = N*2;

    last_registered_compression_type = &root_registered_compression_type;
    registerCompressionType( "SPB", SPB_COMPRESSION );
    registerCompressionType( "JPG", NO_COMPRESSION );
    registerCompressionType( "GIF", NO_COMPRESSION );
}

DirectReader::~DirectReader()
{
    if (file_full_path) delete[] file_full_path;
    if (file_sub_path)  delete[] file_sub_path;

    delete[] capital_name;
    delete[] capital_name_tmp;
#if defined(RECODING_FILENAMES) && !defined(MACOSX)
    if (--iconv_ref_count == 0){
        iconv_close(iconv_cd);
        iconv_cd = NULL;
    }
#endif
    delete[] read_buf;
    delete[] decomp_buffer;
    
    last_registered_compression_type = root_registered_compression_type.next;
    while ( last_registered_compression_type ){
        RegisteredCompressionType *cur = last_registered_compression_type;
        last_registered_compression_type = last_registered_compression_type->next;
        delete cur;
    }
}

FILE *DirectReader::fopen(const char *path, const char *mode)
{
    FILE *fp = NULL;

    size_t len = archive_path->max_path_len() + strlen(path) + 1;

    if (file_path_len < len){
        file_path_len = len;
        if (file_full_path) delete[] file_full_path;
        file_full_path = new char[file_path_len];
        if (file_sub_path) delete[] file_sub_path;
        file_sub_path = new char[file_path_len];
    }
    for (int n=0; n<archive_path->get_num_paths(); n++) {
        sprintf( file_full_path, "%s%s", archive_path->get_path(n), path );
	printf("filename: \"%s\": ", file_full_path);
        fp = ::fopen( file_full_path, mode );
	puts(fp ? "found" : "not found");
	fflush(stdout);
        if (fp) return fp;
    }

#if !defined(WIN32) && !defined(MACOS9) && !defined(PSP) && !defined(__OS2__)
    // If the file wasn't found, try a case-insensitive search.
    char *cur_p = NULL;
    DIR *dp = NULL;
    
    len = 0;
    int n = archive_path->get_num_paths();
    int i=1;
    if (n > 0)
        len = strlen(archive_path->get_path(0));
    if (len > 0) {
        dp = opendir(archive_path->get_path(0));
        sprintf( file_full_path, "%s%s", archive_path->get_path(0), path );
    } else {
        dp = opendir(".");
        sprintf( file_full_path, "%s", path );
    }
    cur_p = file_full_path+len;

    struct dirent *entp = NULL;
    while (1){
        if (dp == NULL) {
            if (i < n) {
                len = strlen(archive_path->get_path(i));
                dp = opendir(archive_path->get_path(i));
                sprintf( file_full_path, "%s%s",
                        archive_path->get_path(i), path );
                cur_p = file_full_path+len;
                i++;
            } else
                return NULL;
        }
        if (dp == NULL) continue;

        char *delim_p = NULL;
        while(1){
            delim_p = strchr( cur_p, (char)DELIMITER );
            if (delim_p != cur_p) break;
            cur_p++;
        }
        
        if (delim_p) len = delim_p - cur_p;
        else         len = strlen(cur_p);
        memcpy(file_sub_path, cur_p, len);
        file_sub_path[len] = '\0';
        
        while ( (entp = readdir(dp)) != NULL ){
            if ( !strcasecmp( file_sub_path, entp->d_name ) ){
                memcpy(cur_p, entp->d_name, len);
                break;
            }
        }
        closedir( dp );
        dp = NULL;

        if (entp == NULL) continue;
        if (delim_p == NULL) break;

        memcpy(file_sub_path, file_full_path, delim_p-file_full_path);
        file_sub_path[delim_p-file_full_path]='\0';
        dp = opendir(file_sub_path);

        cur_p = delim_p+1;
    }
    if (entp == NULL) return NULL;

    fp = ::fopen( file_full_path, mode );
#endif

    return fp;
}

unsigned char DirectReader::readChar( FILE *fp )
{
    unsigned char ret;
    
    fread( &ret, 1, 1, fp );
    return key_table[ret];
}

unsigned short DirectReader::readShort( FILE *fp )
{
    unsigned short ret;
    unsigned char buf[2];
    
    fread( &buf, 1, 2, fp );
    ret = key_table[buf[0]] << 8 | key_table[buf[1]];
    return ret;
}

unsigned long DirectReader::readLong( FILE *fp )
{
    unsigned long ret;
    unsigned char buf[4];
    
    fread( &buf, 1, 4, fp );
    ret = key_table[buf[0]];
    ret = ret << 8 | key_table[buf[1]];
    ret = ret << 8 | key_table[buf[2]];
    ret = ret << 8 | key_table[buf[3]];
    return ret;
}

void DirectReader::writeChar( FILE *fp, unsigned char ch )
{
    fwrite( &ch, 1, 1, fp );
}

void DirectReader::writeShort( FILE *fp, unsigned short ch )
{
    unsigned char buf[2];

    buf[0] = (ch>>8) & 0xff;
    buf[1] = ch & 0xff;
    fwrite( &buf, 1, 2, fp );
}

void DirectReader::writeLong( FILE *fp, unsigned long ch )
{
    unsigned char buf[4];
    
    buf[0] = (unsigned char)((ch>>24) & 0xff);
    buf[1] = (unsigned char)((ch>>16) & 0xff);
    buf[2] = (unsigned char)((ch>>8)  & 0xff);
    buf[3] = (unsigned char)(ch & 0xff);
    fwrite( &buf, 1, 4, fp );
}

int DirectReader::open( const char *name, int archive_type )
{
    return 0;
}

int DirectReader::close()
{
    return 0;
}

void DirectReader::setArchivePath( DirPaths *path ) {
    if ( path != NULL ){
        archive_path = path;
    }
    else{
        archive_path = new DirPaths("");
    }
}


const char *DirectReader::getArchiveName() const
{
    return "direct";
}

int DirectReader::getNumFiles()
{
    return 0;
}
    
void DirectReader::registerCompressionType( const char *ext, int type )
{
    last_registered_compression_type->next = new RegisteredCompressionType(ext, type);
    last_registered_compression_type = last_registered_compression_type->next;
}
    
int DirectReader::getRegisteredCompressionType( const char *file_name )
{
    const char *ext_buf = file_name + strlen(file_name);
    while( *ext_buf != '.' && ext_buf != file_name ) ext_buf--;
    ext_buf++;
    
    strcpy( capital_name, ext_buf );
    for ( unsigned int i=0 ; i<strlen(ext_buf)+1 ; i++ )
        if ( capital_name[i] >= 'a' && capital_name[i] <= 'z' )
            capital_name[i] += 'A' - 'a';
    
    RegisteredCompressionType *reg = root_registered_compression_type.next;
    while (reg){
        if ( !strcmp( capital_name, reg->ext ) ) return reg->type;

        reg = reg->next;
    }

    return NO_COMPRESSION;
}
    
struct DirectReader::FileInfo DirectReader::getFileByIndex( unsigned int index )
{
    DirectReader::FileInfo fi;
    memset(&fi, 0, sizeof(DirectReader::FileInfo));
    return fi;
}

FILE *DirectReader::getFileHandle( const char *file_name, int &compression_type, size_t *length )
{
    FILE *fp;
    unsigned int i;

    compression_type = NO_COMPRESSION;
    size_t len = strlen( file_name );
    if ( len > MAX_FILE_NAME_LENGTH ) len = MAX_FILE_NAME_LENGTH;
    memcpy( capital_name, file_name, len );
    capital_name[ len ] = '\0';
//Mion: need to do more careful SJIS checking in this next part
    for ( i=0 ; i<len ; i++ ){
        if (IS_TWO_BYTE(capital_name[i])) {
            i++;
        } else if ( capital_name[i] == '/' || capital_name[i] == '\\' )
            capital_name[i] = (char)DELIMITER;
    }

    // On non-Windows platforms it's hard to predict how filenames
    // will be encoded.
    //
    // What does this mean?  Simple: if you have any choice in the
    // matter, DON'T USE JAPANESE IN FILENAMES.  It's nothing but
    // trouble.  Games that directly access files with Japanese names
    // should be considered not just essentially unsupported, but
    // unsupportable.  We do our best, but it's basically impossible
    // to guarantee that someone on OS X or Linux will manage to
    // install the game in such a way that it's even _possible_ to
    // access the filenames (e.g. they may be mangled irretrievably
    // during the installation process, if the user has a Latin-1
    // locale...)
    //
    // But we DO do our best, which is to try the likely options in
    // turn; this is inefficent, but hopefully a bit more robust than
    // requiring a single encoding to be compiled in.
#ifdef RECODING_FILENAMES
    // Try Shift_JIS
    if (!(fp = fopen(capital_name, "rb"))) {
	// Try UTF-8
	convertFromSJISToUTF8(capital_name_tmp, capital_name, len);
	if ((fp = fopen(capital_name_tmp, "rb"))) {
	    strcpy(capital_name, capital_name_tmp);
	    len = strlen(capital_name);
	}
	else {
	    // Try EUC-JP
	    convertFromSJISToEUC(capital_name);
	    if (!(fp = fopen(capital_name, "rb"))) {
		fprintf(stderr, "Warning: couldn't access %s even after "
			"transcoding the filename.\n", file_name);
	    }
	}
    }
    if (fp) fclose(fp);
#endif
    
    *length = 0;
    if ( (fp = fopen( capital_name, "rb" )) != NULL && len >= 3 ){
        compression_type = getRegisteredCompressionType( capital_name );
        if ( compression_type == NBZ_COMPRESSION || compression_type == SPB_COMPRESSION ){
            *length = getDecompressedFileLength( compression_type, fp, 0 );
        }
        else{
            fseek( fp, 0, SEEK_END );
            *length = ftell( fp );
            fseek( fp, 0, SEEK_SET );
        }
    }
            
    return fp;
}

size_t DirectReader::getFileLength( const char *file_name )
{
    int compression_type;
    size_t len;
    FILE *fp = getFileHandle( file_name, compression_type, &len );

    if ( fp ) fclose( fp );
    
    return len;
}

size_t DirectReader::getFile( const char *file_name, unsigned char *buffer, int *location )
{
    int compression_type;
    size_t len, c, total = 0;
    FILE *fp = getFileHandle( file_name, compression_type, &len );
    
    if ( fp ){
        if      ( compression_type & NBZ_COMPRESSION ) return decodeNBZ( fp, 0, buffer );
        else if ( compression_type & SPB_COMPRESSION ) return decodeSPB( fp, 0, buffer );

        total = len;
        while( len > 0 ){
            if ( len > READ_LENGTH ) c = READ_LENGTH;
            else                     c = len;
            len -= c;
            fread( buffer, 1, c, fp );
            buffer += c;
        }
        fclose( fp );
        if ( location ) *location = ARCHIVE_TYPE_NONE;
    }

    return total;
}

void DirectReader::convertFromSJISToEUC( char *buf )
{
    int i = 0;
    while ( buf[i] ) {
        if ( (unsigned char)buf[i] > 0x80 ) {
            unsigned char c1, c2;
            c1 = buf[i];
            c2 = buf[i+1];

            c1 -= (c1 <= 0x9f) ? 0x71 : 0xb1;
            c1 = c1 * 2 + 1;
            if (c2 > 0x9e) {
                c2 -= 0x7e;
                c1++;
            }
            else if (c2 >= 0x80) {
                c2 -= 0x20;
            }
            else {
                c2 -= 0x1f;
            }

            buf[i]   = c1 | 0x80;
            buf[i+1] = c2 | 0x80;
            i++;
        }
        i++;
    }
}

void DirectReader::convertFromSJISToUTF8( char *dst_buf, char *src_buf, size_t src_len )
{
#ifdef RECODING_FILENAMES
#ifdef MACOSX
    CFStringRef unicodeStrRef =
	CFStringCreateWithBytes(nil, (const UInt8*)src_buf, src_len, 
				kCFStringEncodingShiftJIS, false);
    Boolean ret = CFStringGetCString(unicodeStrRef, dst_buf, src_len*2+1,
				     kCFStringEncodingUTF8);
    if (!ret) strcpy(dst_buf, src_buf);
    CFRelease(unicodeStrRef);
#else
    src_len++;
    size_t dst_len = src_len*2+1;
    int ret = iconv(iconv_cd, ICONVCAST &src_buf, &src_len, &dst_buf, &dst_len);
    if (ret == -1) strcpy(dst_buf, src_buf);
#endif
#endif
}

size_t DirectReader::decodeNBZ( FILE *fp, size_t offset, unsigned char *buf )
{
    if (key_table_flag)
        fprintf(stderr, "may not decode NBZ with key_table enabled.\n");
    
    unsigned int original_length, count;
	BZFILE *bfp;
	void *unused;
	int err, len, nunused;

    fseek( fp, offset, SEEK_SET );
    original_length = count = readLong( fp );

	bfp = BZ2_bzReadOpen( &err, fp, 0, 0, NULL, 0 );
	if ( bfp == NULL || err != BZ_OK ) return 0;

	while( err == BZ_OK && count > 0 ){
        if ( count >= READ_LENGTH )
            len = BZ2_bzRead( &err, bfp, buf, READ_LENGTH );
        else
            len = BZ2_bzRead( &err, bfp, buf, count );
        count -= len;
		buf += len;
	}

	BZ2_bzReadGetUnused(&err, bfp, &unused, &nunused );
	BZ2_bzReadClose( &err, bfp );

    return original_length - count;
}

size_t DirectReader::encodeNBZ( FILE *fp, size_t length, unsigned char *buf )
{
    unsigned int bytes_in, bytes_out;
	int err;

	BZFILE *bfp = BZ2_bzWriteOpen( &err, fp, 9, 0, 30 );
	if ( bfp == NULL || err != BZ_OK ) return 0;

	while( err == BZ_OK && length > 0 ){
        if ( length >= WRITE_LENGTH ){
            BZ2_bzWrite( &err, bfp, buf, WRITE_LENGTH );
            buf += WRITE_LENGTH;
            length -= WRITE_LENGTH;
        }
        else{
            BZ2_bzWrite( &err, bfp, buf, length );
            break;
        }
	}

	BZ2_bzWriteClose( &err, bfp, 0, &bytes_in, &bytes_out );
    
    return bytes_out;
}

int DirectReader::getbit( FILE *fp, int n )
{
    int i, x = 0;
    static int getbit_buf;
    
    for ( i=0 ; i<n ; i++ ){
        if ( getbit_mask == 0 ){
            if (getbit_len == getbit_count){
                getbit_len = fread(read_buf, 1, READ_LENGTH, fp);
                if (getbit_len == 0) return EOF;
                getbit_count = 0;
            }

            getbit_buf = key_table[read_buf[getbit_count++]];
            getbit_mask = 128;
        }
        x <<= 1;
        if ( getbit_buf & getbit_mask ) x++;
        getbit_mask >>= 1;
    }
    return x;
}

size_t DirectReader::decodeSPB( FILE *fp, size_t offset, unsigned char *buf )
{
    unsigned int count;
    unsigned char *pbuf, *psbuf;
    size_t i, j, k;
    int c, n, m;

    getbit_mask = 0;
    getbit_len = getbit_count = 0;
    
    fseek( fp, offset, SEEK_SET );
    size_t width  = readShort( fp );
    size_t height = readShort( fp );

    size_t width_pad  = (4 - width * 3 % 4) % 4;

    size_t total_size = (width * 3 + width_pad) * height + 54;

    /* ---------------------------------------- */
    /* Write header */
    memset( buf, 0, 54 );
    buf[0] = 'B'; buf[1] = 'M';
    buf[2] = total_size & 0xff;
    buf[3] = (total_size >>  8) & 0xff;
    buf[4] = (total_size >> 16) & 0xff;
    buf[5] = (total_size >> 24) & 0xff;
    buf[10] = 54; // offset to the body
    buf[14] = 40; // header size
    buf[18] = width & 0xff;
    buf[19] = (width >> 8)  & 0xff;
    buf[22] = height & 0xff;
    buf[23] = (height >> 8)  & 0xff;
    buf[26] = 1; // the number of the plane
    buf[28] = 24; // bpp
    buf[34] = total_size - 54; // size of the body

    buf += 54;

    if (decomp_buffer_len < width*height+4){
        if (decomp_buffer) delete[] decomp_buffer;
        decomp_buffer_len = width*height+4;
        decomp_buffer = new unsigned char[decomp_buffer_len];
    }
    
    for ( i=0 ; i<3 ; i++ ){
        count = 0;
        decomp_buffer[count++] = c = getbit( fp, 8 );
        while ( count < (unsigned)(width * height) ){
            n = getbit( fp, 3 );
            if ( n == 0 ){
                decomp_buffer[count++] = c;
                decomp_buffer[count++] = c;
                decomp_buffer[count++] = c;
                decomp_buffer[count++] = c;
                continue;
            }
            else if ( n == 7 ){
                m = getbit( fp, 1 ) + 1;
            }
            else{
                m = n + 2;
            }

            for ( j=0 ; j<4 ; j++ ){
                if ( m == 8 ){
                    c = getbit( fp, 8 );
                }
                else{
                    k = getbit( fp, m );
                    if ( k & 1 ) c += (k>>1) + 1;
                    else         c -= (k>>1);
                }
                decomp_buffer[count++] = c;
            }
        }

        pbuf  = buf + (width * 3 + width_pad)*(height-1) + i;
        psbuf = decomp_buffer;

        for ( j=0 ; j<height ; j++ ){
            if ( j & 1 ){
                for ( k=0 ; k<width ; k++, pbuf -= 3 ) *pbuf = *psbuf++;
                pbuf -= width * 3 + width_pad - 3;
            }
            else{
                for ( k=0 ; k<width ; k++, pbuf += 3 ) *pbuf = *psbuf++;
                pbuf -= width * 3 + width_pad + 3;
            }
        }
    }
    
    return total_size;
}

size_t DirectReader::decodeLZSS( struct ArchiveInfo *ai, int no, unsigned char *buf )
{
    unsigned int count = 0;
    int i, j, k, r, c;

    getbit_mask = 0;
    getbit_len = getbit_count = 0;

    fseek( ai->file_handle, ai->fi_list[no].offset, SEEK_SET );
    memset( decomp_buffer, 0, N-F );
    r = N - F;

    while ( count < ai->fi_list[no].original_length ){
        if ( getbit( ai->file_handle, 1 ) ) {
            if ((c = getbit( ai->file_handle, 8 )) == EOF) break;
            buf[ count++ ] = c;
            decomp_buffer[r++] = c;  r &= (N - 1);
        } else {
            if ((i = getbit( ai->file_handle, EI )) == EOF) break;
            if ((j = getbit( ai->file_handle, EJ )) == EOF) break;
            for (k = 0; k <= j + 1  ; k++) {
                c = decomp_buffer[(i + k) & (N - 1)];
                buf[ count++ ] = c;
                decomp_buffer[r++] = c;  r &= (N - 1);
            }
        }
    }

    return count;
}

size_t DirectReader::getDecompressedFileLength( int type, FILE *fp, size_t offset )
{
    fpos_t pos;
    size_t length=0;
    fgetpos( fp, &pos );
    fseek( fp, offset, SEEK_SET );
    
    if ( type == NBZ_COMPRESSION ){
        length = readLong( fp );
    }
    else if ( type == SPB_COMPRESSION ){
        size_t width  = readShort( fp );
        size_t height = readShort( fp );
        size_t width_pad  = (4 - width * 3 % 4) % 4;
            
        length = (width * 3 +width_pad) * height + 54;
    }
    fsetpos( fp, &pos );

    return length;
}
