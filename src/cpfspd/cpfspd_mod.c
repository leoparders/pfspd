/*
 *  Copyright (c) KONINKLIJKE PHILIPS ELECTRONICS N.V. 1997-2010
 *  All rights reserved.
 *  For licensing and warranty information, see the file COPYING.LGPLv21.txt
 *  in the root directory of this package.
 *
 *  Philips Research Laboratories
 *  Eindhoven, The Netherlands
 *
 *  CVS id      :  $Id: cpfspd_mod.c,v 2.32 2010/01/05 14:18:32 riemens Exp $
 *
 *  Name        :  cpfspd_mod.c
 *
 *  Author      :  Robert Jan Schutten
 *
 *  Function    :  header MODify functions of cpfspd.
 *                        ---
 *
 *  Description :  All functions to modify the contents of the
 *                 header structure are in this file.
 *
 *  Functions   :  The following external cpfspd functions are
 *                 defined in this file:
 *
 *                 - p_mod_num_frames()
 *                 - p_mod_color_format()
 *                 - p_mod_aspect_ratio()
 *                 - p_mod_to_progressive()
 *                 - p_mod_to_interlaced()
 *                 - p_mod_to_dbl_image_rate()
 *                 - p_mod_to_onehalf_image_rate()
 *                 - p_mod_image_size()
 *                 - p_mod_defined_image_size()
 *                 - p_mod_all_freqs()
 *                 - p_mod_defined_image_freq()
 *                 - p_mod_file_data_format()
 *                 - p_mod_file_description()
 *                 - p_mod_add_comp()
 *                 - p_mod_set_comp_2()
 *                 - p_mod_rm_comp()
 *                 - p_mod_rm_extra_comps()
 *
 */

/******************************************************************************/

#include <string.h>
#include "cpfspd.h"
#include "cpfspd_hdr.h"

/******************************************************************************/

/*
 * Functions to modify the header
 */

/* modify number of frames */

pT_status
p_mod_num_frames (pT_header *header,
                  int num_frames)
{
    const pT_status status = P_OK;
    header->nr_images = num_frames * header->interlace;
    /* the header is now modified; set the modified bit to one */
    header->modified = 1;
    return status;
} /* end of p_mod_num_frames () */

/* modify color format */

pT_status
p_mod_color_format (pT_header *header,
                    pT_color color_format)
{
    pT_status         status = P_OK;
    const pT_color    old_color_format = p_get_color_format (header);
    const double      ima_freq = header->ima_freq;
    const double      lin_freq = header->lin_freq;
    const double      pix_freq = header->pix_freq;
    const int         act_lines = header->act_lines;
    const int         act_pixel = header->act_pixel;
    const int         interlace_factor = header->interlace;
    const int         h_ratio = header->h_pp_size;
    const int         v_ratio = header->v_pp_size;
    const int         num_frames = p_get_num_frames (header);
    const pT_data_fmt file_data_fmt = p_get_file_data_format (header);

    if (old_color_format != color_format) {
        /* construct new header */
        status = p_create_free_header (header, color_format,
                                       ima_freq, lin_freq,
                                       pix_freq,
                                       act_lines, act_pixel,
                                       interlace_factor,
                                       h_ratio, v_ratio);
        /* num_frames has to be reset,
           because p_create_free_header set it to zero */
        if (status == P_OK) {
            status = p_mod_num_frames (header, num_frames);
        } /* end of if (status == P_OK) */
        /* file_data_fmt has to be reset,
           because p_create_ext_header set it to P_8_BIT_FILE */
        if (status == P_OK) {
            status = p_mod_file_data_format (header, file_data_fmt);
        } /* end of if (status == P_OK) */
    } /* end of if (old_color_format != color_format) */
    /* the header is now modified; set the modified bit to one */
    header->modified = 1;
    return status;
} /* end of p_mod_color_format () */

/* largest common divider (positive numbers only)
 * small helper function for calculations related
 * to P_AS_WH aspect ratio.
 * note: identical code is in cpfspd_hdr.c
 */

static int lcd (int x, int y)
{
    if ((x <= 0) || (y <= 0)) return(1);

    while (x * y != 0) {
        if (x > y)
            x = x % y;
        else
            y = y % x;
    }

    return(x+y);
} /* end of lcd () */

/* modify aspect ratio */

