/*
 *  Copyright (c) KONINKLIJKE PHILIPS ELECTRONICS N.V. 1997-2010
 *  All rights reserved.
 *  For licensing and warranty information, see the file COPYING.LGPLv21.txt
 *  in the root directory of this package.
 *
 *  Philips Research Laboratories
 *  Eindhoven, The Netherlands
 *
 *  CVS id      :  $Id: cpfspd_aux.c,v 2.14 2010/01/05 14:18:31 riemens Exp $
 *
 *  Name        :  cpfspd_aux.c
 *
 *  Author      :  Harold Schmeitz
 *
 *  Function    :  cpfspd AUXiliary data.
 *                        ---
 *
 *  Description :  Access routines for auxiliary data (unrelated to image size)
 *                 such as audio, subtitles or quality metrics.
 *
 *  Functions   :  The following external cpfspd functions are
 *                 defined in this file:
 *
 *                 - p_mod_add_aux()
 *                 - p_mod_rm_aux()
 *                 - p_get_num_aux()
 *                 - p_get_aux_by_name()
 *                 - p_get_aux()
 *                 - p_read_aux()
 *                 - p_write_aux()
 *
 */

/******************************************************************************/

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "cpfspd.h"
#include "cpfspd_low.h"

/******************************************************************************/

/* These defines indicate the number of bytes used for each header element */
#define P_SAUX_HDRLEN   (8)     /* length of auxiliary header */
/* #define P_SAUX_NAME     (16)    *//* name of auxiliary data */ /* moved to cpfspd.h */
#define P_SMAX_LEN      (8)     /* maximum data length */
#define P_SRESERVED     (16)    /* reserved for future extension */
#define P_SMIN_VALID    (48)    /* minimum size of a valid auxiliary header */
#define P_MAX_FIELD_LEN (8)     /* maximum length of an 'int' field */

/* define minimum/maximum macros */
#define MIN(x,y)             ( ((x) < (y)) ? (x) : (y) )
#define MAX(x,y)             ( ((x) > (y)) ? (x) : (y) )

/******************************************************************************/

/* internal function to get an int value from a buffer */
static int
p_aux_parse_int (const unsigned char *buf,
                 const int  len )
{
    int              i;
    char             ch;
    char             temp[P_MAX_FIELD_LEN + 1];

    /* Syntax checking and copy into local buffer. */
    for (i = 0; i < MIN(len,P_MAX_FIELD_LEN); i++)
    {
        ch = buf[i];
        if ( !( (ch == ' ') || isdigit((int)ch) ) )
            return 0;   /* abort */
        temp[i] = ch;
    }
    temp[len] = (char)'\0';

    return atoi (temp);
}  /* end of p_aux_parse_int */


/* internal function to calculate total required size of auxiliary data records */
static unsigned int
p_aux_get_total (
        const unsigned char * aux_hdrs )
{
    int len = 0;
    int total = 0;
    int offset = 0;
    int maxlen = 0;

    do {
        offset += len;
        len = p_aux_parse_int( &aux_hdrs[offset], P_SAUX_HDRLEN );
        if (len >= P_SMIN_VALID) {
            maxlen = p_aux_parse_int( &aux_hdrs[offset+P_SAUX_HDRLEN+P_SAUX_NAME], P_SMAX_LEN );
            if (maxlen > 0) {
                total += maxlen + P_SMAX_LEN;
            }
        }
    } while (len >= P_SMIN_VALID);

    return total;
} /* end of p_aux_get_total */


/* internal function to calculate data offset in auxiliary data records */
static unsigned int
p_aux_get_data_offset (
        const unsigned char * aux_hdrs,
        int aux_id )
{
    int i;
    int len = 0;
    int total = 0;
    int offset = 0;
    int maxlen = 0;

    for (i=0; i<aux_id; i++) {
        offset += len;
        len = p_aux_parse_int( &aux_hdrs[offset], P_SAUX_HDRLEN );
        maxlen = p_aux_parse_int( &aux_hdrs[offset+P_SAUX_HDRLEN+P_SAUX_NAME], P_SMAX_LEN );
        if (maxlen > 0) {
            total += maxlen + P_SMAX_LEN;
        }
    }
    return total;
} /* end of p_aux_get_offset */


/******************************************************************************/

