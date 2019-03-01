/*
 *  CVS id      :  $Id: cpfspd.h,v 2.65 2015/10/01 10:23:25 vleutenr Exp $ 
 *
 *  Name        :  cpfspd.h
 *
 *  Authors     :  Robert Jan Schutten
 *                 Bram Riemens
 *
 *  Function    :  Header file for cpfspd library
 */

/**
\mainpage
\verbatim

    Copyright (c) KONINKLIJKE PHILIPS ELECTRONICS N.V. 1997-2012
    Copyright (c) NXP Semiconductors Netherlands B.V. 2007-2010
    All rights reserved.
    For licensing and warranty information, see the file COPYING.LGPLv21.txt
    in the root directory of this package.

    Philips Research Laboratories
    Eindhoven, The Netherlands

    NXP Research
    Eindhoven, The Netherlands

\endverbatim 


\section intro Introduction
PFSPD = Philips File Standard for Pictorial Data

The routines in this file provide an interface to 
pfspd video files. 

The routines in this file provide the following 
functionality:
- Reads/writes:
    - progressive video files
    - interlaced video files
    - access per field and/or frame
- Reads/writes:
    - luminance only 
    - 4:4:4 YUV (planar UV)
    - 4:2:2 YUV (multiplexed or planar UV)
    - 4:2:0 YUV (multiplexed or planar UV)
    - 4:4:4 RGB
    - streaming files ("S" files, e.g. cvbs encoded files)
- File formats supported:
    - 8, 10, 12, 14 & 16 bit per pixel files
      (pfspd types: B*8, B*10, B*12, B*14 & I*2).
- Memory formats supported:
    - 8 bits (unsigned char, with 8 bits data)
    - 16 bits (unsigned short, with 8, 10, 12, 14 
      or 16 bits data)
- Extra components with application defined name,
  independent file format and subsample factors.
- Easy header definition and modification for
  Standard Definition and several High Definition 
  formats (including all formats defined by ATSC)


\section coding Coding rules used in this library

General prefix for cpfspd library is p.
- all functions start with p_  followed by lower case name.
- all typedefs  start with pT_ followed by lower case name.
- all defines   start with P_  followed by upper case name.

*/

/**
\page images Definition of images

Naming:
- frame: Complete (progressive image), in interlaced format 
it consists of two fields.
- field: Half of a frame (in interlaced format), it is __not_defined__
in the progressive format.
- image: Either a field or a frame.

Size & storage format of image buffers:
\verbatim
           stride
        <----------------->
       ^+------------+----+
       ||            |    |
height || Image data |    |
       ||            |    |
       v+------------+----+
        <------------>
           width
\endverbatim

\section stride Comments on the use of stride

In many cases stride can be equal to width. In some cases the 
starting address of a line needs to have an aligned memory address
(this would be something that the application demands). 
In that case stride is larger than width to provide the proper 
memory alignment.
If you do not know what to do with the stride (i.e. no special alignment
constraints are known for the application), you can make the stride equal 
to the width.
The stride can also be used to access a field in a frame buffer. In that 
case the stride for accessing the field in the frame buffer needs to be 
two times the stride of the frame buffer.

\section adressing Addressing of image buffers

All image buffers used are one-dimensional arrays (i.e. they are of type
        unsigned char* or of type unsigned short*).
To address a single (2-D) pixel on coordinates (x,y) in such a buffer use:
\code   pixel = buffer[y * stride + x];     \endcode


*/

#ifndef CPFSPD_H_INCLUDED
#define CPFSPD_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* mandatory include of stdio.h; because of use of (FILE *) */
#include <stdio.h>


/** \defgroup ret_code Return type of pfspd functions
 * \ingroup error
 @{ */
typedef enum {
    P_OK                            = 0,
    P_FILE_OPEN_FAILED              = 100,
    P_FILE_CREATE_FAILED            = 101,
    P_FILE_MODIFY_FAILED            = 102,
    P_FILE_IS_NOT_PFSPD_FILE        = 103,
    P_WRITE_FAILED                  = 110,
    P_READ_FAILED                   = 111,
    P_SEEK_FAILED                   = 112,
    P_NEGATIVE_SEEK_ON_STDIO        = 113,
    P_WRITE_BEYOND_EOF_STDOUT       = 115,
    P_REWRITE_ON_STDOUT             = 116,
    P_REWRITE_MODIFIED_HEADER       = 117,
    P_TOO_MANY_IMAGES               = 199,
    P_TOO_MANY_COMPONENTS           = 200,
    P_INVALID_COMPONENT             = 201,
    P_NO_IH_RECORDS_ALLOWED         = 202,
    P_ILLEGAL_BYTES_PER_REC         = 203,
    P_ILLEGAL_TEM_SBSMPL            = 204,
    P_INVALID_AUXILIARY             = 205,
    P_ILLEGAL_LIN_SBSMPL            = 206,
    P_ILLEGAL_PIX_SBSMPL            = 207,
    P_SHOULD_BE_INTERLACED          = 210,
    P_READ_CHR_FROM_LUM_ONLY        = 211,
    P_READ_RGB_FROM_LUM_ONLY        = 212,
    P_READ_PLANAR_CHR_FROM_MULT_CHR = 213,
    P_READ_RGB_FROM_YUV             = 214,
    P_READ_CHR_FROM_RGB             = 215,
    P_READ_CHR_FROM_STREAM          = 216,
    P_READ_RGB_FROM_STREAM          = 217,
    P_READ_INVALID_COMPONENT        = 218,
    P_WRITE_INVALID_COMPONENT       = 219,
    P_WRONG_LUM_COMP_SIZE           = 220,
    P_WRONG_CHR_COMP_SIZE           = 221,
    P_WRONG_RGB_COMP_SIZE           = 222,
    P_WRONG_STREAM_COMP_SIZE        = 223,
    P_WRONG_XYZ_COMP_SIZE           = 224,
    P_EXCEEDING_DESCRIPTION_SIZE    = 225,
    P_WRONG_EXTRA_COMP_SIZE         = 226,
    P_WRONG_SUBSAMPLE_FACTOR        = 227,
    P_EXCEEDING_AUXILIARY_DATA_SIZE = 228,
    P_EXCEEDING_AUXILIARY_HDR_SIZE  = 229,
    P_HEADER_IS_MODIFIED            = 230,
    P_INCOMP_MULT_COLOR_FORMAT      = 242,
    P_INCOMP_PLANAR_COLOR_FORMAT    = 243,
    P_ILLEGAL_COLOR_FORMAT          = 300,
    P_ILLEGAL_IMAGE_FREQUENCY       = 400,
    P_ILLEGAL_IMAGE_FREQ_MOD        = 410,
    P_ILLEGAL_ILP_FREQ_MOD          = 420,
    P_ILLEGAL_IMAGE_SIZE            = 500,
    P_ILLEGAL_INTERLACE             = 501,
    P_ILLEGAL_COMP_SIZE             = 502,
    P_ILLEGAL_PHSHFT                = 503,
    P_ILLEGAL_ASPECT_RATIO          = 600,
    P_ILLEGAL_SIZE_FREQUENCY        = 700,
    P_ILLEGAL_SIZE_INTERLACED_MODE  = 701,
    P_ILLEGAL_SIZE_PROGRESSIVE_MODE = 702,
    P_ILLEGAL_FORMAT_INTERL_MODE    = 703,
    P_ILLEGAL_NUM_OF_PIX_PER_LINE   = 710,
    P_ILLEGAL_FILE_DATA_FORMAT      = 800,
    P_FILE_DATA_FORMATS_NOT_EQUAL   = 810,
    P_ILLEGAL_MEM_DATA_FORMAT       = 820,
    P_UNKNOWN_FILE_TYPE             = 830,
    P_UNKNOWN_MEM_TYPE              = 840,
    P_INCOMP_FLOAT_CONVERSION       = 850,
    P_MALLOC_FAILED                 = 900
} pT_status;
/** @} */

/** \defgroup header PFSPD header
For each pfspd file, accessed by the program, a separate pT_header
structure shall be allocated. This is required since the header structure
contains apart from the info in the file header, also some administrative
data, used by cpfspd. The file and header structure shall always
be kept together.
*/

/** \defgroup header_def Pfspd header definition
 * \ingroup header
 * @{ */

/** Maximum length of filenames, including trailing zero
    Note that some operating systems may not support this full length */
#define P_FILENAME_MAX 1024

/** \defgroup field_len Sizes of character fields in the header (not directly used by the user)
 * \ingroup header_def
 @{ */

/* Total number of global header records: global header structure and attributes */
#define P_NUM_GLOB_RECS   2

