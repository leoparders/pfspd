/*
 *  Copyright (c) KONINKLIJKE PHILIPS ELECTRONICS N.V. 1997-2015
 *  Copyright (c) NXP Semiconductors Netherlands B.V. 2008-2010
 *  All rights reserved.
 *  For licensing and warranty information, see the file COPYING.LGPLv21.txt
 *  in the root directory of this package.
 *
 *  Philips Research Laboratories
 *  Eindhoven, The Netherlands
 *
 *  NXP Research
 *  Eindhoven, The Netherlands
 *
 *  CVS id      :  $Id: cpfspd_cce.c,v 1.28 2015/10/02 12:50:01 vleutenr Exp $
 *
 *  Name        :  cpfspd_cce.c
 *
 *  Author      :  Bram Riemens
 *
 *  Function    :  cpfspd ConvenienCE routines.
 *                        -        --
 *
 *  Description :  Read and write routines with type conversion
 *                 gain and offset handling.
 *
 *  Functions   :  The following external cpfspd functions are
 *                 defined in this file:
 *
 *                 - p_cce_write_comp()
 *                 - p_cce_read_comp()
 *                 - p_cce_read_float_xyz()
 *                 - p_cce_write_float_xyz()
 *
 */

/******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include "cpfspd.h"
#include "cpfspd_hdr.h"


/******************************************************************************/

#if UINT_MAX==4294967295
typedef unsigned int p_uint32;
#else
#error cpfspd requires 32 bit integers.
#endif

/* This union type is used to cast between IEEE 754 float and bitpattern */
typedef union {
    p_uint32    u32;
    float       f32;
} float_u32;

/******************************************************************************/

/* Convert 16 bit float to IEEE 754 float */
float
p_cce_f16_to_float( unsigned short f16 )
{
    p_uint32 sign = (p_uint32)(f16 & 0x8000u) << 16;
    signed int exponent = (signed int)((f16 >> 10) & 0x001F);
    p_uint32 mantissa = (p_uint32)(f16 & 0x03FFu);
    float_u32 bits; /* holds bitpattern of IEEE 754 float */

    if (exponent == 0x1F)
    {   /* infinity or NaN */
        exponent = 0xFF;
    }
    else if (exponent == 0)
    {   /* zero or unnormalized */
        if (mantissa != 0)
        {   /* not zero, normalize */
            exponent = 127 - 15;
            while ((mantissa & (1 << 10)) == 0)
            {
                exponent--;
                mantissa <<= 1;
            }
            exponent++;
            mantissa &= ~(1 << 10);
        }
    }
    else
    {   /* regular number */
        exponent += 127 - 15;
    }

    bits.u32 = sign | (((p_uint32)exponent) << 23) | (mantissa << 13);
    return bits.f32;
} /* end of p_cce_f16_to_float */


/* Convert IEEE 754 float to 16 bit float */
unsigned short
p_cce_float_to_f16( float f32 )
{
    unsigned short sign = 0;
    signed int exponent = 0;
    p_uint32 mantissa = 0;
    float_u32 bits; /* holds bitpattern of IEEE 754 float */

    bits.f32 = f32;
    sign = (unsigned short)((bits.u32 >> 16) & 0x00008000);
    exponent = (signed int)((bits.u32 >> 23) & 0x000000FF);
    mantissa = bits.u32 & 0x007FFFFF;

    if ((exponent == 0) && (mantissa == 0))
    {   /* zero: return +/- zero */
        return (unsigned short)(sign | 0 | 0);
    }
    if (exponent == 0xFF)
    {
        if (mantissa == 0)
        {   /* infinity */
            return (unsigned short)(sign | (0x1F << 10) | 0);
        }
        if ((mantissa >> 13) == 0)
        {   /* NaN */
            return (unsigned short)(sign | (0x1F << 10) | 1);
        }
        else
        {   /* NaN */
            return (unsigned short)(sign | (0x1F << 10) | (mantissa >> 13));
        }
    }
    exponent -= 127 - 15;
    if (exponent < -9) 
    {   /* underflow: return +/- zero */
        return (unsigned short)(sign | 0 | 0);
    }
    if  (exponent <= 0)
    {   /* denormalized */
        mantissa = (mantissa | 0x800000u) >> (1 - exponent);
        mantissa += 0x1000;     /* rounding */
        return (unsigned short)(sign | 0 | mantissa >> 13);
    }
    
    /* regular number */
    mantissa += 0x1000;     /* rounding */

    if (mantissa & 0x800000)
    {   /* overflow in mantissa; adjust exponent */
        mantissa = 0;
        exponent++;
    }
    if (exponent > 30)
    {   /* overflow; convert to infinity */
        return (unsigned short)(sign | (0x1F << 10) | 0);
    }
    return (unsigned short)(sign | (exponent << 10) | (mantissa >> 13));

} /* end of p_cce_float_to_f16 */


