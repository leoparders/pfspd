/*
 *  Copyright (c) KONINKLIJKE PHILIPS ELECTRONICS N.V. 1997-2010
 *  All rights reserved.
 *  For licensing and warranty information, see the file COPYING.LGPLv21.txt
 *  in the root directory of this package.
 *
 *  Philips Research Laboratories
 *  Eindhoven, The Netherlands
 *
 *  CVS id      :  $Id: cpfspd_low.h,v 2.15 2010/01/05 14:18:32 riemens Exp $
 *
 *  Name        :  cpfspd_low.h
 *
 *  Authors     :  Robert Jan Schutten
 *                 Bram Riemens
 *
 *  Function    :  Header file for cpfspd_low.c
 *
 */

/******************************************************************************/

#ifndef CPFSPD_LOW_H
#define CPFSPD_LOW_H

/* mandatory include of stdio.h; because of use of (FILE *) */
#include <stdio.h>
/* mandatory include of cpfspd.h; because of used typedefs */
#include "cpfspd.h"

/* Auxiliary data records            */

#define P_SDATA_LEN             (8)     /* actual data length */

/* basic low level i/o functions */

#define P_UNSIGNED_CHAR         8
#define P_UNSIGNED_SHORT        16

extern pT_status  p_read_hdr 
        (const char *filename, pT_header *header, 
         FILE *stream_error, int print_error);

extern pT_status  p_write_hdr 
        (const char *filename, pT_header *header, 
         FILE *stream_error, int print_error, int rewrite);

extern pT_status  p_read_image 
        (const char *filename, pT_header *header, 
         int nr, int comp_nr, 
         void *mem_buffer,
         int mem_type,        /* unsigned char = 8, unsigned short = 16 */
         int mem_data_fmt,    /* see description in cpfspd.h            */
         int width,           /* width, height & stride:                */
         int height,          /*   define the buffer size to            */
         int stride,          /*   store the data in                    */
         FILE *stream_error, int print_error);

extern pT_status  p_write_image 
        (const char *filename, pT_header *header, 
         int nr, int comp_nr, 
         const void *mem_buffer, 
         int mem_type,        /* unsigned char = 8, unsigned short = 16 */
         int mem_data_fmt,    /* see description in cpfspd.h            */
         int width,           /* width, height & stride:                */
         int height,          /*   define the buffer size to            */
         int stride,          /*   store the data in                    */
         FILE *stream_error, int print_error);

extern pT_status p_read_aux_data (
        const char    * filename,
        pT_header     * header,
        int             image_no,
        int             data_offset, /* offset within aux data records */
        int           * size,   /* actual data length */
        unsigned char * buf );  /* buffer must be aux_header.max_size long */

extern pT_status p_write_aux_data (
        const char    * filename,
        pT_header     * header,
        int             image_no,
        int             data_offset, /* offset within aux data records */
        int             size,   /* actual data length */
        unsigned char * buf );  /* data buffer */


#endif /* end of #ifndef CPFSPD_LOW_H */

/******************************************************************************/
