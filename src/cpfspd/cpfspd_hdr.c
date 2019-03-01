/*
 *  Copyright (c) KONINKLIJKE PHILIPS ELECTRONICS N.V. 1997-2015
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
 *  CVS id      :  $Id: cpfspd_hdr.c,v 2.46 2015/10/02 12:50:01 vleutenr Exp $
 *
 *  Name        :  cpfspd_hdr.c
 *
 *  Author      :  Robert Jan Schutten
 *
 *  Function    :  cpfspd HeaDeR check/read/write/copy/print/create routines.
 *                        -  - -
 *
 *  Description :  Header functions to check the validity of
 *                 headers, to read/write headers, to copy headers,
 *                 to print headers & to create new headers.
 *
 *  Functions   :  The following external cpfspd functions are
 *                 defined in this file:
 *
 *                 - p_check_header()
 *                 - p_read_header()
 *                 - p_write_header()
 *                 - p_rewrite_header()
 *                 - p_copy_header()
 *                 - p_print_header()
 *                 - p_create_header()
 *                 - p_create_ext_header()
 *
 *                 The following functions are also used in other
 *                 files, but are not external cpfspd functions:
 *
 *                 - p_get_comp_data_format()
 *                 - p_check_color_format()
 *                 - p_create_free_header()
 *
 */

/******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include "cpfspd.h"
#include "cpfspd_hdr.h"
#include "cpfspd_low.h"

/******************************************************************************/

/* define minimum/maximum macros */
#define MIN(x,y)             ( ((x) < (y)) ? (x) : (y) )
#define MAX(x,y)             ( ((x) > (y)) ? (x) : (y) )

/* Poor man's pow() function (int only) to avoid use of math.h pow() */
static int poor_mans_pow (int x, int y) {
    int res = 1;
    while (y--) res *= x;
    return res;
} /* end of poor_mans_pow () */

/* calculate maximum decimal value that fits in a text field of s chars (excluding \0 termination) */
#define MAX_DECIMAL(s) ((int)(poor_mans_pow(10, s) - 1))

/* switch off debug & error printing */
#define NOPRINT         0

/******************************************************************************/

/* standard color format components */
typedef struct {
    pT_color    format; /* color format */
    int         nr_comp;/* number of components */
    struct {
        char *  name;   /* component name */
        int     pix_ss; /* pixel subsampling */
        int     lin_ss; /* line subsampling */
        int     mplex;  /* multiplex factor (2 for U/v) */
    } comp[3];
} pT_default_format;

/* Predefined components for standard color formats */
static const pT_default_format p_default_formats[] = {
    /* No color, only luminace */
    { P_NO_COLOR,       1, { { P_Y_COM_CODE,    1, 1, 1 },
                             { NULL,            0, 0, 0 },
                             { NULL,            0, 0, 0 } } },
    /* YUV 4:2:2 (multiplexed UV) */
    { P_COLOR_422,      2, { { P_Y_COM_CODE,    1, 1, 1 },
                             { P_UV_COM_CODE,   2, 1, 2 },
                             { NULL,            0, 0, 0 } } },
    /* YUV 4:2:0 (multiplexed UV) */
    { P_COLOR_420,      2, { { P_Y_COM_CODE,    1, 1, 1 },
                             { P_UV_COM_CODE,   2, 2, 2 },
                             { NULL,            0, 0, 0 } } },
    /* YUV 4:4:4 (planar) */
    { P_COLOR_444_PL,   3, { { P_Y_COM_CODE,    1, 1, 1 },
                             { P_U_COM_CODE,    1, 1, 1 },
                             { P_V_COM_CODE,    1, 1, 1 } } },
    /* YUV 4:2:2 (planar) */
    { P_COLOR_422_PL,   3, { { P_Y_COM_CODE,    1, 1, 1 },
                             { P_U_COM_CODE,    2, 1, 1 },
                             { P_V_COM_CODE,    2, 1, 1 } } },
    /* YUV 4:2:0 (planar) */
    { P_COLOR_420_PL,   3, { { P_Y_COM_CODE,    1, 1, 1 },
                             { P_U_COM_CODE,    2, 2, 1 },
                             { P_V_COM_CODE,    2, 2, 1 } } },
    /* RGB (planar) */
    { P_COLOR_RGB,      3, { { P_R_COM_CODE,    1, 1, 1 },
                             { P_G_COM_CODE,    1, 1, 1 },
                             { P_B_COM_CODE,    1, 1, 1 } } },
    /* XYZ */
    { P_COLOR_XYZ,      3, { { P_XYZX_COM_CODE, 1, 1, 1 },
                             { P_XYZY_COM_CODE, 1, 1, 1 },
                             { P_XYZZ_COM_CODE, 1, 1, 1 } } },
    /* Stream format (used for CVBS) */
    { P_STREAM,         1, { { P_S_COM_CODE,    1, 1, 1 },
                             { NULL,            0, 0, 0 },
                             { NULL,            0, 0, 0 } } },
    /* sentinel */
    { P_UNKNOWN_COLOR,  0, { { NULL,            0, 0, 0 },
                             { NULL,            0, 0, 0 },
                             { NULL,            0, 0, 0 } } }
};

/******************************************************************************/

/*
 * Lower level header routines that are also used in other files
 */

/* get data format of a single component */
pT_data_fmt
p_get_comp_data_format (const pT_header *header,
                        int comp_nr)
{
    pT_data_fmt data_fmt;

    if (!strncmp(header->comp[comp_nr].data_fmt, P_B8_DATA_FMT,
                 (size_t)P_SDATA_FMT)) {
        data_fmt = P_8_BIT_FILE;
    } else if (!strncmp(header->comp[comp_nr].data_fmt, P_B10_DATA_FMT,
                        (size_t)P_SDATA_FMT)) {
        data_fmt = P_10_BIT_FILE;
    } else if (!strncmp(header->comp[comp_nr].data_fmt, P_B12_DATA_FMT,
                        (size_t)P_SDATA_FMT)) {
        data_fmt = P_12_BIT_FILE;
    } else if (!strncmp(header->comp[comp_nr].data_fmt, P_B14_DATA_FMT,
                        (size_t)P_SDATA_FMT)) {
        data_fmt = P_14_BIT_FILE;
    } else if (!strncmp(header->comp[comp_nr].data_fmt, P_I2_DATA_FMT,
                        (size_t)P_SDATA_FMT)) {
        data_fmt = P_16_BIT_FILE;
    } else if (!strncmp(header->comp[comp_nr].data_fmt, P_R2_DATA_FMT,
                        (size_t)P_SDATA_FMT)) {
        data_fmt = P_16_REAL_FILE;
    } else {
        data_fmt = P_UNKNOWN_DATA_FORMAT;
    } /* end of if (!strncmp(... */

    return(data_fmt);
} /* end of p_get_comp_data_format () */