/* Global header structure */
#define P_SNR_IMAGES        7      /* number of images                          */
#define P_SNR_COMPON        5      /* number of components                      */
#define P_SNR_FD_RECS       5      /* number of file description records        */
#define P_SNR_AUXDAT_RECS   5      /* number of auxiliary data records          */
#define P_SAPPL_TYPE       25      /* application type                          */
#define P_SBYTES_REC        7      /* bytes per record                          */
#define P_SENDIAN_CODE      1      /* A: big endian U: little endian            */
#define P_SAUX_HDR_RECS     5      /* number of aux header records              */
#define P_LEN_GLOB_STR     60      /* length of global header structure record  */
                                   /* = sum of all above record lengths         */

/* Global header attributes */
#define P_SIMA_FREQ        12      /* image frequency                           */
#define P_SLIN_FREQ        12      /* line frequency                            */
#define P_SPIX_FREQ        12      /* pixel frequency                           */
#define P_SACT_LINES        6      /* active lines                              */
#define P_SACT_PIXEL        6      /* active pixels                             */
#define P_SINTERLACE        2      /* interlace factor                          */
#define P_SH_PP_SIZE        5      /* horizontal proportional  picture size     */
#define P_SV_PP_SIZE        5      /* vertical proportional picture size        */
#define P_LEN_GLOB_ATT     60      /* length of global header attributes record */
                                   /* = sum of all above record lengths         */

/* Total number of component records: component structure and attributes */
#define P_NUM_COMP_RECS     2

/* Component structure */
#define P_SLIN_IMAGE        6      /* lines per image                           */
#define P_SPIX_LINE         6      /* pixels per line                           */
#define P_SDATA_FMT         4      /* data format                               */
#define P_LEN_COMP_STR     16      /* length of component structure record      */
                                   /* = sum of all above record lengths         */

/* Component attributes */
#define P_STEM_SBSMPL       2      /* temporal subsampling                      */
#define P_SLIN_SBSMPL       2      /* line subsampling                          */
#define P_SPIX_SBSMPL       2      /* pixel subsampling                         */
#define P_STEM_PHSHFT       2      /* temporal phase shift                      */
#define P_SLIN_PHSHFT       2      /* line phase shift                          */
#define P_SPIX_PHSHFT       2      /* pixel phase shift                         */
#define P_SCOM_CODE         5      /* component code                            */
#define P_LEN_COMP_ATT     17      /* length of component attributes record     */
                                   /* = sum of all above record lengths         */

/* Description and auxilliary data */
#define P_SDESCRIPTION   2048      /* description                               */
#define P_BYTES_REC       512      /* alignment size for nr of bytes/rec        */
#define P_SAUX_NAME        16      /* name of auxiliary data                    */
#define P_SAUX_HDR      16384      /* size of auxiliary header                  */
/** @} */

/** Maximum number of components in pfspd file (not directly used by the user) */
#define P_PFSPD_MAX_COMP 128

/** \defgroup com_codes Standard defines for com_code (not directly used by the user)
 * \ingroup header_def
 @{ */
#define P_Y_COM_CODE            "Y    "
#define P_UV_COM_CODE           "U/V  "
#define P_U_COM_CODE            "U    "
#define P_V_COM_CODE            "V    "
#define P_R_COM_CODE            "R    "
#define P_G_COM_CODE            "G    "
#define P_B_COM_CODE            "B    "
#define P_S_COM_CODE            "S    "
#define P_P_COM_CODE            "P    "
#define P_XYZX_COM_CODE         "X    "
#define P_XYZY_COM_CODE         "Y    "
#define P_XYZZ_COM_CODE         "Z    "
#define P_VOID_COM_CODE         "void "
/** @} */

/** Standard defines for appl_type (not directly used by the user) */
#define P_VIDEO_APPL_TYPE       "VIDEO                    "

/** Sentinel for linked list of aux headers (not directly used by the user) */
#define P_AUX_LAST              "       8"

/** \defgroup data_fmt Standard defines for data_fmt (not directly used by the user) 
 * \ingroup header_def
 @{ */
#define P_B8_DATA_FMT           "B*8 "
#define P_B10_DATA_FMT          "B*10"
#define P_B12_DATA_FMT          "B*12"
#define P_B14_DATA_FMT          "B*14"
#define P_I2_DATA_FMT           "I*2 "
#define P_R2_DATA_FMT           "R*2 "
/** @} */


/*
 * The following structures contain all information that is 
 * present in the pfspd headers.
 *
 * N.B. The names of all fields are identical to the names
 *      that are used in the standard pfspd access library.
 */

/** Information of one image component */
typedef struct pT_component_struct {
    int             lin_image;                  /**< lines per image */
    int             pix_line;                   /**< pixels per line */
    char            data_fmt[P_SDATA_FMT + 1];  /**< data format code (e.g. B*8, I*2) */
    int             tem_sbsmpl;                 /**< temporal subsampling (must be 1) */
    int             lin_sbsmpl;                 /**< line subsampling */
    int             pix_sbsmpl;                 /**< pixel subsampling */
    int             tem_phshft;                 /**< temporal phase shift */
    int             lin_phshft;                 /**< line phase shift */
    int             pix_phshft;                 /**< pixel phase shift */
    char            com_code[P_SCOM_CODE + 1];  /**< component code (e.g. Y, U/V, R) */
} pT_component;

/** pfspd header */
typedef struct pT_header_struct {
    /*
     * These fields are actually written on disk.
     */
    int            nr_images;
    int            nr_compon;
    int            nr_fd_recs;
    int            nr_aux_data_recs;
    char           appl_type[P_SAPPL_TYPE + 1];
    int            bytes_rec;
    int            little_endian;    /* boolean: file endian mode */
    int            nr_aux_hdr_recs;
    double         ima_freq;
    double         lin_freq;
    double         pix_freq;
    int            act_lines;
    int            act_pixel;
    int            interlace;
    int            h_pp_size;
    int            v_pp_size;
    pT_component   comp[P_PFSPD_MAX_COMP];

    /*
     * The following field is an extension to the original header structure.
     * This is also written on disk.
     */
    char           description[P_SDESCRIPTION];

    /*
     * The following field modifies the behaviour of cpfspd functions.
     * The application may use this to control cpfspd.
     *
     * Note that the value is initialized at 0 in all functions that
     * initialize a header structure:
     *    p_create_header(), p_create_ext_header(), p_read_header().
     */
    int            disable_hdr_checks; /* boolean: 0: normal behaviour        */
                                       /* Value 1: function p_check_header()  */
                                       /* returns always P_OK. This function  */
                                       /* is also used internally by others.  */
                                       /* With checking disabled, and using   */
                                       /* the low level data access functions */
                                       /* on single components, any pfspd     */
                                       /* file can be accessed. This flag     */
                                       /* must be set by the application (no  */
                                       /* API support) and all risks of       */
                                       /* incorrect use are with the          */
                                       /* application programmer.             */

    /*
     * The following fields are internal administration of cpfspd.
     * They shall not be modified by the application.
     */
    int            modified;   /* this bit tells if the header has been    */
                               /* modified, when compared to a header that */
                               /* exists on disk                           */
    unsigned long  offset_lo;  /* position of current file pointer;        */
                               /* used for stdin/stdout read/write         */
                               /* the offset is maintained by cpfspd_low.c */
    unsigned long  offset_hi;  /* the offset_hi and offset_lo parts are    */
                               /* used to construct a 64 bit value on      */
                               /* platforms that support large files       */
                               /* this way, the cpfspd external interface  */
                               /* still complies to ANSI-C                 */
                               /* see module cpfspd_fio.c for more info    */
    unsigned char  aux_hdrs[P_SAUX_HDR];  /* (when used) auxiliary headers */
} pT_header;

/** \defgroup header_init Initialize the header with a known state.
 * \ingroup header_def
 * @{
 *
 * This avoids some warnings on some compilers on "variable used 
 * before being set" (since the compiler does not know that the header
 * is assigned inside the functions p_read_header or p_create_header).
 * 
 * N.B. qac complains about the brackets in the following two defines. 
 */
#define P_COMPONENT_INIT   {0, 0, "", 0, 0, 0, 0, 0, 0, ""}
#define P_HEADER_INIT      {0, 0, 0, 0, "", 0, 0, 0, \
                            0.0, 0.0, 0.0, 0, 0, 0, 0, 0, \
                            {P_COMPONENT_INIT, P_COMPONENT_INIT, /*   2 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*   4 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*   6 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*   8 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  10 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  12 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  14 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  16 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  18 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  20 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  22 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  24 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  26 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  28 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  30 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  32 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  34 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  36 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  38 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  40 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  42 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  44 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  46 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  48 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  50 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  52 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  54 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  56 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  58 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  60 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  62 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  64 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  66 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  68 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  70 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  72 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  74 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  76 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  78 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  80 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  82 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  84 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  86 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  88 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  90 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  92 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  94 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  96 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /*  98 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /* 100 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /* 102 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /* 104 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /* 106 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /* 108 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /* 110 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /* 112 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /* 114 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /* 116 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /* 118 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /* 120 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /* 122 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /* 124 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT, /* 126 */ \
                             P_COMPONENT_INIT, P_COMPONENT_INIT},/* 128 */ \
                            "", 0, 0, 0, 0, P_AUX_LAST}