pT_status
p_mod_aspect_ratio (pT_header *header,
                    pT_asp_rat ratio)
{
    pT_status      status = P_OK;
    int            h_ratio;
    int            v_ratio;
    int            width;
    int            height;
    int            div;

    switch (ratio) {
    case P_4_3:
        h_ratio = 4;
        v_ratio = 3;
        break;
    case P_16_9:
        h_ratio = 16;
        v_ratio = 9;
        break;
    case P_AS_WH:
        width   = header->act_pixel;
        height  = header->act_lines;
        div = lcd (width, height);
        h_ratio = width / div;
        v_ratio = height / div;
        break;
    default:
        h_ratio = 0;
        v_ratio = 0;
        status = P_ILLEGAL_ASPECT_RATIO;
    } /* end of switch (ratio) */
    if (status == P_OK) {
        header->h_pp_size = h_ratio;
        header->v_pp_size = v_ratio;
    } /* end of if (status == P_OK) */
    /* the header is now modified; set the modified bit to one */
    header->modified = 1;
    return status;
} /* end of p_mod_aspect_ratio () */

/* modify header to progressive format */

pT_status
p_mod_to_progressive (pT_header *header)
{
    const pT_status status = P_OK;
    const pT_color  color_format = p_get_color_format (header);
    if (p_is_interlaced (header)) {
        header->nr_images /= 2;
        header->lin_freq *= 2.0;
        header->pix_freq *= 2.0;
        header->interlace = 1;
        switch (color_format) {
        case P_NO_COLOR:
            header->comp[0].lin_image *= 2;
            break;
        case P_COLOR_422:
        case P_COLOR_420:
            header->comp[0].lin_image *= 2;
            header->comp[1].lin_image *= 2;
            break;
        case P_COLOR_444_PL:
        case P_COLOR_422_PL:
        case P_COLOR_420_PL:
        case P_COLOR_RGB:
        case P_COLOR_XYZ:
            header->comp[0].lin_image *= 2;
            header->comp[1].lin_image *= 2;
            header->comp[2].lin_image *= 2;
            break;
        default:
            break;
        } /* end of switch color_format */
    } /* end of if (p_is_interlaced (header)) */
    /* the header is now modified; set the modified bit to one */
    header->modified = 1;
    return status;
} /* end of p_mod_to_progressive () */

/* modify header to interlaced format */

pT_status
p_mod_to_interlaced (pT_header *header)
{
    const pT_status status = P_OK;
    const pT_color  color_format = p_get_color_format (header);
    if (p_is_progressive (header)) {
        header->nr_images *= 2;
        header->lin_freq /= 2.0;
        header->pix_freq /= 2.0;
        header->interlace = 2;
        switch (color_format) {
        case P_NO_COLOR:
            header->comp[0].lin_image /= 2;
            break;
        case P_COLOR_422:
        case P_COLOR_420:
            header->comp[0].lin_image /= 2;
            header->comp[1].lin_image /= 2;
            break;
        case P_COLOR_444_PL:
        case P_COLOR_422_PL:
        case P_COLOR_420_PL:
        case P_COLOR_RGB:
        case P_COLOR_XYZ:
            header->comp[0].lin_image /= 2;
            header->comp[1].lin_image /= 2;
            header->comp[2].lin_image /= 2;
            break;
        default:
            break;
        } /* end of switch color_format */
    } /* end of if (p_is_progressive (header)) */
    /* the header is now modified; set the modified bit to one */
    header->modified = 1;
    return status;
} /* end of p_mod_to_interlaced () */

/* modify header to doubled image rate */

pT_status
p_mod_to_dbl_image_rate (pT_header *header)
{
    pT_status     status = P_OK;
    const pT_freq old_image_freq = p_get_image_freq (header);
    switch (old_image_freq) {
    case P_50HZ:
    case P_60HZ:
    case P_REAL_60HZ:
        header->nr_images *= 2;
        header->ima_freq *= 2.0;
        header->lin_freq *= 2.0;
        header->pix_freq *= 2.0;
        break;
    default:
        status = P_ILLEGAL_IMAGE_FREQ_MOD;
    } /* end of switch (old_image_freq) */
    /* the header is now modified; set the modified bit to one */
    header->modified = 1;
    return status;
} /* end of p_mod_to_dbl_image_rate () */

/* modify header to 1.5 times image rate */

pT_status
p_mod_to_onehalf_image_rate (pT_header *header)
{
    pT_status     status = P_OK;
    const pT_freq old_image_freq = p_get_image_freq (header);
    switch (old_image_freq) {
    case P_50HZ:
    case P_60HZ:
    case P_REAL_60HZ:
        header->nr_images = (int)((double)header->nr_images * 1.5);
        header->ima_freq *= 1.5;
        header->lin_freq *= 1.5;
        header->pix_freq *= 1.5;
        break;
    default:
        status = P_ILLEGAL_IMAGE_FREQ_MOD;
    } /* end of switch (old_image_freq) */
    /* the header is now modified; set the modified bit to one */
    header->modified = 1;
    return status;
} /* end of p_mod_to_onehalf_image_rate () */