/* check data format of header */

pT_status
p_check_file_data_format (const pT_header *header,
                          pT_color color_format,
                          pT_data_fmt *file_data_fmt)
{
    pT_status   status = P_OK;
    int         nr_compon;
    int         i;
    pT_data_fmt temp_data_fmt;
    pT_data_fmt first_comp_data_fmt = P_UNKNOWN_DATA_FORMAT;

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
        break;
    } /* end of switch (color_format) */

    for (i = 0; i < nr_compon; i++) {
        /* try to find data format */
        temp_data_fmt = p_get_comp_data_format(header, i);
        if (i == 0) {
            /* first component */
            first_comp_data_fmt = temp_data_fmt;
        } else {
            /* additional components */
            if (first_comp_data_fmt != temp_data_fmt) {
                status = P_FILE_DATA_FORMATS_NOT_EQUAL;
            } /* end of if (first_comp_data_fmt != temp_data_fmt) */
        } /* end of if (i == 0) */
    } /* end of for (i = 0... */

    /* set data format equal to first one */
    (*file_data_fmt) = first_comp_data_fmt;

    if ((*file_data_fmt) == P_UNKNOWN_DATA_FORMAT
            || (!header->disable_hdr_checks
                && (((*file_data_fmt) == P_16_REAL_FILE)
                && ((color_format != P_COLOR_RGB) && (color_format != P_COLOR_XYZ)))) ) {
        status = P_ILLEGAL_FILE_DATA_FORMAT;
    } /* end of if ((*file_data_fmt) ==... */
    /* return status */
    return status;
} /* end of p_check_file_data_format () */

/* check color format of header */

pT_status
p_check_color_format (const pT_header *header, pT_color *color_format)
{
    pT_status   status = P_OK;
    int f, i, match;

    (*color_format) = P_UNKNOWN_COLOR;
    /* go through the defaults list, last matching entry is returned */
    for (f=0; p_default_formats[f].format != P_UNKNOWN_COLOR; f++)
    {
        if (header->nr_compon >= p_default_formats[f].nr_comp)
        {
            match = 0;
            for (i=0; i<p_default_formats[f].nr_comp; i++)
            {
                if (!strncmp(header->comp[i].com_code, p_default_formats[f].comp[i].name, (size_t)P_SCOM_CODE))
                {
                    if ((header->comp[i].pix_sbsmpl == p_default_formats[f].comp[i].pix_ss) &&
                        (header->comp[i].lin_sbsmpl == p_default_formats[f].comp[i].lin_ss) &&
                        ((header->comp[i].pix_line * header->comp[i].pix_sbsmpl) ==
                         (header->act_pixel * p_default_formats[f].comp[i].mplex) ) )
                    {
                        match++;
                    }
                }
            }
            if (match == p_default_formats[f].nr_comp)
            {
                (*color_format) = p_default_formats[f].format;
            }
        }
    }

    /* if no legal color format is found return error code */
    if ((*color_format) == P_UNKNOWN_COLOR)
    {
        status = P_ILLEGAL_COLOR_FORMAT;
    }

    return status;
} /* end of p_check_color_format () */

/* construct new header from unrestricted values */

pT_status
p_create_free_header (pT_header *header, pT_color color,
                      double ima_freq, double lin_freq,
                      double pix_freq,
                      int act_lines, int act_pixel,
                      int interlace_factor,
                      int h_ratio, int v_ratio /*add data format*/)
{
    pT_status status = P_OK;
    int f, i, comp;

    /*
     * Set all individual fields of header structure
     */

    /* Initialize with defined values everywhere.*/
    memset (header, 0, sizeof(pT_header));

    /* set global header */
    switch (color) {
    case P_NO_COLOR:
    case P_COLOR_422:
    case P_COLOR_420:
    case P_COLOR_444_PL:
    case P_COLOR_422_PL:
    case P_COLOR_420_PL:
    case P_COLOR_RGB:
    case P_COLOR_XYZ:
    case P_STREAM:
        /* global header */
        header->nr_images = 0;
        header->nr_compon = 0;
        header->nr_fd_recs = (P_SDESCRIPTION + P_BYTES_REC - 1) / P_BYTES_REC;
        header->nr_aux_data_recs = 0;
        strncpy (header->appl_type, P_VIDEO_APPL_TYPE,
                 (size_t)(P_SAPPL_TYPE + 1));
        header->bytes_rec = P_BYTES_REC;
        header->little_endian = 0;
        header->nr_aux_hdr_recs = (P_SAUX_HDR + P_BYTES_REC - 1) / P_BYTES_REC;
        header->ima_freq = ima_freq;
        header->lin_freq = lin_freq;
        header->pix_freq = pix_freq;
        header->act_lines = act_lines;
        header->act_pixel = act_pixel;
        header->interlace = interlace_factor;
        header->h_pp_size = h_ratio;
        header->v_pp_size = v_ratio;

        /* nr_fd_recs is sum of actual file description records and auxiliary header records */
        header->nr_fd_recs += header->nr_aux_hdr_recs;
        strcpy((char *)header->aux_hdrs, P_AUX_LAST);
        break;
    default:
        status = P_ILLEGAL_COLOR_FORMAT;
    } /* end of switch (color); set global header */

    if (status == P_OK) {
        /* find the correct index in default formats */
        f = 0;
        while ( (p_default_formats[f].format != color) &&
                (p_default_formats[f].format != P_UNKNOWN_COLOR)) {
            f++;
        }

        for (i=0; i<p_default_formats[f].nr_comp; i++) {
            /* add component and set its properties */
            comp = p_mod_add_comp( header );
            status = p_mod_set_comp_2( header, comp,
                    p_default_formats[f].comp[i].name, P_8_BIT_FILE,
                    p_default_formats[f].comp[i].pix_ss,
                    p_default_formats[f].comp[i].lin_ss,
                    p_default_formats[f].comp[i].mplex );
            if (status != P_OK) {
                break;
            }
        };
    } /* end of if (status == P_OK); set all component headers */

    return status;
} /* end of p_create_free_header () */

/******************************************************************************/

/*
 * Lower level header routines that are only internally used
 */

/* check generic pfspd file format and cpfspd limitations */

