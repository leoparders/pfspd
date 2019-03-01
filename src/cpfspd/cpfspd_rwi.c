/*
 *  Copyright (c) KONINKLIJKE PHILIPS ELECTRONICS N.V. 1997-2010
 *  All rights reserved.
 *  For licensing and warranty information, see the file COPYING.LGPLv21.txt
 *  in the root directory of this package.
 *
 *  Philips Research Laboratories
 *  Eindhoven, The Netherlands
 *
 *  CVS id      :  $Id: cpfspd_rwi.c,v 2.13 2010/01/05 14:18:32 riemens Exp $
 *
 *  Name        :  cpfspd_rwi.c
 *
 *  Authors     :  Robert Jan Schutten
 *                 Bram Riemens
 *
 *  Function    :  Read/Write Image functions of cpfspd.
 *                 -    -     -
 *
 *  Description :  All image data read/write functions are in this file.
 *                 Note that at these functions map to single low level
 *                 read and write function.
 *
 *  Functions   :  The following external cpfspd functions are
 *                 defined in this file:
 *
 *                 - p_read_field()
 *                 - p_read_frame()
 *                 - p_write_field()
 *                 - p_write_frame()
 *                 - p_read_field_16()
 *                 - p_read_frame_16()
 *                 - p_write_field_16()
 *                 - p_write_frame_16()
 *                 - p_read_field_planar()
 *                 - p_read_frame_planar()
 *                 - p_write_field_planar()
 *                 - p_write_frame_planar()
 *                 - p_read_field_planar_16()
 *                 - p_read_frame_planar_16()
 *                 - p_write_field_planar_16()
 *                 - p_write_frame_planar_16()
 *                 - p_read_field_comp()
 *                 - p_read_frame_comp()
 *                 - p_write_field_comp()
 *                 - p_write_frame_comp()
 *                 - p_read_field_comp_16()
 *                 - p_read_frame_comp_16()
 *                 - p_write_field_comp_16()
 *                 - p_write_frame_comp_16()
 *
 */

/******************************************************************************/

#include "cpfspd.h"
#include "cpfspd_low.h"

/******************************************************************************/

/* switch off debug & error printing */
#define NOPRINT         0

/* mask to extract component_mode from read_mode */
#define P_COMPONENT_MODE_MASK   7u

/* mask to extract mem_data_fmt from read_mode */
#define P_MEM_DATA_FMT_MASK     112u

/* special value for comp parameter; can also hold the component no */
#define P_NORMAL_COMP           (-1)

/******************************************************************************/

/*
 * Generic lowlevel read/write functions
 */

/* read an image */