/* modify image size */

/* The original code of p_mod_image_size is provided by:                     */
/*   Karl Wittig, Philips Research Briarcliff.                               */
/*                                                                           */
/* As of version 1.6.0 the functionality of this function is changed:        */
/* - It does not modify the line and pixel frequency anymore when the new    */
/*   image size is smaller than the existing image size (since this does     */
/*   not make any sense).                                                    */
/*   This way when one first creates a header than contains a _larger_       */
/*   image than the target image size, the modified image size can be        */
/*   used on the video simulator to display the sequence using the larger    */
/*   raster (the simulator will add black data around the smaller image).    */
/* - When the new image size is larger than the existing image size all      */
/*   line and pixel frequencies are set to _zero_.                           */
/*                                                                           */
/* Note that is a larger image size is required in combination with a legal  */
/* image raster the function p_mod_defined_image_size() should be used       */

pT_status
p_mod_image_size (pT_header *header,
                  int width,
                  int height)
{
    const pT_status status = P_OK;
    int             i;
    int             h_subsample;
    int             v_subsample;
    int             reset_freq = 1;
    /* if new image size is smaller than existing image size,
       then do not reset the frequencies */
    if ((width  <= header->act_pixel) &&
        (height <= header->act_lines)) {
        reset_freq = 0;
    }
    /* modify component image size */
    for (i = 0; i < header->nr_compon; i++) {
        /* store subsample factors;
           note that if interlace is 2, v_subsample will be doubled */
        h_subsample = header->act_pixel / header->comp[i].pix_line;
        v_subsample = header->act_lines / header->comp[i].lin_image;
        /* calculate new sizes */
        header->comp[i].pix_line  = (width  / h_subsample);
        header->comp[i].lin_image = (height / v_subsample);
    }
    /* modify global image size */
    header->act_pixel = width;
    header->act_lines = height;
    /* modify pixel & line frequency */
    if (reset_freq) {
        header->pix_freq = 0.0;
        header->lin_freq = 0.0;
    }
    /* the header is now modified; set the modified bit to one */
    header->modified = 1;
    return status;
} /* end of p_mod_image_size () */

/* modify to a defined image size */

pT_status
p_mod_defined_image_size (pT_header* header,
                          pT_size image_size,
                          int pixels_per_line)
{
    pT_status         status = P_OK;
    const pT_color    color_format = p_get_color_format (header);
    const pT_freq     image_freq   = p_get_image_freq (header);
    const int         progressive = p_is_progressive (header);
    const pT_asp_rat  ratio = p_get_aspect_ratio (header);
    const pT_data_fmt file_data_fmt = p_get_file_data_format (header);
    const int         num_frames = p_get_num_frames (header);
    /* recreate the header with the new image size: */
    status = p_create_ext_header (header, color_format,
                                  image_freq, image_size,
                                  pixels_per_line, progressive,
                                  ratio);
    /* num_frames has to be reset,
       because p_create_ext_header set it to zero */
    if (status == P_OK) {
        status = p_mod_num_frames (header, num_frames);
    } /* end of if (status == P_OK) */
    /* file_data_fmt has to be reset,
       because p_create_ext_header set it to P_8_BIT_FILE */
    if (status == P_OK) {
        status = p_mod_file_data_format (header, file_data_fmt);
    } /* end of if (status == P_OK) */
    /* the header is now modified; set the modified bit to one */
    header->modified = 1;
    return status;
} /* end of p_mod_defined_image_size () */

/* modify header frequencies */

pT_status
p_mod_all_freqs (pT_header *header,
                 double image_freq,
                 double line_freq,
                 double pixel_freq)
{
    pT_status      status = P_OK;

    if ((image_freq >= 0) && (line_freq >= 0) && (pixel_freq >= 0)) {
        header->ima_freq = image_freq;
        header->lin_freq = line_freq;
        header->pix_freq = pixel_freq;
        /* the header is now modified; set the modified bit to one */
        header->modified = 1;
    } else {
        status = P_ILLEGAL_ILP_FREQ_MOD;
    }
    return status;
} /* end of p_mod_all_freqs () */

/* modify to a defined image frequency */