static pT_status
p_check_hdr_basic (const pT_header *header)
{
    pT_status   status = P_OK;
    int         i;

    if ((header->nr_images < 0) || (header->nr_images > MAX_DECIMAL(P_SNR_IMAGES))) {
        status = P_TOO_MANY_IMAGES;
    }

    if ((header->nr_compon < 0) || (header->nr_compon > P_PFSPD_MAX_COMP)) {
        status = P_TOO_MANY_COMPONENTS;
    }

    if (header->nr_aux_hdr_recs * header->bytes_rec > P_SAUX_HDR)
    {
        status = P_EXCEEDING_AUXILIARY_HDR_SIZE;
    }

    if ((header->act_lines < 0) || (header->act_lines > MAX_DECIMAL(P_SACT_LINES))) {
        status = P_ILLEGAL_IMAGE_SIZE;
    }

    if ((header->act_pixel < 0) || (header->act_pixel > MAX_DECIMAL(P_SACT_PIXEL))) {
        status = P_ILLEGAL_IMAGE_SIZE;
    }

    if ((header->interlace < 0) || (header->interlace > 2)) {
        status = P_ILLEGAL_INTERLACE;
    }

    for (i=0; (status == P_OK) && (i<header->nr_compon); i++) {
        if ((header->comp[i].lin_image < 0) || (header->comp[i].lin_image > MAX_DECIMAL(P_SLIN_IMAGE))) {
            status = P_ILLEGAL_COMP_SIZE;
        }
        if ((header->comp[i].pix_line < 0) || (header->comp[i].pix_line > MAX_DECIMAL(P_SPIX_LINE))) {
            status = P_ILLEGAL_COMP_SIZE;
        }
        if (header->comp[i].tem_sbsmpl != 1) {
            status = P_ILLEGAL_TEM_SBSMPL;
        }
        if ((header->comp[i].lin_sbsmpl < 0) || (header->comp[i].lin_sbsmpl > MAX_DECIMAL(P_SLIN_SBSMPL))) {
            status = P_ILLEGAL_LIN_SBSMPL;
        }
        if ((header->comp[i].pix_sbsmpl < 0) || (header->comp[i].pix_sbsmpl > MAX_DECIMAL(P_SPIX_SBSMPL))) {
            status = P_ILLEGAL_PIX_SBSMPL;
        }
        if ((header->comp[i].tem_phshft < 0) || (header->comp[i].tem_phshft > MAX_DECIMAL(P_STEM_PHSHFT))) {
            status = P_ILLEGAL_PHSHFT;
        }
        if ((header->comp[i].lin_phshft < 0) || (header->comp[i].lin_phshft > MAX_DECIMAL(P_SLIN_PHSHFT))) {
            status = P_ILLEGAL_PHSHFT;
        }
        if ((header->comp[i].pix_phshft < 0) || (header->comp[i].pix_phshft > MAX_DECIMAL(P_SPIX_PHSHFT))) {
            status = P_ILLEGAL_PHSHFT;
        }
    }

    return status;
} /* end of p_check_hdr_basic */

/* check component sizes of header */

static pT_status
p_check_component_sizes (const pT_header *header, pT_color color_format)
{
    pT_status status = P_OK;
    int       i;
    int       multiplex;

    /* check first component spec (or all components of RGB header) */
    switch (color_format) {
    case P_NO_COLOR:
    case P_COLOR_422:
    case P_COLOR_420:
    case P_COLOR_444_PL:
    case P_COLOR_422_PL:
    case P_COLOR_420_PL:
        if ((header->comp[0].pix_line != header->act_pixel)
        ||  (header->comp[0].lin_image*header->interlace != header->act_lines)) {
            status = P_WRONG_LUM_COMP_SIZE;
        }
        break;
    case P_STREAM:
        if ((header->comp[0].pix_line != header->act_pixel)
        ||  (header->comp[0].lin_image*header->interlace != header->act_lines)) {
            status = P_WRONG_STREAM_COMP_SIZE;
        }
        break;
    case P_COLOR_RGB:
        if ((header->comp[0].pix_line != header->act_pixel)
        ||  (header->comp[0].lin_image*header->interlace != header->act_lines)
        ||  (header->comp[1].pix_line != header->act_pixel)
        ||  (header->comp[1].lin_image*header->interlace != header->act_lines)
        ||  (header->comp[2].pix_line != header->act_pixel)
        ||  (header->comp[2].lin_image*header->interlace != header->act_lines)) {
            status = P_WRONG_RGB_COMP_SIZE;
        }
        break;
    case P_COLOR_XYZ:
        if ((header->comp[0].pix_line != header->act_pixel)
        ||  (header->comp[0].lin_image*header->interlace != header->act_lines)
        ||  (header->comp[1].pix_line != header->act_pixel)
        ||  (header->comp[1].lin_image*header->interlace != header->act_lines)
        ||  (header->comp[2].pix_line != header->act_pixel)
        ||  (header->comp[2].lin_image*header->interlace != header->act_lines)) {
            status = P_WRONG_XYZ_COMP_SIZE;
        }
        break;
    default:
        /* unknown color format */
        status = P_ILLEGAL_COLOR_FORMAT;
        break;
    }

    if (status == P_OK) {
        /* check chrominance component spec */
        switch (color_format) {
        case P_COLOR_422:
        case P_COLOR_420:
            if (((header->comp[1].pix_line * header->comp[1].pix_sbsmpl) !=
                  header->act_pixel*2)
            ||  ((header->comp[1].lin_image * header->comp[1].lin_sbsmpl * header->interlace) !=
                  header->act_lines)) {
                status = P_WRONG_CHR_COMP_SIZE;
            }
            break;
        case P_COLOR_444_PL:
        case P_COLOR_422_PL:
        case P_COLOR_420_PL:
            if (((header->comp[1].pix_line * header->comp[1].pix_sbsmpl) !=
                  header->act_pixel) ||
                ((header->comp[1].lin_image * header->comp[1].lin_sbsmpl * header->interlace) !=
                  header->act_lines) ||
                ((header->comp[2].pix_line * header->comp[2].pix_sbsmpl) !=
                  header->act_pixel) ||
                ((header->comp[2].lin_image * header->comp[2].lin_sbsmpl * header->interlace) !=
                  header->act_lines)) {
                status = P_WRONG_CHR_COMP_SIZE;
            }
            break;
        default:
            break;
        }
    }

    if (status == P_OK) {
        /* check extra components */

        i = 0;
        switch (color_format) {
        case P_NO_COLOR:
        case P_STREAM:
            i = 1;
            break;
        case P_COLOR_422:
        case P_COLOR_420:
            i = 2;
            break;
        case P_COLOR_444_PL:
        case P_COLOR_422_PL:
        case P_COLOR_420_PL:
        case P_COLOR_RGB:
        case P_COLOR_XYZ:
            i = 3;
            break;
        default:
            break;
        }
        while (i < header->nr_compon) {
            /* Padding component can have any size - so we don't check it */
            if (strncmp(header->comp[i].com_code,P_P_COM_CODE,P_SCOM_CODE) != 0) {
                /* horizontal size may by a multiple of act_pixel (due to multiplexing) */
                multiplex = (header->comp[i].pix_line * header->comp[i].pix_sbsmpl) / header->act_pixel;
                if ((((header->comp[i].pix_line * header->comp[i].pix_sbsmpl) !=
                      header->act_pixel * multiplex) ||
                     ((header->comp[i].lin_image * header->comp[i].lin_sbsmpl * header->interlace) !=
                      header->act_lines))) {
                    status = P_WRONG_EXTRA_COMP_SIZE;
                }
            }
            i++;
        }
    }

    return status;
} /* end of p_check_component_sizes () */

