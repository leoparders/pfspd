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
 *  CVS id      :  $Id: cpfspd_low.c,v 2.78 2013/06/20 07:17:33 vleutenr Exp $
 *
 *  Name        :  cpfspd_low.c
 *
 *  Authors     :  Robert Jan Schutten
 *                 Bram Riemens
 *
 *  Function    :  LOW-level interface to pfspd video files.
 *                 ---
 *
 *  Description :  Basic low level pfspd io routines written
 *                 in ansi-c so that they are portable to any
 *                 unix-like platform.
 *                 These functions are only used internally in
 *                 cpfspd.
 *
 *                 The following external variables are defined
 *                 (required in exceptional cases, see cpfspd.h)
 *                 - p_file_buffer_size_kb
 *
 *                 The following external functions are defined
 *                 ( required in exceptional cases, see cpfspd.h)
 *                 - p_open_file()
 *                 - p_close_file()
 *                 - p_set_file_buf_size()
 *                 - p_get_file_buf_size()
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

/* The plain win32 executable is usually controlled from a DOS command
 * window. In this environment, stdio data is treated as text data,
 * i.e. at input, \cr\lf is substituted by \cr and at output,
 * a \cr is substituted by \cr\lf. Since pfspd data must be treated
 * binary, special measures are required.
 * Hence, this conditional compilation for the MSVC environment,
 * consisting of extra include files and the follwing control
 * in function p_get_file_pointer():
 *     _setmode(_fileno(stream),_O_BINARY);
 */
#ifdef __MSVC__
#define STDIO_SET_BIN 1
#endif

#ifdef STDIO_SET_BIN
#include <fcntl.h>
#include <io.h>
#endif

/******************************************************************************/

#include "cpfspd_low.h"
#include "cpfspd_hdr.h"
#include "cpfspd_fio.h"


/******************************************************************************/

/***************************************************************
*                                                              *
*       General defines                                        *
*                                                              *
***************************************************************/

/* define minimum/maximum macros */
#define MIN(x,y)             ( ((x) < (y)) ? (x) : (y) )
#define MAX(x,y)             ( ((x) > (y)) ? (x) : (y) )

/* number of files that can be opened simultaneously */
#define P_MAX_OPEN_FILES           10

/* range of number of bytes per record */
#define P_MIN_BYTES_PER_REC        64
#define P_BIG_BUFFER_SIZE          2048
#define P_MAX_FIELD_LEN            25   /* Maximum length of a hdr int/float */


/******************************************************************************/

/******************************************
* Static data to keep track of open files *
* and buffer size                         *
******************************************/

typedef enum {
    p_mode_read = 0,
    p_mode_write = 1,
    p_mode_update = 2
} p_file_mode;

typedef struct {
    FILE          *fp;                    /* The file pointer; NULL denotes an empty slot */
    char          name[P_FILENAME_MAX];   /* File name, what else ;-) */
    p_file_mode   mode;                   /* Open for read/write access */
    unsigned long timestamp;              /* Timestamp of last access */
    long          no_of_images;           /* Highest image number actually written to file */
    long          size_header;            /* Cached value derived from header to calculate file positions */
    long          size_image;             /* (image = aux data + all components */
    long          hdr_nr_images;          /* Number of images in file header */
} p_file_admin_t;

static char           *p_mode_str[] = {"rb", "wb", "rb+"};
static p_file_admin_t p_files[P_MAX_OPEN_FILES];
static int            p_file_count = 0;
static unsigned long  p_event_count = 0;
static int            p_atexit_done = 0;
static int            p_file_buffer_size_kb = 0;
static int            p_stdin_used = 0;


#ifdef _ONLY_FOR_DEBUG
/* Debug convenience routine
 * I (BR) do not want to remove this code, since it may be useful
 * in future debugging. It is conditional compiled
 * to avoid compiler warnings.
 */
static void p_dump_file_admin(void)
{
    int i;
    printf("FILE_ADMIN Fp Tstamp Mode NoImgs SzHead SzImag Name\n");
    for (i=0; i<p_file_count; i++) {
        printf("%2d", i);
        printf(" 0x%08lx", (unsigned long)p_files[i].fp);
        printf(" %6ld",  p_files[i].timestamp);
        printf(" %4d",   p_files[i].mode);
        printf(" %6ld",  p_files[i].no_of_images);
        printf(" %6ld",  p_files[i].size_header);
        printf(" %6ld",  p_files[i].size_image);
        printf(" %s",    p_files[i].name);
        printf("\n");
    }
}
#endif /* _ONLY_FOR_DEBUG */

/* internal function to determine the byte order of the system */
static int
p_system_is_little_endian(void)
{
    unsigned short test = 0x1234u;
    return (0x34 == *(unsigned char *) &test) ? 1 : 0;
}


/* internal function to determine the size of a component in the file */
static int
p_get_size_comp (int width, int height, char *data_fmt)
{
    int data_size;

    if (!strncmp(data_fmt, P_B8_DATA_FMT, (size_t)P_SDATA_FMT)) {
        data_size = 1;
    } else {
        data_size = 2;
    }

    return(data_size * width * height);
} /* end of p_get_size_comp () */


/* internal function to determine the size of an image (all components) in the file */
static long
p_get_size_image (pT_header *header)
{
    long size;
    int  i;

    /* Auxiliary data records */
    size = header->nr_aux_data_recs * header->bytes_rec;
    for (i = 0; i < header->nr_compon; i++) {
        /* Add component size */
        size += p_get_size_comp (header->comp[i].pix_line,
                                 header->comp[i].lin_image,
                                 header->comp[i].data_fmt);
    }

    return(size);
} /* end of p_get_size_image */


/* internal function to determine the size of the header in the file */
static long
p_get_size_header (pT_header *header)
{
    long size;

    size  = P_NUM_GLOB_RECS * header->bytes_rec;
    size += header->nr_fd_recs * header->bytes_rec;
    size += P_NUM_COMP_RECS * header->nr_compon * header->bytes_rec;

    return(size);
} /* end of p_get_size_header */


/*
 * Close the file identified by the index.
 * In case original length in the header is not correct,
 * write it at the beginning of the file.
 */
static pT_status
p_close_idx (const int idx)
{
    pT_status status;
    char temp[P_SAPPL_TYPE + 1]; /* largest possible character string */

    status = P_OK;

    /* Still need to write the amount of images? */
    if (p_files[idx].no_of_images > p_files[idx].hdr_nr_images) {
        p_fio_fseek(p_files[idx].fp, 0, SEEK_SET);
        sprintf(temp, "%7ld", p_files[idx].no_of_images);
        if (p_fio_fwrite(temp, (size_t)sizeof(char), (size_t)P_SNR_IMAGES, p_files[idx].fp) != P_SNR_IMAGES) {
            status = P_WRITE_FAILED;
        }
    }

    p_fio_fclose(p_files[idx].fp);
    p_files[idx].fp = NULL;

#ifdef FIO_WIN32_FILE
    {
        fio_offset_t offset;

        /* Truncate file if it was opened for writing.
         * Only required with win32 file system, because there we
         * used unbuffered asynchronous I/O. Therefore the file size
         * is a multiple of the block size.
         */
        if ((status == P_OK) && (p_files[idx].mode == p_mode_write)) {
            offset  = p_files[idx].size_header;
            offset += (fio_offset_t)p_files[idx].no_of_images * p_files[idx].size_image;
            /* truncate file to size offset */
            if (p_fio_set_end_of_file(p_files[idx].name, offset)) {
                status = P_WRITE_FAILED;
            }
        }
    }
#endif

    return(status);
} /* p_close_idx () */


/*
 * Close open files.
 * For a single file, specify its name.
 * NULL closes all files.
 */
pT_status
p_close_file (const char *filename)
{
    int  i;
    pT_status status;

    status = P_OK;
    for (i=0; i<p_file_count; i++) {
        /* file is open && file needs to be closed? */
        if ((p_files[i].fp != NULL) && ((filename == NULL) || (strcmp(filename, p_files[i].name) == 0))) {
            status = p_close_idx(i);
        }
    }

    if (filename == NULL) {
        p_file_count = 0;
    }

    return(status);
} /* p_close_file () */


static void
p_atexit_close (void)
{
    char      temp[P_BIG_BUFFER_SIZE];

    /* Close all regular files */
    p_close_file(NULL);

    /* If stdin was used, then flush it to avoid a "broken pipe" error */
    if (p_stdin_used) {
        while (!feof(stdin) && !ferror(stdin)) {
            fread(temp, (size_t)sizeof(char), (size_t)P_BIG_BUFFER_SIZE, stdin);
        }
    }
} /* p_atexit_close () */