pT_status
p_cce_check_float_conversion( void )
{
    float f32 = 0;
    unsigned short f16 = 0;
    int i;

    int sof = sizeof(float);
    int sou = sizeof(p_uint32);
    if (sof != sou)
        return P_INCOMP_FLOAT_CONVERSION;

    f32 = p_cce_f16_to_float( 0xD140 ); /* -42 */
    if (f32 != -42.0f)  /* note: -42 can be exactly represented as float */
        return P_INCOMP_FLOAT_CONVERSION;

    f16 = p_cce_float_to_f16( 65504.0f );
    if (f16 != 0x7BFF)
        return P_INCOMP_FLOAT_CONVERSION;

    f16 = p_cce_float_to_f16( -65536.0f );
    if (f16 != 0xFC00)  /* -Inf */
        return P_INCOMP_FLOAT_CONVERSION;

    f16 = p_cce_float_to_f16( 0.0123f );
    if (f16 != 0x224C)
        return P_INCOMP_FLOAT_CONVERSION;

    for (i=0; i<=31744; i++) /* go up to inf; skip nan */
    {
        if (p_cce_float_to_f16(p_cce_f16_to_float((unsigned short)i)) != i)
        {   
            return P_INCOMP_FLOAT_CONVERSION;
        }
        if (p_cce_float_to_f16(p_cce_f16_to_float((unsigned short)(i | 32768))) != (i | 32768))
        {   
            return P_INCOMP_FLOAT_CONVERSION;
        }
    }

    return P_OK;
} /* end of p_cce_check_float_conversion */