/* largest common divider (positive numbers only)
 * small helper function for calculations related
 * to P_AS_WH aspect ratio.
 * note: identical code is in cpfspd_mod.c
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

/* set new header values */

static pT_status
p_set_header_values (pT_freq image_freq,
                     pT_size image_size,
                     int pixels_per_line,
                     int progressive,
                     pT_asp_rat ratio,
                     double *ima_freq,
                     double *lin_freq,
                     double *pix_freq,
                     int *act_lines,
                     int *act_pixel,
                     int *interlace_factor,
                     int *h_ratio,
                     int *v_ratio)
{
    pT_status status = P_OK;
    int       div;

    /*
     * Set all required header values
     */

    /* set image frequency */
    switch (image_freq) {
    case P_50HZ:
        (*ima_freq) = P_STD_IMA_FREQ_50HZ;
        break;
    case P_25HZ:
        (*ima_freq) = P_STD_IMA_FREQ_50HZ / 2.0;
        break;
    case P_60HZ:
        (*ima_freq) = P_STD_IMA_FREQ_60HZ;
        break;
    case P_24HZ:
        (*ima_freq) = P_STD_IMA_FREQ_60HZ / 2.5;
        break;
    case P_30HZ:
        (*ima_freq) = P_STD_IMA_FREQ_60HZ / 2.0;
        break;
    case P_REAL_60HZ:
        (*ima_freq) = P_STD_IMA_FREQ_REAL_60HZ;
        break;
    case P_REAL_24HZ:
        (*ima_freq) = P_STD_IMA_FREQ_REAL_60HZ / 2.5;
        break;
    case P_REAL_30HZ:
        (*ima_freq) = P_STD_IMA_FREQ_REAL_60HZ / 2.0;
        break;
    default:
        /* assign values to avoid compiler warnings */
        (*ima_freq) = 0.0;
        status = P_ILLEGAL_IMAGE_FREQUENCY;
    }

    /* exit on errors */
    if (status != P_OK) {
        return status;
    }

    /*
     * Formulas to calculate line/pixel frequencies:
     *
     *   (*pix_freq) = tot_pixel * (*lin_freq);
     *   (*lin_freq) = tot_lines * (*ima_freq) / 2; (interlaced!)
     *
     * The following values of line/pixel frequencies are
     * for interlaced sequences:
     */
    if ((image_freq == P_50HZ) || (image_freq == P_25HZ)) {
        /* 50 Hz case */

        /* set line frequency */
        switch (image_size) {
        case P_QCIF:
        case P_CIF:
        case P_SD:
            /* tot_lines = 625 */
            (*lin_freq) = 15.625;
            break;
        case P_HDi:
            /* tot_lines = 1250 */
            (*lin_freq) = 31.25;
            break;
        default:
            /* assign values to avoid compiler warnings */
            (*lin_freq) = 0.0;
            status = P_ILLEGAL_IMAGE_SIZE;
        } /* end of switch (image_size); set line frequency */

        /* set active number of lines */
        switch (image_size) {
        case P_QCIF:
            (*act_lines) = 144;
            break;
        case P_CIF:
            (*act_lines) = 288;
            break;
        case P_SD:
            (*act_lines) = 576;
            break;
        case P_HDi:
            (*act_lines) = 1152;
            break;
        default:
            /* assign values to avoid compiler warnings */
            (*act_lines) = 0;
            status = P_ILLEGAL_IMAGE_SIZE;
        } /* end of switch (image_size); set active number of lines */

    } else {
        /* 60 Hz case: */

        /* set line frequency */
        switch (image_size) {
        case P_QCIF:
        case P_CIF:
        case P_SD:
            if ((image_freq == P_60HZ) || (image_freq == P_24HZ) ||
                (image_freq == P_30HZ)) {
                /* tot_lines = 525 */
                (*lin_freq) = 15.734264; /* from ITU-R BT.470-6 */
            } else {
                (*lin_freq) = 15.75;     /* 60 x 525 / 2 */
            }
            break;
        case P_HDp:
            if ((image_freq == P_60HZ) || (image_freq == P_24HZ) ||
                (image_freq == P_30HZ)) {
                /* tot_lines = 750; from ATSC document A/54 */
                (*lin_freq) = 22.4775; /* 59.94 x 750 / 2 */
            } else {
                (*lin_freq) = 22.5;    /* 60 x 750 / 2 */
            }
            break;
        case P_HDi:
            if ((image_freq == P_60HZ) || (image_freq == P_24HZ) ||
                (image_freq == P_30HZ)) {
                /* tot_lines = 1125; from ATSC document A/54 */
                (*lin_freq) = 33.71625; /* 59.94 x 1125 / 2 */
            } else {
                (*lin_freq) = 33.75;    /* 60 x 1125 / 2 */
            }
            break;
        default:
            /* Assign values to avoid compiler warnings */
            (*lin_freq) = 0.0;
            status = P_ILLEGAL_IMAGE_SIZE;
        } /* end of switch (image_size); set line frequency */

        /* set active number of lines */
        switch (image_size) {
        case P_QCIF:
            (*act_lines) = 120;
            break;
        case P_CIF:
            (*act_lines) = 240;
            break;
        case P_SD:
            (*act_lines) = 480;
            break;
        case P_HDp:
            (*act_lines) = 720;
            break;
        case P_HDi:
            (*act_lines) = 1080;
            break;
        default:
            /* assign values to avoid compiler warnings */
            (*act_lines) = 0;
            status = P_ILLEGAL_IMAGE_SIZE;
        } /* end of switch (image_size); set active number of lines */
    } /* end of if ((image_freq == P_50HZ) || (image_freq == P_25HZ)) */

    /* exit on errors */
    if (status != P_OK) {
        return status;
    }

    /* set pixel frequency & active number of pixels */
    switch (image_size) {
    case P_QCIF:
        switch (pixels_per_line) {
        case 0:
        case 176:
            (*pix_freq)  = 13.5;
            (*act_pixel) = 176;
            break;
        case 180:
            (*pix_freq)  = 13.5;
            (*act_pixel) = 180;
            break;
        default:
            /* assign values to avoid compiler warnings */
            (*pix_freq)  = 0.0;
            (*act_pixel) = 0;
            status = P_ILLEGAL_NUM_OF_PIX_PER_LINE;
        } /* end of switch (pixels_per_line); case P_QCIF */
        break;
    case P_CIF:
        switch (pixels_per_line) {
        case 0:
        case 352:
            (*pix_freq)  = 13.5;
            (*act_pixel) = 352;
            break;
        case 360:
            (*pix_freq)  = 13.5;
            (*act_pixel) = 360;
            break;
        default:
            /* assign values to avoid compiler warnings */
            (*pix_freq)  = 0.0;
            (*act_pixel) = 0;
            status = P_ILLEGAL_NUM_OF_PIX_PER_LINE;
        } /* end of switch (pixels_per_line); case P_CIF */
        break;
    case P_SD:
        switch (pixels_per_line) {
        case 512:
            (*pix_freq)  = 9.6;
            (*act_pixel) = 512;
            break;
        case 640:
            (*pix_freq)  = 12.0;
            (*act_pixel) = 640;
            break;
        case 704:
            (*pix_freq)  = 13.5;
            (*act_pixel) = 704;
            break;
        case 0:
        case 720:
            /* tot_pixel = 864 for 50 Hz; 858 for 59.94 Hz */
            (*pix_freq)  = 13.5;
            (*act_pixel) = 720;
            break;
        case 848:
            (*pix_freq)  = 16.0;
            (*act_pixel) = 848;
            break;
        case 960:
            (*pix_freq)  = 18.0;
            (*act_pixel) = 960;
            break;
        case 1024:
            (*pix_freq)  = 19.2;
            (*act_pixel) = 1024;
            break;
        case 1280:
            (*pix_freq)  = 24.0;
            (*act_pixel) = 1280;
            break;
        case 1440:
            (*pix_freq)  = 27.0;
            (*act_pixel) = 1440;
            break;
        default:
            /* assign values to avoid compiler warnings */
            (*pix_freq)  = 0.0;
            (*act_pixel) = 0;
            status = P_ILLEGAL_NUM_OF_PIX_PER_LINE;
        } /* end of switch (pixels_per_line); case P_SD */
        break;
    case P_HDp:
        /* only for 60 Hz;
           note that these values are for interlaced;
           they will be corrected later on */
        switch (pixels_per_line) {
        case 960:
            (*pix_freq)  = 27.84375;
            (*act_pixel) = 960;
            break;
        case 1024:
            (*pix_freq)  = 29.7;
            (*act_pixel) = 1024;
            break;
        case 0:
        case 1280:
            /* tot_pixel = 1650 */
            (*pix_freq)  = 37.125;
            (*act_pixel) = 1280;
            break;
        case 1440:
            (*pix_freq)  = 41.765625;
            (*act_pixel) = 1440;
            break;
        case 1920:
            (*pix_freq)  = 55.6875;
            (*act_pixel) = 1920;
            break;
        default:
            /* assign values to avoid compiler warnings */
            (*pix_freq)  = 0.0;
            (*act_pixel) = 0;
            status = P_ILLEGAL_NUM_OF_PIX_PER_LINE;
        } /* end of switch (pixels_per_line); case P_HDp */
        break;
    case P_HDi:
        if ((image_freq == P_50HZ) || (image_freq == P_25HZ)) {
            /* 50 Hz case */
            switch (pixels_per_line) {
            case 960:
                (*pix_freq)  = 36.0;
                (*act_pixel) = 960;
                break;
            case 1024:
                (*pix_freq)  = 38.4;
                (*act_pixel) = 1024;
                break;
            case 1280:
                (*pix_freq)  = 48.0;
                (*act_pixel) = 1280;
                break;
            case 0:
            case 1440:
                 /* tot_pixel = 1728 */
                (*pix_freq)  = 54.0;
                (*act_pixel) = 1440;
                break;
            case 1920:
                (*pix_freq)  = 72.0;
                (*act_pixel) = 1920;
                break;
            default:
                /* assign values to avoid compiler warnings */
                (*pix_freq)  = 0.0;
                (*act_pixel) = 0;
                status = P_ILLEGAL_NUM_OF_PIX_PER_LINE;
            } /* end of switch (pixels_per_line); case P_HDi, 50 Hz */
        } else {
            /* 60 Hz case */
            switch (pixels_per_line) {
            case 960:
                (*pix_freq)  = 37.125;
                (*act_pixel) = 960;
                break;
            case 1024:
                (*pix_freq)  = 39.6;
                (*act_pixel) = 1024;
                break;
            case 1280:
                (*pix_freq)  = 49.5;
                (*act_pixel) = 1280;
                break;
            case 1440:
                (*pix_freq)  = 55.6875;
                (*act_pixel) = 1440;
                break;
            case 0:
            case 1920:
                /* tot_pixel = 2200 */
                (*pix_freq)  = 74.25;
                (*act_pixel) = 1920;
                break;
            default:
                /* assign values to avoid compiler warnings */
                (*pix_freq)  = 0.0;
                (*act_pixel) = 0;
                status = P_ILLEGAL_NUM_OF_PIX_PER_LINE;
            } /* end of switch (pixels_per_line); case P_HDi, 60 Hz */
        } /* end of if ((image_freq == P_50HZ) || (image_freq == P_25HZ)) */
        break;
    default:
        /* assign values to avoid compiler warnings */
        (*pix_freq)  = 0.0;
        (*act_pixel) = 0;
        status = P_ILLEGAL_IMAGE_SIZE;
    } /* end of switch (image_size); set pixel frequency */

    /* exit on errors */
    if (status != P_OK) {
        return status;
    }

    /* correction for special film modes */
    switch (image_freq) {
    case P_25HZ:
    case P_24HZ:
    case P_30HZ:
    case P_REAL_24HZ:
    case P_REAL_30HZ:
        (*lin_freq) = 0.0;
        (*pix_freq) = 0.0;
        break;
    default:
        break;
    }

    /* correction for progressive sequences */
    if (progressive) {
        (*interlace_factor) = 1;
        (*pix_freq) *= 2.0;
        (*lin_freq) *= 2.0;
    } else {
        (*interlace_factor) = 2;
    }

    /* Automatic determination of most appropriate default aspect ratio */
    if (ratio == P_UNKNOWN_ASPECT_RATIO) {
        switch (image_size) {
        case P_SD:
            if (pixels_per_line > 720) {
                ratio = P_16_9;
            } else {
                ratio = P_4_3;
            }
            break;
        case P_CIF:
            if (pixels_per_line > 352) {
                ratio = P_16_9;
            } else {
                ratio = P_4_3;
            }
            break;
        case P_QCIF:
            if (pixels_per_line > 176) {
                ratio = P_16_9;
            } else {
                ratio = P_4_3;
            }
            break;
        case P_HDp:
        case P_HDi:
            ratio = P_16_9;
            break;
        default:
            /* Ignore - will become an error below */
            break;
        }
    }

    /* set aspect ratio */
    switch (ratio) {
    case P_4_3:
        (*h_ratio) = 4;
        (*v_ratio) = 3;
        break;
    case P_16_9:
        (*h_ratio) = 16;
        (*v_ratio) = 9;
        break;
    case P_AS_WH:
        div = lcd (*act_pixel, *act_lines);
        (*h_ratio) = *act_pixel / div;
        (*v_ratio) = *act_lines / div;
        break;
    default:
        /* assign values to avoid compiler warnings */
        (*h_ratio) = 0;
        (*v_ratio) = 0;
        status = P_ILLEGAL_ASPECT_RATIO;
    }

    return status;
}