static FILE *
p_get_file_pointer (const char  *name,      /* file name */
                    int          stdio,     /* bool: stdio or a file */
                    p_file_mode  mode,      /* file open mode */
                    fio_offset_t size)      /* allocation size (ony used when mode==write) */
{
    FILE *fp;
    int  i;
    int  idx;    /* index of found/used file admin record */
    int  lru;    /* index of least recently used (open) file */
    int  ept;    /* index of empty slot */


    /* Check file name length limit */
    assert(strlen(name) < P_FILENAME_MAX);

    /* Make sure the exit cleanup function is registered once. */
    if (!p_atexit_done) {
        p_atexit_done = 1;
        if (atexit(p_atexit_close) != 0) {
            assert(0);
        }
    }

    if (stdio) {
        if (mode == p_mode_read) {
            fp = stdin;
#ifdef STDIO_SET_BIN
            _setmode(_fileno(stdin),_O_BINARY);
#endif
            p_stdin_used = 1;
        } else {
            fp = stdout;
#ifdef STDIO_SET_BIN
            _setmode(_fileno(stdout),_O_BINARY);
#endif
        }
    } else {
        idx = -1;
        lru = -1;
        ept = -1;
        for (i=0; i<p_file_count; i++) {
            if (p_files[i].fp == NULL) {
                /* Empty slot; register first empty slot in ept */
                if (ept == -1) {
                    ept = i;
                }
            } else {
                /* Is this the least recently used file up till now? */
                if (lru == -1) {
                    lru = i;
                } else {
                    if (p_files[i].timestamp < p_files[lru].timestamp) {
                        lru = i;
                    }
                }
            }

            /* Is this the file we need? */
            if (strcmp(name, p_files[i].name) == 0) {
                idx = i;
            }
        }

        /* If open file found */
        if ((idx != -1) && (p_files[idx].fp != NULL)) {
            /* Opened as read, write access required? Close it */
            /* Opened as write, read access required? Close it */
            if ( ((mode != p_mode_read) && (p_files[idx].mode == p_mode_read)) ||
                 ((mode == p_mode_read) && (p_files[idx].mode == p_mode_write)) ) {
                p_close_idx(idx); /* Ignore status - we'll access the same file later */
            }
        }

        /* Not found yet or closed? then open */
        if ((idx == -1) || (p_files[idx].fp == NULL)) {
            /* Get an index */
            if (idx == -1) {
                /* No index yet: get an empty slot, a new slot or close the lru file */
                if (ept != -1) {
                    /* Empty slot available: use it */
                    idx = ept;
                } else {
                    /* No empty slot available */
                    if (p_file_count < P_MAX_OPEN_FILES) {
                        /* Room for new open file available: use it */
                        idx = p_file_count;
                        p_file_count++;
                    } else {
                        /* All slots occupied: close least recently used file and use its admin record */
                        idx = lru;
                        p_close_idx(idx); /* Ignore status -- close error is a different file... */
                    }
                }
            }

            p_files[idx].fp = p_fio_fopen(name, p_mode_str[mode], (mode==p_mode_write)?size:-1);

            /* only if the fopen succeeds register the result! */
            /* this will prevent a crash if the same file is
               later opened again! */
            if (p_files[idx].fp != NULL) {
                strcpy(p_files[idx].name, name);
                p_files[idx].mode = mode;
                p_files[idx].timestamp = 0;
                p_files[idx].no_of_images = 0;
                p_files[idx].size_header = 0;
                p_files[idx].size_image = 0;
                p_files[idx].hdr_nr_images = 0;
            }
        }
        /*
         * Set buffer size. Has only effect immediately after
         * an open. It is placed here since the p_open_file()
         * call cannot pass the buffer size. So when the file
         * is opened via p_open_file(), then the buffer size
         * is actually set at the first other file access
         * call.
         */
        if (p_file_buffer_size_kb != 0) {
            p_fio_bufsize(p_files[idx].fp, p_file_buffer_size_kb*1024);
        }

        /* Keep track of events on this file */
        p_event_count++;
        p_files[idx].timestamp = p_event_count;
        fp = p_files[idx].fp;
    } /* end of if (stdio) */

    return(fp);
} /* end of p_get_file_pointer () */


/*
 * Set the file length of the file pointer
 * (in the administration only; will be written to disk
 * at file close).
 */
static void
p_set_file_length (FILE *fp, const long no_of_images)
{
    int  i;

    for (i=0; i<p_file_count; i++) {
        if (p_files[i].fp == fp) {
            if (no_of_images > p_files[i].no_of_images) {
                p_files[i].no_of_images = no_of_images;
            }
        }
    }
} /* p_set_file_length () */


/*
 * Set the file header and image sizes
 * (in the administration only, will be used to adjest file
 * length when closing the file)
 */
static void
p_set_file_size_info (FILE *fp, const long size_header, const long size_image, const long hdr_nr_images)
{
    int  i;

    for (i=0; i<p_file_count; i++) {
        if (p_files[i].fp == fp) {
            p_files[i].size_header   = size_header;
            p_files[i].size_image    = size_image;
            p_files[i].hdr_nr_images = hdr_nr_images;
        }
    }
} /* p_set_file_size_info () */


pT_status
p_open_file (const char *filename, int write)
{
    pT_status status;
    FILE      *fp;
    const int        stdio = !strcmp(filename, "-");


    status = P_OK;
    fp = p_get_file_pointer(filename, stdio, (write) ? p_mode_write : p_mode_read, (fio_offset_t)-1);

    if (fp == NULL) {
        if (write) {
            status = P_FILE_CREATE_FAILED;
        } else {
            status = P_FILE_OPEN_FAILED;
        }
    }

    return(status);
} /* end of p_open_file () */


pT_status
p_set_file_buf_size (const int size_kb)
{
    p_file_buffer_size_kb = size_kb;
    return P_OK;
} /* end of p_set_file_buf_size */


int
p_get_file_buf_size (void)
{
    return(p_file_buffer_size_kb);
} /* end of p_get_file_buf_size */


/******************************************************************************/

static void
p_add_offset (unsigned long *offset_hi,
              unsigned long *offset_lo,
              long  value)
{
    fio_offset_t  offset;

#if FIO_LARGE_FILE_SUPPORTED
    offset = ((fio_offset_t)(*offset_hi) << 32) + ((fio_offset_t)(*offset_lo) & 0xFFFFFFFF);
#else
    offset = (fio_offset_t)*offset_lo;
#endif

    offset += value;

#if FIO_LARGE_FILE_SUPPORTED
    (*offset_lo) = (unsigned long)(offset & 0xFFFFFFFF);
    (*offset_hi) = (unsigned long)(offset >> 32);
#else
    (*offset_lo) = (unsigned long)offset;
#endif
} /* end of p_add_offset () */


static pT_status
p_write_data (FILE   *file_ptr,
              int    stdio,
              void   *buf,
              size_t size)
{
    size_t    ret;
    pT_status status = P_OK;
    if (stdio) {
        ret = fwrite(buf, (size_t)sizeof(char), size, file_ptr);
        ret = size; /* Make writes to stdout appear succesful */
    } else {
        ret = p_fio_fwrite(buf, (size_t)sizeof(char), size, file_ptr);
    }

    if (ret != size) {
        status = P_WRITE_FAILED;
    }

    return(status);
} /* end of p_write_data () */


static pT_status
p_read_data (FILE   *file_ptr,
             int    stdio,
             void   *buf,
             size_t size)
{
    size_t    ret;
    pT_status status;

    status = P_OK;
    if (stdio) {
        ret = fread(buf, (size_t)sizeof(char), size, file_ptr);
    } else {
        ret = p_fio_fread(buf, (size_t)sizeof(char), size, file_ptr);
    }

    if (ret != size) {
        status = P_READ_FAILED;
    }

    return(status);
} /* end of p_read_data () */


static int
p_end_of_file (FILE   *file_ptr,
               int    stdio)
{
    int       ret;

    if (stdio) {
        ret = feof(file_ptr);
    } else {
        ret = p_fio_feof(file_ptr);
    }

    return(ret);
} /* end of p_end_of_file () */


static pT_status
p_position_pointer (FILE *file_ptr,
                    int stdio,
                    unsigned long *offset_hi,
                    unsigned long *offset_lo,
                    fio_offset_t  new_offset,
                    int write)
{
    pT_status          status = P_OK;
#if FIO_LARGE_FILE_SUPPORTED
    const fio_offset_t current_offset = ((fio_offset_t)(*offset_hi) << 32)
                                             + ((fio_offset_t)(*offset_lo) & 0xFFFFFFFF);
#else
    const fio_offset_t current_offset = (fio_offset_t)*offset_lo;
#endif
    const fio_offset_t relative_offset = new_offset - current_offset;
    const long         num_big_chunks = (long)(relative_offset / P_BIG_BUFFER_SIZE);
    const long         remaining_chunk = (long)((relative_offset -
                                       (num_big_chunks * P_BIG_BUFFER_SIZE)));
    char               temp[P_BIG_BUFFER_SIZE];
    int                i;

    if (stdio) {
        /* note that fseek does not work for stdio */
        /* on read: skip the remaining number of bytes */
        /* on write: write the remaining number of zero's */
        if (relative_offset < 0) {
            status = P_NEGATIVE_SEEK_ON_STDIO;
        } /* end of if (relative_offset < 0) */
        if (status == P_OK) {
            if (write) {
                /* seek on write: write zero's to stdout */
                if (relative_offset > 0) {
                    memset(temp, 0, (size_t)P_BIG_BUFFER_SIZE);
                    for (i = 0; i < num_big_chunks; i++ ) {
                        if (status == P_OK) {
                            status = p_write_data( file_ptr, stdio, temp, (size_t)P_BIG_BUFFER_SIZE );
                        }
                    }
                    if (status == P_OK) {
                        status = p_write_data( file_ptr, stdio, temp, (size_t)remaining_chunk );
                    }
                }
            } else {
                /* seek on read: read and skip data */
                if (relative_offset > 0) {
                    for (i = 0; i < num_big_chunks; i++ ) {
                        if ((status == P_OK) &&
                            (fread (temp, (size_t)sizeof(char),
                                    (size_t)P_BIG_BUFFER_SIZE, file_ptr) !=
                             (size_t)P_BIG_BUFFER_SIZE)) {
                            status = P_READ_FAILED;
                        }
                    }
                    if ((status == P_OK) &&
                        (fread (temp, (size_t)sizeof(char),
                                (size_t)remaining_chunk, file_ptr) !=
                         (size_t)remaining_chunk)) {
                        status = P_READ_FAILED;
                    }
                } /* end of if (relative_offset > 0) */
            } /* end of if (write) */
        } /* end of if (status == P_OK) */
    } else {
        if (p_fio_fseek (file_ptr, new_offset, SEEK_SET) != 0) {
            status = P_SEEK_FAILED;
        }
    } /* end of if stdio */

#if FIO_LARGE_FILE_SUPPORTED
    (*offset_lo) = (unsigned long)(new_offset & 0xFFFFFFFF);
    (*offset_hi) = (unsigned long)(new_offset >> 32);
#else
    (*offset_lo) = (unsigned long)new_offset;
#endif

    return status;
} /* end of p_position_pointer () */