/** @} */

/** @} header_def */

/********************
* Header data types *
********************/


/**
 * The following types are abstract representations of the information
 * that is present in the header. 
 * This should make it easier for the user of this library to interpret
 * the information that is contained in the header of a pfspd file.
 */

/** Standard image frequencies (not directly used by the user) */
#define P_STD_IMA_FREQ_50HZ      (50.00)
#define P_STD_IMA_FREQ_60HZ      (59.94)
#define P_STD_IMA_FREQ_REAL_60HZ (60.00)

/** Image frequency types */
typedef enum {
    P_50HZ = 0,             /* 50 Hz                                          */
    P_60HZ,                 /* 60 Hz = 59.94 Hz !!                            */
    P_REAL_60HZ,            /* Real 60 Hz frequency;                          */
                            /*   (i.s.o. 59.94 Hz defined in ITU-R BT.470-6)  */
    P_75HZ,                 /* 1.5 times P_50HZ                               */
    P_90HZ,                 /* 1.5 times P_60HZ                               */
    P_REAL_90HZ,            /* 1.5 times P_REAL_60HZ                          */
    P_100HZ,                /* 2 times P_50HZ                                 */
    P_120HZ,                /* 2 times P_60HZ                                 */
    P_REAL_120HZ,           /* 2 times P_REAL_60HZ                            */
    P_25HZ,                 /* >   'Special' frequency types for movie        */
    P_24HZ,                 /*  >  material (note that these formats cannot   */
    P_REAL_24HZ,            /*   > be displayed directly by a regular         */
    P_30HZ,                 /*  >  display. The accompanying line/pixel       */
    P_REAL_30HZ,            /* >   frequencies will be set to zero!)          */
    P_UNKNOWN_FREQ          /* unknown frequency                              */
} pT_freq;

/** Color types (or component format types) */
typedef enum {
    P_NO_COLOR = 0,         /* no color; only luminance                       */
    P_COLOR_422,            /* multiplexed yuv formats: YUV 4:2:2             */
    P_COLOR_420,            /*                          YUV 4:2:0             */
    P_COLOR_444_PL,         /* planar yuv formats:      YUV 4:4:4             */
    P_COLOR_422_PL,         /*                          YUV 4:2:2             */
    P_COLOR_420_PL,         /*                          YUV 4:2:0             */
    P_COLOR_RGB,            /* RGB format                                     */
    P_STREAM,               /* streaming ("S") format; used for CVBS streams; */
                            /* only frames are supported in the "S" format    */
    P_COLOR_XYZ,            /* XYZ format                                     */
    P_UNKNOWN_COLOR         /* unknown color format                           */
} pT_color;

/** Aspect ratio types */
typedef enum {
    P_4_3 = 0,              /* 4:3 aspect ratio                               */
    P_16_9,                 /* 16:9 aspect ratio                              */
    P_AS_WH,                /* as image width/height (square pixel)           */
    P_UNKNOWN_ASPECT_RATIO  /* unknown aspect ratio                           */
} pT_asp_rat;

/** Data format types of pfspd file */
typedef enum {
    P_8_BIT_FILE = 0,       /* 8 bits file format                             */
    P_10_BIT_FILE,          /* 10 bits file format                            */
    P_12_BIT_FILE,          /* 12 bits file format                            */
    P_14_BIT_FILE,          /* 14 bits file format                            */
    P_16_BIT_FILE,          /* 16 bits file format                            */
    P_16_REAL_FILE,         /* 16 bits real file format                       */
    P_UNKNOWN_DATA_FORMAT   /* unknown data format                            */
} pT_data_fmt;

/** Image size types */
typedef enum {
    P_SD = 0,               /* Standard definition                            */
    P_CIF,                  /* CIF & QCIF size (note that for CIF & QCIF      */
    P_QCIF,                 /* sized images line/pixel frequencies will be    */
                            /* set such that they can be displayed as SD      */
                            /* images, i.e. 13.5 MHz sample frequency &       */
                            /* approx. 16 or 32 kHz line frequency            */
    P_HDp,                  /* Progressive HD format                          */
    P_HDi,                  /* Interlaced HD format                           */
    P_UNKNOWN_SIZE          /* Unknown size                                   */
} pT_size;

/** Application buffer types for convenience routines */
typedef enum {
  P_FLOAT = 0,              /* float *abuf                                    */
  P_DOUBLE,                 /* double *abuf                                   */
  P_LONG,                   /* signed long *abuf                              */
  P_ULONG,                  /* unsigned long *abuf                            */
  P_INT,                    /* signed int *abuf                               */
  P_UINT,                   /* unsigned int *abuf                             */
  P_SHORT,                  /* signed short *abuf                             */
  P_USHORT,                 /* unsigned short *abuf                           */
  P_CHAR,                   /* signed char *abuf                              */
  P_UCHAR                   /* unsigned char *abuf                            */
} pT_buf_type;


/**
\page standards Supported combinations of color format, image frequency & image size

\section color_fmt Standard color formats
Standard image sizes that are supported for standard color formats (color 
formats P_NO_COLOR, P_COLOR_422, P_COLOR_420, P_COLOR_444_PL, P_COLOR_422_PL,
P_COLOR_420_PL & P_COLOR_RGB):

- x: number of pixels per line supported
- D: default number of pixels per line

\subsection euro P_50HZ (or P_25HZ)
\verbatim
  size | lines | pixels                                                      
       |       | 176 180 352 360 512 640 704 720 848 960 1024 1280 1440 1920 
  ---------------------------------------------------------------------------
  QCIF | 144   |  D   x                                                      
  CIF  | 288   |          D   x                                              
  SD   | 576   |                  x   x   x   D   x   x   x    x    x        
  HDi  | 1152  |                                      x   x    x    D    x   

  Notes:
  1) ITU-R BT.601-5 standard gives 720x576 (@ 13.5 MHz) & 960x576 (@ 18 MHz)
     as standard image formats.
  2) HDi image size can only be intialized in interlaced mode.
\endverbatim

\subsection usa P_60HZ, P_REAL_60HZ (or P_24HZ, P_REAL_24HZ, P_30HZ, P_REAL_30HZ)
\verbatim
  size | lines | pixels                                                      
       |       | 176 180 352 360 512 640 704 720 848 960 1024 1280 1440 1920 
  ---------------------------------------------------------------------------
  QCIF | 120   |  D   x                                                      
  CIF  | 240   |          D   x                                              
  SD   | 480   |                  x   x   x   D   x   x   x    x    x        
  HDp  | 720   |                                      x   x    D    x    x   
  HDi  | 1080  |                                      x   x    x    x    D   

  Notes:
  1) ITU-R BT.601-5 standard gives 720x480 (@ 13.5 MHz) & 960x480 (@ 18 MHz)
     as standard image formats.
  2) HDi image size can only be intialized in interlaced mode.
  3) HDp image size can only be intialized in progressive mode.
  4) The ATSC standard (document A/53) gives 640x480 (4:3, i or p), 704x480 
     (4:3 or 16:9, i or p), 1280x720 (16:9, p) & 1920x1080 (16:9, i) as 
     possible formats. Note that these are the display formats; intermediate
     film formats at 24 or 30 Hz (all p) are also defined. However, these 
     formats are never directly displayed at that frequency.
\endverbatim

\section encoded P_STREAM
Image sizes for "S" format (color format P_STREAM), this is intended for
CVBS encoded PAL, NTSC or SECAM streams:

\subsection euro2 P_25HZ
\verbatim
  size | lines | pixels        
       |       | 864 1024 1152 
  -----------------------------
  SD   | 625   |  D   x    x   

  Notes:
  1) Sample frequencies are 13.5, 16 & 18 MHz respectively; only supported in
     progressive mode.
\endverbatim

\subsection usa2 P_30HZ
\verbatim
  size | lines | pixels        
       |       | 858      1144 
  -----------------------------
  SD   | 525   |  D        x   

  Notes:
  1) Sample frequencies are 13.5 & 18 MHz respectively; only supported in 
     progressive mode.
\endverbatim

\subsection asprat Aspect ratio
\verbatim
  The pfspd file format supports a display aspect ratio setting.
  This setting defines the intended shape of the physical display
  used by the image.
  Function p_create_ext_header() accepts the aspect ratio as input
  parameter. The default aspect ratio setting is 4:3 when using
  function p_create_header().
  When using function p_create_ext_header() with ratio argument
  P_UNKNOWN_ASPECT_RATIO, then an automatic aspect ratio is choosen,
  according to:
  - For image size HDi and HDp: the aspect ratio is always 16:9.
  - For other image sizes (SD, CIF, QCIF):
    If the number of pixels per line is larger than the default value,
    then the aspect ratio is 16:9; otherwise, the aspect ratio is 4:3.

  Notes:
  1) Aspect ratio setting P_AS_WH indicates that the aspect ratio
  of the display equals the aspect ratio of the image width/height.
  This is equal to a "square pixel" setting. However, after scaling or
  cropping (i.e. changing the width and/or height of the image) then
  the application is responsible to maintain a square pixel setting,
  since the pfspd file stores the display aspect ratio (without
  any knowledge on the pixel aspect ratio).
  This can be achieved by calling p_mod_aspect_ratio().
\endverbatim

\section terminology Terminology
\subsection cifvssif Notes on the use of CIF or SIF:

This seems to cause a lot of confusion. 
- CIF and QCIF are image formats standardized by the ITU.
- SIF seems to be only mentioned as the image format for MPEG1 video,
    however it is identical to the CIF format.
- We only use CIF (and QCIF).
 */


