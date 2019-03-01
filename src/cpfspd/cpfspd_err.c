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
 *  CVS id      :  $Id: cpfspd_err.c,v 2.24 2010/01/05 14:18:31 riemens Exp $
 *
 *  Name        :  cpfspd_err.c
 *
 *  Author      :  Robert Jan Schutten
 *
 *  Function    :  ERRor handling of cpfspd.
 *                 ---
 *
 *  Description :  Error handling (check error messages & printing)
 *                 routines of cpfspd the library are in this file.
 *
 *  Functions   :  The following external cpfspd functions are
 *                 defined in this file:
 *
 *                 - p_fatal_error()
 *                 - p_get_error_string()
 *
 */

/******************************************************************************/

#include <stdlib.h>
#include "cpfspd.h"

/******************************************************************************/

/* error codes return strings */
#define P_OK_STR                            \
        "Success"
#define P_FILE_OPEN_FAILED_STR              \
        "File open failed (reading from file)"
#define P_FILE_CREATE_FAILED_STR            \
        "File create failed (writing to file)"
#define P_FILE_MODIFY_FAILED_STR            \
        "File modify failed (writing to file)"
#define P_FILE_IS_NOT_PFSPD_FILE_STR        \
        "File is not a pfspd file (reading from file)"
#define P_WRITE_FAILED_STR                  \
        "Write failed"
#define P_READ_FAILED_STR                   \
        "Read failed"
#define P_SEEK_FAILED_STR                   \
        "Seek failed"
#define P_NEGATIVE_SEEK_ON_STDIO_STR        \
        "Negative seek on stdio cannot be performed"
#define P_WRITE_BEYOND_EOF_STDOUT_STR       \
        "Write beyond number of specified images on stdout"
#define P_REWRITE_ON_STDOUT_STR             \
        "No rewrite on stdout possible"
#define P_REWRITE_MODIFIED_HEADER_STR       \
        "Rewrite header that is inconsistent with data in file"
#define P_TOO_MANY_IMAGES_STR               \
        "Too many images"
#define P_TOO_MANY_COMPONENTS_STR           \
        "Too many components"
#define P_INVALID_COMPONENT_STR             \
        "Invalid component"
#define P_INVALID_AUXILIARY_STR             \
        "Invalid auxiliary ID"
#define P_NO_IH_RECORDS_ALLOWED_STR         \
        "No image header description records allowed"
#define P_ILLEGAL_BYTES_PER_REC_STR         \
        "Illegal number of bytes per record"
#define P_ILLEGAL_TEM_SBSMPL_STR            \
        "Illegal temporal subsampling"
#define P_ILLEGAL_LIN_SBSMPL_STR            \
        "Illegal line subsampling"
#define P_ILLEGAL_PIX_SBSMPL_STR            \
        "Illegal pixel subsampling"
#define P_SHOULD_BE_INTERLACED_STR          \
        "Format should be interlaced"
#define P_READ_CHR_FROM_LUM_ONLY_STR        \
        "Read chrominance (U,V) from luminance only file"
#define P_READ_RGB_FROM_LUM_ONLY_STR        \
        "Read R,G,B from luminance only file"
#define P_READ_PLANAR_CHR_FROM_MULT_CHR_STR \
        "Read planar chrominance (U,V) from multiplexed chrominance (U,V) file"
#define P_READ_RGB_FROM_YUV_STR             \
        "Read R,G,B from Y,U,V file"
#define P_READ_CHR_FROM_RGB_STR             \
        "Read chrominance (U,V) from R,G,B file"
#define P_READ_CHR_FROM_STREAM_STR          \
        "Read chrominance (U,V) from stream (S) file"
#define P_READ_RGB_FROM_STREAM_STR          \
        "Read R,G,B from stream (S) file"
#define P_READ_INVALID_COMPONENT_STR        \
        "Read invalid component number"
#define P_WRITE_INVALID_COMPONENT_STR       \
        "Write invalid component number"
#define P_WRONG_LUM_COMP_SIZE_STR           \
        "Wrong luminance component size"
#define P_WRONG_CHR_COMP_SIZE_STR           \
        "Wrong chrominance component size"
#define P_WRONG_RGB_COMP_SIZE_STR           \
        "Wrong RGB component size"