/* internal function to determine the word width of the file data
   format and the memory data format and the file type (the memory
   type is already known) */
static pT_status
p_get_word_widths (pT_data_fmt file_data_fmt, int mem_data_fmt,
                   int *file_no_bits, int *mem_no_bits,
                   int *file_type)
{
    pT_status status = P_OK;

    switch (file_data_fmt) {
    case P_8_BIT_FILE:
        (*file_no_bits) = 8;
        (*file_type)    = P_UNSIGNED_CHAR;
        break;
    case P_10_BIT_FILE:
        (*file_no_bits) = 10;
        (*file_type)    = P_UNSIGNED_SHORT;
        break;
    case P_12_BIT_FILE:
        (*file_no_bits) = 12;
        (*file_type)    = P_UNSIGNED_SHORT;
        break;
    case P_14_BIT_FILE:
        (*file_no_bits) = 14;
        (*file_type)    = P_UNSIGNED_SHORT;
        break;
    case P_16_BIT_FILE:
        (*file_no_bits) = 16;
        (*file_type)    = P_UNSIGNED_SHORT;
        break;
    case P_16_REAL_FILE:
        (*file_no_bits) = 16;
        (*file_type)    = P_UNSIGNED_SHORT;
        break;
    default:
        status = P_ILLEGAL_FILE_DATA_FORMAT;
        (*file_no_bits) = 0;
        (*file_type)    = 0;
        break;
    } /* end of switch (file_data_fmt) */

    switch (mem_data_fmt) {
    case P_8_BIT_MEM:
        (*mem_no_bits) = 8;
        break;
    case P_10_BIT_MEM:
        (*mem_no_bits) = 10;
        break;
    case P_12_BIT_MEM:
        (*mem_no_bits) = 12;
        break;
    case P_14_BIT_MEM:
        (*mem_no_bits) = 14;
        break;
    case P_16_BIT_MEM:
    case P_16_BIT_MEM_LSB:
        (*mem_no_bits) = 16;
        break;
    case P_AF_BIT_MEM:
        (*mem_no_bits) = (*file_no_bits);
        break;
    default:
        status = P_ILLEGAL_MEM_DATA_FORMAT;
        (*mem_no_bits) = 0;
        break;
    } /* end of switch (mem_data_fmt) */

    return status;
} /* end of p_get_word_widths () */


/* internal function to determine the element size */
static pT_status
p_get_element_size (int type, size_t *el_size)
{
    pT_status status = P_OK;

    switch (type) {
    case P_UNSIGNED_CHAR:
        (*el_size) = (size_t)sizeof(unsigned char);
        break;
    case P_UNSIGNED_SHORT:
        (*el_size) = (size_t)sizeof(unsigned short);
        break;
    default:
        (*el_size) = (size_t)0;
        status = P_UNKNOWN_FILE_TYPE;
        break;
    } /* end of switch (file_type) */

    return status;
} /* end of p_get_element_size () */


/* internal function to get an int value from a read buffer */
static pT_status
p_parse_get_int (const char *buf,
                 int        *pos,
                 const int  len,
                 int        *val)
{
    pT_status        status = P_OK;
    int              i;
    char             ch;
    char             temp[P_MAX_FIELD_LEN + 1];

    /* Syntax checking and copy into local buffer. */
    for (i = 0; i < len; i++) {
        ch = buf[*pos + i];
        if (ch == '\0') {
            /* Interpret null character as space */
            ch = (char)' ';
        } else {
            if ( !( (ch == ' ') || isdigit((int)ch) ) ) {
                status = P_FILE_IS_NOT_PFSPD_FILE;
            }
        }
        temp[i] = ch;
    }
    temp[len] = (char)'\0';

    /* Get value. */
    if (status == P_OK) {
        *val = atoi (temp);
        *pos += len;
    }

    return status;
}  /* end of p_parse_get_int () */


/* internal function to get a float value from a read buffer */
static pT_status
p_parse_get_float (const char *buf,
                   int        *pos,
                   const int  len,
                   double     *val)
{
    pT_status        status = P_OK;
    int              i;
    char             ch;
    char             temp[P_MAX_FIELD_LEN + 1];

    /* Syntax checking and copy into local buffer. */
    for (i = 0; i < len; i++) {
        ch = buf[*pos + i];
        if (ch == '\0') {
            /* Interpret null character as space */
            ch = (char)' ';
        } else {
            if ( !( (ch == ' ')
                 || (ch == '-')
                 || (ch == '+')
                 || (ch == 'e')
                 || (ch == 'E')
                 || (ch == '.')
                 || isdigit((int)ch) ) ) {
                status = P_FILE_IS_NOT_PFSPD_FILE;
            }
        }
        temp[i] = ch;
    }
    temp[len] = (char)'\0';

    /* Get value. */
    if (status == P_OK) {
        *val = atof (temp);
        *pos += len;
    }

    return status;
} /* end of p_parse_get_float () */


/* internal function to get a string from a read buffer */
static pT_status
p_parse_get_str(const char *buf,
                int        *pos,
                const int  len,
                char       *val)
{
    pT_status        status = P_OK;
    int              i;
    char             ch;

    /* Syntax checking and copy into local buffer. */
    for (i = 0; i < len; i++) {
        ch = buf[*pos + i];
        if (ch == '\0') {
            /* Interpret null character as space */
            ch = (char)' ';
        } else {
            if ( ! isprint((int)ch) ) {
                status = P_FILE_IS_NOT_PFSPD_FILE;
            }
        }
        val[i] = ch;
    }
    val[len] = (char)'\0';

    /* Get value. */
    if (status == P_OK) {
        *pos += len;
    }

    return status;
} /* end of p_parse_get_str () */


/* internal function to get the endian mode character from a read buffer */
static pT_status
p_parse_get_le(const char *buf,
               int        *pos,
               int        *val)
{
    const char ch = buf[*pos];

    *pos += 1;

    /*
     * There is some ancient pfspd history here. In the past,
     * this character was used to identify the machine type
     * that created the file. This is not usefull information,
     * so we proposed a different approach.
     * In an email dd. April 4, 2001 Jack Kuppers (pfspd support
     * Nat. Lab.) confirmed this approach.
     * The only interesting property of the machine architecture
     * is the endian mode. Old pfspd files are written in the
     * same endian mode as the machine architecture that wrote them.
     * Cpfspd works as follows:
     * - All old pfspd files can be read.
     *   All existing machine types are mapped to either
     *   little or big endian.
     * - Newly created files are always written as big endian "A".
     *   In case we are going to support writing little endian files
     *   in the future, then code "U" will be used.
     * - An unknown machine is interpreted as big endian.
     *   This introduces a little risk, but it is compatible with
     *   previous cpfspd releases. In the past, cpfspd did not read
     *   this character at all. Note that most pfspd files contain
     *   8 bit data where endian mode does not matter at all.
     */

    switch (ch) {
    case 'A':       /* Originated from Apollo, also used on HP and SGI */
    case 'S':       /* Originated from Sun */
        *val = 0;   /* big endian */
        break;
    case '\0':      /* Vax, no definition at that time yet... */
    case ' ':       /* Vax, no definition at that time yet... */
    case 'U':       /* Originated from Ultrix */
    case 'Q':       /* Originated from Risk-Ultrix */
        *val = 1;   /* little endian */
        break;
    default:        /* There is a little risk here, this is an unknown machine.
                       In the past, cpfspd did not read this character at all,
                       so this is compatible with previous cpfspd releases.
                       For most files, */
        *val = 0;   /* big endian */
        break;
    }

    return P_OK;
} /* end of p_parse_get_le () */


