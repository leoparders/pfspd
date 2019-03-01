/*
 *  Copyright (c) KONINKLIJKE PHILIPS ELECTRONICS N.V. 1997-2010
 *  Copyright (c) NXP Semiconductors Netherlands B.V. 2006-2010
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
 *  CVS id      :  $Id: cpfspd_get.c,v 2.24 2010/01/05 14:18:32 riemens Exp $
 *
 *  Name        :  cpfspd_get.c
 *
 *  Author      :  Robert Jan Schutten
 *
 *  Function    :  GET info from header of cpfspd.
 *                 ---
 *
 *  Description :  All functions to get information from the
 *                 header structure are in this file.
 *
 *  Functions   :  The following external cpfspd functions are
 *                 defined in this file:
 *
 *                 - p_get_num_frames()
 *                 - p_is_interlaced()
 *                 - p_is_progressive()
 *                 - p_get_frame_width()
 *                 - p_get_frame_height()
 *                 - p_get_y_buffer_size()
 *                 - p_get_uv_buffer_size()
 *                 - p_get_rgb_buffer_size()
 *                 - p_get_s_buffer_size()
 *                 - p_get_color_format()
 *                 - p_get_all_freqs()
 *                 - p_get_image_size()
 *                 - p_get_aspect_ratio()
 *                 - p_get_file_data_format()
 *                 - p_get_file_description()
 *                 - p_get_comp_by_name()
 *                 - p_get_num_comps()
 *                 - p_get_comp_2()
 *                 - p_get_comp_buffer_size()
 *
 */

/******************************************************************************/

#include <string.h>
/* #include <math.h> */
#include "cpfspd.h"
#include "cpfspd_hdr.h"

/******************************************************************************/

/* define abs macro - avoid use of fabs() from math.h */
#define ABS(x)               ( (x) < 0 ? (-(x)) : (x) )

/******************************************************************************/

/*
 * Get information from header
 */

/* get number of frames */

int
p_get_num_frames (const pT_header *header)
{
    return (header->nr_images / header->interlace);
}

/* return 1 is header in interlaced */

int
p_is_interlaced (const pT_header *header)
{
    int interlaced;
    if (header->interlace == 2) {
        interlaced = 1;
    } else {
        interlaced = 0;
    }
    return interlaced;
}

/* return 1 is header in progressive */

int
p_is_progressive (const pT_header *header)
{
    int progressive;
    if (header->interlace == 1) {
        progressive = 1;
    } else {
        progressive = 0;
    }
    return progressive;
}

/* get frame width */

int
p_get_frame_width (const pT_header *header)
{
    return header->act_pixel;
}

/* get frame height */

int
p_get_frame_height (const pT_header *header)
{
    return header->act_lines;
}

/* get size of Y buffer */

void
p_get_y_buffer_size (const pT_header *header, 
                     int *width, 
                     int *height)
{
    (*width)  = header->comp[0].pix_line;
    (*height) = header->comp[0].lin_image;
    return;
}

/* get size of U,V buffer */

void
p_get_uv_buffer_size (const pT_header *header, 
                      int *width, 
                      int *height)
{
    (*width)  = header->comp[1].pix_line;    
    (*height) = header->comp[1].lin_image;
    return;
}

/* get size of R,G,B buffer */

void
p_get_rgb_buffer_size (const pT_header *header, 
                       int *width, 
                       int *height)
{
    (*width)  = header->comp[0].pix_line;
    (*height) = header->comp[0].lin_image;
    return;
}

/* get size of S buffer */

void
p_get_s_buffer_size (const pT_header *header, 
                     int *width, 
                     int *height)
{
    (*width)  = header->comp[0].pix_line;
    (*height) = header->comp[0].lin_image;
    return;
}

/* get color format */

pT_color
p_get_color_format (const pT_header *header)
{
    pT_status status = P_OK;
    pT_color  color_format;
    status = p_check_color_format (header, &color_format);
    if (status != P_OK) {
        color_format = P_UNKNOWN_COLOR;
    }
    return color_format;
}

/* get all frequencies from the header as doubles */

void
p_get_all_freqs (const pT_header *header, 
                 double *image_freq, 
                 double *line_freq, 
                 double *pixel_freq)
{
    (*image_freq) = header->ima_freq;
    (*line_freq)  = header->lin_freq;
    (*pixel_freq) = header->pix_freq;
    return;
}

/* get image frequency */