pT_status
p_cce_read_comp (const char *filename, pT_header *header,
                 int frame, int field, int comp,
                 void *abuf, pT_buf_type atype,
                 int offset, int gain,
                 int width, int height, int stride)
{
    pT_status      status = P_OK;
    unsigned short *buf;
    char           comp_name[P_SCOM_CODE+1];
    pT_data_fmt    comp_fmt = -1;
    int            comp_pix_sub;
    int            comp_lin_sub;
    int            read_mode;
    int            px, py;

    buf = malloc(width * height * sizeof(unsigned short));
    if (buf == NULL) {
        status = P_MALLOC_FAILED;
    }

    if (status == P_OK) {
        status = p_get_comp(header, comp,
                            comp_name, &comp_fmt,
                            &comp_pix_sub, &comp_lin_sub);

        /* No shifting in component read routine,
         * just read the plain data into our "buf".
         * Scaling and shifting is handled by the offset
         * and gain parameters.
         */
        switch (comp_fmt) {
        case P_8_BIT_FILE:
            read_mode = P_8_BIT_MEM;
            break;
        case P_10_BIT_FILE:
            read_mode = P_10_BIT_MEM;
            break;
        case P_12_BIT_FILE:
            read_mode = P_12_BIT_MEM;
            break;
        case P_14_BIT_FILE:
            read_mode = P_14_BIT_MEM;
            break;
        case P_16_REAL_FILE:
            if (atype != P_FLOAT && atype != P_DOUBLE)
                return P_ILLEGAL_MEM_DATA_FORMAT;
            status = p_cce_check_float_conversion();
            if (status != P_OK)
                return status;
            /* FALLTHROUGH */
        case P_16_BIT_FILE:
            /* FALLTHROUGH */
        default:
            read_mode = P_16_BIT_MEM;
            break;
        }
    }

    if (status == P_OK) {
        if (field == 0) {
            status = p_read_frame_comp_16 (filename, header, frame,
                                           comp, buf, read_mode,
                                           width, height, width);
        } else {
            status = p_read_field_comp_16 (filename, header, frame, field,
                                           comp, buf, read_mode,
                                           width, height, width);
        }
    }

    if (status == P_OK) {
        switch (atype) {
        case P_FLOAT:
            if (comp_fmt == P_16_REAL_FILE) {
                for (py=0; py<height; py++) {
                    for (px=0; px<width; px++) {
                        ((float *)abuf)[py*stride+px] = ((float)p_cce_f16_to_float(buf[py*width+px])-offset) / gain;
                    }
                }
            } else {
                for (py=0; py<height; py++) {
                    for (px=0; px<width; px++) {
                        ((float *)abuf)[py*stride+px] = ((float)buf[py*width+px]-offset)/gain;
                    }
                }
            }
            break;
        case P_DOUBLE:
            if (comp_fmt == P_16_REAL_FILE) {
                for (py=0; py<height; py++) {
                    for (px=0; px<width; px++) {
                        ((double *)abuf)[py*stride+px] = ((double)p_cce_f16_to_float(buf[py*width+px])-offset) / gain;
                    }
                }
            } else {
                for (py=0; py<height; py++) {
                    for (px=0; px<width; px++) {
                        ((double *)abuf)[py*stride+px] = ((double)buf[py*width+px]-offset)/gain;
                    }
                }
            }
            break;
        case P_LONG:
            for (py=0; py<height; py++) {
                for (px=0; px<width; px++) {
                    ((long *)abuf)[py*stride+px] = (long)(((double)buf[py*width+px]-offset)/gain + 0.5);
                }
            }
            break;
        case P_ULONG:
            for (py=0; py<height; py++) {
                for (px=0; px<width; px++) {
                    ((unsigned long *)abuf)[py*stride+px] = (unsigned long)(((double)buf[py*width+px]-offset)/gain + 0.5);
                }
            }
            break;
        case P_INT:
            for (py=0; py<height; py++) {
                for (px=0; px<width; px++) {
                    ((int *)abuf)[py*stride+px] = (int)(((double)buf[py*width+px]-offset)/gain + 0.5);
                }
            }
            break;
        case P_UINT:
            for (py=0; py<height; py++) {
                for (px=0; px<width; px++) {
                    ((unsigned int *)abuf)[py*stride+px] = (unsigned int)(((double)buf[py*width+px]-offset)/gain + 0.5);
                }
            }
            break;
        case P_SHORT:
            for (py=0; py<height; py++) {
                for (px=0; px<width; px++) {
                    ((short *)abuf)[py*stride+px] = (short)(((double)buf[py*width+px]-offset)/gain + 0.5);
                }
            }
            break;
        case P_USHORT:
            for (py=0; py<height; py++) {
                for (px=0; px<width; px++) {
                    ((unsigned short *)abuf)[py*stride+px] = (unsigned short)(((double)buf[py*width+px]-offset)/gain + 0.5);
                }
            }
            break;
        case P_CHAR:
            for (py=0; py<height; py++) {
                for (px=0; px<width; px++) {
                    ((char *)abuf)[py*stride+px] = (char)(((double)buf[py*width+px]-offset)/gain + 0.5);
                }
            }
            break;
        case P_UCHAR:
            for (py=0; py<height; py++) {
                for (px=0; px<width; px++) {
                    ((unsigned char *)abuf)[py*stride+px] = (unsigned char)(((double)buf[py*width+px]-offset)/gain + 0.5);
                }
            }
            break;
        default:
            assert(0);
            break;
        }
    }

    if (buf != NULL) {
        free(buf);
    }

    return status;
} /* end of p_cce_read_comp */