/***************************************************************
*                                                              *
*       Read header                                            *
*                                                              *
***************************************************************/
pT_status
p_read_hdr (const char *filename, pT_header *header,
            FILE *stream_error, int print_error)
{
    pT_status        status = P_OK;
    FILE            *file_ptr = NULL;
    char             buf[P_MIN_BYTES_PER_REC]; /* fread buffer;
                                       each read always fits in min. record length */
    int              pos;                     /* Current pos in buf */
    int              i;
    const int        stdio = !strcmp(filename, "-");
    fio_offset_t     new_offset;
    long             amount;

    file_ptr = p_get_file_pointer(filename, stdio, p_mode_read, (fio_offset_t)-1);

    if (file_ptr == NULL) {
        if (print_error) {
            fprintf (stream_error, "\nERROR: Unable to open file: %s\n",
                     filename);
            fprintf (stream_error, "errno: %d\n", errno);
        }
        status = P_FILE_OPEN_FAILED;
    } else {
        /* initialize all in header structure */
        memset(header, 0, sizeof(pT_header));

        /* read global header structure */
        header->offset_hi = 0ul;
        header->offset_lo = 0ul;

        /* and position pointer at beginning of file */
        new_offset = (fio_offset_t)0;
        status = p_position_pointer (file_ptr, stdio,
                                     &header->offset_hi,
                                     &header->offset_lo,
                                     new_offset, 0);

        if (status == P_OK) {
            status = p_read_data(file_ptr, stdio, buf, (size_t)P_LEN_GLOB_STR);
        }
        if (status != P_OK) {
            if (p_end_of_file(file_ptr, stdio)) {
                status = P_FILE_IS_NOT_PFSPD_FILE;
                if (print_error) {
                    fprintf (stream_error, "\nERROR: Unexpected read end-of-file\n");
                }
            } else {
                status = P_READ_FAILED;
                if (print_error) {
                    fprintf (stream_error, "\nERROR: Read failure; errno %d\n", errno);
                }
            }
        }
        /* save current file pointer */
        p_add_offset(&header->offset_hi, &header->offset_lo, (long)P_LEN_GLOB_STR);

        pos = 0;
        if (status == P_OK) {
            status = p_parse_get_int(buf, &pos, P_SNR_IMAGES, &header->nr_images);
        }
        if (status == P_OK) {
            status = p_parse_get_int(buf, &pos, P_SNR_COMPON, &header->nr_compon);
        }
        if (status == P_OK) {
            status = p_parse_get_int(buf, &pos, P_SNR_FD_RECS, &header->nr_fd_recs);
        }
        if (status == P_OK) {
            status = p_parse_get_int(buf, &pos, P_SNR_AUXDAT_RECS, &header->nr_aux_data_recs);
        }
        if (status == P_OK) {
            status = p_parse_get_str(buf, &pos, P_SAPPL_TYPE, header->appl_type);
        }
        if (status == P_OK) {
            status = p_parse_get_int(buf, &pos, P_SBYTES_REC, &header->bytes_rec);
        }
        if (status == P_OK) {
            status = p_parse_get_le(buf, &pos, &header->little_endian);
        }
        if (status == P_OK) {
            status = p_parse_get_int(buf, &pos, P_SAUX_HDR_RECS, &header->nr_aux_hdr_recs);
        }

        /* new file offset */
        new_offset = (fio_offset_t)header->bytes_rec;

        /* go to new file offset */
        if (status == P_OK) {
            status = p_position_pointer (file_ptr, stdio,
                                         &header->offset_hi,
                                         &header->offset_lo,
                                         new_offset, 0);
        }

        /* read global header attributes */
        if (status == P_OK) {
            status = p_read_data(file_ptr, stdio, buf, (size_t)P_LEN_GLOB_ATT);
            if (status != P_OK) {
                if (p_end_of_file(file_ptr, stdio)) {
                    status = P_FILE_IS_NOT_PFSPD_FILE;
                    if (print_error) {
                        fprintf (stream_error, "\nERROR: Unexpected read end-of-file\n");
                    }
                } else {
                    status = P_READ_FAILED;
                    if (print_error) {
                        fprintf (stream_error, "\nERROR: Read failure; errno %d\n", errno);
                    }
                }
            }
        }

        /* update current file pointer */
        p_add_offset(&header->offset_hi, &header->offset_lo, (long)P_LEN_GLOB_ATT);

        pos = 0;
        if (status == P_OK) {
            status = p_parse_get_float(buf, &pos, P_SIMA_FREQ, &header->ima_freq);
        }
        if (status == P_OK) {
            status = p_parse_get_float(buf, &pos, P_SLIN_FREQ, &header->lin_freq);
        }
        if (status == P_OK) {
            status = p_parse_get_float(buf, &pos, P_SPIX_FREQ, &header->pix_freq);
        }
        if (status == P_OK) {
            status = p_parse_get_int(buf, &pos, P_SACT_LINES, &header->act_lines);
        }
        if (status == P_OK) {
            status = p_parse_get_int(buf, &pos, P_SACT_PIXEL, &header->act_pixel);
        }
        if (status == P_OK) {
            status = p_parse_get_int(buf, &pos, P_SINTERLACE, &header->interlace);
        }
        if (status == P_OK) {
            status = p_parse_get_int(buf, &pos, P_SH_PP_SIZE, &header->h_pp_size);
        }
        if (status == P_OK) {
            status = p_parse_get_int(buf, &pos, P_SV_PP_SIZE, &header->v_pp_size);
        }

        /* Init description data and auxiliary header list in case this is not in the file */
        memset(header->description, 0, P_SDESCRIPTION);
        strcpy((char *)header->aux_hdrs, P_AUX_LAST);

        if ((status == P_OK) && (header->nr_fd_recs > 0)) {
            /* read file description records */

            /* new file offset */
            new_offset = (fio_offset_t)P_NUM_GLOB_RECS * header->bytes_rec;

            /* go to new file offset */
            if (status == P_OK) {
                status = p_position_pointer (file_ptr, stdio,
                                             &header->offset_hi,
                                             &header->offset_lo,
                                             new_offset, 0);
            }

            /* read file description records */
            /* in case more data in file then we can handle: truncate string */
            amount = MIN(header->bytes_rec * (header->nr_fd_recs - header->nr_aux_hdr_recs), P_SDESCRIPTION);
            if (status == P_OK) {
                status = p_read_data(file_ptr, stdio,
                                     header->description,
                                     (size_t)amount);
                header->description[P_SDESCRIPTION-1] = '\0';
            }
            if (status != P_OK) {
                if (p_end_of_file(file_ptr, stdio)) {
                    status = P_FILE_IS_NOT_PFSPD_FILE;
                    if (print_error) {
                        fprintf (stream_error, "\nERROR: Unexpected read end-of-file\n");
                    }
                } else {
                    status = P_READ_FAILED;
                    if (print_error) {
                        fprintf (stream_error, "\nERROR: Read failure; errno %d\n", errno);
                    }
                }
            }

            /* update current file pointer */
            p_add_offset(&header->offset_hi, &header->offset_lo, (long)amount);

            /* read auxiliary header records */
            if ((status == P_OK) && (header->nr_aux_hdr_recs > 0))
            {
                /* new file offset */
                new_offset = (fio_offset_t)(P_NUM_GLOB_RECS + header->nr_fd_recs - header->nr_aux_hdr_recs)
                                            * header->bytes_rec;
                /* go to new file offset */
                status = p_position_pointer (file_ptr, stdio, &header->offset_hi, &header->offset_lo, new_offset, 0);

                amount = 0; /* avoid compiler warnings */
                if (status == P_OK) {
                    /* read all records in one go, but read only as much as there is space */
                    amount = header->bytes_rec * header->nr_aux_hdr_recs;
                    if (amount > P_SAUX_HDR) {
                        status = P_EXCEEDING_AUXILIARY_HDR_SIZE;
                        if (print_error) {
                            fprintf (stream_error, "\nERROR: amount of auxiliary data in the file exceeds available memory space\n");
                        }
                    }
                }

                if (status == P_OK) {
                    status = p_read_data(file_ptr, stdio,
                            header->aux_hdrs, (size_t)amount);
                }
                if (status != P_OK) {
                    if (p_end_of_file(file_ptr, stdio)) {
                        status = P_FILE_IS_NOT_PFSPD_FILE;
                        if (print_error) {
                            fprintf (stream_error, "\nERROR: Unexpected read end-of-file\n");
                        }
                    } else {
                        status = P_READ_FAILED;
                        if (print_error) {
                            fprintf (stream_error, "\nERROR: Read failure; errno %d\n", errno);
                        }
                    }
                }
                /* update current file pointer */
                p_add_offset(&header->offset_hi, &header->offset_lo, (long)amount);
            }   /* end of if (header->nr_aux_hdr_recs > 0) */
        } /* end of if (header->nr_fd_recs > 0) */

        if ((status == P_OK) && (header->nr_compon > P_PFSPD_MAX_COMP)) {
            if (print_error) {
                fprintf (stream_error, "\nERROR: Too many components: %s\n",
                         filename);
            }
            status = P_TOO_MANY_COMPONENTS;
        } else {
            for (i = 0; (status == P_OK) && (i < header->nr_compon); i++) {

                /* new file offset */
                new_offset = (fio_offset_t)(P_NUM_GLOB_RECS +
                                            header->nr_fd_recs +
                                            P_NUM_COMP_RECS * i) * header->bytes_rec;

                /* go to new file offset */
                if (status == P_OK) {
                    status = p_position_pointer (file_ptr, stdio,
                                                 &header->offset_hi,
                                                 &header->offset_lo,
                                                 new_offset, 0);
                }

                /* read component header structure */
                if (status == P_OK) {
                    status = p_read_data(file_ptr, stdio, buf, (size_t)P_LEN_COMP_STR);
                    if (status != P_OK) {
                        if (p_end_of_file(file_ptr, stdio)) {
                            status = P_FILE_IS_NOT_PFSPD_FILE;
                            if (print_error) {
                                fprintf (stream_error, "\nERROR: Unexpected read end-of-file\n");
                            }
                        } else {
                            status = P_READ_FAILED;
                            if (print_error) {
                                fprintf (stream_error, "\nERROR: Read failure; errno %d\n", errno);
                            }
                        }
                    }
                }

                /* update current file pointer */
                p_add_offset(&header->offset_hi, &header->offset_lo, (long)P_LEN_COMP_STR);

                pos = 0;
                if (status == P_OK) {
                    status = p_parse_get_int(buf, &pos, P_SLIN_IMAGE, &header->comp[i].lin_image);
                }
                if (status == P_OK) {
                    status = p_parse_get_int(buf, &pos, P_SPIX_LINE, &header->comp[i].pix_line);
                }
                if (status == P_OK) {
                    status = p_parse_get_str(buf, &pos, P_SDATA_FMT, header->comp[i].data_fmt);
                }

                /* new file offset */
                new_offset = (fio_offset_t)(1 +
                                            P_NUM_GLOB_RECS +
                                            header->nr_fd_recs +
                                            P_NUM_COMP_RECS * i) * header->bytes_rec;

                /* go to new file offset */
                if (status == P_OK) {
                    status = p_position_pointer (file_ptr, stdio,
                                                 &header->offset_hi,
                                                 &header->offset_lo,
                                                 new_offset, 0);
                }

                /* read component header attributes */
                if (status == P_OK) {
                    status = p_read_data(file_ptr, stdio, buf, (size_t)P_LEN_COMP_ATT);
                    if (status != P_OK) {
                        if (p_end_of_file(file_ptr, stdio)) {
                            status = P_FILE_IS_NOT_PFSPD_FILE;
                            if (print_error) {
                                fprintf (stream_error, "\nERROR: Unexpected read end-of-file\n");
                            }
                        } else {
                            status = P_READ_FAILED;
                            if (print_error) {
                                fprintf (stream_error, "\nERROR: Read failure; errno %d\n", errno);
                            }
                        }
                    }
                }

                /* update current file pointer */
                p_add_offset(&header->offset_hi, &header->offset_lo, (long)P_LEN_COMP_ATT);

                pos = 0;
                if (status == P_OK) {
                    status = p_parse_get_int(buf, &pos, P_STEM_SBSMPL, &header->comp[i].tem_sbsmpl);
                }
                if (status == P_OK) {
                    status = p_parse_get_int(buf, &pos, P_SLIN_SBSMPL, &header->comp[i].lin_sbsmpl);
                }
                if (status == P_OK) {
                    status = p_parse_get_int(buf, &pos, P_SPIX_SBSMPL, &header->comp[i].pix_sbsmpl);
                }
                if (status == P_OK) {
                    status = p_parse_get_int(buf, &pos, P_STEM_PHSHFT, &header->comp[i].tem_phshft);
                }
                if (status == P_OK) {
                    status = p_parse_get_int(buf, &pos, P_SLIN_PHSHFT, &header->comp[i].lin_phshft);
                }
                if (status == P_OK) {
                    status = p_parse_get_int(buf, &pos, P_SPIX_PHSHFT, &header->comp[i].pix_phshft);
                }
                if (status == P_OK) {
                    status = p_parse_get_str(buf, &pos, P_SCOM_CODE, header->comp[i].com_code);
                }
            } /* end of for (i = 0; i < header->nr_compon; i++) { */
        } /* end of if (header->nr_compon > P_PFSPD_MAX_COMP) { */

        /* Cache header and image sizes */
        p_set_file_size_info (file_ptr, p_get_size_header(header), p_get_size_image(header), header->nr_images);
    } /* end of if (file_ptr == NULL) { */

    return status;
} /* end of p_read_hdr () */