/* set new stream header values */

static pT_status
p_set_stream_header_values (pT_freq image_freq,
                            pT_size image_size,
                            int pixels_per_line,
                            pT_asp_rat ratio,
                            double *ima_freq,
                            double *lin_freq,
                            double *pix_freq,
                            int *act_lines,
                            int *act_pixel,
                            int *interlace_factor,
                            int *h_ratio,
                            int *v_ratio)
{
    pT_status status = P_OK;
    int       div;

    /* set image frequency; only 25 & 30 Hz are supported */
    switch (image_freq) {
    case P_25HZ:
        (*ima_freq) = P_STD_IMA_FREQ_50HZ / 2.0;
        /* set line frequency, active number of lines,
               pixel frequency & active number of pixels */
        switch (image_size) {
        case P_SD:
            /* tot_lines = 625 */
            (*lin_freq) = 15.625;
            (*act_lines) = 625;
            switch (pixels_per_line) {
            case 0:
            case 864:
                /* tot_pixel = 864 for 50 Hz */
                (*pix_freq)  = 13.5;
                (*act_pixel) = 864;
                break;
            case 1024:
                (*pix_freq)  = 16.0;
                (*act_pixel) = 1024;
                break;
            case 1152:
                (*pix_freq)  = 18.0;
                (*act_pixel) = 1152;
                break;
            default:
                /* assign values to avoid compiler warnings */
                (*pix_freq)  = 0.0;
                (*act_pixel) = 0;
                status = P_ILLEGAL_NUM_OF_PIX_PER_LINE;
            }  /* end of switch (pixels_per_line); case P_SD */
            break;
        default:
            /* assign values to avoid compiler warnings */
            (*lin_freq) = 0.0;
            (*act_lines) = 0;
            status = P_ILLEGAL_IMAGE_SIZE;
        } /* end of switch (image_size); set line frequency, etc. */
        break;
    case P_30HZ:
        (*ima_freq) = P_STD_IMA_FREQ_60HZ / 2.0;
        /* set line frequency, active number of lines,
               pixel frequency & active number of pixels */
        switch (image_size) {
        case P_SD:
            /* tot_lines = 525 */
            (*lin_freq) = 15.734264; /* from ITU-R BT.470-6 */
            (*act_lines) = 525;
            switch (pixels_per_line) {
            case 0:
            case 858:
                /* tot_pixel = 858 for 59.94 Hz */
                (*pix_freq)  = 13.5;
                (*act_pixel) = 858;
                break;
            case 1144:
                (*pix_freq)  = 18.0;
                (*act_pixel) = 1144;
                break;
            default:
                /* assign values to avoid compiler warnings */
                (*pix_freq)  = 0.0;
                (*act_pixel) = 0;
                status = P_ILLEGAL_NUM_OF_PIX_PER_LINE;
            }  /* end of switch (pixels_per_line); case P_SD */
            break;
        default:
            /* Assign values to avoid compiler warnings */
            (*lin_freq) = 0.0;
            (*act_lines) = 0  ;
            status = P_ILLEGAL_IMAGE_SIZE;
        } /* end of switch (image_size); set line frequency, etc. */
        break;
    default:
        /* assign values to avoid compiler warnings */
        (*ima_freq) = 0.0;
        status = P_ILLEGAL_IMAGE_FREQUENCY;
    }

    /* exit on errors */
    if (status != P_OK) {
        return status;
    }

    /* stream format is always progressive */
    (*interlace_factor) = 1;

    /* Automatic determination of most appropriate default aspect ratio */
    if (ratio == P_UNKNOWN_ASPECT_RATIO) {
        if (pixels_per_line > 720) {
            ratio = P_16_9;
        } else {
            ratio = P_4_3;
        }
    }

    /* set aspect ratio */
    switch (ratio) {
    case P_4_3:
        (*h_ratio) = 4;
        (*v_ratio) = 3;
        break;
    case P_16_9:
        (*h_ratio) = 16;
        (*v_ratio) = 9;
        break;
    case P_AS_WH:
        div = lcd (*act_pixel, *act_lines);
        (*h_ratio) = *act_pixel / div;
        (*v_ratio) = *act_lines / div;
        break;
    default:
        /* assign values to avoid compiler warnings */
        (*h_ratio) = 0;
        (*v_ratio) = 0;
        status = P_ILLEGAL_ASPECT_RATIO;
    }

    return status;
}