/** \defgroup hdr_func Functions operating on pfspd header 
 * \ingroup header
 * @{ */


/** Check validity of header (color format, data format & relative component sizes).
 *  \return P_OK on valid header.
 *  \return Error code on invalid header.
 *  \remarks This function is also implicitly called by the following functions:
 *     - p_read_header()
 *     - p_write_header()
 *     - p_rewrite_header()
 *     - p_copy_header()
 *     - p_read/write_field/frame[_*] ()
 */
extern pT_status p_check_header (const pT_header *header);

/** Read header from filename specified and store result in header. */
extern pT_status p_read_header (const char *filename, 
                                pT_header *header);

/** Create new pfspd file based on specification of header.
 *  \remarks Space for the number of images specified in header is allocated on disk.
 */
extern pT_status p_write_header (const char *filename, 
                                 pT_header *header);

/** 
 * \remarks Can only execute on existing pfspd file (not on stdio)
 * \remarks Typical use: replace the file description
 *   - Main header fields that may be modified:
 *     - description
 *     - appl_type
 *     - bytes_rec
 *     - ima_freq
 *     - lin_freq
 *     - pix_freq
 *     - h_pp_size
 *     - v_pp_size
 *   - Component header fields that may be modified:
 *     - tem_sbsmpl
 *     - lin_sbsmpl
 *     - pix_sbsmpl
 *     - tem_phshft
 *     - lin_phshft
 *     - pix_phshft
 *     - com_code
 */
extern pT_status p_rewrite_header (const char *filename, 
                                   pT_header *header);

/**
 * Copy header to new_header
 * \remarks This is required when one wants to use a header that is read from
 *          a file as a template for a new file that will be written to disk;
 *          internally there is bookkeeping in the header structure.
 */
extern pT_status p_copy_header (pT_header *new_header, 
                                const pT_header *header);

/** 
 * Print contents of header to stream (can be file, stdout or stderr)
 * in a human readable format.
 */
extern pT_status p_print_header (const pT_header *header, 
                                 FILE *stream);

/** 
 *   - Standard definition, interlaced, 4:3 aspect ratio
 *     (note that this is the 'normal case', otherwise use 
 *      p_create_ext_header).
 *   - Image frequency as specified in image_freq (P_50HZ, P_60HZ, 
 *     P_REAL_60HZ, P_24HZ, P_REAL_24HZ, P25HZ, P_30HZ & P_REAL_30HZ 
 *     are allowed).
 *   - Color format as specified in color (note that P_STREAM is only 
 *     supported for P_25HZ & P_30HZ).
 *   - Number of images is set to zero.
 *   - Data format = P_8_BIT_FILE.
 *
 *   See further \ref standards
 */
extern pT_status p_create_header (pT_header *header, 
                                  pT_color color, 
                                  pT_freq image_freq);

/** 
 * This is an extended form of p_create_header().
 *   - SD & HD sizes; image size as specified in image_size & pixels_per_line.
 *     If pixels_per_line is zero, a default value of pixels per line will 
 *     be set.
 *   - Aspect ratio is set as in aspect_ratio.
 *     When value P_UNKNOWN_ASPECT_RATIO is used, the choise between
 *     16:9 and 4:3 is automatically determined according to
 *     the rules described in \ref asprat .
 *   - If progressive==0 the file is interlaced, otherwise it is
 *     progressive.
 *   - Image frequency as specified in image_freq (P_50HZ, P_60HZ, 
 *     P_REAL_60HZ, P_24HZ, P_REAL_24HZ, P25HZ, P_30HZ & P_REAL_30HZ 
 *     are allowed).
 *   - Color format as specified in color (note that P_STREAM is only 
 *     supported for P_25HZ & P_30HZ).
 *   - Number of images is set to zero.
 *   - Data format = P_8_BIT_FILE.
 *
 *   See further \ref standards
 */
extern pT_status p_create_ext_header (pT_header *header, 
                                      pT_color color, 
                                      pT_freq image_freq,
                                      pT_size image_size,
                                      int pixels_per_line,
                                      int progressive,
                                      pT_asp_rat ratio);
/** @} */

/** \defgroup mod_hdr Modify header.
 * \ingroup hdr_func
 * @{
 * Note: These modifications are not possible after the header is written.
 *
 * Note: Only the most commonly used modifications are possible with these
 *       routines. It is always possible to directly modify the header
 *       in the cases that more that just the provided modifications are
 *       required.
 */

/** Change number of frames to num_frames. */
extern pT_status p_mod_num_frames (pT_header *header, 
                                   int num_frames);
/** Change color format to color_format. */
extern pT_status p_mod_color_format (pT_header *header, 
                                     pT_color color_format);
/** Change aspect ratio to ratio. */
extern pT_status p_mod_aspect_ratio (pT_header *header, 
                                     pT_asp_rat ratio);

/** p_mod_to_progressive   : Change header to progressive format. */
extern pT_status p_mod_to_progressive (pT_header *header);

/** Change header to interlaced format. */
extern pT_status p_mod_to_interlaced (pT_header *header);

/** Double the image rate (e.g. from 50 to 100 Hz).
 *  - Note that the number of frames in the file is doubled.
 *  - Operates only on P_50HZ, P_60HZ or P_REAL_60HZ image rates!
 */
extern pT_status p_mod_to_dbl_image_rate (pT_header *header);

/** Image rate times 1.5 (e.g. from 50 to 75 Hz).
 * - Note that the number of frames in the file is multiplied with 1.5.
 * - Operates only on P_50HZ, P_60HZ or P_REAL_60HZ image rates!
 */
extern pT_status p_mod_to_onehalf_image_rate (pT_header *header);

/** Change image size to width and height. */
extern pT_status p_mod_image_size (pT_header* header,
                                   int width,
                                   int height);

/** Change image size to defined image_size.
 * - Image size as specified in image_size & pixels_per_line.
 *   If pixels_per_line is zero, a default value of pixels per line 
 *   will be set.
 * - Only the following mage frequencies are supported: P_50HZ, P_60HZ, 
 *   P_REAL_60HZ, P_24HZ, P_REAL_24HZ, P25HZ, P_30HZ & P_REAL_30HZ.
 */
extern pT_status p_mod_defined_image_size (pT_header* header,
                                           pT_size image_size,
                                           int pixels_per_line);

/** Change image-, line-, and pixel frequencies.
 * This is useful for "unknown" frequencies or sizes that cannot be
 * handled by p_mod_defined_image_freq and p_mod_defined_image_size.
 */
extern pT_status p_mod_all_freqs (pT_header *header,
                                  double image_freq,
                                  double line_freq,
                                  double pixel_freq);

/** Change image size to defined image_freq.
 *     - Only the following mage frequencies are supported: P_50HZ, P_60HZ, 
 *       P_REAL_60HZ, P_24HZ, P_REAL_24HZ, P25HZ, P_30HZ & P_REAL_30HZ.
 *     - Only standar size images can be changed: P_SD, P_CIF, P_QCIF, 
 *       P_HDp & P_HDi. If p_get_image_size() returns P_UNKNOWN_SIZE a 
 *       modification is not possible.
 */
extern pT_status p_mod_defined_image_freq (pT_header* header,
                                           pT_freq image_freq);

/** Change the data format of the image. */
extern pT_status p_mod_file_data_format (pT_header *header, 
                                         pT_data_fmt file_data_fmt);

/** Change the file description. */
extern pT_status p_mod_file_description (pT_header *header, 
                                         char *description);
/** @} */

/** \defgroup extract_hdr Extract information from header.
 * \ingroup hdr_func
 * @{
 * No error codes are returned since the header should contain legal values 
 * i.e. created by p_read_header() or p_create_header().
 */