pT_status
p_mod_defined_image_freq (pT_header* header,
                          pT_freq image_freq)
{
    pT_status         status = P_OK;
    const pT_color    color_format = p_get_color_format (header);
    const pT_size     image_size = p_get_image_size (header);
    const int         pixels_per_line = p_get_frame_width (header);
    const int         progressive = p_is_progressive (header);
    const pT_asp_rat  ratio = p_get_aspect_ratio (header);
    const pT_data_fmt file_data_fmt = p_get_file_data_format (header);
    const int         num_frames = p_get_num_frames (header);
    /* recreate the header with the new image size: */
    status = p_create_ext_header (header, color_format,
                                  image_freq, image_size,
                                  pixels_per_line, progressive,
                                  ratio);
    /* num_frames has to be reset,
       because p_create_ext_header set it to zero */
    if (status == P_OK) {
        status = p_mod_num_frames (header, num_frames);
    } /* end of if (status == P_OK) */
    /* file_data_fmt has to be reset,
       because p_create_ext_header set it to P_8_BIT_FILE */
    if (status == P_OK) {
        status = p_mod_file_data_format (header, file_data_fmt);
    } /* end of if (status == P_OK) */
    /* the header is now modified; set the modified bit to one */
    header->modified = 1;
    return status;
} /* end of p_mod_defined_image_freq () */

/* modify data format */

pT_status
p_mod_file_data_format (pT_header *header,
                        pT_data_fmt file_data_fmt)
{
    pT_status      status = P_OK;
    const pT_color color_format = p_get_color_format (header);
    char           format_str[P_SDATA_FMT + 1];
    int            i;
    int            nr_compon;
    /* create correct format string */
    switch (file_data_fmt) {
    case P_8_BIT_FILE:
        strcpy (format_str, P_B8_DATA_FMT);
        break;
    case P_10_BIT_FILE:
        strcpy (format_str, P_B10_DATA_FMT);
        break;
    case P_12_BIT_FILE:
        strcpy (format_str, P_B12_DATA_FMT);
        break;
    case P_14_BIT_FILE:
        strcpy (format_str, P_B14_DATA_FMT);
        break;
    case P_16_BIT_FILE:
        strcpy (format_str, P_I2_DATA_FMT);
        break;
    case P_16_REAL_FILE:
        /* We only allow RGB and XYZ video components in float format. */
        /* If this is really needed for other formats, use p_mod_set_comp() */
        if ((color_format == P_COLOR_RGB) || (color_format == P_COLOR_XYZ)) {
            strcpy (format_str, P_R2_DATA_FMT);
            break;
        }
        /* else: FALLTHROUGH */
    default:
        strcpy (format_str, " ");
        status = P_ILLEGAL_FILE_DATA_FORMAT;
        break;
    } /* end of switch (file_data_fmt) */
    /* determine number of components */
    switch (color_format) {
    case P_NO_COLOR:
    case P_STREAM:
        nr_compon = 1;
        break;
    case P_COLOR_422:
    case P_COLOR_420:
        nr_compon = 2;
        break;
    case P_COLOR_444_PL:
    case P_COLOR_422_PL:
    case P_COLOR_420_PL:
    case P_COLOR_RGB:
    case P_COLOR_XYZ:
        nr_compon = 3;
        break;
    default:
        nr_compon = 0;
        status = P_ILLEGAL_COLOR_FORMAT;
        break;
    } /* end of switch (color_format) */
    /* set individuale component data format */
    if (status == P_OK) {
        for (i = 0; i < nr_compon; i++) {
            /* set data format */
            strcpy (header->comp[i].data_fmt, format_str);
        } /* end of for (i = 0... */
    } /* end of if (status == P_OK) */
    /* the header is now modified; set the modified bit to one */
    header->modified = 1;
    return status;
} /* end of p_mod_file_data_format () */

/* modify file description */

pT_status
p_mod_file_description (pT_header *header,
                        char *description)
{
    pT_status      status = P_OK;

    if (strlen(description) >= (size_t)P_SDESCRIPTION) {
        status = P_EXCEEDING_DESCRIPTION_SIZE;
    } else {
        memset(header->description, 0, P_SDESCRIPTION);
        strcpy(header->description, description);
    }

    /* the header is now modified; set the modified bit to one */
    header->modified = 1;
    return status;
} /* end of p_mod_file_description () */

/* add new component to header structure */

int p_mod_add_comp (pT_header *header)
{
    int comp = -1;

    if (header->nr_compon < P_PFSPD_MAX_COMP) {
        comp = header->nr_compon;
        header->nr_compon++;

        header->comp[comp].tem_sbsmpl = 1;
        header->comp[comp].lin_sbsmpl = 1;
        header->comp[comp].pix_sbsmpl = 1;
        header->comp[comp].tem_phshft = 0;
        header->comp[comp].lin_phshft = 0;
        header->comp[comp].pix_phshft = 0;
        header->comp[comp].lin_image = header->act_lines / header->interlace;
        header->comp[comp].pix_line  = header->act_pixel;
        strcpy(header->comp[comp].data_fmt, P_B8_DATA_FMT);
        strcpy(header->comp[comp].com_code, P_VOID_COM_CODE);

        /* the header is now modified; set the modified bit to one */
        header->modified = 1;
    }

    return comp;
} /* end of p_mod_add_comp */