/******************************************************************************/

/*
 * External functions
 */

/* check header */

pT_status
p_check_header (const pT_header *header)
{
    pT_status   status = P_OK;
    pT_color    color_format = -1;
    pT_data_fmt file_data_fmt;

    if (status == P_OK) {
        status = p_check_hdr_basic (header);
    } /* end of if (status == P_OK) */

    /* Return in case further checking disabled by the application */
    if (header->disable_hdr_checks) {
        return status;
    }
    if (status == P_OK) {
        status = p_check_color_format (header, &color_format);
    } /* end of if (status == P_OK) */
    if (status == P_OK) {
        status = p_check_component_sizes (header, color_format);
    } /* end of if (status == P_OK) */
    if (status == P_OK) {
        status = p_check_file_data_format (header, color_format,
                                           &file_data_fmt);
    } /* end of if (status == P_OK) */

    return status;
} /* end of p_check_header () */

/* read header */

pT_status
p_read_header (const char *filename,
               pT_header *header)
{
    pT_status status = P_OK;

    if (filename == NULL) {
        status = P_FILE_OPEN_FAILED;
    }
    if (status == P_OK) {
        status = p_read_hdr (filename, header,
                             stderr, NOPRINT);
    }
    if (status == P_OK) {
        status = p_check_header (header);
    } /* end of if (status == P_OK) */
    /* the header that exists on disk is now identical to
       the header in the structure that exists in memory;
       so the modified bit is set to zero */
    header->modified = 0;
    return status;
} /* end of p_read_header () */