#define P_WRONG_XYZ_COMP_SIZE_STR           \
        "Wrong XYZ component size"
#define P_WRONG_STREAM_COMP_SIZE_STR        \
        "Wrong streaming (S) component size"
#define P_EXCEEDING_DESCRIPTION_SIZE_STR    \
        "File description exceeds maximum size"
#define P_WRONG_EXTRA_COMP_SIZE_STR         \
        "Wrong extra component size"
#define P_WRONG_SUBSAMPLE_FACTOR_STR        \
        "Image size is not a multiple of subsample factor"
#define P_EXCEEDING_AUXILIARY_DATA_SIZE_STR \
        "Auxiliary data exceeds maximum size"
#define P_EXCEEDING_AUXILIARY_HDR_SIZE_STR \
        "Auxiliary header exceeds maximum size"
#define P_HEADER_IS_MODIFIED_STR            \
        "Header in memory is modified or not yet written to disk"
#define P_INCOMP_MULT_COLOR_FORMAT_STR      \
        "Incompatible color format on read/write_frame/field"
#define P_INCOMP_PLANAR_COLOR_FORMAT_STR    \
        "Incompatible color format on read/write_frame/field_planar"
#define P_ILLEGAL_COLOR_FORMAT_STR          \
        "Illegal file or color format"
#define P_ILLEGAL_IMAGE_FREQUENCY_STR       \
        "Illegal image frequency"
#define P_ILLEGAL_IMAGE_FREQ_MOD_STR        \
        "Illegal image frequency modification"
#define P_ILLEGAL_ILP_FREQ_MOD_STR          \
        "Illegal image-, line-, or pixel frequency modification"
#define P_ILLEGAL_IMAGE_SIZE_STR            \
        "Illegal image size"
#define P_ILLEGAL_INTERLACE_STR             \
        "Illegal interlace value"
#define P_ILLEGAL_COMP_SIZE_STR             \
        "Illegal component size"
#define P_ILLEGAL_PHSHFT_STR                \
        "Illegal (temporal|line|pixel) phase shift"
#define P_ILLEGAL_ASPECT_RATIO_STR          \
        "Illegal aspect ratio"
#define P_ILLEGAL_SIZE_FREQUENCY_STR        \
        "Illegal combination of image size and image frequency"
#define P_ILLEGAL_SIZE_INTERLACED_MODE_STR  \
        "Illegal combination of image size and interlaced mode"
#define P_ILLEGAL_SIZE_PROGRESSIVE_MODE_STR \
        "Illegal combination of image size and progressive mode"
#define P_ILLEGAL_FORMAT_INTERL_MODE_STR    \
        "Illegal combination of format specifier and interlaced mode"
#define P_ILLEGAL_NUM_OF_PIX_PER_LINE_STR   \
        "Illegal number of pixels per line"
#define P_ILLEGAL_FILE_DATA_FORMAT_STR      \
        "Illegal file data format"
#define P_FILE_DATA_FORMATS_NOT_EQUAL_STR   \
        "Not all file data formats of individual components are equal"
#define P_ILLEGAL_MEM_DATA_FORMAT_STR       \
        "Illegal memory data format"
#define P_UNKNOWN_FILE_TYPE_STR             \
        "Unknown file buffer type"
#define P_UNKNOWN_MEM_TYPE_STR              \
        "Unknown memory buffer type"
#define P_INCOMP_FLOAT_CONVERSION_STR       \
        "This machine does not conform to IEEE 754 float format"
#define P_MALLOC_FAILED_STR                 \
        "Malloc failed"
#define P_DEFAULT_STR                       \
        "Unknown error code"

/******************************************************************************/

/*
 * Error handling
 */

/* check error message & print error + exit if fatal */

void
p_fatal_error (pT_status status,
               FILE *stream)
{
    if (status != P_OK) {
        fprintf (stream, "Error no: %d, description: %s\n",
                 (int)status,
                 p_get_error_string (status));
        exit (EXIT_FAILURE);
    } /* end of if (status != P_OK) */
    return;
} /* end of p_fatal_error () */

/* check error message & print error, filename + exit if fatal */