pT_status
p_cce_write_comp (const char *filename, pT_header *header,
                  int frame, int field, int comp,
                  const void *abuf, pT_buf_type atype,
                  int offset, int gain,
                  int width, int height, int stride)
{
    pT_status      status = P_OK;
    unsigned short *buf;
    char           comp_name[P_SCOM_CODE+1];
    pT_data_fmt    comp_fmt = -1;
    int            comp_pix_sub;
    int            comp_lin_sub;
    int            write_mode;
    int            px, py;

    buf = malloc(width * height * sizeof(unsigned short));
    if (buf == NULL) {
        status = P_MALLOC_FAILED;
    }

    if (status == P_OK) {
        status = p_get_comp(header, comp,
                            comp_name, &comp_fmt,
                            &comp_pix_sub, &comp_lin_sub);

        /* No shifting in component write routine,
         * just write the plain data into our "buf".
         * Scaling and shifting is handled by the offset
         * and gain parameters.
         */
        switch (comp_fmt) {
        case P_8_BIT_FILE:
            write_mode = P_8_BIT_MEM;
            break;
        case P_10_BIT_FILE:
            write_mode = P_10_BIT_MEM;
            break;
        case P_12_BIT_FILE:
            write_mode = P_12_BIT_MEM;
            break;
        case P_14_BIT_FILE:
            write_mode = P_14_BIT_MEM;
            break;
        case P_16_REAL_FILE:
            if (atype != P_FLOAT && atype != P_DOUBLE)
                return P_ILLEGAL_MEM_DATA_FORMAT;
            status = p_cce_check_float_conversion();
            if (status != P_OK)
                return status;
            /* FALLTHROUGH */
        case P_16_BIT_FILE:
            /* FALLTHROUGH */
        default:
            write_mode = P_16_BIT_MEM;
            break;
        }
    }

    if (status == P_OK) {
        switch (atype) {
        case P_FLOAT:
            if (comp_fmt == P_16_REAL_FILE) {
                for (py=0; py<height; py++) {
                    for (px=0; px<width; px++) {
                        buf[py*width+px] = p_cce_float_to_f16( ((float *)abuf)[py*stride+px] * gain + offset);
                    }
                }
            } else {
                for (py=0; py<height; py++) {
                    for (px=0; px<width; px++) {
                        buf[py*width+px] = (unsigned short)((double)((float *)abuf)[py*stride+px] * gain + offset + 0.5);
                    }
                }
            }
            break;
        case P_DOUBLE:
            if (comp_fmt == P_16_REAL_FILE) {
                for (py=0; py<height; py++) {
                    for (px=0; px<width; px++) {
                        buf[py*width+px] = p_cce_float_to_f16( (float)(((double *)abuf)[py*stride+px] * gain + offset) );
                    }
                }
            } else {
                for (py=0; py<height; py++) {
                    for (px=0; px<width; px++) {
                        buf[py*width+px] = (unsigned short)(((double *)abuf)[py*stride+px] * gain + offset + 0.5);
                    }
                }
            }
            break;
        case P_LONG:
            for (py=0; py<height; py++) {
                for (px=0; px<width; px++) {
                    buf[py*width+px] = (unsigned short)((double)((long *)abuf)[py*stride+px] * gain + offset + 0.5);
                }
            }
            break;
        case P_ULONG:
            for (py=0; py<height; py++) {
                for (px=0; px<width; px++) {
                    buf[py*width+px] = (unsigned short)((double)((unsigned long *)abuf)[py*stride+px] * gain + offset + 0.5);
                }
            }
            break;
        case P_INT:
            for (py=0; py<height; py++) {
                for (px=0; px<width; px++) {
                    buf[py*width+px] = (unsigned short)((double)((int *)abuf)[py*stride+px] * gain + offset + 0.5);
                }
            }
            break;
        case P_UINT:
            for (py=0; py<height; py++) {
                for (px=0; px<width; px++) {
                    buf[py*width+px] = (unsigned short)((double)((unsigned int *)abuf)[py*stride+px] * gain + offset + 0.5);
                }
            }
            break;
        case P_SHORT:
            for (py=0; py<height; py++) {
                for (px=0; px<width; px++) {
                    buf[py*width+px] = (unsigned short)((double)((short *)abuf)[py*stride+px] * gain + offset + 0.5);
                }
            }
            break;
        case P_USHORT:
            for (py=0; py<height; py++) {
                for (px=0; px<width; px++) {
                    buf[py*width+px] = (unsigned short)((double)((unsigned short *)abuf)[py*stride+px] * gain + offset + 0.5);
                }
            }
            break;
        case P_CHAR:
            for (py=0; py<height; py++) {
                for (px=0; px<width; px++) {
                    buf[py*width+px] = (unsigned short)((double)((char *)abuf)[py*stride+px] * gain + offset + 0.5);
                }
            }
            break;
        case P_UCHAR:
            for (py=0; py<height; py++) {
                for (px=0; px<width; px++) {
                    buf[py*width+px] = (unsigned short)((double)((unsigned char *)abuf)[py*stride+px] * gain + offset + 0.5);
                }
            }
            break;
        default:
            assert(0);
            break;
        }
    }

    if (status == P_OK) {
        if (field == 0) {
            status = p_write_frame_comp_16 (filename, header, frame,
                                            comp, buf, write_mode,
                                            width, height, width);
        } else {
            status = p_write_field_comp_16 (filename, header, frame, field,
                                            comp, buf, write_mode,
                                            width, height, width);
        }
    }

    if (buf != NULL) {
        free(buf);
    }

    return status;
} /* end of p_cce_write_comp */