/* write header */

pT_status
p_write_header (const char *filename,
                pT_header *header)
{
    pT_status status = P_OK;

    if (filename == NULL) {
        status = P_FILE_CREATE_FAILED;
    }
    if (status == P_OK) {
        status = p_check_header (header);
    }
    if (status == P_OK) {
        /* force a minimum file description and auxiliary header size */
        if (!(header->disable_hdr_checks)) {
            int min_aux_recs;
            int min_fd_recs;

            min_aux_recs = P_SAUX_HDR / header->bytes_rec;
            min_fd_recs = P_SDESCRIPTION / header->bytes_rec;

            header->nr_aux_hdr_recs = MAX(header->nr_aux_hdr_recs, min_aux_recs);
            header->nr_fd_recs = MAX(header->nr_fd_recs, header->nr_aux_hdr_recs+min_fd_recs);
        }
        status = p_write_hdr (filename, header,
                              stderr, NOPRINT, 0);
    } /* end of if (status == P_OK) */

    /* the header that exists on disk is now identical to
       the header in the structure that exists in memory;
       so the modified bit is set to zero */
    header->modified = 0;
    return status;
} /* end of p_write_header () */

/* rewrite header */

pT_status
p_rewrite_header (const char *filename,
                  pT_header *header)
{
    pT_status status = P_OK;
    pT_header old_header;
    int       aux_id;
    int       old_aux_id;
    int       max_size;
    int       old_max_size;
    int       old_size;
    int       new_size;
    int       i;

    if (filename == NULL) {
        status = P_FILE_MODIFY_FAILED;
    }
    if (status == P_OK) {
        status = p_check_header (header);
    }
    if (status == P_OK) {
        /*
         * Check which fields are different from the data in the file.
         * So, first read the header and compare old and new one.
         */

        status = p_read_hdr (filename, &old_header,
                             stderr, NOPRINT);

        if (status == P_OK) {
            /* Following main header fields may be different:
             *     appl_type
             *     ima_freq
             *     lin_freq
             *     pix_freq
             *     h_pp_size
             *     v_pp_size
             */
            old_size = old_header.act_lines * old_header.act_pixel;
            new_size = header->act_lines * header->act_pixel;
            if ((old_header.nr_images       != header->nr_images) ||
                (old_header.nr_compon       != header->nr_compon) ||
                (old_header.nr_fd_recs      != header->nr_fd_recs) ||
                (old_header.nr_aux_data_recs!= header->nr_aux_data_recs) ||
                (old_header.bytes_rec       != header->bytes_rec) ||
                (old_header.little_endian   != header->little_endian) ||
                (old_header.nr_aux_hdr_recs != header->nr_aux_hdr_recs) ||
                (old_header.interlace       != header->interlace) ||
                (old_size                   != new_size)) {
                status = P_REWRITE_MODIFIED_HEADER;
            }
        }

        if (status == P_OK) {
            /* Following component header fields may be different:
             *     tem_sbsmpl
             *     lin_sbsmpl
             *     pix_sbsmpl
             *     tem_phshft
             *     lin_phshft
             *     pix_phshft
             *     com_code
             */
            for (i=0; i<header->nr_compon; i++) {
                old_size = old_header.comp[i].lin_image * old_header.comp[i].pix_line;
                new_size = header->comp[i].lin_image * header->comp[i].pix_line;
                if ((old_size != new_size) ||
                    strcmp(old_header.comp[i].data_fmt, header->comp[i].data_fmt) != 0) {
                    status = P_REWRITE_MODIFIED_HEADER;
                }
            }
        }

        /* Check whether there is data in description but no space in file */
        if ((status == P_OK) && (header->nr_fd_recs == 0)) {
            for (i=0; (status == P_OK) && (i < P_SDESCRIPTION); i++) {
                if (header->description[i] != '\0') {
                    status = P_EXCEEDING_DESCRIPTION_SIZE;
                }
            }
        }

        /* Check whether aux data definition is equal (same order, max_size)
         * The following properties may be different:
         *     name
         *     description
         *     descr_len
         *     any aux header with max_size == 0 (i.e. without any aux data)
         */
        if (status == P_OK) {
            /* Get first old aux header with data */
            old_aux_id = -1;
            old_max_size = 0;
            do {
                old_aux_id++;
                if (p_get_aux(&old_header, old_aux_id, &old_max_size, NULL, NULL, NULL) != P_OK) {
                    old_aux_id = -1;
                }
            } while ((old_aux_id >= 0) && (old_max_size == 0));

            /* Check all old aux headers with data */
            aux_id = -1;
            max_size = 0;
            while ((status == P_OK) && (old_aux_id != -1)) {
                /* Get next new aux header with data */
                do {
                    aux_id++;
                    if (p_get_aux(header, aux_id, &max_size, NULL, NULL, NULL) != P_OK) {
                        aux_id = -1;
                    }
                } while ((aux_id >= 0) && (max_size == 0));

                /* Compare: new must be found and size must be equal */
                if ((aux_id == -1) || (old_max_size != max_size)) {
                    status = P_REWRITE_MODIFIED_HEADER;
                }

                /* Get next old aux header with data */
                do {
                    old_aux_id++;
                    if (p_get_aux(&old_header, old_aux_id, &old_max_size, NULL, NULL, NULL) != P_OK) {
                        old_aux_id = -1;
                    }
                } while ((old_aux_id >= 0) && (old_max_size == 0));
            }

            /* Search for next new aux header with data (shall not exist) */
            do {
                aux_id++;
                if (p_get_aux(header, aux_id, &max_size, NULL, NULL, NULL) != P_OK) {
                    aux_id = -1;
                }
            } while ((aux_id >= 0) && (max_size == 0));

            if (aux_id >= 0) {
                /* One more aux header with data found: error */
                status = P_REWRITE_MODIFIED_HEADER;
            }
        }

        if (status == P_OK) {
            status = p_write_hdr (filename, header,
                                  stderr, NOPRINT, 1);
        }
    } /* end of if (status == P_OK) */

    /* the header that exists on disk is now identical to
       the header in the structure that exists in memory;
       so the modified bit is set to zero */
    header->modified = 0;
    return status;
} /* end of p_rewrite_header () */