void
p_fatal_error_fileio (pT_status status,
                      const char *filename,
                      FILE *stream)
{
    if (status != P_OK) {
        fprintf (stream, "Error no: %d, description: %s, on file: %s\n",
                (int)status,
                 p_get_error_string (status),
                 filename);
        exit (EXIT_FAILURE);
    } /* end of if (status != P_OK) */
    return;
} /* end of p_fatal_error () */

/* return string associated with error code */

char *
p_get_error_string (pT_status status)
{
    switch (status) {
    case P_OK:
        return P_OK_STR;
    case P_FILE_OPEN_FAILED:
        return P_FILE_OPEN_FAILED_STR;
    case P_FILE_CREATE_FAILED:
        return P_FILE_CREATE_FAILED_STR;
    case P_FILE_MODIFY_FAILED:
        return P_FILE_MODIFY_FAILED_STR;
    case P_FILE_IS_NOT_PFSPD_FILE:
        return P_FILE_IS_NOT_PFSPD_FILE_STR;
    case P_WRITE_FAILED:
        return P_WRITE_FAILED_STR;
    case P_READ_FAILED:
        return P_READ_FAILED_STR;
    case P_SEEK_FAILED:
        return P_SEEK_FAILED_STR;
    case P_NEGATIVE_SEEK_ON_STDIO:
        return P_NEGATIVE_SEEK_ON_STDIO_STR;
    case P_WRITE_BEYOND_EOF_STDOUT:
        return P_WRITE_BEYOND_EOF_STDOUT_STR;
    case P_REWRITE_ON_STDOUT:
        return P_REWRITE_ON_STDOUT_STR;
    case P_REWRITE_MODIFIED_HEADER:
        return P_REWRITE_MODIFIED_HEADER_STR;
    case P_TOO_MANY_IMAGES:
        return P_TOO_MANY_IMAGES_STR;
    case P_TOO_MANY_COMPONENTS:
        return P_TOO_MANY_COMPONENTS_STR;
    case P_INVALID_COMPONENT:
        return P_INVALID_COMPONENT_STR;
    case P_INVALID_AUXILIARY:
        return P_INVALID_AUXILIARY_STR;
    case P_NO_IH_RECORDS_ALLOWED:
        return P_NO_IH_RECORDS_ALLOWED_STR;
    case P_ILLEGAL_BYTES_PER_REC:
        return P_ILLEGAL_BYTES_PER_REC_STR;
    case P_ILLEGAL_TEM_SBSMPL:
        return P_ILLEGAL_TEM_SBSMPL_STR;
    case P_ILLEGAL_LIN_SBSMPL:
        return P_ILLEGAL_LIN_SBSMPL_STR;
    case P_ILLEGAL_PIX_SBSMPL:
        return P_ILLEGAL_PIX_SBSMPL_STR;
    case P_SHOULD_BE_INTERLACED:
        return P_SHOULD_BE_INTERLACED_STR;
    case P_READ_CHR_FROM_LUM_ONLY:
        return P_READ_CHR_FROM_LUM_ONLY_STR;
    case P_READ_RGB_FROM_LUM_ONLY:
        return P_READ_RGB_FROM_LUM_ONLY_STR;
    case P_READ_PLANAR_CHR_FROM_MULT_CHR:
        return P_READ_PLANAR_CHR_FROM_MULT_CHR_STR;
    case P_READ_RGB_FROM_YUV:
        return P_READ_RGB_FROM_YUV_STR;
    case P_READ_CHR_FROM_RGB:
        return P_READ_CHR_FROM_RGB_STR;
    case P_READ_CHR_FROM_STREAM:
        return P_READ_CHR_FROM_STREAM_STR;
    case P_READ_RGB_FROM_STREAM:
        return P_READ_RGB_FROM_STREAM_STR;
    case P_READ_INVALID_COMPONENT:
        return P_READ_INVALID_COMPONENT_STR;
    case P_WRITE_INVALID_COMPONENT:
        return P_WRITE_INVALID_COMPONENT_STR;
    case P_WRONG_LUM_COMP_SIZE:
        return P_WRONG_LUM_COMP_SIZE_STR;
    case P_WRONG_CHR_COMP_SIZE:
        return P_WRONG_CHR_COMP_SIZE_STR;
    case P_WRONG_RGB_COMP_SIZE:
        return P_WRONG_RGB_COMP_SIZE_STR;
    case P_WRONG_XYZ_COMP_SIZE:
        return P_WRONG_XYZ_COMP_SIZE_STR;
    case P_WRONG_STREAM_COMP_SIZE:
        return P_WRONG_STREAM_COMP_SIZE_STR;
    case P_EXCEEDING_DESCRIPTION_SIZE:
        return P_EXCEEDING_DESCRIPTION_SIZE_STR;
    case P_WRONG_EXTRA_COMP_SIZE:
        return P_WRONG_EXTRA_COMP_SIZE_STR;
    case P_WRONG_SUBSAMPLE_FACTOR:
        return P_WRONG_SUBSAMPLE_FACTOR_STR;
    case P_EXCEEDING_AUXILIARY_DATA_SIZE:
        return P_EXCEEDING_AUXILIARY_DATA_SIZE_STR;
    case P_EXCEEDING_AUXILIARY_HDR_SIZE:
        return P_EXCEEDING_AUXILIARY_HDR_SIZE_STR;
    case P_HEADER_IS_MODIFIED:
        return P_HEADER_IS_MODIFIED_STR;
    case P_INCOMP_MULT_COLOR_FORMAT:
        return P_INCOMP_MULT_COLOR_FORMAT_STR;
    case P_INCOMP_PLANAR_COLOR_FORMAT:
        return P_INCOMP_PLANAR_COLOR_FORMAT_STR;
    case P_ILLEGAL_COLOR_FORMAT:
        return P_ILLEGAL_COLOR_FORMAT_STR;
    case P_ILLEGAL_IMAGE_FREQUENCY:
        return P_ILLEGAL_IMAGE_FREQUENCY_STR;
    case P_ILLEGAL_IMAGE_FREQ_MOD:
        return P_ILLEGAL_IMAGE_FREQ_MOD_STR;
    case P_ILLEGAL_ILP_FREQ_MOD:
        return P_ILLEGAL_ILP_FREQ_MOD_STR;
    case P_ILLEGAL_IMAGE_SIZE:
        return P_ILLEGAL_IMAGE_SIZE_STR;
    case P_ILLEGAL_INTERLACE:
        return P_ILLEGAL_INTERLACE_STR;
    case P_ILLEGAL_COMP_SIZE:
        return P_ILLEGAL_COMP_SIZE_STR;
    case P_ILLEGAL_PHSHFT:
        return P_ILLEGAL_PHSHFT_STR;
    case P_ILLEGAL_ASPECT_RATIO:
        return P_ILLEGAL_ASPECT_RATIO_STR;
    case P_ILLEGAL_SIZE_FREQUENCY:
        return P_ILLEGAL_SIZE_FREQUENCY_STR;
    case P_ILLEGAL_SIZE_INTERLACED_MODE:
        return P_ILLEGAL_SIZE_INTERLACED_MODE_STR;
    case P_ILLEGAL_SIZE_PROGRESSIVE_MODE:
        return P_ILLEGAL_SIZE_PROGRESSIVE_MODE_STR;
    case P_ILLEGAL_FORMAT_INTERL_MODE:
        return P_ILLEGAL_FORMAT_INTERL_MODE_STR;
    case P_ILLEGAL_NUM_OF_PIX_PER_LINE:
        return P_ILLEGAL_NUM_OF_PIX_PER_LINE_STR;
    case P_ILLEGAL_FILE_DATA_FORMAT:
        return P_ILLEGAL_FILE_DATA_FORMAT_STR;
    case P_FILE_DATA_FORMATS_NOT_EQUAL:
        return P_FILE_DATA_FORMATS_NOT_EQUAL_STR;
    case P_ILLEGAL_MEM_DATA_FORMAT:
        return P_ILLEGAL_MEM_DATA_FORMAT_STR;
    case P_UNKNOWN_FILE_TYPE:
        return P_UNKNOWN_FILE_TYPE_STR;
    case P_UNKNOWN_MEM_TYPE:
        return P_UNKNOWN_MEM_TYPE_STR;
    case P_INCOMP_FLOAT_CONVERSION:
        return P_INCOMP_FLOAT_CONVERSION_STR;
    case P_MALLOC_FAILED:
        return P_MALLOC_FAILED_STR;
    default:
        return P_DEFAULT_STR;
    } /* end of switch (status) */
} /* end of p_get_error_string () */

/******************************************************************************/