/***************************************************************
*                                                              *
*       Write Header                                           *
*                                                              *
***************************************************************/
pT_status
p_write_hdr (const char *filename, pT_header *header,
             FILE *stream_error, int print_error, int rewrite)
{
    pT_status        status = P_OK;
    FILE            *file_ptr = NULL;
    char            *buf;                /* fwrite buffer */
    int              i;
    const int        stdio = !strcmp(filename, "-");
    fio_offset_t     new_offset;
    long             amount;

    buf = malloc ((size_t)(header->bytes_rec + 1));
    if (buf == NULL) {
        status = P_MALLOC_FAILED;
    }
    if (status == P_OK) {
        if (header->bytes_rec < P_MIN_BYTES_PER_REC) {
            if (print_error) {
                fprintf (stream_error,
                         "\nERROR: Number of bytes per record incorrect\n");
            }
            status = P_ILLEGAL_BYTES_PER_REC;
        }
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        if (stdio && rewrite) {
            if (print_error) {
                fprintf (stream_error,
                         "\nERROR: Attempting rewrite on stdout\n");
            }
            status = P_REWRITE_ON_STDOUT;
        } else {
            /* For real files, we try to allocate a continuous part of the
             * storage medium. This will give better performance later,
             * when the file will be read by an application. Furthermore
             * this assures that the whole file fits on the disk so we
             * know it makes sense to start calculation
             * (this avoids a situations that after half an hour of intense
             * calculations, the job fails due to insufficient disk space).
             */
            new_offset = (fio_offset_t) p_get_size_header( header );
            new_offset += (fio_offset_t) header->nr_images * p_get_size_image( header );
            file_ptr = p_get_file_pointer(filename, stdio, rewrite ? p_mode_update : p_mode_write, new_offset );
        }

        if (file_ptr == NULL) {
            if (rewrite) {
                if (print_error) {
                    fprintf (stream_error,
                             "\nERROR: File open for rewrite failed: %s\n", filename);
                    fprintf (stream_error, "errno: %d\n", errno);
                }
                status = P_FILE_MODIFY_FAILED;
            } else {
                if (print_error) {
                    fprintf (stream_error,
                             "\nERROR: Unable to create file: %s\n", filename);
                    fprintf (stream_error, "errno: %d\n", errno);
                }
                status = P_FILE_CREATE_FAILED;
            }
        } else {
            /* file opened; reset position */
            /* keep track of position in file */
            header->offset_hi = 0ul;
            header->offset_lo = 0ul;

            /* and position pointer at beginning of file */
            new_offset = (fio_offset_t)0;
            status = p_position_pointer (file_ptr, stdio,
                                         &header->offset_hi,
                                         &header->offset_lo,
                                         new_offset, 0);
        }
    } /* end of if (status == P_OK) */

    if (!rewrite) {
        /* File endian mode is determined by the system's endianness */
        header->little_endian = p_system_is_little_endian();
    }

    if (status == P_OK) {
        /* fill buf with global header structure */
        sprintf(buf, "%*d%*d%*d%*d%-*s%*d%*s%*d%*s",
                P_SNR_IMAGES,       header->nr_images,
                P_SNR_COMPON,       header->nr_compon,
                P_SNR_FD_RECS,      header->nr_fd_recs,
                P_SNR_AUXDAT_RECS,  header->nr_aux_data_recs,
                P_SAPPL_TYPE,       header->appl_type,
                P_SBYTES_REC,       header->bytes_rec,
                P_SENDIAN_CODE,     header->little_endian ? "U" : "A",
                P_SAUX_HDR_RECS,    header->nr_aux_hdr_recs,
                header->bytes_rec-P_LEN_GLOB_STR, " " /* space padding */);

        /* write global header structure */
        status = p_write_data(file_ptr, stdio, buf, (size_t)(header->bytes_rec));
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        /* fill buf with global attributes */
        sprintf(buf, "%*f%*f%*f%*d%*d%*d%*d%*d%*s",
                P_SIMA_FREQ,  header->ima_freq,
                P_SLIN_FREQ,  header->lin_freq,
                P_SPIX_FREQ,  header->pix_freq,
                P_SACT_LINES, header->act_lines,
                P_SACT_PIXEL, header->act_pixel,
                P_SINTERLACE, header->interlace,
                P_SH_PP_SIZE, header->h_pp_size,
                P_SV_PP_SIZE, header->v_pp_size,
                header->bytes_rec-P_LEN_GLOB_ATT, " " /* space padding */);

        /* write global attributes */
        status = p_write_data(file_ptr, stdio, buf, (size_t)(header->bytes_rec));
    } /* end of if (status == P_OK) */

    p_add_offset(&header->offset_hi, &header->offset_lo,
                 (long)P_NUM_GLOB_RECS * header->bytes_rec);

    /* write the file descriptor records */
    if ((status == P_OK) && (header->nr_fd_recs > 0)) {
        amount = MIN(header->bytes_rec * (header->nr_fd_recs - header->nr_aux_hdr_recs), P_SDESCRIPTION);
        status = p_write_data(file_ptr, stdio, header->description, (size_t)amount);
        p_add_offset(&header->offset_hi, &header->offset_lo, amount);

        /* put file pointer just after file description records */
        new_offset = (fio_offset_t)P_NUM_GLOB_RECS * header->bytes_rec +
                     (header->nr_fd_recs - header->nr_aux_hdr_recs) * header->bytes_rec;

        /* go to new file offset */
        if (status == P_OK) {
            status = p_position_pointer (file_ptr, stdio,
                                         &header->offset_hi,
                                         &header->offset_lo,
                                         new_offset, 1);
        } /* end of if (status == P_OK) */

        /* write auxiliary header records */
        if ((status == P_OK) && (header->nr_aux_hdr_recs > 0))
        {
            amount = MIN(header->bytes_rec * header->nr_aux_hdr_recs, P_SAUX_HDR);
            status = p_write_data(file_ptr, stdio, header->aux_hdrs, (size_t)amount);
            p_add_offset(&header->offset_hi, &header->offset_lo, amount);

            /* put file pointer just after file description records */
            new_offset = (fio_offset_t)P_NUM_GLOB_RECS * header->bytes_rec +
                header->nr_fd_recs * header->bytes_rec;

            /* go to new file offset */
            if (status == P_OK) {
                status = p_position_pointer (file_ptr, stdio,
                        &header->offset_hi,
                        &header->offset_lo,
                        new_offset, 1);
            } /* end of if (status == P_OK) */
        }   /* end of if (header->nr_aux_hdr_recs > 0) */
    }   /* end of if (header->nr_fd_recs > 0) */

    if (status == P_OK) {
        for (i = 0; i < header->nr_compon; i++) {
            /* fill component header structure */
            sprintf(buf, "%*d%*d%-*s%*s",
                    P_SLIN_IMAGE, header->comp[i].lin_image,
                    P_SPIX_LINE,  header->comp[i].pix_line,
                    P_SDATA_FMT,  header->comp[i].data_fmt,
                    header->bytes_rec-P_LEN_COMP_STR, " " /* space padding */);

            /* write component header structure */
            if (status == P_OK) {
                status = p_write_data(file_ptr, stdio, buf, (size_t)(header->bytes_rec));
                p_add_offset(&header->offset_hi, &header->offset_lo, header->bytes_rec);
            }

            /* fill component header attributes */
            sprintf(buf, "%*d%*d%*d%*d%*d%*d%-*s%*s",
                    P_STEM_SBSMPL, header->comp[i].tem_sbsmpl,
                    P_SLIN_SBSMPL, header->comp[i].lin_sbsmpl,
                    P_SPIX_SBSMPL, header->comp[i].pix_sbsmpl,
                    P_STEM_PHSHFT, header->comp[i].tem_phshft,
                    P_SLIN_PHSHFT, header->comp[i].lin_phshft,
                    P_SPIX_PHSHFT, header->comp[i].pix_phshft,
                    P_SCOM_CODE,   header->comp[i].com_code,
                    header->bytes_rec-P_LEN_COMP_ATT, " " /* space padding */);

            /* write component header attributes */
            if (status == P_OK) {
                status = p_write_data(file_ptr, stdio, buf, (size_t)(header->bytes_rec));
                p_add_offset(&header->offset_hi, &header->offset_lo, header->bytes_rec);
            }
        } /* end of for (i = 0; i < header->nr_compon; i++) { */
    } /* end of if (status == P_OK) */

    /* Cache header and image sizes */
    p_set_file_size_info (file_ptr, p_get_size_header(header), p_get_size_image(header), header->nr_images);

    if (buf != NULL) {
        free (buf);
    }

    return status;
} /* end of p_write_hdr () */