int
p_mod_add_aux (
        pT_header     * header,
        int             max_size,
        const char    * name,
        int             descr_len,
        const char    * description )
{
    int aux_id = -1;
    int offset = 0;
    int len = 0;

    /* Check for duplicate name */
    if (p_get_aux_by_name(header, name) >= 0) {
        return -1;   /* error exit; attempt to insert duplicate aux hdr name */
    }

    /* find offset to sentinel */
    do {
        offset += len;
        aux_id++;
        len = p_aux_parse_int( &header->aux_hdrs[offset], P_SAUX_HDRLEN );
    } while (len >= P_SMIN_VALID);

    /* At this point, 'offset' points where we can write the new header */
    /* check whether this header can be added in the available space */
    /* take null terminated sentinel into account */
    if (offset + P_SMIN_VALID + descr_len + P_SAUX_HDRLEN + 1 > P_SAUX_HDR) {
        return -1;    /* error exit; no space available for new aux hdr info */
    }

    /* write auxiliary header (mandatory part) */
    sprintf( (char *) &header->aux_hdrs[offset], "%*d%-*s%*d%-*s",
            P_SAUX_HDRLEN,  P_SMIN_VALID+descr_len, /* size of this header */
            P_SAUX_NAME,    name,                   /* name of this auxiliary header/data */
            P_SMAX_LEN,     max_size,               /* maximum data size */
            P_SRESERVED,    " " );                  /* for future extension */
    offset += P_SMIN_VALID;
    /* copy the description (when present) */
    if (descr_len > 0) {
        memcpy( &header->aux_hdrs[offset], description, descr_len );
        offset += descr_len;
    }
    /* write sentinel */
    strcpy( (char *) &header->aux_hdrs[offset], P_AUX_LAST);
    
    /* update nr_aux_data_recs */
    header->nr_aux_data_recs = (p_aux_get_total( header->aux_hdrs ) 
                                + header->bytes_rec-1) / header->bytes_rec;

    /* This modified the header. */
    header->modified = 1;

    return aux_id;
} /* end of p_mod_add_aux */


int
p_get_num_aux (
        const pT_header *header )
{
    int result = 0;
    int offset = 0;
    int len = 0;

    do {
        len = p_aux_parse_int( &header->aux_hdrs[offset], P_SAUX_HDRLEN );
        if (len >= P_SMIN_VALID)
            result++;
        offset += len;
    } while (len >= P_SMIN_VALID);

    return result;
} /* end of p_get_num_aux */


int
p_get_aux_by_name (
        const pT_header *header,
        const char    * name )
{
    int i = 0;
    int len = 0;
    int offset = 0;
    int result = -1;
    char name_space[P_SAUX_NAME+1];

    sprintf(name_space, "%-*s", P_SAUX_NAME, name);

    do {
        len = p_aux_parse_int( &header->aux_hdrs[offset], P_SAUX_HDRLEN );
        if ((result == -1) && (len >= P_SMIN_VALID)) {
            if (strncmp((char *)&header->aux_hdrs[offset+P_SAUX_HDRLEN], name_space, P_SAUX_NAME) == 0) {
                result = i;
            }
        }
        offset += len;
        i++;
    } while (len >= P_SMIN_VALID && (result == -1));

    return result;
} /* end of p_get_aux_by_name */


pT_status
p_mod_rm_aux (
        pT_header     * header,
        int             aux_id )
{
    pT_status      status = P_OK;
    int i;
    int len = 0;
    int offset = 0;
 
    if ((aux_id < 0) || (aux_id >= p_get_num_aux(header))) {
        status = P_INVALID_AUXILIARY;
    }
    
    if (status == P_OK) {
        for (i=0; i<aux_id; i++) {
            len = p_aux_parse_int( &header->aux_hdrs[offset], P_SAUX_HDRLEN );
            offset += len;
        }
        /* offset points to the header we want to remove */
        /* remove the header by copying the remaining headers forwards */
        len = p_aux_parse_int( &header->aux_hdrs[offset], P_SAUX_HDRLEN );
        memmove( &header->aux_hdrs[offset], &header->aux_hdrs[offset+len],
                P_SAUX_HDR - offset - len );
        /* clear the released space (no undefined data in the header) */
        memset( &header->aux_hdrs[P_SAUX_HDR - len], 0, len );
        /* update nr_aux_data_recs */
        header->nr_aux_data_recs = (p_aux_get_total( header->aux_hdrs ) 
                                    + header->bytes_rec-1) / header->bytes_rec;
    }
    
    /* This modified the header. */
    header->modified = 1;

    return status;
} /* end of p_mod_rm_aux */