/** Return number of frames. */
extern int         p_get_num_frames (const pT_header *header);
/** Return 1 if interlaced, 0 if progressive. */
extern int         p_is_interlaced (const pT_header *header);
/** Return 0 if interlaced, 1 if progressive. */
extern int         p_is_progressive (const pT_header *header);
/** Return frame width. */
extern int         p_get_frame_width (const pT_header *header);
/** Return frame height. */
extern int         p_get_frame_height (const pT_header *header);


/** Return height and width of an Y buffer.
 *   - This is the minimum size of a buffer that can be used by 
 *     the read/write field/frame functions.
 *   - For interlaced files this is the size of a field.
 *   - For progressive files this is the size of a frame.
 */
extern void        p_get_y_buffer_size (const pT_header *header, 
                                        int *width, 
                                        int *height);
/** Return height and width of a U/V buffer
 *     For planar U/V this is the size of one U or V buffer; 
 *     for multiplexed U/V this is the size of the combined U/V buffer).
 *   - This is the minimum size of a buffer that can be used by 
 *     the read/write field/frame functions.
 *   - For interlaced files this is the size of a field.
 *   - For progressive files this is the size of a frame.
 */
extern void        p_get_uv_buffer_size (const pT_header *header, 
                                         int *width, 
                                         int *height);
/** Return height and width of an R, G or B buffer.
 *   - This is the minimum size of a buffer that can be used by 
 *     the read/write field/frame functions.
 *   - For interlaced files this is the size of a field.
 *   - For progressive files this is the size of a frame.
 */
extern void        p_get_rgb_buffer_size (const pT_header *header, 
                                          int *width, 
                                          int *height);
/** Return height and width of an S buffer.
 *   - This is the minimum size of a buffer that can be used by 
 *     the read/write frame functions.
 */
extern void        p_get_s_buffer_size (const pT_header *header, 
                                        int *width, 
                                        int *height);

/** Return color format. */
extern pT_color    p_get_color_format (const pT_header *header);

/** Return image, line, and pixel frequencies as doubles. */
extern void        p_get_all_freqs (const pT_header *header, 
                                    double *image_freq, 
                                    double *line_freq, 
                                    double *pixel_freq);

/** Return image frequency. */
extern pT_freq     p_get_image_freq (const pT_header *header);

/** Return image size. */
extern pT_size     p_get_image_size (const pT_header *header);

/** Return aspect ratio. */
extern pT_asp_rat  p_get_aspect_ratio (const pT_header *header);

/** Return data format. */
extern pT_data_fmt p_get_file_data_format (const pT_header *header);

/** Return the file description string
 * The returned string is valid as long as the header structure exists.
 */
extern char       *p_get_file_description (const pT_header *header);

/** @} */

/**
\defgroup readwrite Reading/writing images

\section read Read functions:

  What is read from file is controlled by the "read_mode":

  read_mode = component_mode | mem_data_fmt

\subsection component_mode  component_mode 
component_mode controls the components that are read

\verbatim
    color_format    | component_mode | read from file
    -------------------------------------------------
    P_NO_COLOR      | P_READ_Y       | Y             *)
                    | other          | error
    -------------------------------------------------
    P_COLOR_422,    | P_READ_ALL     | Y, U/V
    P_COLOR_420     | P_READ_Y       | Y             *)
                    | P_READ_UV      | U/V           *)
                    | other          | error
    -------------------------------------------------
    P_COLOR_444_PL, | P_READ_ALL     | Y, U, V
    P_COLOR_422_PL, | P_READ_Y       | Y             *)
    P_COLOR_420_PL  | P_READ_UV      | U, V          *)
                    | P_READ_U       | U             *)
                    | P_READ_V       | V             *)
                    | other          | error
    -------------------------------------------------
    P_COLOR_RGB     | P_READ_ALL     | R, G, B
                    | P_READ_R       | R             *)
                    | P_READ_G       | G             *)
                    | P_READ_B       | B             *)
                    | P_READ_Y       | R, G, B (for backwards compatibility)
                    | other          | error
    -------------------------------------------------
    P_STREAM        | P_READ_ALL     | S             *)
                    | P_READ_Y       | S       (for backwards compatibility)
                    | other          | error
    -------------------------------------------------

    *) Only these buffers are written to the memory; pointers to
       other buffers (e.g. U/V) can be NULL pointers.
\endverbatim
\subsection mem_data_fmt mem_data_fmt
mem_data_fmt (memory data format) controls the data format that is read:

  - For unsigned char read functions:

    NOte that these are all supported conversions by these functions.
\verbatim
    file_data_fmt   | mem_data_fmt     | conversion
    ---------------------------------------------------------------------
    P_8_BIT_FILE    | P_8_BIT_MEM      | direct memcpy, no conversion
                    | P_16_BIT_MEM_LSB | result is always 0 (zero)
                    | P_AF_BIT_MEM     | error
    ---------------------------------------------------------------------
    P_10_BIT_FILE   | P_8_BIT_MEM      |  (sample & 0x03ff) >> 2
                    | P_16_BIT_MEM_LSB | ((sample & 0x03ff) << 6) && 0xff
                    | P_AF_BIT_MEM     | error
    ---------------------------------------------------------------------
    P_12_BIT_FILE   | P_8_BIT_MEM      |  (sample & 0x0fff) >> 4
                    | P_16_BIT_MEM_LSB | ((sample & 0x0fff) << 4) && 0xff
                    | P_AF_BIT_MEM     | error
    ---------------------------------------------------------------------
    P_14_BIT_FILE   | P_8_BIT_MEM      |  (sample & 0x3fff) >> 6
                    | P_16_BIT_MEM_LSB | ((sample & 0x3fff) << 2) && 0xff
                    | P_AF_BIT_MEM     | error
    ---------------------------------------------------------------------
    P_16_BIT_FILE   | P_8_BIT_MEM      |  (sample & 0xffff) >> 8
                    | P_16_BIT_MEM_LSB |  (sample & 0xffff)       && 0xff
                    | P_AF_BIT_MEM     | error
    ---------------------------------------------------------------------
\endverbatim
  - For unsigned short read functions:

    Note that these are not all supported formats, just the most common 
    ones; all possible combinations of file_data_fmt & mem_data_fmt are 
    supported.

    Note that conversion may be required even if the file_data_fmt
    is P_16_BIT_FILE and the mem_data_fmt is P_16_BIT_MEM due to
    the endian format.
    When writing, a file is stored in the endian mode of the platform.
    When reading on a platform with different endian mode, the
    LSB and MSB bytes are swapped.

\verbatim
    file_data_fmt   | mem_data_fmt    | conversion
    ----------------------------------------------------------------
    P_8_BIT_FILE    | P_8_BIT_MEM     | (sample & 0x00ff)
                    | P_16_BIT_MEM    | (sample & 0x00ff) << 8
                    | P_AF_BIT_MEM    | (sample)
    ----------------------------------------------------------------
    P_10_BIT_FILE   | P_10_BIT_MEM    | (sample & 0x03ff)
                    | P_16_BIT_MEM    | (sample & 0x03ff) << 6
                    | P_AF_BIT_MEM    | (sample)
    ----------------------------------------------------------------
    P_12_BIT_FILE   | P_12_BIT_MEM    | (sample & 0x0fff)
                    | P_16_BIT_MEM    | (sample & 0x0fff) << 4
                    | P_AF_BIT_MEM    | (sample)
    ----------------------------------------------------------------
    P_14_BIT_FILE   | P_14_BIT_MEM    | (sample & 0x3fff)
                    | P_16_BIT_MEM    | (sample & 0x3fff) << 2
                    | P_AF_BIT_MEM    | (sample)
    ----------------------------------------------------------------
    P_16_BIT_FILE   | P_16_BIT_MEM    | (sample)
                    | P_AF_BIT_MEM    | (sample)
    ----------------------------------------------------------------
\endverbatim

\section write Write functions

  What is written to file is controlled by the "write_mode":

  write_mode = \ref mem_data_fmt

\subsection components The header of the file controls the components that are written

    Since the components that are written to disk only depend on 
    the header, there is no component_mode used with the write
    functions.
\verbatim
    color_format    | written to file
    ---------------------------------
    P_NO_COLOR      | Y              *)
    ---------------------------------
    P_COLOR_422,    | Y, UV
    P_COLOR_420     |
    ---------------------------------
    P_COLOR_444_PL, | Y, U, V
    P_COLOR_422_PL, |
    P_COLOR_420_PL  |
    ---------------------------------
    P_COLOR_RGB     | R, G, B
    ---------------------------------
    P_STREAM        | S              *)
    ---------------------------------

    *) Only these buffers are read from the memory; pointers to
       other buffers (e.g. U/V) can be NULL pointers.
\endverbatim

\subsection mem_data_fmt2 mem_data_fmt controls the data format that is written:

  - For unsigned char write functions:

\verbatim
    file_data_fmt | conversion   
    --------------------------------------------
    P_8_BIT_FILE  | direct copy, no conversion
    --------------------------------------------
    P_10_BIT_FILE | (sample & 0x00ff) << 2
    --------------------------------------------
    P_12_BIT_FILE | (sample & 0x00ff) << 4
    --------------------------------------------
    P_14_BIT_FILE | (sample & 0x00ff) << 6
    --------------------------------------------
    P_16_BIT_FILE | (sample & 0x00ff) << 8
    --------------------------------------------
\endverbatim
    The mem_data_fmt is always P_8_BIT_MEM for these functions.
    Since mem_data_fmt is constant, these functions do not have
    the parameter write_mode as one of the function arguments.
  

  - For unsigned short write functions:

    Note that these are not all supported formats, just the most common 
    ones; all possible combinations of file_data_fmt & mem_data_fmt are 
    supported.

    Note that the endian mode is not converted when writing a file
    since a file is always written in the endian mode of the
    current platform.

\verbatim
    file_data_fmt | mem_data_fmt    | conversion
    --------------------------------------------------------------
    P_8_BIT_FILE  | P_8_BIT_MEM     | (sample & 0x00ff)
                  | P_16_BIT_MEM    | (sample & 0xffff) >> 8
                  | P_AF_BIT_MEM    | (sample)
    --------------------------------------------------------------
    P_10_BIT_FILE | P_10_BIT_MEM    | (sample & 0x03ff)
                  | P_16_BIT_MEM    | (sample & 0xffff) >> 6
                  | P_AF_BIT_MEM    | (sample)
    --------------------------------------------------------------
    P_12_BIT_FILE | P_12_BIT_MEM    | (sample & 0x0fff)
                  | P_16_BIT_MEM    | (sample & 0xffff) >> 4
                  | P_AF_BIT_MEM    | (sample)
    --------------------------------------------------------------
    P_14_BIT_FILE | P_14_BIT_MEM    | (sample & 0x3fff)
                  | P_16_BIT_MEM    | (sample & 0xffff) >> 2
                  | P_AF_BIT_MEM    | (sample)
    --------------------------------------------------------------
    P_16_BIT_FILE | P_16_BIT_MEM    | (sample)
                  | P_AF_BIT_MEM    | (sample)
    --------------------------------------------------------------
\endverbatim
*/