/***************************************************************
*                                                              *
*       Read an image                                          *
*                                                              *
***************************************************************/
pT_status
p_read_image (const char *filename, pT_header *header,
              int nr, int comp_nr,
              void *mem_buffer,
              int mem_type,     /* unsigned char = 8, unsigned short = 16 */
              int mem_data_fmt, /* see description in cpfspd.h            */
              int width,
              int height,
              int stride,
              FILE *stream_error, int print_error)
{
    pT_status     status = P_OK;
    fio_offset_t  offset;
    int           i;
    const int     stdio = !strcmp(filename, "-");
    FILE         *file_ptr = NULL;
    const int     local_width  = MIN(width, header->comp[comp_nr].pix_line);
    const int     local_height = MIN(height, header->comp[comp_nr].lin_image);
    pT_data_fmt   file_data_fmt = P_UNKNOWN_DATA_FORMAT;
    int           file_no_bits;        /* no of bits per element in file     */
    int           mem_no_bits;         /* no of bits per element in memory   */
    int           file_type;           /* unsigned char=8, unsigned short=16 */
    void         *file_buffer = NULL;  /* pointer to file buffer             */
    size_t        file_el_size = 0ul;  /* size of one element in file buffer */
    unsigned int  sample;
    int           shift_left_factor = 0;
    int           shift_right_factor = 0;
    unsigned int  pre_mask  = 0u;
    unsigned int  post_mask = 0u;
    int           x;
    int           y;
    int           file_buffer_allocated = 0;
    int           skip_conversion = 0;

    /* determine file data format of this component  */
    file_data_fmt = p_get_comp_data_format (header, comp_nr);

    /* determine word widths */
    status = p_get_word_widths (file_data_fmt, mem_data_fmt,
                                &file_no_bits, &mem_no_bits,
                                &file_type);

    if (status == P_OK) {
        /* determine size of one element in file buffer */
        status = p_get_element_size (file_type, &file_el_size);

        /* determine shift_left/right_factor, pre_mask & post_mask */
        shift_left_factor  = mem_no_bits - file_no_bits;
        shift_right_factor = -shift_left_factor;
        shift_left_factor  = MAX(0, shift_left_factor);
        shift_right_factor = MAX(0, shift_right_factor);
        switch (file_data_fmt) {
        case P_8_BIT_FILE:
            pre_mask = 0x00ffu;
            break;
        case P_10_BIT_FILE:
            pre_mask = 0x03ffu;
            break;
        case P_12_BIT_FILE:
            pre_mask = 0x0fffu;
            break;
        case P_14_BIT_FILE:
            pre_mask = 0x3fffu;
            break;
        case P_16_REAL_FILE: /* FALLTHROUGH */
        case P_16_BIT_FILE:
            pre_mask = 0xffffu;
            break;
        default:
            status = P_ILLEGAL_FILE_DATA_FORMAT;
            pre_mask = 0x0000u;
            break;
        } /* end of switch (file_data_fmt) */
        if (mem_data_fmt == P_16_BIT_MEM_LSB) {
            post_mask = 0x00ffu; /* 8 bit mask  */
        } else {
            post_mask = 0xffffu; /* 16 bit mask */
        } /* end of if (mem_data_fmt == P_16_BIT_LSB_MEM) */

        /* parameter check */
        if ((mem_type != P_UNSIGNED_CHAR) &&
            (mem_type != P_UNSIGNED_SHORT)) {
            status = P_UNKNOWN_MEM_TYPE;
        }
        if ((mem_type == P_UNSIGNED_CHAR) &&
            (mem_data_fmt == P_AF_BIT_MEM)) {
            status = P_ILLEGAL_MEM_DATA_FORMAT;
        }

        /* is fast file reading possible ? */
        if ((mem_type == file_type) &&
            (mem_no_bits == file_no_bits) &&
            (   (mem_no_bits == 8) ||
                (p_system_is_little_endian() == header->little_endian) )) {
            skip_conversion = 1;
        }

        if (skip_conversion) {
            /* to avoid memcpy, we assign mem_buffer to file_buffer */
            file_buffer = (void *) mem_buffer;
        } else {
            if (status == P_OK) {
                /* allocate file buffer (one line) */
                file_buffer = malloc ((size_t)local_width * file_el_size);
                if (file_buffer == NULL) {
                    status = P_MALLOC_FAILED;
                } /* end of if (local_buffer == NULL) */
                file_buffer_allocated = 1;
            }  /* end of if (status == P_OK) */
        }
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        file_ptr = p_get_file_pointer(filename, stdio, p_mode_read, (fio_offset_t)-1);

        if (file_ptr == NULL) {
            if (print_error) {
                fprintf (stream_error, "\nERROR: Unable to open file: %s\n",
                         filename);
                fprintf (stream_error, "errno: %d\n", errno);
            }
            status = P_FILE_OPEN_FAILED;
        } else {
            /* skip header and previous images */
            offset  = p_get_size_header(header);
            offset += (nr - 1) * (fio_offset_t)p_get_size_image(header);
            /* skip current image aux data */
            offset += header->nr_aux_data_recs * header->bytes_rec;
            /* skip previous components of current image */
            for (i = 0; i < comp_nr; i++) {
                offset += p_get_size_comp (header->comp[i].pix_line,
                                           header->comp[i].lin_image,
                                           header->comp[i].data_fmt);
            }

            /* go to new file offset */
            if (status == P_OK) {
                status = p_position_pointer (file_ptr, stdio,
                                             &header->offset_hi,
                                             &header->offset_lo,
                                             offset, 0);
            } /* end of if (status == P_OK) */

            for (y = 0; y < local_height; y++) {
                if (status == P_OK) {
                    status = p_read_data(file_ptr, stdio, file_buffer, (size_t)local_width * file_el_size);
                }

                /* update current file pointer */
                p_add_offset(&header->offset_hi,
                             &header->offset_lo,
                             (long)((size_t)local_width * file_el_size));

                /* new offset */
                offset += header->comp[comp_nr].pix_line * (int)file_el_size;
                /* go to new file offset */
                if (status == P_OK) {
                    status = p_position_pointer (file_ptr, stdio,
                                                 &header->offset_hi,
                                                 &header->offset_lo,
                                                 offset, 0);
                } /* end of if (status == P_OK) */

                /* copy file_buffer to mem_buffer */
                if (!skip_conversion) {
                    /* convert file buffer types into samples
                     * and store in mem buffers */

                    /* Note, there is a little bit duplicated code
                       here for performance optimization. No switch/if
                       statements in the inner loop. */

                    switch (file_type) {
                    case P_UNSIGNED_CHAR:
                        if (mem_type == P_UNSIGNED_CHAR) {
                            /* we normally shouldn't get here
                             * no need to assert, just a bit slower */
                            for (x = 0; x < local_width; x++) {
                                ((unsigned char*)mem_buffer)[x] = ((unsigned char*)file_buffer)[x];
                            }
                        } else {
                            for (x = 0; x < local_width; x++) {
                                sample = (unsigned int) ((unsigned char*)file_buffer)[x];
                                sample &= pre_mask;
                                sample >>= shift_right_factor;  /* either shift left or */
                                sample <<= shift_left_factor;   /* shift right is zero */
                                sample &= post_mask;
                                ((unsigned short*)mem_buffer)[x] = (unsigned short) sample;
                            }
                        }
                        break;
                    case P_UNSIGNED_SHORT:
                        if (header->little_endian) {
                            /* little endian file format */
                            if (mem_type == P_UNSIGNED_CHAR) {
                                for (x = 0; x < local_width; x++) {
                                    sample = ((unsigned int) ((unsigned char*)file_buffer)[2*x]) +
                                             ((unsigned int) ((unsigned char*)file_buffer)[2*x+1] << 8);
                                    sample &= pre_mask;
                                    sample >>= shift_right_factor;  /* either shift left or */
                                    sample <<= shift_left_factor;   /* shift right is zero */
                                    sample &= post_mask;
                                    ((unsigned char*)mem_buffer)[x] = (unsigned char) sample;
                                }
                            } else {
                                for (x = 0; x < local_width; x++) {
                                    sample = ((unsigned int) ((unsigned char*)file_buffer)[2*x]) +
                                             ((unsigned int) ((unsigned char*)file_buffer)[2*x+1] << 8);
                                    sample &= pre_mask;
                                    sample >>= shift_right_factor;  /* either shift left or */
                                    sample <<= shift_left_factor;   /* shift right is zero */
                                    sample &= post_mask;
                                    ((unsigned short*)mem_buffer)[x] = (unsigned short) sample;
                                }
                            }
                        } else {
                            /* big endian file format */
                            if (mem_type == P_UNSIGNED_CHAR) {
                                for (x = 0; x < local_width; x++) {
                                    sample = ((unsigned int) ((unsigned char*)file_buffer)[2*x] << 8) +
                                             ((unsigned int) ((unsigned char*)file_buffer)[2*x+1]);
                                    sample &= pre_mask;
                                    sample >>= shift_right_factor;  /* either shift left or */
                                    sample <<= shift_left_factor;   /* shift right is zero */
                                    sample &= post_mask;
                                    ((unsigned char*)mem_buffer)[x] = (unsigned char) sample;
                                }
                            } else {
                                for (x = 0; x < local_width; x++) {
                                    sample = ((unsigned int) ((unsigned char*)file_buffer)[2*x] << 8) +
                                             ((unsigned int) ((unsigned char*)file_buffer)[2*x+1]);
                                    sample &= pre_mask;
                                    sample >>= shift_right_factor;  /* either shift left or */
                                    sample <<= shift_left_factor;   /* shift right is zero */
                                    sample &= post_mask;
                                    ((unsigned short*)mem_buffer)[x] = (unsigned short) sample;
                                }
                            }
                        }
                        break;
                    default:
                        status = P_UNKNOWN_FILE_TYPE;
                        sample = 0u;
                        break;
                    } /* end of switch (file_type) */
                } /* end of  if (!skip_conversion) */

                /* advance pointer to buffer */
                switch (mem_type) {
                case P_UNSIGNED_CHAR:
                    mem_buffer = (void*)((unsigned char*)mem_buffer + stride);
                    if (skip_conversion) {
                        file_buffer = (void *) mem_buffer;
                    }
                    break;
                case P_UNSIGNED_SHORT:
                    mem_buffer = (void*)((unsigned short*)mem_buffer + stride);
                    if (skip_conversion) {
                        file_buffer = (void *) mem_buffer;
                    }
                    break;
                default:
                    status = P_UNKNOWN_MEM_TYPE;
                    break;
                } /* end of switch (mem_type) */
            } /* end of for (y = 0;... */
        } /* end of if (file_ptr == NULL) { */
    } /* end of if (status == P_OK) */

    if (file_buffer_allocated && ((file_buffer != NULL))) {
        free (file_buffer);
    } /* end of if (local_buffer != NULL) */

    return status;
} /* end of p_read_image () */


