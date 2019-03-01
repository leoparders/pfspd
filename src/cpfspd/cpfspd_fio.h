/*
 *  Copyright (c) KONINKLIJKE PHILIPS ELECTRONICS N.V. 2000-2010
 *  All rights reserved.
 *  For licensing and warranty information, see the file COPYING.LGPLv21.txt
 *  in the root directory of this package.
 *
 *  Philips Research Laboratories
 *  Eindhoven, The Netherlands
 *
 *  CVS id      :  $Id: cpfspd_fio.h,v 2.19 2010/01/05 14:18:32 riemens Exp $
 *
 *  Author      :  Bram Riemens
 *
 *  Description :  File I/O functions that may allow files of >2 Gbyte size.
 *                 All functions behave exactly as their counterpart in
 *                 the standard C library. Only side effects are not
 *                 defined, e.g. errno.
 *
 *  Comment     :  Conditional compilation to handle platform specific behavior.
 *                 See fio.c file.
 *
 *                 This file defines FIO_LARGE_FILE_SUPPORTED which may
 *                 be used by the application for conditional compilation
 *                 e.g. to handle large offset values.
 */

#define FIO_LARGE_FILE_SUPPORTED 1
#ifndef FIO_OFFSET_T
    #define FIO_OFFSET_T long long
#endif

typedef FIO_OFFSET_T fio_offset_t;


extern FILE *p_fio_fopen(const char *filename, const char *mode, fio_offset_t size );

extern int p_fio_fclose(FILE *stream);

extern size_t p_fio_fread(void *ptr, size_t size, size_t nobj, FILE *stream);

extern size_t p_fio_fwrite(void *ptr, size_t size, size_t nobj, FILE *stream);

extern int p_fio_fseek(FILE *stream, fio_offset_t offset, int origin);

extern int p_fio_feof(FILE *stream);

extern int p_fio_bufsize(FILE *stream, size_t size);

extern int p_fio_set_end_of_file(const char* filename, fio_offset_t offset);