/* modify properties of a component */

pT_status p_mod_set_comp_2 (pT_header *header, const int comp,
                            const char *name,
                            const pT_data_fmt file_data_fmt,
                            const int pix_subsample,
                            const int line_subsample,
                            const int multiplex_factor)
{
    pT_status      status = P_OK;
    char           *format_str;
    char           name_str[P_SCOM_CODE+1];

    /* check component id */
    if ((comp < 0) || (comp >= header->nr_compon)) {
        status = P_INVALID_COMPONENT;
    }

    if ((header->act_pixel % pix_subsample != 0) ||
        (header->act_lines % line_subsample != 0)) {
        status = P_WRONG_SUBSAMPLE_FACTOR;
    }

    /* create corrent format string */
    switch (file_data_fmt) {
    case P_8_BIT_FILE:
        format_str = P_B8_DATA_FMT;
        break;
    case P_10_BIT_FILE:
        format_str = P_B10_DATA_FMT;
        break;
    case P_12_BIT_FILE:
        format_str = P_B12_DATA_FMT;
        break;
    case P_14_BIT_FILE:
        format_str = P_B14_DATA_FMT;
        break;
    case P_16_BIT_FILE:
        format_str = P_I2_DATA_FMT;
        break;
    case P_16_REAL_FILE:
        format_str = P_R2_DATA_FMT;
        break;
    default:
        format_str = " ";
        status = P_ILLEGAL_FILE_DATA_FORMAT;
        break;
    } /* end of switch (file_data_fmt) */

    /* Truncate name string */
    strncpy(name_str, name, (size_t)P_SCOM_CODE);
    name_str[P_SCOM_CODE] = '\0';

    if (status == P_OK) {
        header->comp[comp].tem_sbsmpl = 1;
        header->comp[comp].lin_sbsmpl = line_subsample;
        header->comp[comp].pix_sbsmpl = pix_subsample;
        header->comp[comp].tem_phshft = 0;
        header->comp[comp].lin_phshft = 0;
        header->comp[comp].pix_phshft = 0;
        header->comp[comp].lin_image = (header->act_lines / line_subsample) / header->interlace;
        header->comp[comp].pix_line  = multiplex_factor * header->act_pixel / pix_subsample;
        strcpy(header->comp[comp].data_fmt, format_str);
        /* Pad name with spaces in header structure */
        sprintf(header->comp[comp].com_code, "%-*s", P_SCOM_CODE, name_str);
        header->comp[comp].com_code[P_SCOM_CODE] = '\0';
    }

    /* the header is now modified; set the modified bit to one */
    header->modified = 1;
    return status;
} /* end of p_mod_set_comp_2 */

/* remove a component */

pT_status p_mod_rm_comp (pT_header *header, const int comp)
{
    pT_status      status = P_OK;
    int            i;

    /* Ignore component id -1 */
    if (comp != -1) {
        /* check component id */
        if ((comp < 0) || (comp >= header->nr_compon)) {
            status = P_INVALID_COMPONENT;
        }

        if (status == P_OK) {
            for (i=comp; i<header->nr_compon-1; i++) {
                header->comp[i] = header->comp[i+1];
            }
            header->nr_compon--;
        }

        /* the header is now modified; set the modified bit to one */
        header->modified = 1;
    }

    return status;
} /* end of p_mod_rm_comp */

/* remove all extra components */

pT_status p_mod_rm_extra_comps (pT_header *header)
{
    pT_status      status = P_OK;
    pT_color       color_format;

    status = p_check_color_format(header, &color_format);
    if (status == P_OK) {
        switch (color_format) {
        case P_NO_COLOR:
        case P_STREAM:
            header->nr_compon = 1;
            break;
        case P_COLOR_422:
        case P_COLOR_420:
            header->nr_compon = 2;
            break;
        case P_COLOR_444_PL:
        case P_COLOR_422_PL:
        case P_COLOR_420_PL:
        case P_COLOR_RGB:
        case P_COLOR_XYZ:
            header->nr_compon = 3;
            break;
        case P_UNKNOWN_COLOR:
            /* Ignore, does not occur */
            break;
        }
    }

    /* the header is now modified; set the modified bit to one */
    header->modified = 1;
    return status;
} /* end of p_mod_rm_extra_comps */

/******************************************************************************/