/***************************************************************
*                                                              *
*       Write an image                                         *
*                                                              *
***************************************************************/
pT_status
p_write_image (const char *filename, pT_header *header,
               int nr, int comp_nr,
               const void *mem_buffer,
               int mem_type,     /* unsigned char = 8, unsigned short = 16 */
               int mem_data_fmt, /* see description in cpfspd.h            */
               int width,
               int height,
               int stride,
               FILE *stream_error, int print_error)
{
    pT_status     status = P_OK;
    fio_offset_t  offset;
    int           i;
    const int     stdio = !strcmp(filename, "-");
    FILE         *file_ptr = NULL;
    const int     local_width  = MIN(width, header->comp[comp_nr].pix_line);
    const int     local_height = MIN(height, header->comp[comp_nr].lin_image);
    pT_data_fmt   file_data_fmt = P_UNKNOWN_DATA_FORMAT;
    int           file_no_bits;        /* no of bits per element in file     */
    int           mem_no_bits;         /* no of bits per element in memory   */
    int           file_type;           /* unsigned char=8, unsigned short=16 */
    void         *file_buffer = NULL;  /* pointer to file buffer             */
    void         *temp_conversion_buffer = NULL; /* pointer to temp conversion buffer from file_buffer */
    size_t        file_el_size = 0ul;  /* size of one element in file buffer */
    unsigned int  sample;
    int           shift_left_factor = 0;
    int           shift_right_factor = 0;
    unsigned int  mask  = 0u;
    int           x;
    int           y;
    int           file_buffer_allocated = 0;
    int           skip_conversion = 0;
    size_t		  comp_size = 0;	/* total bytes write */
    int			  file_stride = 0;

    /* determine file data format of this component  */
    file_data_fmt = p_get_comp_data_format (header, comp_nr);

    /* determine word widths */
    status = p_get_word_widths (file_data_fmt, mem_data_fmt,
                                &file_no_bits, &mem_no_bits,
                                &file_type);

    if (status == P_OK) {
        /* determine size of one element in file buffer */
        status = p_get_element_size (file_type, &file_el_size);

        /* determine shift_left/right_factor, pre_mask & post_mask*/
        shift_left_factor = file_no_bits - mem_no_bits;
        shift_right_factor = -shift_left_factor;
        shift_left_factor  = MAX(0, shift_left_factor);
        shift_right_factor = MAX(0, shift_right_factor);
        switch (mem_data_fmt) {
        case P_8_BIT_MEM:
            mask = 0x00ffu;
            break;
        case P_10_BIT_MEM:
            mask = 0x03ffu;
            break;
        case P_12_BIT_MEM:
            mask = 0x0fffu;
            break;
        case P_14_BIT_MEM:
            mask = 0x3fffu;
            break;
        case P_16_BIT_MEM:
            mask = 0xffffu;
            break;
        default:
            status = P_ILLEGAL_MEM_DATA_FORMAT;
            mask = 0x0000u;
            break;
        } /* end of switch (file_data_fmt) */

        /* is fast file writing possible ? */
        if ((mem_type == file_type) &&
            (mem_no_bits == file_no_bits) &&
            (   (mem_no_bits == 8) ||
                (p_system_is_little_endian() == header->little_endian) )) {
            skip_conversion = 1;
        }

		comp_size = p_get_size_comp (header->comp[comp_nr].pix_line,
                                           header->comp[comp_nr].lin_image,
                                           header->comp[comp_nr].data_fmt);

        /* copy mem_buffer to file_buffer */
        if (skip_conversion) {
            /* to avoid memcpy, we assign mem_buffer to file_buffer */
            file_buffer = (void *)((unsigned char*)mem_buffer);
        } else {
            if (status == P_OK) {
                /* allocate file buffer (one component) */
                file_buffer = malloc (comp_size);
                if (file_buffer == NULL) {
                    status = P_MALLOC_FAILED;
                } /* end of if (local_buffer == NULL) */
                file_buffer_allocated = 1;
                temp_conversion_buffer = file_buffer;
                file_stride = local_width * file_el_size;
            } /* end of if (status == P_OK) */
        }
    } /* end of if (status == P_OK) */

    if (status == P_OK) {
        file_ptr = p_get_file_pointer(filename, stdio, p_mode_update, (fio_offset_t)-1);

        if (file_ptr == NULL) {
            if (print_error) {
                fprintf (stream_error, "\nERROR: Unable to modify file: %s\n",
                         filename);
                fprintf (stream_error, "errno: %d\n", errno);
            }
            status = P_FILE_MODIFY_FAILED;
        } else {
            /* skip header and previous images */
            offset  = p_get_size_header(header);
            offset += (nr - 1) * (fio_offset_t)p_get_size_image(header);
            /* skip current image aux data */
            offset += header->nr_aux_data_recs * header->bytes_rec;
            /* skip previous components of current image */
            for (i = 0; i < comp_nr; i++) {
                offset += p_get_size_comp (header->comp[i].pix_line,
                                           header->comp[i].lin_image,
                                           header->comp[i].data_fmt);
            }


            /* go to new file offset */
            if (status == P_OK) {
                status = p_position_pointer (file_ptr, stdio,
                                             &header->offset_hi,
                                             &header->offset_lo,
                                             offset, 1);
            }


            for (y = 0; y < local_height; y++) {

                if (!skip_conversion) {
                    /* convert memory buffer types to file buffer types */

                    /* Note, there is a little bit duplicated code
                       here for performance optimization. No switch/if
                       statements in the inner loop. */

                    switch (file_type) {
                    case P_UNSIGNED_CHAR:
                        if (mem_type == P_UNSIGNED_CHAR) {
                            /* we normally shouldn't get here
                             * no need to assert, just a bit slower */
                            for (x = 0; x < local_width; x++) {
                                ((unsigned char*)temp_conversion_buffer)[x] = ((unsigned char*)mem_buffer)[x];
                            }
                        } else {
                            for (x = 0; x < local_width; x++) {
                                sample = (unsigned int) ((unsigned short*)mem_buffer)[x];
                                sample &= mask;
                                sample >>= shift_right_factor;  /* either shift left or */
                                sample <<= shift_left_factor;   /* shift right is zero */
                                ((unsigned char*)temp_conversion_buffer)[x] = (unsigned char) sample;
                            }
                        }
                        break;
                    case P_UNSIGNED_SHORT:
                        /* file endianness is forced equal to system endianness
                         * (header->little_endian was set in p_write_hdr() )*/
                        if (header->little_endian) {
                            if (mem_type == P_UNSIGNED_CHAR) {
                                for (x = 0; x < local_width; x++) {
                                    sample = (unsigned int) ((unsigned char*)mem_buffer)[x];
                                    sample &= mask;
                                    sample >>= shift_right_factor;  /* either shift left or */
                                    sample <<= shift_left_factor;   /* shift right is zero */
                                    ((unsigned char*)temp_conversion_buffer)[2*x] = (unsigned char) sample;
                                    ((unsigned char*)temp_conversion_buffer)[2*x+1] = (unsigned char) (sample >> 8);
                                }
                            } else {
                                for (x = 0; x < local_width; x++) {
                                    sample = (unsigned int) ((unsigned short*)mem_buffer)[x];
                                    sample &= mask;
                                    sample >>= shift_right_factor;  /* either shift left or */
                                    sample <<= shift_left_factor;   /* shift right is zero */
                                    ((unsigned char*)temp_conversion_buffer)[2*x] = (unsigned char) sample;
                                    ((unsigned char*)temp_conversion_buffer)[2*x+1] = (unsigned char) (sample >> 8);
                                }
                            }
                        } else {    /* big endian */
                            if (mem_type == P_UNSIGNED_CHAR) {
                                for (x = 0; x < local_width; x++) {
                                    sample = (unsigned int) ((unsigned char*)mem_buffer)[x];
                                    sample &= mask;
                                    sample >>= shift_right_factor;  /* either shift left or */
                                    sample <<= shift_left_factor;   /* shift right is zero */
                                    ((unsigned char*)temp_conversion_buffer)[2*x] = (unsigned char) (sample >> 8);
                                    ((unsigned char*)temp_conversion_buffer)[2*x+1] = (unsigned char) sample;
                                }
                            } else {
                                for (x = 0; x < local_width; x++) {
                                    sample = (unsigned int) ((unsigned short*)mem_buffer)[x];
                                    sample &= mask;
                                    sample >>= shift_right_factor;  /* either shift left or */
                                    sample <<= shift_left_factor;   /* shift right is zero */
                                    ((unsigned char*)temp_conversion_buffer)[2*x] = (unsigned char) (sample >> 8);
                                    ((unsigned char*)temp_conversion_buffer)[2*x+1] = (unsigned char) sample;
                                }
                            }
                        }
                        break;
                    default:
                        status = P_UNKNOWN_FILE_TYPE;
                        break;
                    } /* end of switch (file_type) */
                } /* end of  if (!skip_conversion) */

                //Update temp_conversion_buffer position
                temp_conversion_buffer = (void*)((unsigned char*)temp_conversion_buffer + file_stride);
                /* advance pointer to buffer */
                switch (mem_type) {
                case P_UNSIGNED_CHAR:
                    mem_buffer = (void*)((unsigned char*)mem_buffer + stride);
                    break;
                case P_UNSIGNED_SHORT:
                    mem_buffer = (void*)((unsigned short*)mem_buffer + stride);
                    break;
                default:
                    status = P_UNKNOWN_MEM_TYPE;
                    break;
                } /* end of switch (mem_type) */

            } /* end of for (y = 0;... */


			if (status == P_OK) {
                status = p_write_data(file_ptr, stdio, file_buffer, comp_size);
            }

            /* update current file pointer */
            p_add_offset(&header->offset_hi,
                         &header->offset_lo,
                         comp_size);


            /* Keep track of actual amount of images written to disk,
             * so actual amount of images can be written in header when we close the file.
             */
            p_set_file_length (file_ptr, nr);
        } /* end of if (file_ptr == NULL) { */
    } /* end of if (status == P_OK) */

    if (file_buffer_allocated && (file_buffer != NULL)) {
        free (file_buffer);
    } /* end of if (local_buffer != NULL) */

    return status;
} /* end of p_write_image () */