pT_status
p_get_aux (
        const pT_header *header,
        int              aux_id,
        int  * max_size,
        char * name,
        int  * descr_len,
        char * description )
{
    pT_status      status = P_OK;
    int i;
    int len = 0;
    int offset = 0;
    unsigned char * space;
 
    if ((aux_id < 0) || (aux_id >= p_get_num_aux(header))) {
        status = P_INVALID_AUXILIARY;
    }
    
    if (status == P_OK) {
        for (i=0; i<aux_id; i++) {
            len = p_aux_parse_int( &header->aux_hdrs[offset], P_SAUX_HDRLEN );
            offset += len;
        }
        /* offset points to the header we want to read */
        if (max_size != NULL) {
            *max_size = p_aux_parse_int( &header->aux_hdrs[offset+P_SAUX_HDRLEN+P_SAUX_NAME], P_SMAX_LEN );
        }
        if (name != NULL) {
            strncpy( name, (char *) &header->aux_hdrs[offset+P_SAUX_HDRLEN], (size_t)P_SAUX_NAME );
            name[P_SAUX_NAME] = '\0';
            /* Remove trailing spaces from name */
            space = (unsigned char *) strchr(name, ' ');
            if (space != NULL) {
                *space = '\0';
            }
        }
        len = p_aux_parse_int( &header->aux_hdrs[offset], P_SAUX_HDRLEN );
        if (descr_len != NULL) {
            *descr_len = len - P_SMIN_VALID;
        }
        if (description != NULL) {
            memcpy( description, &header->aux_hdrs[offset+P_SMIN_VALID], len-P_SMIN_VALID );
        }
    }
    
    return status;
} /* end of p_get_aux */


pT_status
p_read_aux (
        const char    * filename,
        pT_header     * header,
        int             frame,
        int             field,
        int             aux_id,
        int           * size,
        unsigned char * buf )
{
    pT_status       status = P_OK;
    int offset = 0;
    int len = 0;
    int max_size = 0;
    int image;
    int i;

    if ((aux_id < 0) || (aux_id >= p_get_num_aux(header))) {
        status = P_INVALID_AUXILIARY;
    }
    
    if (status == P_OK) {
        for (i=0; i<aux_id; i++) {
            len = p_aux_parse_int( &header->aux_hdrs[offset], P_SAUX_HDRLEN );
            offset += len;
        }
        /* offset points to the header we want to read */
        max_size = p_aux_parse_int( &header->aux_hdrs[offset+P_SAUX_HDRLEN+P_SAUX_NAME], P_SMAX_LEN );
    }
    if ((status == P_OK) && (max_size > 0)) {
        /* get offset within aux data records */
        offset = p_aux_get_data_offset( header->aux_hdrs, aux_id );
        /* read auxiliary data */
        if (field > 0) {
            image = 2 * (frame - 1) + field;
        } else {
            image = frame;
        }
        status = p_read_aux_data( filename, header, image, offset, size, buf );
    }
    else {
        *size = 0;
    }

    return status;
} /* end of p_read_aux */


pT_status
p_write_aux (
        const char    * filename,
        pT_header     * header,
        int             frame,
        int             field,
        int             aux_id,
        int             size,
        unsigned char * buf )
{
    pT_status      status = P_OK;
    int offset = 0;
    int len = 0;
    int max_size;
    int image;
    int i;

    if ((aux_id < 0) || (aux_id >= p_get_num_aux(header))) {
        status = P_INVALID_AUXILIARY;
    }
    
    if (status == P_OK) {
        for (i=0; i<aux_id; i++) {
            len = p_aux_parse_int( &header->aux_hdrs[offset], P_SAUX_HDRLEN );
            offset += len;
        }
        /* offset points to the header we want to read */
        max_size = p_aux_parse_int( &header->aux_hdrs[offset+P_SAUX_HDRLEN+P_SAUX_NAME], P_SMAX_LEN );
        if (size > max_size) {
            status = P_EXCEEDING_AUXILIARY_DATA_SIZE;
        }
    }

    if (status == P_OK) {
        /* get offset within aux data records */
        offset = p_aux_get_data_offset( header->aux_hdrs, aux_id );
        /* write auxiliary data */
        if (field > 0) {
            image = 2 * (frame - 1) + field;
        } else {
            image = frame;
        }
        status = p_write_aux_data( filename, header, image, offset, size, buf );
    }
    
    return status;
} /* end of p_write_aux */


/******************************************************************************/