static pT_status
p_read_buffers (const char *filename,
                pT_header *header,
                pT_color color_format,
                int frame,
                int field,
                int comp,
                int read_field, /* 0=read_frame; 1=read_field */
                void *buf_0,
                void *buf_1,
                void *buf_2,
                int mem_type,
                int read_mode,
                int width,
                int height,     /* frm_height for frame;
                                   fld_height for field */
                int stride_0,
                int stride_1,
                int stride_2)
{
    pT_status status = P_OK;
    int       component_mode  = 0;
    int       mem_data_fmt = 0;
    int       read_0 = 0;
    int       read_1 = 0;
    int       read_2 = 0;
    int       comp_0 = 0;
    int       width_0 = 0;
    int       width_1 = 0;
    int       width_2 = 0;
    int       height_0 = 0;
    int       height_1 = 0;
    int       height_2 = 0;
    int       image_number;

    /* extract component mode & mem_data_fmt from read_mode */
    component_mode = (int)((unsigned int)read_mode & P_COMPONENT_MODE_MASK);
    mem_data_fmt   = (int)((unsigned int)read_mode & P_MEM_DATA_FMT_MASK);

    if (comp != P_NORMAL_COMP) {
        /* read single component (low level read interface) */
        read_0 = 1; read_1 = 0; read_2 = 0;
        comp_0 = comp;
    } else {
        /* check color_format/component_mode & set components to read */
        switch (color_format) {
        case P_NO_COLOR:
            switch (component_mode) {
            case P_READ_Y:
                read_0 = 1; read_1 = 0; read_2 = 0;
                break;
            case P_READ_ALL:
            case P_READ_UV:
            case P_READ_U:
            case P_READ_V:
                status = P_READ_CHR_FROM_LUM_ONLY;
                read_0 = 0; read_1 = 0; read_2 = 0;
                break;
            default:
                status = P_READ_RGB_FROM_LUM_ONLY;
                read_0 = 0; read_1 = 0; read_2 = 0;
                break;
            } /* end of switch (component_mode) */
            break; /* end of case P_NO_COLOR */
        case P_COLOR_422:
        case P_COLOR_420:
            switch (component_mode) {
            case P_READ_ALL:
                read_0 = 1; read_1 = 1; read_2 = 0;
                break;
            case P_READ_Y:
                read_0 = 1; read_1 = 0; read_2 = 0;
                break;
            case P_READ_UV:
                read_0 = 0; read_1 = 1; read_2 = 0;
                break;
            case P_READ_U:
            case P_READ_V:
                status = P_READ_PLANAR_CHR_FROM_MULT_CHR;
                read_0 = 0; read_1 = 0; read_2 = 0;
                break;
            default:
                status = P_READ_RGB_FROM_YUV;
                read_0 = 0; read_1 = 0; read_2 = 0;
                break;
            } /* end of switch (component_mode) */
            break; /* end of case P_COLOR_422, P_COLOR_420 */
        case P_COLOR_444_PL:
        case P_COLOR_422_PL:
        case P_COLOR_420_PL:
            switch (component_mode) {
            case P_READ_ALL:
                read_0 = 1; read_1 = 1; read_2 = 1;
                break;
            case P_READ_Y:
                read_0 = 1; read_1 = 0; read_2 = 0;
                break;
            case P_READ_UV:
                read_0 = 0; read_1 = 1; read_2 = 1;
                break;
            case P_READ_U:
                read_0 = 0; read_1 = 1; read_2 = 0;
                break;
            case P_READ_V:
                read_0 = 0; read_1 = 0; read_2 = 1;
                break;
            default:
                status = P_READ_RGB_FROM_YUV;
                read_0 = 0; read_1 = 0; read_2 = 0;
                break;
            } /* end of switch (component_mode) */
            break; /* end of case P_COLOR_444_PL, P_COLOR_422_PL, P_COLOR_420_PL */
        case P_COLOR_RGB:
        case P_COLOR_XYZ:
            switch (component_mode) {
            case P_READ_ALL:
                read_0 = 1; read_1 = 1; read_2 = 1;
                break;
            case P_READ_R:
                read_0 = 1; read_1 = 0; read_2 = 0;
                break;
            case P_READ_G:
                read_0 = 0; read_1 = 1; read_2 = 0;
                break;
            case P_READ_B:
                read_0 = 0; read_1 = 0; read_2 = 1;
                break;
            case P_READ_Y: /* for backwards compatibility */
                read_0 = 1; read_1 = 1; read_2 = 1;
                break;
            default:
                status = P_READ_CHR_FROM_RGB;
                read_0 = 0; read_1 = 0; read_2 = 0;
                break;
            } /* end of switch (component_mode) */
            break; /* end of case P_COLOR_RGB */
        case P_STREAM:
            switch (component_mode) {
            case P_READ_ALL:
                read_0 = 1; read_1 = 0; read_2 = 0;
                break;
            case P_READ_Y: /* for backwards compatibility */
                read_0 = 1; read_1 = 0; read_2 = 0;
                break;
            case P_READ_UV:
            case P_READ_U:
            case P_READ_V:
                status = P_READ_CHR_FROM_STREAM;
                read_0 = 0; read_1 = 0; read_2 = 0;
                break;
            default:
                status = P_READ_RGB_FROM_STREAM;
                read_0 = 0; read_1 = 0; read_2 = 0;
                break;
            } /* end of switch (component_mode) */
            break; /* end of case P_STREAM */
        default:
            read_0 = 0; read_1 = 0; read_2 = 0;
            break;
        } /* end of switch (color_format) */
    }

    /* set component width & height */
    if (read_0) {
        width_0  = width  / header->comp[0].pix_sbsmpl;
        height_0 = height / header->comp[0].lin_sbsmpl;
    } /* end of if if (read_0) */
    if (read_1) {
        width_1  = width  / header->comp[1].pix_sbsmpl;
        height_1 = height / header->comp[1].lin_sbsmpl;
    } /* end of if if (read_1) */
    if (read_2) {
        width_2  = width  / header->comp[2].pix_sbsmpl;
        height_2 = height / header->comp[2].lin_sbsmpl;
    } /* end of if if (read_2) */

    /* for multiplexed color formats we need to double width_1 */
    if ((color_format == P_COLOR_422) ||
        (color_format == P_COLOR_420)) {
        width_1 *= 2;
    } /* end of if ((color_format == ... */

    if (read_field) {
        image_number = 2 * (frame - 1) + field;
    } else {
        image_number = frame;
    } /* end of if (read_field) */

    /* read buffers */
    if ((status == P_OK) && read_0) {
        status = p_read_image (filename, header,
                               image_number, comp_0,
                               buf_0,
                               mem_type, mem_data_fmt,
                               width_0, height_0, stride_0,
                               stderr, NOPRINT);
    }  /* end of if ((status == P_OK) && read_0) */
    if ((status == P_OK) && read_1) {
        status = p_read_image (filename, header,
                               image_number, 1,
                               buf_1,
                               mem_type, mem_data_fmt,
                               width_1, height_1, stride_1,
                               stderr, NOPRINT);
    }  /* end of if ((status == P_OK) && read_1) */
    if ((status == P_OK) && read_2) {
        status = p_read_image (filename, header,
                               image_number, 2,
                               buf_2,
                               mem_type, mem_data_fmt,
                               width_2, height_2, stride_2,
                               stderr, NOPRINT);
    }  /* end of if ((status == P_OK) && read_2) */

    return status;
} /* end of p_read_buffers */

/* write image */