#define P_TO_INT(a)    (int)(100.0 * (a) + 0.5)
pT_freq
p_get_image_freq (const pT_header *header)
{
    pT_freq   image_freq;
    /* comparing floating points is not a good idea, 
       so we first convert to int */
    const int int_image_freq = P_TO_INT(header->ima_freq);
    if (int_image_freq        == P_TO_INT(0.4 * P_STD_IMA_FREQ_60HZ     )) {
        image_freq = P_24HZ;
    } else if (int_image_freq == P_TO_INT(0.4 * P_STD_IMA_FREQ_REAL_60HZ)) {
        image_freq = P_REAL_24HZ;
    } else if (int_image_freq == P_TO_INT(0.5 * P_STD_IMA_FREQ_50HZ     )) {
        image_freq = P_25HZ;
    } else if (int_image_freq == P_TO_INT(0.5 * P_STD_IMA_FREQ_60HZ     )) {
        image_freq = P_30HZ;
    } else if (int_image_freq == P_TO_INT(0.5 * P_STD_IMA_FREQ_REAL_60HZ)) {
        image_freq = P_REAL_30HZ;
    } else if (int_image_freq == P_TO_INT(      P_STD_IMA_FREQ_50HZ     )) {
        image_freq = P_50HZ;
    } else if (int_image_freq == P_TO_INT(      P_STD_IMA_FREQ_60HZ     )) {
        image_freq = P_60HZ;
    } else if (int_image_freq == P_TO_INT(      P_STD_IMA_FREQ_REAL_60HZ)) {
        image_freq = P_REAL_60HZ;
    } else if (int_image_freq == P_TO_INT(1.5 * P_STD_IMA_FREQ_50HZ     )) {
        image_freq = P_75HZ;
    } else if (int_image_freq == P_TO_INT(1.5 * P_STD_IMA_FREQ_60HZ     )) {
        image_freq = P_90HZ;
    } else if (int_image_freq == P_TO_INT(1.5 * P_STD_IMA_FREQ_REAL_60HZ)) {
        image_freq = P_REAL_90HZ;
    } else if (int_image_freq == P_TO_INT(2.0 * P_STD_IMA_FREQ_50HZ     )) {
        image_freq = P_100HZ;
    } else if (int_image_freq == P_TO_INT(2.0 * P_STD_IMA_FREQ_60HZ     )) {
        image_freq = P_120HZ;
    } else if (int_image_freq == P_TO_INT(2.0 * P_STD_IMA_FREQ_REAL_60HZ)) {
        image_freq = P_REAL_120HZ;
    } else {
        image_freq = P_UNKNOWN_FREQ;
    }
    return image_freq;
}
#undef P_TO_INT

/* get image size */

pT_size
p_get_image_size (const pT_header *header)
{
    pT_size        image_size = P_UNKNOWN_SIZE;
    const pT_color color_format = p_get_color_format (header);
    if (color_format == P_STREAM) {
        switch (header->act_lines) {
        case 525:
        case 625:
            image_size = P_SD;
            break;
        default:
            image_size = P_UNKNOWN_SIZE;
        } /* end of switch (header->act_lines) */
    } else {
        switch (header->act_lines) {
        case 120:
        case 144:
            image_size = P_QCIF;
            break;
        case 240:
        case 288:
            image_size = P_CIF;
            break;
        case 480:
        case 576:
            image_size = P_SD;
            break;
        case 1080:
        case 1152:
            image_size = P_HDi;
            break;
        case 720:
            image_size = P_HDp;
            break;
        default:
            image_size = P_UNKNOWN_SIZE;
        } /* end of switch (header->act_lines) */
    } /* end of p_get_color_format (... */
    return image_size;
} /* end of p_get_image_size () */

/* get aspect ratio */

pT_asp_rat
p_get_aspect_ratio (const pT_header *header)
{
    pT_asp_rat ratio;
    const int  h_ratio = header->h_pp_size;
    const int  v_ratio = header->v_pp_size;
    const int  width   = header->act_pixel;
    const int  height  = header->act_lines;

    if ((h_ratio == 4) && (v_ratio == 3)){
        ratio = P_4_3;
    } else if ((h_ratio == 16) && (v_ratio == 9)){
        ratio = P_16_9;
    } else if (ABS((double)width/h_ratio - (double)height/v_ratio) < 0.001) {
        /* Allow marginal difference by floating point rounding */
        ratio = P_AS_WH;
    } else {
        ratio = P_UNKNOWN_ASPECT_RATIO;
    } /* end of if ((h_ratio ==... */
    return ratio;
} /* end of p_get_aspect_ratio () */

/* get data format */