pT_status
p_read_aux_data (
        const char    * filename,
        pT_header     * header,
        int             image_no,
        int             data_offset,
        int           * size,
        unsigned char * buf )
{
    pT_status       status = P_OK;
    FILE *          file_ptr;
    char            temp[P_SDATA_LEN+1];
    int             pos = 0;
    fio_offset_t    offset;
    const int       stdio = !strcmp(filename, "-");

    *size = 0;
    file_ptr = p_get_file_pointer(filename, stdio, p_mode_read, (fio_offset_t)-1);
    if (file_ptr == NULL) {
        status = P_FILE_OPEN_FAILED;
    }
    if (status == P_OK) {
        /* skip header and previous images */
        offset  = p_get_size_header(header);
        offset += (fio_offset_t)(image_no - 1) * p_get_size_image(header);
        /* skip previous auxiliary data (within this set of records) */
        offset += data_offset;

        /* go to new file offset */
        status = p_position_pointer (file_ptr, stdio,
                &header->offset_hi, &header->offset_lo,
                offset, 0);

        if (status == P_OK) {
            status = p_read_data(file_ptr, stdio, temp, P_SDATA_LEN);
            p_add_offset(&header->offset_hi, &header->offset_lo, P_SDATA_LEN);
        }
        if (status == P_OK) {
            status = p_parse_get_int(temp, &pos, P_SDATA_LEN, size );
        }
        if ((status == P_OK) && (*size > 0)) {
            status = p_read_data(file_ptr, stdio, buf, *size);
            p_add_offset(&header->offset_hi, &header->offset_lo, *size);
        }
    } /* end of if (status == P_OK) */
    return status;
} /* end of p_read_aux_data */


pT_status
p_write_aux_data (
        const char    * filename,
        pT_header     * header,
        int             image_no,
        int             data_offset,
        int             size,
        unsigned char * buf )
{
    pT_status       status = P_OK;
    FILE *          file_ptr;
    char            temp[P_SDATA_LEN+1];
    fio_offset_t    offset;
    const int       stdio = !strcmp(filename, "-");

    file_ptr = p_get_file_pointer(filename, stdio, p_mode_update, (fio_offset_t)-1);
    if (file_ptr == NULL) {
        status = P_FILE_OPEN_FAILED;
    }
    if (status == P_OK) {
        /* skip header and previous images */
        offset  = p_get_size_header(header);
        offset += (fio_offset_t)(image_no - 1) * p_get_size_image(header);
        /* skip previous auxiliary data (within this set of records) */
        offset += data_offset;

        /* go to new file offset */
        status = p_position_pointer (file_ptr, stdio,
                &header->offset_hi, &header->offset_lo,
                offset, 1);

        if ((status == P_OK) && (size > 0)) {
            sprintf( temp, "%*d", P_SDATA_LEN, size );
            status = p_write_data(file_ptr, stdio, temp, P_SDATA_LEN);
            p_add_offset(&header->offset_hi, &header->offset_lo, P_SDATA_LEN);
        }
        if ((status == P_OK) && (size > 0)) {
            status = p_write_data(file_ptr, stdio, buf, size);
            p_add_offset(&header->offset_hi, &header->offset_lo, size);
        }
    } /* end of if (status == P_OK) */
    return status;
} /* end of p_write_aux_data */