static pT_status
p_write_buffers (const char *filename,
                 pT_header *header,
                 pT_color color_format,
                 int frame,
                 int field,
                 int comp,
                 int write_field, /* 0=read_frame; 1=read_field*/
                 const void *buf_0,
                 const void *buf_1,
                 const void *buf_2,
                 int mem_type,
                 int write_mode,
                 int width,
                 int height,      /* frm_height for frame;
                                     fld_height for field */
                 int stride_0,
                 int stride_1,
                 int stride_2)
{
    pT_status status = P_OK;
    int       mem_data_fmt = 0;
    int       write_0  = 0;
    int       write_1  = 0;
    int       write_2  = 0;
    int       comp_0   = 0;
    int       width_0  = 0;
    int       width_1  = 0;
    int       width_2  = 0;
    int       height_0 = 0;
    int       height_1 = 0;
    int       height_2 = 0;
    int       image_number;

    /* extract mem_data_fmt from write_mode */
    mem_data_fmt = (int)((unsigned int)write_mode & P_MEM_DATA_FMT_MASK);

    if (comp != P_NORMAL_COMP) {
        /* write single component (low level read interface) */
        write_0 = 1; write_1 = 0; write_2 = 0;
        comp_0 = comp;
    } else {
        /* check color_format & set components to write */
        switch (color_format) {
        case P_NO_COLOR:
        case P_STREAM:
            write_0 = 1; write_1 = 0; write_2 = 0;
            break;
        case P_COLOR_422:
        case P_COLOR_420:
            write_0 = 1; write_1 = 1; write_2 = 0;
            break;
        case P_COLOR_444_PL:
        case P_COLOR_422_PL:
        case P_COLOR_420_PL:
        case P_COLOR_RGB:
        case P_COLOR_XYZ:
            write_0 = 1; write_1 = 1; write_2 = 1;
            break;
        default:
            write_0 = 0; write_1 = 0; write_2 = 0;
            break;
        } /* end of switch (color_format) */
    }

    /* set component width & height */
    if (write_0) {
        width_0  = width  / header->comp[0].pix_sbsmpl;
        height_0 = height / header->comp[0].lin_sbsmpl;
    } /* end of if if (write_0) */
    if (write_1) {
        width_1  = width  / header->comp[1].pix_sbsmpl;
        height_1 = height / header->comp[1].lin_sbsmpl;
    } /* end of if if (write_1) */
    if (write_2) {
        width_2  = width  / header->comp[2].pix_sbsmpl;
        height_2 = height / header->comp[2].lin_sbsmpl;
    } /* end of if if (write_2) */

    /* for multiplexed color formats we need to double width_1 */
    if ((color_format == P_COLOR_422) ||
        (color_format == P_COLOR_420)) {
        width_1 *= 2;
    } /* end of if ((color_format == ... */

    if (write_field) {
        image_number = 2 * (frame - 1) + field;
    } else {
        image_number = frame;
    } /* end of if (write_field) */

    /* write buffers */
    if ((status == P_OK) && write_0) {
        status = p_write_image (filename, header,
                               image_number, comp_0,
                               buf_0,
                               mem_type, mem_data_fmt,
                               width_0, height_0, stride_0,
                               stderr, NOPRINT);
    }  /* end of if ((status == P_OK) && write_0) */
    if ((status == P_OK) && write_1) {
        status = p_write_image (filename, header,
                               image_number, 1,
                               buf_1,
                               mem_type, mem_data_fmt,
                               width_1, height_1, stride_1,
                               stderr, NOPRINT);
    }  /* end of if ((status == P_OK) && write_1) */
    if ((status == P_OK) && write_2) {
        status = p_write_image (filename, header,
                               image_number, 2,
                               buf_2,
                               mem_type, mem_data_fmt,
                               width_2, height_2, stride_2,
                               stderr, NOPRINT);
    }  /* end of if ((status == P_OK) && write_2) */

    return status;
} /* end of p_write_buffers */

/******************************************************************************/

/*
 * Utility functions
 */

/* get strides of all components */

static void
p_get_strides (pT_color color_format,
               int stride,
               int uv_stride,
               int *stride_0,
               int *stride_1,
               int *stride_2)
{
    /* for all YUV color_formats:
       if header->u_v_stride is set use this as stride for U & V */
    switch (color_format) {
    case P_COLOR_422:
    case P_COLOR_420:
    case P_COLOR_444_PL:
    case P_COLOR_422_PL:
    case P_COLOR_420_PL:
        if (uv_stride != 0) {
            (*stride_0) = stride;
            (*stride_1) = uv_stride;
            (*stride_2) = uv_stride;
        } else {
            (*stride_0) = stride;
            (*stride_1) = stride;
            (*stride_2) = stride;
        }
        break;
    default:
        (*stride_0) = stride;
        (*stride_1) = stride;
        (*stride_2) = stride;
    }
    return;
} /* end of p_get_strides */

/******************************************************************************/

/*
 * Generic read/write functions for field & frame
 */

/* read a field */