pT_data_fmt
p_get_file_data_format (const pT_header *header)
{
    pT_data_fmt    file_data_fmt;
    pT_status      status = P_OK;
    const pT_color color_format = p_get_color_format (header);    
    status = p_check_file_data_format (header, color_format, &file_data_fmt);
    if (status != P_OK) {
        file_data_fmt = P_UNKNOWN_DATA_FORMAT;
    }
    return file_data_fmt;
} /* end of p_get_file_data_format () */

/* get file description */

char *
p_get_file_description (const pT_header *header)
{
    return ((char *)header->description);
} /* end of p_get_file_description () */

/* locate component by name */

int p_get_comp_by_name (const pT_header *header, const char *name)
{
    int i;
    int comp;
    char name_space[P_SCOM_CODE+1];

    sprintf(name_space, "%-*s", P_SCOM_CODE, name);
    comp = -1;
    for (i = 0; (comp == -1) && (i < header->nr_compon); i++) {
        if (strncmp(header->comp[i].com_code, name_space, P_SCOM_CODE) == 0) {
            comp = i;
        }
    }

    return comp;
} /* end of p_get_comp_by_name */

/* get number of components */

int p_get_num_comps (const pT_header *header)
{
    return(header->nr_compon);
} /* end of p_get_num_comps */

/* get component properties */

pT_status p_get_comp_2 (const pT_header *header, const int comp,
                        char *name,
                        pT_data_fmt *file_data_fmt,
                        int *pix_subsample,
                        int *line_subsample,
                        int *multiplex_factor)
{
    pT_status      status = P_OK;
    char           *space;

    /* check component id */
    if ((comp < 0) || (comp >= header->nr_compon)) {
        status = P_INVALID_COMPONENT;
    }

    if (status == P_OK) {
        if (file_data_fmt != NULL) {
            if (!strncmp(header->comp[comp].data_fmt, P_B8_DATA_FMT, 
                        (size_t)P_SDATA_FMT)) {
                *file_data_fmt = P_8_BIT_FILE;
            } else if (!strncmp(header->comp[comp].data_fmt, P_B10_DATA_FMT, 
                        (size_t)P_SDATA_FMT)) {
                *file_data_fmt = P_10_BIT_FILE;
            } else if (!strncmp(header->comp[comp].data_fmt, P_B12_DATA_FMT, 
                        (size_t)P_SDATA_FMT)) {
                *file_data_fmt = P_12_BIT_FILE;
            } else if (!strncmp(header->comp[comp].data_fmt, P_B14_DATA_FMT, 
                        (size_t)P_SDATA_FMT)) {
                *file_data_fmt = P_14_BIT_FILE;
            } else if (!strncmp(header->comp[comp].data_fmt, P_I2_DATA_FMT, 
                        (size_t)P_SDATA_FMT)) {
                *file_data_fmt = P_16_BIT_FILE;
            } else if (!strncmp(header->comp[comp].data_fmt, P_R2_DATA_FMT, 
                        (size_t)P_SDATA_FMT)) {
                *file_data_fmt = P_16_REAL_FILE;
            } else {
                *file_data_fmt = P_UNKNOWN_DATA_FORMAT;
                status = P_ILLEGAL_FILE_DATA_FORMAT;
            } /* end of if (!strncmp(... */
        }

        if (name != NULL) {
            strncpy(name, (char *)header->comp[comp].com_code, (size_t)P_SCOM_CODE);
            name[P_SCOM_CODE] = '\0';
            /* Remove trailing spaces from name */
            space = strchr(name, ' ');
            if (space != NULL) {
                *space = '\0';
            }
        }

        if (pix_subsample != NULL) {
            *pix_subsample  = header->comp[comp].pix_sbsmpl;
        }
        if (line_subsample != NULL) {
            *line_subsample = header->comp[comp].lin_sbsmpl;
        }
        if (multiplex_factor != NULL) {
            *multiplex_factor = (header->comp[comp].pix_line * header->comp[comp].pix_sbsmpl) / header->act_pixel;
        }
    }

    return status;
} /* end of p_get_comp_2 */

/* get buffer size of a single component */

pT_status p_get_comp_buffer_size (const pT_header *header, const int comp,
                                  int *width, int *height)
{
    pT_status      status = P_OK;

    /* check component id */
    if ((comp < 0) || (comp >= header->nr_compon)) {
        status = P_INVALID_COMPONENT;
    }

    if (status == P_OK) {
        *width  = header->comp[comp].pix_line;
        *height = header->comp[comp].lin_image;
    }

    return status;
} /* end of p_get_comp_buffer_size */

/******************************************************************************/