/** \weakgroup comp_mode Component Mode
 * \ingroup readwrite
 * @{ */
#define P_READ_Y         0
#define P_READ_ALL       1
#define P_READ_UV        2
#define P_READ_U         3
#define P_READ_V         4
#define P_READ_R         5
#define P_READ_G         6
#define P_READ_B         7
/** @} */

/** \weakgroup mem_data_fmt Memory data format
 * \ingroup readwrite
 * @{ */
#define P_8_BIT_MEM      (0 * 16)
#define P_10_BIT_MEM     (1 * 16)
#define P_12_BIT_MEM     (2 * 16)
#define P_14_BIT_MEM     (3 * 16)
#define P_16_BIT_MEM     (4 * 16)
#define P_16_BIT_MEM_LSB (5 * 16)
#define P_AF_BIT_MEM     (7 * 16)
/** @} */

/** \defgroup rwfunc Read/write functions
 * \ingroup readwrite
 @{
\section all All read/write functions

- read_* functions read specified frame/field from file.
- write_* functions write specified frame/field to file.
- *field* functions only operate on interlaced formats 
  (these functions are not supported for progressive formats).
- *frame* functions operate on both interlaced and
  progressive formats (for interlaced formats two fields
  are combined within one frame).
- *_planar functions operate on formats with three components
  (rgb & planar yuv files).
- *_16 functions operate on buffers of type unsigned short.

See also \ref images

\param filename                 name of file to read/write.
\param header                   header (for read functions: first read with p_read_header;
                                        for write functions: written with p_write_header).
\param frame                    frame number to read/write (1,2,3,...).
\param field                    field number to read/write (1 or 2).
\param y_fld                    address of Y buffer.
\param y_or_s_frm               address of Y or S buffer.
\param y_or_r_fld/y_or_r_frm    address of Y or R buffer.
\param uv_fld/uv_frm            address of U/V buffer.
\param u_or_g_fld/u_or_g_frm    address of U or G buffer.
\param v_or_b_fld/v_or_b_frm    address of V or B buffer.
\param read_mode                = component_mode | mem_data_fmt, also see \ref readwrite.
\param write_mode               = mem_data_fmt, also see \ref mem_data_fmt.
\param width                    width of field/frame in memory.
\param fld_height               height of field in memory.
\param frm_height               height of frame in memory.
\param stride                   stride of field/frame in memory.
\param uv_stride                U & V buffer stride of field/frame in memory:
                                - if set to zero the Y buffer stride is taken.
                                - don't care for RGB files.
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
extern pT_status p_read_field
        (const char *filename, pT_header *header,
         int frame, int field,
         unsigned char *y_fld,
         unsigned char *uv_fld,
         int read_mode,
         int width, int fld_height, int stride);
extern pT_status p_read_frame
        (const char *filename, pT_header *header,
         int frame,
         unsigned char *y_or_s_frm,
         unsigned char *uv_frm,
         int read_mode,
         int width, int frm_height, int stride);
extern pT_status p_write_field
        (const char *filename, pT_header *header,
         int frame, int field,
         const unsigned char *y_fld,
         const unsigned char *uv_fld,
         int width, int fld_height, int stride);
extern pT_status p_write_frame
        (const char *filename, pT_header *header,
         int frame,
         const unsigned char *y_or_s_frm,
         const unsigned char *uv_frm,
         int width, int frm_height, int stride);

extern pT_status p_read_field_16
        (const char *filename, pT_header *header,
         int frame, int field,
         unsigned short *y_fld,
         unsigned short *uv_fld,
         int read_mode,
         int width, int fld_height, int stride);
extern pT_status p_read_frame_16
        (const char *filename, pT_header *header,
         int frame,
         unsigned short *y_or_s_frm,
         unsigned short *uv_frm,
         int read_mode,
         int width, int frm_height, int stride);
extern pT_status p_write_field_16
        (const char *filename, pT_header *header,
         int frame, int field,
         const unsigned short *y_fld,
         const unsigned short *uv_fld,
         int write_mode,
         int width, int fld_height, int stride);
extern pT_status p_write_frame_16
        (const char *filename, pT_header *header,
         int frame,
         const unsigned short *y_or_s_frm,
         const unsigned short *uv_frm,
         int write_mode,
         int width, int frm_height, int stride);

/*
 * Planar luminance/chrominance (YUV) or red/green/blue (RGB) files.
 *
 * Supported color_format: P_NO_COLOR, 
 *                         P_COLOR_444_PL, 
 *                         P_COLOR_422_PL, 
 *                         P_COLOR_420_PL, 
 *                         P_COLOR_RGB.
 *
 */
extern pT_status p_read_field_planar
        (const char *filename, pT_header *header,
         int frame, int field,
         unsigned char *y_or_r_fld,
         unsigned char *u_or_g_fld,
         unsigned char *v_or_b_fld,
         int read_mode,
         int width, int fld_height, int stride, int uv_stride);
extern pT_status p_read_frame_planar
        (const char *filename, pT_header *header,
         int frame,
         unsigned char *y_or_r_frm,
         unsigned char *u_or_g_frm,
         unsigned char *v_or_b_frm,
         int read_mode,
         int width, int frm_height, int stride, int uv_stride);
extern pT_status p_write_field_planar
        (const char *filename, pT_header *header,
         int frame, int field,
         const unsigned char *y_or_r_fld,
         const unsigned char *u_or_g_fld,
         const unsigned char *v_or_b_fld,
         int width, int fld_height, int stride, int uv_stride);
extern pT_status p_write_frame_planar
        (const char *filename, pT_header *header,
         int frame,
         const unsigned char *y_or_r_frm,
         const unsigned char *u_or_g_frm,
         const unsigned char *v_or_b_frm,
         int width, int frm_height, int stride, int uv_stride);

extern pT_status p_read_field_planar_16
        (const char *filename, pT_header *header,
         int frame, int field,
         unsigned short *y_or_r_fld,
         unsigned short *u_or_g_fld,
         unsigned short *v_or_b_fld,
         int read_mode,
         int width, int fld_height, int stride, int uv_stride);
extern pT_status p_read_frame_planar_16
        (const char *filename, pT_header *header,
         int frame,
         unsigned short *y_or_r_frm,
         unsigned short *u_or_g_frm,
         unsigned short *v_or_b_frm,
         int read_mode,
         int width, int frm_height, int stride, int uv_stride);