static pT_status
p_read_field_all (const char *filename,
                  pT_header *header,
                  pT_color color_format,
                  int frame,
                  int field,
                  int comp,
                  void *buf_0,
                  void *buf_1,
                  void *buf_2,
                  int mem_type,
                  int read_mode,
                  int width,
                  int fld_height,
                  int stride,
                  int uv_stride)
{
    pT_status      status = P_OK;
    int            stride_0;
    int            stride_1;
    int            stride_2;

    /* get strides */
    p_get_strides (color_format, stride, uv_stride,
                   &stride_0, &stride_1, &stride_2);

    /* check if header is progressive */
    if (p_is_progressive (header)) {
        status = P_SHOULD_BE_INTERLACED;
    } /* end of if (p_is_progressive (header)) */

    if (status == P_OK) {
        status = p_read_buffers (filename, header, color_format,
                                 frame, field, comp, 1,
                                 buf_0, buf_1, buf_2,
                                 mem_type, read_mode,
                                 width, fld_height,
                                 stride_0, stride_1, stride_2);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_read_field_all */

/* read a frame */

static pT_status
p_read_frame_all (const char *filename,
                  pT_header *header,
                  pT_color color_format,
                  int frame,
                  int comp,
                  void *buf_0,
                  void *buf_1,
                  void *buf_2,
                  int mem_type,
                  int read_mode,
                  int width,
                  int frm_height,
                  int stride,
                  int uv_stride)
{
    pT_status      status = P_OK;
    int            stride_0;
    int            stride_1;
    int            stride_2;
    void          *second_buf_0;
    void          *second_buf_1;
    void          *second_buf_2;

    /* get strides */
    p_get_strides (color_format, stride, uv_stride,
                   &stride_0, &stride_1, &stride_2);

    if (p_is_interlaced (header)) {
        /* file is interlaced */
        /* use p_read_buffers twice to access individual fields */
        status = p_read_buffers (filename, header, color_format,
                                 frame, 1, comp, 1,
                                 buf_0, buf_1, buf_2,
                                 mem_type, read_mode,
                                 width, frm_height/2,
                                 2*stride_0, 2*stride_1, 2*stride_2);
        if (status == P_OK) {
            switch (mem_type) {
            case P_UNSIGNED_CHAR:
                second_buf_0 = (void*)((unsigned char*)buf_0 + stride_0);
                second_buf_1 = (void*)((unsigned char*)buf_1 + stride_1);
                second_buf_2 = (void*)((unsigned char*)buf_2 + stride_2);
                break;
            case P_UNSIGNED_SHORT:
                second_buf_0 = (void*)((unsigned short*)buf_0 + stride_0);
                second_buf_1 = (void*)((unsigned short*)buf_1 + stride_1);
                second_buf_2 = (void*)((unsigned short*)buf_2 + stride_2);
                break;
            default:
                status = P_UNKNOWN_MEM_TYPE;
                second_buf_0 = NULL;
                second_buf_1 = NULL;
                second_buf_2 = NULL;
                break;
            } /* end of switch (mem_type) */
            if (status == P_OK) {
                status = p_read_buffers
                        (filename, header, color_format,
                         frame, 2, comp, 1,
                         second_buf_0, second_buf_1, second_buf_2,
                         mem_type, read_mode,
                         width, frm_height/2,
                         2*stride_0, 2*stride_1, 2*stride_2);
            } /* end of if (status == P_OK) */
        } /* end of if (status == P_OK) */
    } else {
        /* file is progressive */
        status = p_read_buffers (filename, header, color_format,
                                 frame, 0, comp, 0,
                                 buf_0, buf_1, buf_2,
                                 mem_type, read_mode,
                                 width, frm_height,
                                 stride_0, stride_1, stride_2);
    } /* end of if (p_is_interlaced (header)) */

    return status;
} /* end of p_read_frame_all */

/* write a field */

static pT_status
p_write_field_all (const char *filename,
                   pT_header *header,
                   const pT_color color_format,
                   int frame,
                   int field,
                   int comp,
                   const void *buf_0,
                   const void *buf_1,
                   const void *buf_2,
                   int mem_type,
                   int write_mode,
                   int width,
                   int fld_height,
                   int stride,
                   int uv_stride)
{
    pT_status      status = P_OK;
    int            stride_0;
    int            stride_1;
    int            stride_2;

    /* get strides */
    p_get_strides (color_format, stride, uv_stride,
                   &stride_0, &stride_1, &stride_2);

    /* check if header is progressive */
    if (p_is_progressive (header)) {
        status = P_SHOULD_BE_INTERLACED;
    } /* end of if (p_is_progressive (header)) */

    if (status == P_OK) {
        status = p_write_buffers (filename, header, color_format,
                                  frame, field, comp, 1,
                                  buf_0, buf_1, buf_2,
                                  mem_type, write_mode,
                                  width, fld_height,
                                  stride_0, stride_1, stride_2);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_write_field_all */

/* write a frame */

static pT_status
p_write_frame_all (const char *filename,
                   pT_header *header,
                   pT_color color_format,
                   int frame,
                   int comp,
                   const void *buf_0,
                   const void *buf_1,
                   const void *buf_2,
                   int mem_type,
                   int write_mode,
                   int width,
                   int frm_height,
                   int stride,
                   int uv_stride)
{
    pT_status      status = P_OK;
    int            stride_0;
    int            stride_1;
    int            stride_2;
    void          *second_buf_0;
    void          *second_buf_1;
    void          *second_buf_2;

    /* get strides */
    p_get_strides (color_format, stride, uv_stride,
                   &stride_0, &stride_1, &stride_2);

    if (p_is_interlaced (header)) {
        /* file is interlaced */
        /* use p_write_buffers twice to access individual fields */
        status = p_write_buffers (filename, header, color_format,
                                  frame, 1, comp, 1,
                                  buf_0, buf_1, buf_2,
                                  mem_type, write_mode,
                                  width, frm_height/2,
                                  2*stride_0, 2*stride_1, 2*stride_2);
        if (status == P_OK) {
            switch (mem_type) {
            case P_UNSIGNED_CHAR:
                second_buf_0 =
                    buf_0 ? (void*)((unsigned char*)buf_0 + stride_0) : NULL;
                second_buf_1 =
                    buf_1 ? (void*)((unsigned char*)buf_1 + stride_1) : NULL;
                second_buf_2 =
                    buf_2 ? (void*)((unsigned char*)buf_2 + stride_2) : NULL;
                break;
            case P_UNSIGNED_SHORT:
                second_buf_0 =
                    buf_0 ? (void*)((unsigned short*)buf_0 + stride_0) : NULL;
                second_buf_1 =
                    buf_1 ? (void*)((unsigned short*)buf_1 + stride_1) : NULL;
                second_buf_2 =
                    buf_2 ? (void*)((unsigned short*)buf_2 + stride_2) : NULL;
                break;
            default:
                status = P_UNKNOWN_MEM_TYPE;
                second_buf_0 = NULL;
                second_buf_1 = NULL;
                second_buf_2 = NULL;
                break;
            } /* end of switch (mem_type) */
            if (status == P_OK) {
                status = p_write_buffers
                        (filename, header, color_format,
                         frame, 2, comp, 1,
                         second_buf_0, second_buf_1, second_buf_2,
                         mem_type, write_mode,
                         width, frm_height/2,
                         2*stride_0, 2*stride_1, 2*stride_2);
            } /* end of if (status == P_OK) */
        } /* end of if (status == P_OK) */
    } else {
        /* file is progressive */
        status = p_write_buffers (filename, header, color_format,
                                  frame, 0, comp, 0,
                                  buf_0, buf_1, buf_2,
                                  mem_type, write_mode,
                                  width, frm_height,
                                  stride_0, stride_1, stride_2);
    } /* end of if (p_is_interlaced (header)) */

    return status;
} /* end of p_write_frame_all */

/******************************************************************************/

/*
 * Utility functions
 */

/* check if header has been modified */

static pT_status
p_check_modified (const pT_header *header)
{
    pT_status      status = P_OK;
    if (header->modified == 1) {
        status = P_HEADER_IS_MODIFIED;
    }
    return status;
} /* end of p_check_modified */

/* check if file is in multiplexed format or
   has luminance only */

static pT_status
p_check_multiplexed (pT_color color_format)
{
    pT_status      status = P_OK;
    if ((color_format != P_NO_COLOR) &&
        (color_format != P_COLOR_422) &&
        (color_format != P_COLOR_420)) {
        status = P_INCOMP_MULT_COLOR_FORMAT;
    } /* end of if (color_format != ... */
    return status;
} /* end of p_check_multiplexed */

/* check if file is in multiplexed format,
   stream format or has luminance only */

static pT_status
p_check_multi_or_stream (pT_color color_format)
{
    pT_status      status = P_OK;
    if ((color_format != P_NO_COLOR) &&
        (color_format != P_COLOR_422) &&
        (color_format != P_COLOR_420) &&
        (color_format != P_STREAM)) {
        status = P_INCOMP_MULT_COLOR_FORMAT;
    } /* end of if (color_format != ... */
    return status;
} /* end of p_check_multi_or_stream */

/* check if file is in planar format or
   rgb format */

static pT_status
p_check_planar (pT_color color_format)
{
    pT_status      status = P_OK;
    if ((color_format != P_NO_COLOR) &&
        (color_format != P_COLOR_444_PL) &&
        (color_format != P_COLOR_422_PL) &&
        (color_format != P_COLOR_420_PL) &&
        (color_format != P_COLOR_RGB) &&
        (color_format != P_COLOR_XYZ) ) {
        status = P_INCOMP_PLANAR_COLOR_FORMAT;
    } /* end of if (color_format != ... */
    return status;
} /* end of p_check_planar */

/* Check valid component id */

static pT_status
p_check_comp (const pT_header *header, int comp, int reading)
{
    pT_status      status = P_OK;
    if ((comp < 0) || (comp >= header->nr_compon)) {
        if (reading) {
            status = P_READ_INVALID_COMPONENT;
        } else {
            status = P_WRITE_INVALID_COMPONENT;
        }
    }
    return status;
} /* end of p_check_comp */

/******************************************************************************/

/*
 * External read/write functions
 */

/*
 * Multiplexed luminance/chrominance (YUV) or streaming (S) files.
 *
 * Supported color_format: P_NO_COLOR,
 *                         P_COLOR_422,
 *                         P_COLOR_420,
 *                         P_STREAM.
 *
 */

/* p_read_field */
pT_status
p_read_field (const char *filename, pT_header *header,
              int frame, int field,
              unsigned char *y_fld,
              unsigned char *uv_fld,
              int read_mode,
              int width, int fld_height, int stride)
{
    pT_status      status = P_OK;
    const pT_color color_format = p_get_color_format (header);

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check if header is valid */
    if (status == P_OK) {
        status = p_check_header (header);
    } /* end of if (status == P_OK) */

    /* check if file is in multiplexed format */
    if (status == P_OK) {
        status = p_check_multiplexed (color_format);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_read_field_all (filename, header, color_format,
                                   frame, field, P_NORMAL_COMP,
                                   (void *)y_fld, (void *)uv_fld, NULL,
                                   P_UNSIGNED_CHAR, read_mode,
                                   width, fld_height, stride, 0);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_read_field */

/* p_read_frame */
pT_status
p_read_frame (const char *filename, pT_header *header,
              int frame,
              unsigned char *y_or_s_frm,
              unsigned char *uv_frm,
              int read_mode,
              int width, int frm_height, int stride)
{
    pT_status      status = P_OK;
    const pT_color color_format = p_get_color_format (header);

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check if header is valid */
    if (status == P_OK) {
        status = p_check_header (header);
    } /* end of if (status == P_OK) */

    /* check if file is in multiplexed or stream format */
    if (status == P_OK) {
        status = p_check_multi_or_stream (color_format);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_read_frame_all (filename, header, color_format,
                                   frame, P_NORMAL_COMP,
                                   (void *)y_or_s_frm, (void *)uv_frm, NULL,
                                   P_UNSIGNED_CHAR, read_mode,
                                   width, frm_height, stride, 0);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_read_frame */

/* p_write_field */
pT_status
p_write_field (const char *filename, pT_header *header,
               int frame, int field,
               const unsigned char *y_fld,
               const unsigned char *uv_fld,
               int width, int fld_height, int stride)
{
    pT_status      status = P_OK;
    const pT_color color_format = p_get_color_format (header);

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check if header is valid */
    if (status == P_OK) {
        status = p_check_header (header);
    } /* end of if (status == P_OK) */

    /* check if file is in multiplexed or stream format */
    if (status == P_OK) {
        status = p_check_multiplexed (color_format);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_write_field_all (filename, header, color_format,
                                    frame, field, P_NORMAL_COMP,
                                    (const void *)y_fld, (const void *)uv_fld, NULL,
                                    P_UNSIGNED_CHAR, P_8_BIT_MEM,
                                    width, fld_height, stride, 0);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_write_field */

/* p_write_frame */
pT_status
p_write_frame (const char *filename, pT_header *header,
               int frame,
               const unsigned char *y_or_s_frm,
               const unsigned char *uv_frm,
               int width, int frm_height, int stride)
{
    pT_status      status = P_OK;
    const pT_color color_format = p_get_color_format (header);

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check if header is valid */
    if (status == P_OK) {
        status = p_check_header (header);
    } /* end of if (status == P_OK) */

    /* check if file is in multiplexed format */
    if (status == P_OK) {
        status = p_check_multi_or_stream (color_format);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_write_frame_all (filename, header, color_format,
                                    frame, P_NORMAL_COMP,
                                    (const void *)y_or_s_frm, (const void *)uv_frm, NULL,
                                    P_UNSIGNED_CHAR, P_8_BIT_MEM,
                                    width, frm_height, stride, 0);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_write_frame */

/* p_read_field_16 */
pT_status
p_read_field_16 (const char *filename, pT_header *header,
                 int frame, int field,
                 unsigned short *y_fld,
                 unsigned short *uv_fld,
                 int read_mode,
                 int width, int fld_height, int stride)
{
    pT_status      status = P_OK;
    const pT_color color_format = p_get_color_format (header);

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check if header is valid */
    if (status == P_OK) {
        status = p_check_header (header);
    } /* end of if (status == P_OK) */

    /* check if file is in multiplexed format */
    if (status == P_OK) {
        status = p_check_multiplexed (color_format);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_read_field_all (filename, header, color_format,
                                   frame, field, P_NORMAL_COMP,
                                   (void *)y_fld, (void *)uv_fld, NULL,
                                   P_UNSIGNED_SHORT, read_mode,
                                   width, fld_height, stride, 0);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_read_field_16 */

/* p_read_frame_16 */
pT_status
p_read_frame_16 (const char *filename, pT_header *header,
                 int frame,
                 unsigned short *y_or_s_frm,
                 unsigned short *uv_frm,
                 int read_mode,
                 int width, int frm_height, int stride)
{
    pT_status      status = P_OK;
    const pT_color color_format = p_get_color_format (header);

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check if header is valid */
    if (status == P_OK) {
        status = p_check_header (header);
    } /* end of if (status == P_OK) */

    /* check if file is in multiplexed or stream format */
    if (status == P_OK) {
        status = p_check_multi_or_stream (color_format);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_read_frame_all (filename, header, color_format,
                                   frame, P_NORMAL_COMP,
                                   (void *)y_or_s_frm, (void *)uv_frm, NULL,
                                   P_UNSIGNED_SHORT, read_mode,
                                   width, frm_height, stride, 0);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_read_frame_16 */

/* p_write_field_16 */
pT_status
p_write_field_16 (const char *filename, pT_header *header,
                  int frame, int field,
                  const unsigned short *y_fld,
                  const unsigned short *uv_fld,
                  int write_mode,
                  int width, int fld_height, int stride)
{
    pT_status      status = P_OK;
    const pT_color color_format = p_get_color_format (header);

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check if header is valid */
    if (status == P_OK) {
        status = p_check_header (header);
    } /* end of if (status == P_OK) */

    /* check if file is in multiplexed or stream format */
    if (status == P_OK) {
        status = p_check_multiplexed (color_format);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_write_field_all (filename, header, color_format,
                                    frame, field, P_NORMAL_COMP,
                                    (const void *)y_fld, (const void *)uv_fld, NULL,
                                    P_UNSIGNED_SHORT, write_mode,
                                    width, fld_height, stride, 0);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_write_field_16 */

/* p_write_frame_16 */
pT_status
p_write_frame_16 (const char *filename, pT_header *header,
                  int frame,
                  const unsigned short *y_or_s_frm,
                  const unsigned short *uv_frm,
                  int write_mode,
                  int width, int frm_height, int stride)
{
    pT_status      status = P_OK;
    const pT_color color_format = p_get_color_format (header);

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check if header is valid */
    if (status == P_OK) {
        status = p_check_header (header);
    } /* end of if (status == P_OK) */

    /* check if file is in multiplexed format */
    if (status == P_OK) {
        status = p_check_multi_or_stream (color_format);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_write_frame_all (filename, header, color_format,
                                    frame, P_NORMAL_COMP,
                                    (const void *)y_or_s_frm, (const void *)uv_frm, NULL,
                                    P_UNSIGNED_SHORT, write_mode,
                                    width, frm_height, stride, 0);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_write_frame_16 */

/*
 * Planar luminance/chrominance (YUV) or red/green/blue (RGB) files.
 *
 * Supported color_format: P_COLOR_444_PL,
 *                         P_COLOR_422_PL,
 *                         P_COLOR_420_PL,
 *                         P_COLOR_RGB,
 *                         P_COLOR_XYZ.
 *
 */

/* p_read_field_planar */
pT_status
p_read_field_planar (const char *filename, pT_header *header,
                     int frame, int field,
                     unsigned char *y_or_r_fld,
                     unsigned char *u_or_g_fld,
                     unsigned char *v_or_b_fld,
                     int read_mode,
                     int width, int fld_height, int stride, int uv_stride)
{
    pT_status      status = P_OK;
    const pT_color color_format = p_get_color_format (header);

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check if header is valid */
    if (status == P_OK) {
        status = p_check_header (header);
    } /* end of if (status == P_OK) */

    /* check if file is in planar format */
    if (status == P_OK) {
        status = p_check_planar (color_format);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_read_field_all (filename, header, color_format,
                                   frame, field, P_NORMAL_COMP,
                                   (void *)y_or_r_fld,
                                   (void *)u_or_g_fld,
                                   (void *)v_or_b_fld,
                                   P_UNSIGNED_CHAR, read_mode,
                                   width, fld_height, stride, uv_stride);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_read_field_planar */

/* p_read_frame_planar */
pT_status
p_read_frame_planar (const char *filename, pT_header *header,
                     int frame,
                     unsigned char *y_or_r_frm,
                     unsigned char *u_or_g_frm,
                     unsigned char *v_or_b_frm,
                     int read_mode,
                     int width, int frm_height, int stride, int uv_stride)
{
    pT_status      status = P_OK;
    const pT_color color_format = p_get_color_format (header);

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check if header is valid */
    if (status == P_OK) {
        status = p_check_header (header);
    } /* end of if (status == P_OK) */

    /* check if file is in planar format */
    if (status == P_OK) {
        status = p_check_planar (color_format);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_read_frame_all (filename, header, color_format,
                                   frame, P_NORMAL_COMP,
                                   (void *)y_or_r_frm,
                                   (void *)u_or_g_frm,
                                   (void *)v_or_b_frm,
                                   P_UNSIGNED_CHAR, read_mode,
                                   width, frm_height, stride, uv_stride);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_read_frame_planar */

/* p_write_field_planar */
pT_status
p_write_field_planar (const char *filename, pT_header *header,
                      int frame, int field,
                      const unsigned char *y_or_r_fld,
                      const unsigned char *u_or_g_fld,
                      const unsigned char *v_or_b_fld,
                      int width, int fld_height, int stride, int uv_stride)
{
    pT_status      status = P_OK;
    const pT_color color_format = p_get_color_format (header);

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check if header is valid */
    if (status == P_OK) {
        status = p_check_header (header);
    } /* end of if (status == P_OK) */

    /* check if file is in planar format */
    if (status == P_OK) {
        status = p_check_planar (color_format);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_write_field_all (filename, header, color_format,
                                    frame, field, P_NORMAL_COMP,
                                    (const void *)y_or_r_fld,
                                    (const void *)u_or_g_fld,
                                    (const void *)v_or_b_fld,
                                    P_UNSIGNED_CHAR, P_8_BIT_MEM,
                                    width, fld_height, stride, uv_stride);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_write_field_planar */

/* p_write_frame_planar */
pT_status
p_write_frame_planar (const char *filename, pT_header *header,
                      int frame,
                      const unsigned char *y_or_r_frm,
                      const unsigned char *u_or_g_frm,
                      const unsigned char *v_or_b_frm,
                      int width, int frm_height, int stride, int uv_stride)
{
    pT_status      status = P_OK;
    const pT_color color_format = p_get_color_format (header);

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check if header is valid */
    if (status == P_OK) {
        status = p_check_header (header);
    } /* end of if (status == P_OK) */

    /* check if file is in planar format */
    if (status == P_OK) {
        status = p_check_planar (color_format);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_write_frame_all (filename, header, color_format,
                                    frame, P_NORMAL_COMP,
                                    (const void *)y_or_r_frm,
                                    (const void *)u_or_g_frm,
                                    (const void *)v_or_b_frm,
                                    P_UNSIGNED_CHAR, P_8_BIT_MEM,
                                    width, frm_height, stride, uv_stride);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_write_frame_planar */

/* p_read_field_planar_16 */
pT_status
p_read_field_planar_16 (const char *filename, pT_header *header,
                        int frame, int field,
                        unsigned short *y_or_r_fld,
                        unsigned short *u_or_g_fld,
                        unsigned short *v_or_b_fld,
                        int read_mode,
                        int width, int fld_height, int stride, int uv_stride)
{
    pT_status      status = P_OK;
    const pT_color color_format = p_get_color_format (header);

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check if header is valid */
    if (status == P_OK) {
        status = p_check_header (header);
    } /* end of if (status == P_OK) */

    /* check if file is in planar format */
    if (status == P_OK) {
        status = p_check_planar (color_format);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_read_field_all (filename, header, color_format,
                                   frame, field, P_NORMAL_COMP,
                                   (void *)y_or_r_fld,
                                   (void *)u_or_g_fld,
                                   (void *)v_or_b_fld,
                                   P_UNSIGNED_SHORT, read_mode,
                                   width, fld_height, stride, uv_stride);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_read_field_planar_16 */

/* p_read_frame_planar_16 */
pT_status
p_read_frame_planar_16 (const char *filename, pT_header *header,
                        int frame,
                        unsigned short *y_or_r_frm,
                        unsigned short *u_or_g_frm,
                        unsigned short *v_or_b_frm,
                        int read_mode,
                        int width, int frm_height, int stride, int uv_stride)
{
    pT_status      status = P_OK;
    const pT_color color_format = p_get_color_format (header);

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check if header is valid */
    if (status == P_OK) {
        status = p_check_header (header);
    } /* end of if (status == P_OK) */

    /* check if file is in planar format */
    if (status == P_OK) {
        status = p_check_planar (color_format);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_read_frame_all (filename, header, color_format,
                                   frame, P_NORMAL_COMP,
                                   (void *)y_or_r_frm,
                                   (void *)u_or_g_frm,
                                   (void *)v_or_b_frm,
                                   P_UNSIGNED_SHORT, read_mode,
                                   width, frm_height, stride, uv_stride);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_read_frame_planar_16 */

/* p_write_field_planar_16 */
pT_status
p_write_field_planar_16 (const char *filename, pT_header *header,
                         int frame, int field,
                         const unsigned short *y_or_r_fld,
                         const unsigned short *u_or_g_fld,
                         const unsigned short *v_or_b_fld,
                         int write_mode,
                         int width, int fld_height, int stride, int uv_stride)
{
    pT_status      status = P_OK;
    const pT_color color_format = p_get_color_format (header);

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check if header is valid */
    if (status == P_OK) {
        status = p_check_header (header);
    } /* end of if (status == P_OK) */

    /* check if file is in planar format */
    if (status == P_OK) {
        status = p_check_planar (color_format);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_write_field_all (filename, header, color_format,
                                    frame, field, P_NORMAL_COMP,
                                    (const void *)y_or_r_fld,
                                    (const void *)u_or_g_fld,
                                    (const void *)v_or_b_fld,
                                    P_UNSIGNED_SHORT, write_mode,
                                    width, fld_height, stride, uv_stride);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_write_field_planar_16 */

/* p_write_frame_planar_16 */
pT_status
p_write_frame_planar_16 (const char *filename, pT_header *header,
                         int frame,
                         const unsigned short *y_or_r_frm,
                         const unsigned short *u_or_g_frm,
                         const unsigned short *v_or_b_frm,
                         int write_mode,
                         int width, int frm_height, int stride, int uv_stride)
{
    pT_status      status = P_OK;
    const pT_color color_format = p_get_color_format (header);

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check if file is in planar format */
    if (status == P_OK) {
        status = p_check_planar (color_format);
    } /* end of if (status == P_OK) */

    /* check if header is valid */
    if (status == P_OK) {
        status = p_check_header (header);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_write_frame_all (filename, header, color_format,
                                    frame, P_NORMAL_COMP,
                                    (const void *)y_or_r_frm,
                                    (const void *)u_or_g_frm,
                                    (const void *)v_or_b_frm,
                                    P_UNSIGNED_SHORT, write_mode,
                                    width, frm_height, stride, uv_stride);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_write_frame_planar_16 */

/*
 * Low Level Component Access Functions.
 *
 * Support (almost) all pfspd formats.
 *
 */

/* p_read_field_comp */
pT_status
p_read_field_comp (const char *filename, pT_header *header,
                   int frame, int field, int comp,
                   unsigned char *c_fld,
                   int read_mode,
                   int width, int fld_height, int stride)
{
    pT_status      status = P_OK;

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check component */
    if (status == P_OK) {
        status = p_check_comp (header, comp, 1);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_read_field_all (filename, header, P_UNKNOWN_COLOR,
                                   frame, field, comp,
                                   (void *)c_fld, NULL, NULL,
                                   P_UNSIGNED_CHAR, read_mode,
                                   width, fld_height, stride, 0);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_read_field_comp */

/* p_read_frame_comp */
pT_status
p_read_frame_comp (const char *filename, pT_header *header,
                   int frame, int comp,
                   unsigned char *c_frm,
                   int read_mode,
                   int width, int frm_height, int stride)
{
    pT_status      status = P_OK;

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check component */
    if (status == P_OK) {
        status = p_check_comp (header, comp, 1);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_read_frame_all (filename, header, P_UNKNOWN_COLOR,
                                   frame, comp,
                                   (void *)c_frm, NULL, NULL,
                                   P_UNSIGNED_CHAR, read_mode,
                                   width, frm_height, stride, 0);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_read_frame_comp */

/* p_write_field_comp */
pT_status
p_write_field_comp (const char *filename, pT_header *header,
                    int frame, int field, int comp,
                    const unsigned char *c_fld,
                    int width, int fld_height, int stride)
{
    pT_status      status = P_OK;

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check component */
    if (status == P_OK) {
        status = p_check_comp (header, comp, 0);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_write_field_all (filename, header, P_UNKNOWN_COLOR,
                                    frame, field, comp,
                                    (const void *)c_fld, NULL, NULL,
                                    P_UNSIGNED_CHAR, P_8_BIT_MEM,
                                    width, fld_height, stride, 0);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_write_field_comp */

/* p_write_frame_comp */
pT_status
p_write_frame_comp (const char *filename, pT_header *header,
                    int frame, int comp,
                    const unsigned char *c_frm,
                    int width, int frm_height, int stride)
{
    pT_status      status = P_OK;

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check component */
    if (status == P_OK) {
        status = p_check_comp (header, comp, 0);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_write_frame_all (filename, header, P_UNKNOWN_COLOR,
                                    frame, comp,
                                    (const void *)c_frm, NULL, NULL,
                                    P_UNSIGNED_CHAR, P_8_BIT_MEM,
                                    width, frm_height, stride, 0);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_write_frame_comp */

/* p_read_field_comp_16 */
pT_status
p_read_field_comp_16 (const char *filename, pT_header *header,
                      int frame, int field, int comp,
                      unsigned short *c_fld,
                      int read_mode,
                      int width, int fld_height, int stride)
{
    pT_status      status = P_OK;

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check component */
    if (status == P_OK) {
        status = p_check_comp (header, comp, 1);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_read_field_all (filename, header, P_UNKNOWN_COLOR,
                                   frame, field, comp,
                                   (void *)c_fld, NULL, NULL,
                                   P_UNSIGNED_SHORT, read_mode,
                                   width, fld_height, stride, 0);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_read_field_comp_16 */

/* p_read_frame_comp_16 */
pT_status
p_read_frame_comp_16 (const char *filename, pT_header *header,
                      int frame, int comp,
                      unsigned short *c_frm,
                      int read_mode,
                      int width, int frm_height, int stride)
{
    pT_status      status = P_OK;

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check component */
    if (status == P_OK) {
        status = p_check_comp (header, comp, 1);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_read_frame_all (filename, header, P_UNKNOWN_COLOR,
                                   frame, comp,
                                   (void *)c_frm, NULL, NULL,
                                   P_UNSIGNED_SHORT, read_mode,
                                   width, frm_height, stride, 0);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_read_frame_comp_16 */

/* p_write_field_comp_16 */
pT_status
p_write_field_comp_16 (const char *filename, pT_header *header,
                       int frame, int field, int comp,
                       const unsigned short *c_fld,
                       int write_mode,
                       int width, int fld_height, int stride)
{
    pT_status      status = P_OK;

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check component */
    if (status == P_OK) {
        status = p_check_comp (header, comp, 0);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_write_field_all (filename, header, P_UNKNOWN_COLOR,
                                    frame, field, comp,
                                    (const void *)c_fld, NULL, NULL,
                                    P_UNSIGNED_SHORT, write_mode,
                                    width, fld_height, stride, 0);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_write_field_comp_16 */

/* p_write_frame_comp_16 */
pT_status
p_write_frame_comp_16 (const char *filename, pT_header *header,
                       int frame, int comp,
                       const unsigned short *c_frm,
                       int write_mode,
                       int width, int frm_height, int stride)
{
    pT_status      status = P_OK;

    /* check if the file header is modified */
    status = p_check_modified (header);

    /* check component */
    if (status == P_OK) {
        status = p_check_comp (header, comp, 0);
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_write_frame_all (filename, header, P_UNKNOWN_COLOR,
                                    frame, comp,
                                    (const void *)c_frm, NULL, NULL,
                                    P_UNSIGNED_SHORT, write_mode,
                                    width, frm_height, stride, 0);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_write_frame_comp_16 */

/******************************************************************************/