/* copy header */

pT_status
p_copy_header (pT_header *new_header,
               const pT_header *header)
{
    pT_status status = P_OK;
    status = p_check_header (header);
    if (status == P_OK) {
        memcpy ((void *)new_header, (const void*)header,
                (size_t)sizeof(pT_header));
    } /* end of if (status == P_OK) */
    return status;
}

/* print header */

pT_status
p_print_header (const pT_header *header,
                FILE *stream)
{
    pT_status   status = P_OK;
    int         i;
    fprintf (stream, "GLOBAL\n");
    fprintf (stream, "number of images                     : %d\n",
             header->nr_images);
    fprintf (stream, "number of components                 : %d\n",
             header->nr_compon);
    fprintf (stream, "number of file description records   : %d\n",
             header->nr_fd_recs);
    fprintf (stream, "number of auxiliary data records     : %d\n",
             header->nr_aux_data_recs);
    fprintf (stream, "application type                     : %s\n",
             header->appl_type);
    fprintf (stream, "bytes per record                     : %d\n",
             header->bytes_rec);
    fprintf (stream, "little endian                        : %d\n",
             header->little_endian);
    fprintf (stream, "number of auxiliary header records   : %d\n",
             header->nr_aux_hdr_recs);
    fprintf (stream, "image frequency                      : %f\n",
             header->ima_freq);
    fprintf (stream, "line frequency                       : %f\n",
             header->lin_freq);
    fprintf (stream, "pixel frequency                      : %f\n",
             header->pix_freq);
    fprintf (stream, "active lines                         : %d\n",
             header->act_lines);
    fprintf (stream, "active pixels                        : %d\n",
             header->act_pixel);
    fprintf (stream, "interlace factor                     : %d\n",
             header->interlace);
    fprintf (stream, "horizontal proportional picture size : %d\n",
             header->h_pp_size);
    fprintf (stream, "vertical proportional picture size   : %d\n",
             header->v_pp_size);
    if (header->nr_compon > P_PFSPD_MAX_COMP) {
        status = P_TOO_MANY_COMPONENTS;
    } else {
        for (i = 0; i < header->nr_compon; i++) {
            fprintf (stream, "COMPONENT %d\n", i);
            fprintf (stream, "lines per image      : %d\n",
                     header->comp[i].lin_image);
            fprintf (stream, "pixels per line      : %d\n",
                     header->comp[i].pix_line);
            fprintf (stream, "data format          : %s\n",
                     header->comp[i].data_fmt);
            fprintf (stream, "temporal subsampling : %d\n",
                     header->comp[i].tem_sbsmpl);
            fprintf (stream, "line subsampling     : %d\n",
                     header->comp[i].lin_sbsmpl);
            fprintf (stream, "pixel subsampling    : %d\n",
                     header->comp[i].pix_sbsmpl);
            fprintf (stream, "temporal phase shift : %d\n",
                     header->comp[i].tem_phshft);
            fprintf (stream, "line phase shift     : %d\n",
                     header->comp[i].lin_phshft);
            fprintf (stream, "pixel phase shift    : %d\n",
                     header->comp[i].pix_phshft);
            fprintf (stream, "component code       : %s\n",
                     header->comp[i].com_code);
        } /* end of for (i = 0; loop over all components */
    } /* end of if (header->nr_compon > P_PFSPD_MAX_COMP) */
    return status;
} /* end of p_print_header () */

/* create header */

pT_status
p_create_header (pT_header *header,
                 pT_color color,
                 pT_freq image_freq)
{
    pT_status status = P_OK;
    if (color == P_STREAM) {
        status = p_create_ext_header (header, color, image_freq,
                                      P_SD, 0, 1, P_4_3);
    } else {
        status = p_create_ext_header (header, color, image_freq,
                                      P_SD, 0, 0, P_4_3);
    }
    return status;
} /* end of p_create_header () */

/* create extended header */

pT_status
p_create_ext_header (pT_header *header,
                     pT_color color,
                     pT_freq image_freq,
                     pT_size image_size,
                     int pixels_per_line,
                     int progressive,
                     pT_asp_rat ratio)
{
    pT_status status = P_OK;
    double    ima_freq = 0.0;
    double    lin_freq = 0.0;
    double    pix_freq = 0.0;
    int       act_lines = 0;
    int       act_pixel = 0;
    int       interlace_factor = 0;
    int       h_ratio = 0;
    int       v_ratio = 0;

    /*
     * Check if given combination is valid (combination of
     * image_freq, image_size and interlaced).
     * All (valid) values of color and ratio are accepted.
     */

    if ((image_size == P_HDp) &&
        ((image_freq == P_50HZ) || (image_freq == P_25HZ))) {
        status = P_ILLEGAL_SIZE_FREQUENCY;
    }
    if ((image_size == P_HDp) &&
        (progressive == 0)) {
        status = P_ILLEGAL_SIZE_INTERLACED_MODE;
    }
    if ((image_size == P_HDi) &&
        (progressive == 1)) {
        status = P_ILLEGAL_SIZE_PROGRESSIVE_MODE;
    }
    if ((color == P_STREAM) &&
        (progressive == 0)) {
        status = P_ILLEGAL_FORMAT_INTERL_MODE;
    }

    if (status == P_OK) {
        if (color == P_STREAM) {
            status = p_set_stream_header_values (image_freq,
                                                 image_size, pixels_per_line,
                                                 ratio,
                                                 &ima_freq, &lin_freq,
                                                 &pix_freq,
                                                 &act_lines, &act_pixel,
                                                 &interlace_factor,
                                                 &h_ratio, &v_ratio);
        } else {
            status = p_set_header_values (image_freq,
                                          image_size, pixels_per_line,
                                          progressive, ratio,
                                          &ima_freq, &lin_freq,
                                          &pix_freq,
                                          &act_lines, &act_pixel,
                                          &interlace_factor,
                                          &h_ratio, &v_ratio);
        } /* end of if (color == P_STREAM) */
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        status = p_create_free_header (header, color,
                                       ima_freq, lin_freq,
                                       pix_freq,
                                       act_lines, act_pixel,
                                       interlace_factor,
                                       h_ratio, v_ratio);
    } /* end of if (status == P_OK) */

    /* the new header does not yet exist on disk;
       so the modified bit is set to one */
    header->modified = 1;

    return status;
} /* end of p_create_ext_header () */

/******************************************************************************/