extern pT_status p_write_field_planar_16
        (const char *filename, pT_header *header,
         int frame, int field,
         const unsigned short *y_or_r_fld,
         const unsigned short *u_or_g_fld,
         const unsigned short *v_or_b_fld,
         int write_mode,
         int width, int fld_height, int stride, int uv_stride);
extern pT_status p_write_frame_planar_16
        (const char *filename, pT_header *header,
         int frame,
         const unsigned short *y_or_r_frm,
         const unsigned short *u_or_g_frm,
         const unsigned short *v_or_b_frm,
         int write_mode,
         int width, int frm_height, int stride, int uv_stride);

/** @} */

/** \defgroup single_comp Low level access to single components
 * bladibla
 */

/** \defgroup components Low level data access functions
 * \ingroup single_comp
 * @{
 * Data access per component.
 *
 * Consider these as low level pfspd data access routines.
 * There is no interpretation of the data, so these routines are
 * more generic that all others. However, you can create files
 * that cannot be processed by standard tools.
 *
 * Typical use:
 *   - Add extra component to standard pfspd files.
 *   - Exotic applications applying functionality beyond standard.
 *
 * Supported on (almost) all pfspd file formats.
 * Therefore, these functions use less strict header checking.
 *
 * For standard i/o: the component shall be accessed in the same
 * order as the data is stored in the file. This corresponds with
 * the order in the header. Furthermore, for interlaced files
 * only the field access functions can be used.
 * For file i/o these restriction do not apply.
 *
 * In the following functions:
 * \param comp      the component id. i.e. the index in the header structure.
 * \param c_frm     pointer to frame buffer
 * \param c_fld     pointer to field buffer
 *
 * For this set of functions, specify read_mode as:
 *   read_mode = mem_data_fmt
 *
 * The write mode is identical to the other functions:
 *   write_mode = mem_data_fmt
 *
 */
extern pT_status p_read_field_comp
        (const char *filename, pT_header *header,
         int frame, int field, int comp,
         unsigned char *c_fld,
         int read_mode,
         int width, int fld_height, int stride);
extern pT_status p_read_frame_comp
        (const char *filename, pT_header *header,
         int frame, int comp,
         unsigned char *c_frm,
         int read_mode,
         int width, int frm_height, int stride);
extern pT_status p_write_field_comp
        (const char *filename, pT_header *header,
         int frame, int field, int comp,
         const unsigned char *c_fld,
         int width, int fld_height, int stride);
extern pT_status p_write_frame_comp
        (const char *filename, pT_header *header,
         int frame, int comp,
         const unsigned char *c_frm,
         int width, int frm_height, int stride);

extern pT_status p_read_field_comp_16
        (const char *filename, pT_header *header,
         int frame, int field, int comp,
         unsigned short *c_fld,
         int read_mode,
         int width, int fld_height, int stride);
extern pT_status p_read_frame_comp_16
        (const char *filename, pT_header *header,
         int frame, int comp,
         unsigned short *c_frm,
         int read_mode,
         int width, int frm_height, int stride);
extern pT_status p_write_field_comp_16
        (const char *filename, pT_header *header,
         int frame, int field, int comp,
         const unsigned short *c_fld,
         int write_mode,
         int width, int fld_height, int stride);
extern pT_status p_write_frame_comp_16
        (const char *filename, pT_header *header,
         int frame, int comp,
         const unsigned short *c_frm,
         int write_mode,
         int width, int frm_height, int stride);
/** @} */

/** \defgroup comp_hdr Low level header functions on single components
 * \ingroup single_comp
 *  @{
 * \remarks Typically used after all other header settings are correct.
 * \remarks Do use p_get_comp_buffer_size() to determine buffer
 *       sizes. The functions support subsample factors
 *       which are not a multiple of the file image size.
 *       The component size is rounded UP.
 */

/** Add a component to the header structure
 * - Component properties need to be set by p_mod_comp()
 * \return component id.
 * \return -1 if pfspd header full
 */
extern int p_mod_add_comp (pT_header *header);

/** Uses the name to find the component id in \p header
 * Returns -1 if not found
 */
extern int p_get_comp_by_name (const pT_header *header, const char *name);

/** Returns the total number of components from the header */
extern int p_get_num_comps (const pT_header *header);

/** Sets all properties of \p comp in \p header
 * The multiplex factor is typically 1 for Y, 2 for U/V.
 */
extern pT_status p_mod_set_comp_2
        (pT_header *header, const int comp,
         const char *name,
         const pT_data_fmt file_data_fmt,
         const int pix_subsample,
         const int line_subsample,
         const int multiplex_factor);

/** Obsolete function, use p_mod_set_comp_2() instead
 * maintained for backward compatibility.
 */
#define p_mod_set_comp(h,c,n,d,p,l) p_mod_set_comp_2((h),(c),(n),(d),(p),(l),1)

/** Remove a \p comp from \p header
 * - Shall only be used for extra components
 * - No operation for component id -1
 */
extern pT_status p_mod_rm_comp (pT_header *header, const int comp);

/** Remove all "extra" components from \p header */
extern pT_status p_mod_rm_extra_comps (pT_header *header);

/** Retrieves all properties of component \p comp from \p header
 * name string allocated by caller, length: P_SCOM_CODE+1
 * Any output parameters may be NULL (no value returned)
 */
extern pT_status p_get_comp_2
        (const pT_header *header, const int comp,
         char *name,
         pT_data_fmt *file_data_fmt,
         int *pix_subsample,
         int *line_subsample,
         int *multiplex_factor);

/** Obsolete function, use p_get_comp_2() instead
 * maintained for backward compatibility.
 */
#define p_get_comp(h,c,n,d,p,l) p_get_comp_2((h),(c),(n),(d),(p),(l),NULL)

/** Return height and width of \p comp buffer.
 * This is the minimum size of a buffer that can be used by the read/write 
 * field/frame functions.
 * - For interlaced files this is the size of a field.
 * - For progressive files this is the size of a frame.
 */
extern pT_status p_get_comp_buffer_size (const pT_header *header, const int comp,
                                         int *width, int *height);
/** @} */

/** \defgroup convenience Convenience routines to access a single component
 * \ingroup single_comp
 * @{
 * Characteristic of these routines is that they perform a
 * transformation between the user buffer and the data.
 * In particular, the following is possible:
 * - Type conversions to/from float, int, short user buffer
 * - Signed user buffer with:
 *   - offset parameter
 *   - negation parameter
 * - Scaling parameter
 *
 * As a result, these functions are not fast; their focus
 * is on ease-of-use.
 *
 * Typical use is a vector component with e.g. 2 bits
 * subpixel precision that is stored in a floating point
 * application buffer.
 *
 * These functions are built upon the p_write_frame/field_comp_16()
 * functions. They perform dynamic memory allocation using
 * malloc()/free().
 *
 * Most of the parameters are standard, the special ones are:
 * \param field    0: frame, 1/2: field access.
 * \param abuf     the pointer to the application buffer
 * \param atype    type of the application buffer
 * \param offset   zero value for signed data
 *            pfspd data is always unsigned, this specifies the
 *            pfspd value that maps to zero for signed appl buffers.
 * \param gain     multiplication factor (may be negative)
 *
 * When writing, an application value a is transformed into a pfspd value p:
 * \code  p = (unsigned short)((a * gain) + offset) \endcode
 * When reading, the inverse transformation is:
 * \code  a = (p - offset) / (atype)gain    \endcode
 *
 * When the component has type P_16_REAL_FILE, float and double values 
 * are converted to and from an internal 16-bit float format.
 */
extern pT_status p_cce_read_comp
        (const char *filename, pT_header *header,
         int frame, int field, int comp,
         void *abuf, pT_buf_type atype,
         int offset, int gain,
         int width, int height, int stride);

extern pT_status p_cce_write_comp
        (const char *filename, pT_header *header,
         int frame, int field, int comp,
         const void *abuf, pT_buf_type atype,
         int offset, int gain,
         int width, int height, int stride);
/** @} */


/** \defgroup hdr High Dynamic Range
 * @{
 * Convenience routines to handle high dynamic range data.
 * The application buffers are in float format.
 */

/** Convenience routines to read and write (XYZ) wide colour gamut and/or high dynamic range data.
 * \param   filename        file to read/write
 * \param   header          pointer to pT_header struct
 * \param   frame           frame number
 * \param   field           0: frame, 1/2: field access.
 * \param   r_or_x_frm      address of R or X buffer in float format
 * \param   g_or_y_frm      address of G or Y buffer in float format
 * \param   b_or_z_frm      address of B or Z buffer in float format
 * \param   width           width of frame in memory.
 * \param   height          height of frame in memory.
 * \param   stride          stride of frame in memory.
 *
 * If the file is in integer format (e.g. P_16_BIT_FILE), float data range is assumed to be [0..1].
 * Otherwise, for P_16_REAL_FILE full float range is used.
 */