pT_status p_cce_read_float_xyz(
         const char *filename,
         pT_header *header,
         int frame,
         int field,
         float *r_or_x_frm,
         float *g_or_y_frm,
         float *b_or_z_frm,
         int width, int height, int stride )
{
    pT_status   status = P_OK;
    pT_color    color_format = -1;
    int         gain = 1;
    
    if (status == P_OK)
        status = p_check_color_format (header, &color_format);

    switch (color_format)
    {
        case P_COLOR_444_PL:
        case P_COLOR_RGB:
        case P_COLOR_XYZ:
            switch (p_get_file_data_format(header)) {
                case P_8_BIT_FILE:
                    gain = (1<<8) -1;   break;
                case P_10_BIT_FILE:
                    gain = (1<<10) -1;   break;
                case P_12_BIT_FILE:
                    gain = (1<<12) -1;   break;
                case P_14_BIT_FILE:
                    gain = (1<<14) -1;   break;
                case P_16_BIT_FILE:
                    gain = (1<<16) -1;   break;
                case P_16_REAL_FILE:
                    gain = 1;   break;
                default:
                    status = P_ILLEGAL_FILE_DATA_FORMAT;
            }
            if (status == P_OK)
                status = p_cce_read_comp( filename, header, frame, field, 0,
                        r_or_x_frm, P_FLOAT, 0, gain, width, height, stride );
            if (status == P_OK)
                status = p_cce_read_comp( filename, header, frame, field, 1,
                        g_or_y_frm, P_FLOAT, 0, gain, width, height, stride );
            if (status == P_OK)
                status = p_cce_read_comp( filename, header, frame, field, 2,
                        b_or_z_frm, P_FLOAT, 0, gain, width, height, stride );
            break;
        default:
            status = P_ILLEGAL_COLOR_FORMAT;
            break;
    }
    return status;
}   /* end of p_cce_read_float_xyz() */


pT_status p_cce_write_float_xyz(
         const char *filename,
         pT_header *header,
         int frame,
         int field,
         float *r_or_x_frm,
         float *g_or_y_frm,
         float *b_or_z_frm,
         int width, int height, int stride )
{
    pT_status   status = P_OK;
    pT_color    color_format = -1;
    int         gain = 1;
    
    if (status == P_OK)
        status = p_check_color_format (header, &color_format);

    switch (color_format)
    {
        case P_COLOR_444_PL:
        case P_COLOR_RGB:
        case P_COLOR_XYZ:
            switch (p_get_file_data_format(header)) {
                case P_8_BIT_FILE:
                    gain = (1<<8) -1;   break;
                case P_10_BIT_FILE:
                    gain = (1<<10) -1;   break;
                case P_12_BIT_FILE:
                    gain = (1<<12) -1;   break;
                case P_14_BIT_FILE:
                    gain = (1<<14) -1;   break;
                case P_16_BIT_FILE:
                    gain = (1<<16) -1;   break;
                case P_16_REAL_FILE:
                    gain = 1;   break;
                default:
                    status = P_ILLEGAL_FILE_DATA_FORMAT;
            }
            if (status == P_OK)
                status = p_cce_write_comp( filename, header, frame, field, 0,
                        r_or_x_frm, P_FLOAT, 0, gain, width, height, stride );
            if (status == P_OK)
                status = p_cce_write_comp( filename, header, frame, field, 1,
                        g_or_y_frm, P_FLOAT, 0, gain, width, height, stride );
            if (status == P_OK)
                status = p_cce_write_comp( filename, header, frame, field, 2,
                        b_or_z_frm, P_FLOAT, 0, gain, width, height, stride );
            break;
        default:
            status = P_ILLEGAL_COLOR_FORMAT;
            break;
    }
    return status;
}   /* end of p_cce_write_float_xyz() */