extern pT_status p_cce_read_float_xyz(
         const char *filename,
         pT_header *header,
         int frame,
         int field,
         float *r_or_x_frm,
         float *g_or_y_frm,
         float *b_or_z_frm,
         int width, int height, int stride );
extern pT_status p_cce_write_float_xyz(
         const char *filename,
         pT_header *header,
         int frame,
         int field,
         float *r_or_x_frm,
         float *g_or_y_frm,
         float *b_or_z_frm,
         int width, int height, int stride );
         
/** Convert IEEE 754 float to 16 bit float
 * \param   f32             float to be converted to f16
 */
extern unsigned short p_cce_float_to_f16(
         float f32 );

/** Convert 16 bit float to IEEE 754 float 
 * \param   f16             f16 to be converted to float
 */
extern float p_cce_f16_to_float(
         unsigned short f16 );

/** test float conversion on execution platform
 */
extern pT_status p_cce_check_float_conversion( void );


/** @} */


/** \defgroup auxiliary Auxiliary data
 * @{
 * Auxiliary data can be stored both in the header and along with each image.
 *
 * \section aux_hdr Auxiliary header
 * The auxiliary header contains a description of the auxiliary data (format)
 * attached to each image. Interpretation is left to the application.
 * It is also possible to only store information in the header, without adding
 * to the image size (maximum data length = 0). This way, the description 
 * field in the header contains all relevant information.
 * 
 * \section aux_data Auxiliary data
 * Each image can contain a variable amount of auxiliary data (up to the 
 * maximum size specified in the header).
 * 
 * For standard I/O: the auxiliary data shall be accessed in the same
 * order as the data is stored in the file. This corresponds with
 * the order in the header. Note that auxiliary data must be accessed \em before 
 * any image components.
 * Furthermore, for interlaced files only field access can be used.
 * For file I/O these restriction do not apply.
 */

/** Add an auxiliary header to header structure
 * \param header        The pfspd header to modify
 * \param max_size      Maximum data length per image (0: only information in header)
 * \param name          Name of auxiliary data
 * \param descr_len     Number of bytes in \p description
 * \param description   \p descr_len bytes, to be placed in the auxiliary header
 * \return              -1: error, >= 0: auxiliary ID
 *     NB: -1 can be  returned in two ways:
 *     - 'name' already exists
 *     - not enough space in aux. header to allocate this request
 */
extern int p_mod_add_aux (
        pT_header     * header,
        int             max_size,
        const char    * name,
        int             descr_len,
        const char    * description );

/** Get number of auxiliary header IDs */
extern int p_get_num_aux (
        const pT_header *header );

/** Get ID of an auxiliary header by name */
extern int p_get_aux_by_name (
        const pT_header * header,
        const char      * name );

/** Remove auxiliary header from header structure
 * \param header    The header to modify
 * \param aux_id    ID of the auxiliary data
 * \return          P_OK, P_INVALID_AUXILIARY
 */
extern pT_status p_mod_rm_aux (
        pT_header     * header,
        int             aux_id );

/** Get all properties of an auxiliary header
 * \param   header      The header to read
 * \param   aux_id      Auxiliary ID
 * \param   max_size    Maximum data length per image. May be NULL.
 * \param   name        Name of auxiliary data. May be NULL.
 *                      Must be allocated by the caller, size P_SAUX_NAME+1.
 * \param   descr_len   Number of bytes in \p description. May be NULL.
 * \param   description Data description. May be NULL.
 *                      Must be allocated by the caller, size given by \p descr_len.
 * \return              P_OK, P_INVALID_AUXILIARY
 */
extern pT_status p_get_aux (
        const pT_header *header,
        int              aux_id,
        int  * max_size,
        char * name,
        int  * descr_len,
        char * description );

/** Read auxiliary data of one image
 * \param   filename    Filename ('-' = stdio)
 * \param   header      header structure
 * \param   frame       Frame number
 * \param   field       0: frame, 1/2: field access
 * \param   aux_id      Auxiliary ID
 * \param   size        Actual data length
 * \param   buf         Buffer for auxiliary data.
 *                      Must be allocated by the caller, maximum size given
 *                      by the \p max_size parameter of p_get_aux().
 */
extern pT_status p_read_aux (
        const char    * filename,
        pT_header     * header,
        int             frame,
        int             field,
        int             aux_id,
        int           * size,
        unsigned char * buf );


/** Write auxiliary data of one image
 * \param   filename    Filename ('-' = stdio)
 * \param   header      header structure
 * \param   frame       Frame number
 * \param   field       0: frame, 1/2: field access
 * \param   aux_id      Auxiliary ID
 * \param   size        Actual data length.
 *                      Maximum length is given by the \p max_size parameter of p_get_aux().
 * \param   buf         Buffer for auxiliary data
 * \return      P_OK, P_INVALID_AUXILIARY, P_EXCEEDING_AUXILIARY_SIZE
 */
extern pT_status p_write_aux (
        const char    * filename,
        pT_header     * header,
        int             frame,
        int             field,
        int             aux_id,
        int             size,
        unsigned char * buf );

/** @} */

/** \defgroup openclose File open/close.
 * @{
 * Cpfspd opens a file at its first access. When the application
 * attempts to open more than 10 files simultaneously, the least recently
 * used one is closed. At program termination, all open files are closed.
 * Normally, this is all handled automatically and the application
 * does not need to take action.
 *
 * Only in specific cases, the following functions are required.
 * These functions allow more detailed control over the open files.
 *
 * The open file administration is kept inside the cpfspd library
 * and is stored in global data. Therefore, multithreading applications
 * need to take special precautions.
 *
 * In a multi threading application one of the following two approaches
 * must be followed:
 * 1) First open all files in a single thread, then start threads
 * to process the data from the opened files. More precise:
 * - all open and close operations shall be performed in a single thread
 * - a single file shall be accessed only within a single thread
 * - no more than 10 files shall be accessed
 * 2) Perform all accesses to a file in a single threat and
 * protect all file open and close operations as critical sections.
 * More precise:
 * - all open and close operations shall be sequentialized by putting
 *   them in a critical section (e.g. guarded by a single semaphore)
 * - a single file shall be accessed only within a single thread
 * - no more than 10 files shall be accessed
 *
 * File open: specify the filename and a flag indicating write access.
 * File close: if a filename is specified, only that file is closed
 * (if it was open); if NULL is specified, all open files are closed.
 */
extern pT_status p_open_file (const char *filename, int write);
extern pT_status p_close_file (const char *filename);
/** @} */

/** \defgroup bufsize File buffer size
 * @{ 
 * Set or retrieve buffer size in kbytes for file access buffer.
 * Only used on win32 platform. The value 0 indicates the default. The
 * value of this default may become platform platform dependent in the
 * future. For the win32 platform we chose 256 kbyte.
 * The buffer size is effected when a file is actually opened by
 * the library. At that time, the current buffer size is used.
 * Applications may set this value at initialization. It is used when
 * opening a file to determine the size of the file access buffer.
 * This allows performance optimization. Typical use is on high
 * performance disk systems like 6 raid scsi disks mounted as a
 * single striped volume.
 * Small values: system file access rate increases (hence an interrupt
 *               penalty may become dominant, resulting in reduced
 *               performance).
 * High values: data access overhead for random access (hence for random
 *              access too much data is read but not used resulting
 *              in poor random access behavior).
 */
extern pT_status p_set_file_buf_size (const int size_kb);
extern int       p_get_file_buf_size (void);
/** @} */

/** \defgroup error Error handling
 * @{
 */
/** Check value of status and exit on a fatal error.
 * Error description is printed to stream when a fatal error occurs.
 */
extern void     p_fatal_error (pT_status status, 
                               FILE *stream);
/** Check value of status and exit on a fatal error.
 * Error description is printed to stream when a fatal error occurs.
 * Variable \p filename is printed to stream when a fatal error occurs
 * (this is convenient to link an error with a specific filename).
 */
extern void     p_fatal_error_fileio (pT_status status, 
                                      const char *filename,
                                      FILE *stream);
/** Return the character string describing the error code. */
extern char    *p_get_error_string (pT_status status);
/** @} */

/** \defgroup version Version identification functions
 * @{
 * Note: Data returned by these functions is generated by the
 *       makefile system. See files cpfspd_mag.c and cpfspd_mag.h
 */

/** This character string contains both the version & magic number.
 * The commands "ident" or "what" can extract this string from
 * an object library or executable file. This way, version
 * information can always easily be retrieved.
 */
extern char *p_revision_str;

/** Return string with the version number. */
extern char *p_get_version (void);

/** Return string with the magic number 
 * (depends on the contents of all cpfspd*.[ch] files).
 */
extern char *p_get_magic (void);

/** @} */

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* end of #ifndef CPFSPD_H_INCLUDED */
