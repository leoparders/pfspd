/*
 *  Copyright (c) KONINKLIJKE PHILIPS ELECTRONICS N.V. 2000-2015
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
 *  CVS id      :  $Id: cpfspd_fio.c,v 2.50 2015/10/02 12:50:01 vleutenr Exp $
 *
 *  Author      :  Bram Riemens
 *
 *  Description :  File I/O functions that may allow files of >2 Gbyte size.
 *                 depending on the platform (i.e. the OS support for large files)
 *                 and the file system (not all file systems support large files,
 *                 even if the OS does). All functions in this module behave exactly
 *                 as their counterpart in the standard C library. Only side effects
 *                 are not defined, e.g. errno.
 *
 *  Comment     :  This implements large files on all systems, and
 *                 performance optimized disk access on windows/PC systems.
 *
 *  The ANSI-C library uses a "long" type as offset parameter for fseek().
 *  In all practical implementations, this is a 32 bit integer.
 *  This restricts file size to 2 Gbyte. Also for other reasons,
 *  normal ANSI-C compilation typically does not allow larger files.
 *
 *  Generic information on issues related to large files and the approach the
 *  UNIX communite has taken, see the "Large File Summit" definitions:
 *  http://ftp.sas.com/standards/large.file/
 *  See also platform specific manual pages.
 *
 *  For cygwin, the implementation of large files is already for a year on the
 *  todo list (at least at Feb. 9, 2002). We figured out that we better implement
 *  the small subset of file access functions that we need ourselves based on
 *  the win32 api. Online documentation is available at:
 *  http://www.microsoft.com/
 *    MSDN Home >  MSDN Library > 
 *      Windows Development
 *        Windows Base Services
 *          Files and I/O
 *            SDK Documentation
 *              File Storage
 *                File I/O
 *                  File I/O Reference
 *                    File I/O Functions
 *
 *  Apart from large file support, the win32 implementation of file I/O
 *  is performance optimized. This allows real-time video in "standard"
 *  PC hardware, provided that the harddisk system has sufficient performance.
 *  We have taken a couple of measures to increase file access performance:
 *  - Unbuffered I/O
 *    We have applied unbuffered file I/O and handle the buffer management
 *    ourselves.
 *  - Asynchronous I/O
 *    An I/O operation is executed on one buffer while the application
 *    processes the data in an other buffer. This does work fine for
 *    data reading; data writing however is synchronous in case the
 *    file size is increased by the write operation.
 *    See the article of Microsoft:
 *      http://support.microsoft.com/default.aspx?scid=kb;EN-US;q156932
 *    titled:
 *      Asynchronous Disk I/O on Windows NT, Windows 2000, and Windows XP
 *      Appears as Synchronous (Q156932)
 *    Note that asynchronous I/O is not supported on windows95, windows98
 *    and windowsME. The OS version is detected and asynchronous I/O
 *    is only used on systems that support this.
 *  Other performance improvement measures are taken in module cpfspd_low.c.
 *  These include to keep the file open as long as possible
 *  and to write the length of the file only once when the file
 *  is closed.
 *
 *  For these reasons, we have defined our own FILE structure, called
 *  FIO_FILE internally in this module. The external interface uses a
 *  standard FILE pointer, so application modules to not "see" this system
 *  dependent I/O implementation.
 *
 *  On a PC with a fast file system (three scsi disks with software
 *  striping), we have measured the following performance improvements:
 *           unbuffered   asynchronous     total
 *    write     2.8x           1.0x         2.8x
 *    read      4.0x           1.3x         5.2x
 *
 *  Conditional compilation to handle platform specific behavior is used.
 *  If nothing is defined, each function is simply implemented
 *  by calling the standard C library; hence no large file support.
 *
 *  The following define's can be used.
 *      FIO_WIN32_FILE   Selects between Windows32 native file system
 *                       and UNIX/POSIX file systems.
 *
 *  If FIO_WIN32_FILE is set, the following applies:
 *      FIO_CYGWIN       If defined calls the cygwin function to perform
 *                       the posix to windows path name conversion.
 *
 *  If FIO_WIN32_FILE is not set, the following applies:
 *      Following macros (if defined) specify which function to call.
 *      Macro                                    ANSI-C default if not defined
 *      FIO_FUNC_FOPEN(filename,mode)            fopen(filename,mode)
 *      FIO_FUNC_FCLOSE(stream)                  fclose(stream)
 *      FIO_FUNC_FREAD(ptr,size,nobj,stream)     fread(ptr,size,nobj,stream)
 *      FIO_FUNC_FWRITE(ptr,size,nobj,stream)    fwrite(ptr,size,nobj,stream)
 *      FIO_FUNC_FSEEK(stream,offset,origin)     fseek(stream,offset,origin)
 *      FIO_FUNC_FEOF(stream)                    feof(stream)
 *      The type to use as file offset parameter.
 *      FIO_OFFSET_T     Type to use for offset parameter of fio_fseek() and
 *                       the underlaying actual fseek function. Take care of your
 *                       calculations, this may be 32 bits or 64 bits.
 *                       If not defined, the ANSI-C type "long" is used (see fseek())
 *
 *  Warning:
 *      The functions in this module shall not be mixed with other ANSI-C file I/O
 *      functions to access a single file and/or stream. In particular, this implies
 *      that these functions cannot be used with the standard I/O streams stdin,
 *      stdout and stderr.
 *
 *  Notes:
 *      Binary file support
 *      Only binary files are supported, no text files. So, fopen with mode "r" does
 *      not work. Always use "rb" instead for this module. Unix systems do not 
 *      distinguish text and binary files. On the PC, a text file is treated
 *      differently: a \n is written as cr/nl.
 *
 *      Bug #1506617 workaround
 *      A write operation on a network file may fail with a timeout error
 *      (ERROR_SEM_TIMEOUT). This may happen in two cases: a file is allocated,
 *      but a large part of the file is not yet written. When writing an image
 *      at the end, the non-written file space is zero-filled.
 *      An other case is a file that is not yet allocated. An image
 *      is written far beyond the current end of file. In this case,
 *      the file space is allocated and the unwritten file space is also
 *      zero-filled. This zero-filling may take considerable amount
 *      of time (depending on network bandwidth etc. on a local disk,
 *      5 Gbyte takes approx. 1:30 min).
 *
 *      The issue applies to network file access (e.g. accessing a path
 *      like //pc67243147/share/large.yuv, even though this may
 *      refer to a local disk).
 *
 *      Note also that the NTFS file system by default performs zero-filling
 *      on unwritten parts of the file. So a fseek forward followed by a
 *      fwrite causes the 'hole' to be filled with zero's.
 *      For sequential access, this is no issue since zero-filling
 *      is not required. Hence, high performance applications
 *      are not affected.
 *
 *      The workaround involves catching the error code and
 *      reissuing the failing operation, involving:
 *      the steps of: close file, reopen file, write data.
 *      In function fio_fp_write_buf() for synchronous operation
 *      and in function fio_fp_wait() for asynchronous operation.
 *
 *      Some background information on zero-filling is in the sideline
 *      of this paper:
 *      http://research.microsoft.com/BARC/Sequential_IO/seqio.doc
 *      Section 5.4 explains allocate/extend behaviour:
 *          When a file is being extended (new space allocated at the end),
 *          NT forces synchronous write behavior to prevent requests from
 *          arriving at the disk out-of-order. C2 security mandates that the
 *          value zero be returned to a reader of any byte which is allocated
 *          in a file but has not been previously written. The file system must
 *          balance performance against the need to prevent programs from
 *          allocating files and then reading data from files deallocated
 *          by other users.  The extra allocate/extend writes dramatically
 *          slow file write performance.
 *      In extrapolation from this explanation, it seems viable that
 *      asynchronous network operations are more sensitive to out-of-order
 *      disk behaviour. Appearently, this related to the observed zero-filling
 *      behaviour.
 */

/* Standard include files. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Aplication include files. */
#include "cpfspd_fio.h"


#ifdef FIO_WIN32_FILE

/* Platform specific include files. */
#define WIN32_LEAN_AND_MEAN 1 /* Include only what we need */
#define _WIN32_WINNT 0x0501   /* Windows Server 2003, Windows XP - required for SetFileValidData() */
#include <windows.h>          /* WIN32 API for Windows OS file access functions */

/*
 * Enable use of OpenProcessToken(), LookupPrivilegeValue() and AdjustTokenPrivileges()
 * (this resolves link errors for these functions)
 */
#if __MSVC__
#pragma comment(lib, "advapi32.lib")
#endif

#ifdef FIO_CYGWIN
/*
 * Following definition is in <sys/cygwin.h>
 * However, including that file causes a compilation warning:
 *     In file included from fio.c:97:
 *     /usr/include/sys/cygwin.h:98: warning: ANSI C restricts enumerator
 *     values to range of `int'
 * This is caused by a cygwin definition: PID_EXITED = 0x80000000
 * The gcc compiler is right, this does not fit in signed int range...
 * Since we don't like these warnings, and we only need this single function,
 * lets just declare it here.
 */
extern int cygwin_conv_to_full_win32_path (const char *, char *);
#endif  /* NOT FIO_CYGWIN */

/*
 * Code for fast memcpy and the cpu test function to detect
 * whether the current cpu supports this feature.
 * Note: this contains assembly code.
 *
 * This code is inspired by contributions from Edwin van Eggelen and Marco Bosma.
 */
#define __SSE__ 1
#define __MMX__ 1
#include <xmmintrin.h> /* SSE - <old comment>: should be included before malloc.h! */
#include <memory.h>

#define MIN(a,b) ((a)>(b) ? (b) : (a))
#define TIMEOUT_REPEAT 2000

#ifndef CACHELINE
#define CACHELINE 64
#endif

#ifndef MAXCACHESIZE
#define MAXCACHESIZE 1024*256
#endif

#define SHIFT_MMX_SUPPORT    (0)
#define SHIFT_SSE_SUPPORT    (1)
#define SHIFT_SSE2_SUPPORT   (2)
#define SHIFT_SSE3_SUPPORT   (3)

#define MMX_SUPPORT  (1 << SHIFT_MMX_SUPPORT )
#define SSE_SUPPORT  (1 << SHIFT_SSE_SUPPORT )
#define SSE2_SUPPORT (1 << SHIFT_SSE2_SUPPORT)
#define SSE3_SUPPORT (1 << SHIFT_SSE3_SUPPORT)

#ifdef __GNUC__
#define CPUID(cmd,reg,res) __asm__ __volatile__ ("movl %1,%%eax\n\tcpuid\n\tmovl %%" #reg ", %0" : "=r"(res) : "r"(cmd) : "%eax","%ebx","%ecx","%edx")
#define TOUCH(inp)         __asm__ __volatile__ ("movl %0,%%edx\n\tmovl (%%edx),%%edx" : : "r"(inp) : "%edx")
#elif __MSVC__
#define CPUID(cmd,reg,res) __asm mov eax, cmd __asm cpuid __asm mov res, reg
#define TOUCH(inp)         __asm mov edx, inp __asm mov edx, [edx]
#else
#error "Unknown compiler - do not know how to insert assembly code"
#endif


/*
 * Beware, the code below checks the CPU capabilities.
 * This requires inline assembly code.
 * The 64 bit Microsoft compilers do not support inline code anymore.
 * One solution is described here:
 *     http://software.intel.com/en-us/articles/cpuid-for-x64-platforms-and-microsoft-visual-studio-net-2005/
 * but this requires a separate .asm file.
 * We tried to implemented a pragmatic solution where we assume that
 * a 64 bit processor is a pretty up-to-date one, that will always
 * support the features we want... but this fails linking:
 * the fast memcpy misses _m_empty and _mm_stream_pi operations.
 * As a result, we decided to simply not support the fast
 * memcpy on 64 bit and return all zero flags in the CPUFlags()
 * function.
 */

#if __MSVC__ && _WIN64
static unsigned long CPUFlags(void)
{
    return 0;
} /* end of CPUFlags */

static void *sse_memcpy(void *_out, const void *_in, size_t siz) {
    /* Never used */
    assert(0);
    return NULL;
} /* end of sse_memcpy */

#else

static unsigned long CPUFlags(void)
{
    unsigned long ulCPUFlags=0;
    int           iMMXsupport;
    int           iSSEsupport;
    int           iSSE2support;
    int           iSSE3support;
    unsigned int  cmd;
    unsigned int  res;

    iMMXsupport = 0;
    iSSEsupport = 0;
    cmd = 0;
    CPUID(cmd,ecx,res);
    if (res == 0x444d4163) {         /* "cAMD" : last for chars of "AuthenticAMD" */
        cmd=0x80000001;
        CPUID(cmd,edx,res);
        iSSEsupport = (res>>22) & 1; /* Extended MMX support */
        iMMXsupport = iSSEsupport;   /* SSE implies MMS support */
    }
    cmd = 1;
    CPUID(cmd,edx,res);
    iMMXsupport  |= (res>>23) & 1;
    iSSEsupport  |= (res>>25) & 1;
    iSSE2support  = (res>>26) & 1;
    cmd = 1;
    CPUID(cmd,ecx,res);
    iSSE3support = (res>>0) & 1;

    ulCPUFlags = (iMMXsupport  << SHIFT_MMX_SUPPORT)
               | (iSSEsupport  << SHIFT_SSE_SUPPORT)
               | (iSSE2support << SHIFT_SSE2_SUPPORT)
               | (iSSE3support << SHIFT_SSE3_SUPPORT);

    return ulCPUFlags;
} /* end of CPUFlags */

static void *sse_memcpy(void *_out, const void *_in, size_t siz) {
    unsigned int i;
    char *out=(char *)_out;
    char *iin=(char *)_in;
    char *inp;
    size_t cachesize;

    /* Check alignment of pointers - fallback to regular memcpy */
    if (((long)iin & 0x7) || ((long)out & 0x7))  /* ignore msb part for 64 bits */
        return memcpy(_out, _in, siz);

    /* Move large chunks of data until only small things left */
    while (siz > CACHELINE) {
        /* Determine a fairly large chuck of data to process
         * (large but must fit in the cache)
         */
        cachesize = MIN(MAXCACHESIZE, siz&~(CACHELINE-1));

        /* Read data from the memory - prefetch a few cachelines ahead */
        inp = iin;
        for (i=0; i<cachesize; i+=CACHELINE) {
            /* The hint_to directs prefetch to all cache levels (we need the closest cache) */
            _mm_prefetch(inp+2*CACHELINE, _MM_HINT_T0);
            /* Just a dummy read, otherwise not optimal performance gain - replace by simple dereference? */
            TOUCH(inp);
            inp += CACHELINE;
        }

        /* Write data into the memory */
        for (i=0; i<cachesize; i+=8) {
            _mm_stream_pi((__m64*)out, *(__m64*)iin);
            iin += 8; out += 8;
        }
        siz -= cachesize;
    }

    /* Copy remainder */
    while (siz--) {
       *out++ = *iin++;
    }
    _mm_sfence();
    _mm_empty();

    return(_out);
} /* end of sse_memcpy */
#endif

/*
 * Convenience function for easy detection of instruction set
 * Call as:
 * if (CPUsupport(MMX_SUPPORT)) { ... }
 * if (CPUsupport(SSE_SUPPORT)) { ... }
 * if (CPUsupport(SSE2_SUPPORT)) { ... }
 * if (CPUsupport(SSE3_SUPPORT)) { ... }
 */
static int CPUsupport(const long set)
{
    return((CPUFlags() & set) != 0);
} /* end of CPUsupport */



/*
 * This is our own implementation of file buffering.
 * On PC platforms, we have seen significant performance
 * differences between unbuffered I/O and (normal) buffered I/O.
 * By implementing our own buffering, we can benefit
 * significantly from this performance gain.
 *
 * Therefore, we keep track of our own implementation of the
 * "FILE *" structure.
 *
 * Raw I/O requires block accesses with the following rules:
 * - The memory buffer must be block aligned.
 * - The file offset must be block aligned.
 * - The amount of data must be a multiple of the block size.
 *
 * Assumption: buffer size is larger than maximum fread/fwrite
 * access size used in the application.
 *
 * The requested buffer size is 256 Kbyte.
 * This is a compromise: larger buffer sizes are somewhat faster,
 * (especially on fast disk systems), however the amount of data
 * that is actually read or written is always a multiple of the
 * block size. When considering video images, large block sizes
 * contain more unused data and thus may become less efficient
 * (especially with random access of the file).
 *
 * Preprocessor define FIO_TRACE can be used to enable performance/
 * time measurement to trace the asynchronous I/O behavior.
 */
#define noFIO_TRACE
#ifdef FIO_TRACE
#define FIO_TRC_LINE      128
#define FIO_TRC_LENGTH  10000
#define FIO_TRC_LIN(ffp) ((ffp)->trc[(ffp)->trc_cnt<(FIO_TRC_LENGTH-1)?((ffp)->trc_cnt)++:((ffp)->trc_cnt)])
#define FIO_TRC_EVT(ffp) (QueryPerformanceCounter(&((ffp)->evt[(ffp)->trc_cnt])))
#endif

#define FIO_DEFAULT_BUFFER_SIZE (256*1024)
typedef enum {FIO_NONE, FIO_WRITE, FIO_READ} fio_action;
typedef struct {
    char          win32_path[MAX_PATH]; /* Path name of open file */
    size_t        buf_req;         /* Requested buffer size (may be set by application) */
    size_t        buf_size;        /* Size of allocated buffer (set once at allocation) */
    int           use_olap;        /* Set if this OS version supports overlapped I/O (not on win95, win98, winME) */
    fio_action    action;          /* Asynchronous action that is active */
    HANDLE        hFile;           /* System file handle */
    OVERLAPPED    olap;            /* System overlapped structure (for asynchronous I/O) */
    fio_offset_t  file_offset_ap;  /* File offset of start of application buffer */
    fio_offset_t  file_offset_io;  /* File offset of start of I/O buffer */
    int           file_eof;        /* File read eof detected */
    size_t        buf_offset;      /* Current file pointer in buffer */
    size_t        buf_use_ap;      /* Amount of data in application buffer */
    size_t        buf_use_io;      /* Amount of data in I/O buffer */
    int           dirty;           /* Flag indicating buffer contains data not yet written to disk */
    unsigned char *buf_io;         /* Buffer for asynchronous I/O, correctly aligned */
    unsigned char *buf_ap;         /* Buffer used by application, correctly aligned */
                                   /* Two buffers are swapped at each I/O access */

                                   /* Finally the function to use for memcpy */
    void          *(*memcpy_rtn)(void *, const void *, size_t);

#ifdef FIO_TRACE
    int           trc_cnt;
    char          trc[FIO_TRC_LENGTH][FIO_TRC_LINE];
    LARGE_INTEGER evt[FIO_TRC_LENGTH];
    LARGE_INTEGER freq;   /* no of ticks per second */
#endif
} FIO_FILE;
/*
 * Invariants of the FIO_FILE structure:
 *    file_offset_ap + buf_offset                  is current file position
 *    file_offset_io == file_offset_ap + buf_size  for reading
 *    file_offset_io == file_offset_ap - buf_size  for writing
 *    buf_offset <= buf_use_ap
 *    buf_use_ap - buf_offset + buf_use_io         data available for read
 *    buf_use_ap + buf_use_io                      data ready for writing
 *    buf_use_ap == buf_offset                     for writing
 */

#ifdef FIO_TRACE
static char *fio_trc_action(fio_action action)
{
    char *ret;
    ret = "";
    switch (action) {
    case FIO_NONE:
        ret = "fio_none";
        break;
    case FIO_WRITE:
        ret = "fio_write";
        break;
    case FIO_READ:
        ret = "fio_read";
        break;
    }
    return ret;
} /* end of fio_trc_action */
#endif

/*
 * Allocate *size bytes.
 * Actual allocation is a multiple of the system page size (rounded up).
 * In case the *size fits a multiple of the page size, it is not modified.
 * The actual *size is returned.
 */
static void fio_fp_alloc_aligned(unsigned char **buf, size_t *size)
{
    SYSTEM_INFO   info;
    SIZE_T        amount;

    GetSystemInfo(&info);
    amount = ((*size + info.dwPageSize - 1) / info.dwPageSize) * info.dwPageSize;

    *buf = VirtualAlloc(NULL, amount, MEM_COMMIT, PAGE_READWRITE);
    *size = amount;
} /* end of fio_fp_alloc_aligned */


static void fio_fp_free_aligned(unsigned char *buf)
{
    /* this two liner is to circumvent memeory leaks in Windows
     * where 'assert' is defined as void
      */
#ifdef  _DEBUG
    BOOL result = VirtualFree(buf, 0, MEM_RELEASE);
    assert(result);
#else
    VirtualFree(buf, 0, MEM_RELEASE);
#endif
} /* end of fio_fp_free_aligned */


static FIO_FILE *fio_fp_alloc(void)
{
    FIO_FILE *ffp;
    OSVERSIONINFO osvi;  /* Info about OS version */


    ffp = malloc(sizeof(FIO_FILE));
    if (ffp != NULL) {
        ffp->olap.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (ffp->olap.hEvent == NULL) {
            free(ffp);
            ffp = NULL;
        } else {
            ffp->use_olap = 0;
            osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
            if (GetVersionEx(&osvi)) {
                if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT) {
                    /* All NT variants, winNT, win2000, winXP. */
                    ffp->use_olap = 1;
                }
            }

            ffp->memcpy_rtn = memcpy;
            if (CPUsupport(SSE_SUPPORT)) {
                ffp->memcpy_rtn = sse_memcpy;
            }

            ffp->buf_req        = FIO_DEFAULT_BUFFER_SIZE;
            ffp->buf_size       = 0;
            ffp->buf_io         = NULL;
            ffp->buf_ap         = NULL;
            ffp->action         = FIO_NONE;
            ffp->file_offset_ap = 0;
            ffp->file_offset_io = 0;
            ffp->file_eof       = 0;
            ffp->buf_offset     = 0;
            ffp->buf_use_ap     = 0;
            ffp->buf_use_io     = 0;
            ffp->dirty          = 0;
#ifdef FIO_TRACE
            ffp->trc_cnt        = 0;
            QueryPerformanceFrequency(&ffp->freq);
#endif
        }
    }

    return(ffp);
} /* end of fio_fp_alloc */


static void fio_fp_free(
    FIO_FILE *ffp)
{
#ifdef FIO_TRACE
    int i;
    LARGE_INTEGER prev, curr;
    FILE *fp;

    fp = fopen("trace.log", "w");
    assert(fp != NULL);

    prev = ffp->evt[0];
    for (i=0; i<ffp->trc_cnt; i++) {
        curr = ffp->evt[i];
        fprintf(fp, "--> %8d %s\n", (int)((curr.QuadPart-prev.QuadPart)/(ffp->freq.QuadPart/1000000)), ffp->trc[i]);
        prev = curr;
    }

    fclose(fp);
#endif
    CloseHandle(ffp->olap.hEvent);
    if (ffp->buf_size != 0) {
        fio_fp_free_aligned(ffp->buf_io);
        fio_fp_free_aligned(ffp->buf_ap);
    }

    free(ffp);
} /* end of fio_fp_free */


/* Used in fio_fp_flush and fio_fopen */
static fio_offset_t fio_fp_get_filesize(
    FIO_FILE *ffp)
{
    fio_offset_t   ret;
    DWORD          dwSizeLow;
    DWORD          dwSizeHigh;

    /* Get file size. */
    dwSizeLow = GetFileSize(ffp->hFile, &dwSizeHigh);
    if ((dwSizeLow == INVALID_FILE_SIZE) && (GetLastError()) != NO_ERROR) {
        ret = 0;   /* Error, no end-of-file. */
    } else {
        ret = ((fio_offset_t)dwSizeHigh << (sizeof(DWORD)*8)) | dwSizeLow;
    }

    return(ret);
} /* end of fio_fp_get_filesize */


/*
 * Wait for current asynchronous I/O to complete.
 */
static int fio_fp_wait(
    FIO_FILE *ffp)
{
    DWORD         dwByteCount;
    DWORD         dwErr;
    int           repeat;
    int           err = 0;


    switch (ffp->action) {
    case FIO_WRITE:
#ifdef FIO_TRACE
        FIO_TRC_EVT(ffp);
        sprintf(FIO_TRC_LIN(ffp), "Wait WR   start");
#endif

        /* Repeat loop required as workaround for bug #1506617 -- see note in file header */
        repeat = TIMEOUT_REPEAT;
        while (!err && (repeat>0)) {
            repeat--;
            if (!GetOverlappedResult(ffp->hFile, &ffp->olap, &dwByteCount, TRUE)) {
                err = 1;
            } else {
                if (dwByteCount != (DWORD)ffp->buf_size) {
                    err = 1;
                }
            }

            if (err) {
                dwErr = GetLastError();
                /* Check on timeout, do we need to retry this write as bug #1506617 workaround? */
                if ((dwErr == ERROR_SEM_TIMEOUT) && (repeat > 0)) {
                    err = 0;
                    CloseHandle(ffp->hFile); /* ignore error, best-try action */
#ifdef FIO_TRACE
                    FIO_TRC_EVT(ffp);
                    sprintf(FIO_TRC_LIN(ffp), "GetOverlappedResult timeout - reopening file & restarting write operation");
#endif
                    ffp->hFile = CreateFile(
                        ffp->win32_path,                    /* lpFileName */
                        GENERIC_WRITE|GENERIC_READ,         /* dwDesiredAccess */
                        FILE_SHARE_READ|FILE_SHARE_WRITE,   /* dwShareMode */
                        NULL,                               /* lpSecurityAttributes */
                        OPEN_EXISTING,                      /* dwCreationDisposition */
                        FILE_FLAG_NO_BUFFERING|(ffp->use_olap?FILE_FLAG_OVERLAPPED:0), /* dwFlagsAndAtrributes */
                        NULL);                              /* hTemplateFile */

                    if (ffp->hFile == INVALID_HANDLE_VALUE) {
                        err = 1;
                    }

                    if (WriteFile(ffp->hFile, ffp->buf_io, (DWORD)ffp->buf_size, &dwByteCount, ffp->use_olap ? &ffp->olap : NULL)) {
                        /* I/O action finished immediately. */
                        if (dwByteCount != (DWORD)ffp->buf_size) {
                            err = 1;
                        }
                    } else {
                        dwErr = GetLastError();
                        if ((dwErr != ERROR_IO_PENDING) && (dwErr != ERROR_SEM_TIMEOUT)) {
                            err = 1;
                        }
                    }
                } /* if semaphore timeout */
            } else {
                repeat = 0; /* success, finish repeat loop */
            }
        } /* while (repeat) */

        ffp->action = FIO_NONE;

#ifdef FIO_TRACE
        FIO_TRC_EVT(ffp);
        sprintf(FIO_TRC_LIN(ffp), "Wait WR   buf %08lx %s", (unsigned long)ffp->buf_io, err?"err":"");
#endif
        break;
    case FIO_READ:
#ifdef FIO_TRACE
        FIO_TRC_EVT(ffp);
        sprintf(FIO_TRC_LIN(ffp), "Wait RD   start");
#endif
        if (!GetOverlappedResult(ffp->hFile, &ffp->olap, &dwByteCount, TRUE)) {
            if (GetLastError() == ERROR_HANDLE_EOF) {
                ffp->file_eof = 1;
            } else {
                err = 1;
            }
        }
        ffp->buf_use_io = dwByteCount;
        ffp->action = FIO_NONE;
#ifdef FIO_TRACE
        FIO_TRC_EVT(ffp);
        sprintf(FIO_TRC_LIN(ffp), "Wait RD   buf %08lx amt %08lx %s %s",
                (unsigned long)ffp->buf_io, (unsigned long)ffp->buf_use_io,
                ffp->file_eof?"eof":"", err?"err":"");
#endif
        break;
    default:
        /* No action running, so simply return. */
        break;
    }

    return(err);
} /* end of fio_fp_wait */


/*
 * Swap application and io buffers
 */
static void fio_fp_swap_buffers(
    FIO_FILE *ffp)
{
    unsigned char *buf;

    buf = ffp->buf_io;
    ffp->buf_io = ffp->buf_ap;
    ffp->buf_ap = buf;
    ffp->buf_use_ap = ffp->buf_use_io;
    ffp->buf_use_io = 0;
} /* end of fio_fp_swap_buffers */


/*
 * Start new transfer on current file_offset_io position.
 */
static int fio_fp_start_read(
    FIO_FILE *ffp)
{
    LARGE_INTEGER liPos;
    BOOL          ok;
    DWORD         dwByteCount;
    int           err;


    err = 0;
    if (ffp->use_olap) {
        ffp->olap.OffsetHigh = (DWORD)(ffp->file_offset_io >> 8*sizeof(DWORD));
        ffp->olap.Offset     = (DWORD)(ffp->file_offset_io &  (~(DWORD)0));
    } else {
        liPos.QuadPart = ffp->file_offset_io;
        err = !SetFilePointerEx(ffp->hFile, liPos, NULL, FILE_BEGIN);
    }

#ifdef FIO_TRACE
    FIO_TRC_EVT(ffp);
    sprintf(FIO_TRC_LIN(ffp), "ReadFile  start");
#endif
    ok = ReadFile(ffp->hFile, ffp->buf_io, (DWORD)ffp->buf_size, &dwByteCount, ffp->use_olap ? &ffp->olap : NULL);
    if (ok) {
        /* I/O action finished immediately (and successfully). */
        ffp->buf_use_io = dwByteCount;
        if (dwByteCount != (DWORD)ffp->buf_size) {
            ffp->file_eof = 1;
        }
    } else {
        switch (GetLastError()) {
        case ERROR_HANDLE_EOF: 
            /* I/O action finished immediately (and signals EOF). */
            ffp->file_eof = 1;
            break;
        case ERROR_IO_PENDING:
            /* I/O action initiated, good, continue while I/O is running. */
            ffp->action = FIO_READ;
            break;
        default:
            /* I/O action finished immediately (with some other error). */
            err = 1;
            break;
        }
    }
#ifdef FIO_TRACE
    FIO_TRC_EVT(ffp);
    sprintf(FIO_TRC_LIN(ffp), "ReadFile  buf %08lx pos %08lx prc buf %08lx pos %08lx %s %s %s",
            (unsigned long)ffp->buf_io, (unsigned long)ffp->file_offset_io,
            (unsigned long)ffp->buf_ap, (unsigned long)ffp->file_offset_ap,
            fio_trc_action(ffp->action), ffp->file_eof?"eof":"", err?"err":"");
#endif

    return(err);
} /* end of fio_fp_start_read */


/*
 * Starts reading the next block of data;
 * current block returned in application buffer.
 *
 * Wait for current I/O action.
 * Swaps io and ap buffers.
 * Start new transfer on position file_offset_io.
 */
static int fio_fp_read_buf(
    FIO_FILE *ffp)
{
    int           err;

    /* Wait for current file operation */
    err = fio_fp_wait(ffp);
    if (!err) {
        /* Swap buffers, application buffer becomes io buffer. */
        fio_fp_swap_buffers(ffp);

        /* Start new file IO */
        err = fio_fp_start_read(ffp);
    }

    return(err);
} /* end of fio_fp_read_buf */


/*
 * Starts writing of the application buffer.
 * Waits for current I/O operation.
 * Then swaps the buffers.
 * And initiates the new I/O operation.
 */
static int fio_fp_write_buf(
    FIO_FILE *ffp)
{
    LARGE_INTEGER liPos;
    DWORD         dwByteCount;
    DWORD         dwErr;
    int           err;
    int           repeat;
    unsigned char *buf;


    err = fio_fp_wait(ffp);
    if (!err) {
        /* Swap buffers, application buffer becomes io buffer. */
        buf = ffp->buf_io;
        ffp->buf_io = ffp->buf_ap;
        ffp->buf_ap = buf;

        if (ffp->use_olap) {
            ffp->olap.OffsetHigh = (DWORD)(ffp->file_offset_io >> 8*sizeof(DWORD));
            ffp->olap.Offset     = (DWORD)(ffp->file_offset_io &  (~(DWORD)0));
        } else {
            liPos.QuadPart = ffp->file_offset_io;
            err = !SetFilePointerEx(ffp->hFile, liPos, NULL, FILE_BEGIN);
        }

#ifdef FIO_TRACE
    FIO_TRC_EVT(ffp);
    sprintf(FIO_TRC_LIN(ffp), "WriteFile start");
#endif
        /* Repeat loop required as workaround for bug #1506617 -- see note in file header */
        repeat = TIMEOUT_REPEAT;
        while (!err && (repeat>0)) {
            repeat--;
            if (WriteFile(ffp->hFile, ffp->buf_io, (DWORD)ffp->buf_size, &dwByteCount, ffp->use_olap ? &ffp->olap : NULL)) {
                /* I/O action finished immediately. */
                dwErr = GetLastError();
                if (dwByteCount != (DWORD)ffp->buf_size) {
                    err = 1;
                }
            } else {
                dwErr = GetLastError();
                if (dwErr == ERROR_IO_PENDING) {
                    /* I/O action initiated, good, continue while I/O is running. */
                    ffp->action = FIO_WRITE;
                } else {
                    err = 1;
                }
            }

            if (err) {
                /* Check on timeout, do we need to retry this write as bug #1506617 workaround? */
                if ((dwErr == ERROR_SEM_TIMEOUT) && (repeat > 0)) {
                    err = 0;
                    CloseHandle(ffp->hFile); /* ignore error, best-try action */
#ifdef FIO_TRACE
                    FIO_TRC_EVT(ffp);
                    sprintf(FIO_TRC_LIN(ffp), "WriteFile timeout - reopening file");
#endif
                    ffp->hFile = CreateFile(
                        ffp->win32_path,                    /* lpFileName */
                        GENERIC_WRITE|GENERIC_READ,         /* dwDesiredAccess */
                        FILE_SHARE_READ|FILE_SHARE_WRITE,   /* dwShareMode */
                        NULL,                               /* lpSecurityAttributes */
                        OPEN_EXISTING,                      /* dwCreationDisposition */
                        FILE_ATTRIBUTE_NORMAL|FILE_FLAG_NO_BUFFERING|(ffp->use_olap?FILE_FLAG_OVERLAPPED:0), /* dwFlagsAndAtrributes */
                        NULL);                              /* hTemplateFile */

                    if (ffp->hFile == INVALID_HANDLE_VALUE) {
                        err = 1;
                    }
                }
            } else {
                repeat = 0; /* success, finish repeat loop */
            }
        }
#ifdef FIO_TRACE
    FIO_TRC_EVT(ffp);
    sprintf(FIO_TRC_LIN(ffp), "WriteFile buf %08lx pos %08lx prc buf %08lx pos %08lx %s %s",
           (unsigned long)ffp->buf_io, (unsigned long)ffp->file_offset_io,
           (unsigned long)ffp->buf_ap, (unsigned long)ffp->file_offset_ap,
           fio_trc_action(ffp->action), err?"err":"");
#endif
    }

    return(err);
} /* end of fio_fp_write_buf */


static int fio_fp_flush(
    FIO_FILE *ffp)
{
    int           err;
    size_t        wr_buf_use;
    size_t        rd_buf_use;
    unsigned char *buf;

    err = fio_fp_wait(ffp);
    wr_buf_use = ffp->buf_use_ap;
    if (!err) {
        if ((ffp->buf_use_ap < ffp->buf_size) && (fio_fp_get_filesize(ffp) != ffp->file_offset_ap)) {
            /* Write buffer is not completely filled and not at end of file */
            /* Start reading io buffer (so no swap - do it here: double swap == no swap) */
            buf = ffp->buf_io;
            ffp->buf_io = ffp->buf_ap;
            ffp->buf_ap = buf;
            ffp->file_offset_io = ffp->file_offset_ap;
            err = fio_fp_read_buf(ffp);
            if (!err) {
                err = fio_fp_wait(ffp);
                rd_buf_use = ffp->buf_use_io;
                if (!err && (rd_buf_use > wr_buf_use)) {
                    (*ffp->memcpy_rtn)(ffp->buf_ap+wr_buf_use, ffp->buf_io+wr_buf_use, rd_buf_use-wr_buf_use);
                }
            }
        }
    }

    if (!err) {
        /* Now, swap the buffers and write output. */
        ffp->file_offset_io = ffp->file_offset_ap;
        err = fio_fp_write_buf(ffp);
        if (!err) {
            err = fio_fp_wait(ffp);
        }
    }

    ffp->dirty = 0;

    return(err);
} /* end of fio_fp_flush */



/*
 * Here, the "normal" functions for the ansi C file I/O interface start.
 * fread  -> p_fio_fread
 * fwrite -> p_fio_fwrite
 * fseek  -> p_fio_fseek
 * feof   -> p_fio_feof
 * fopen  -> p_fio_fopen
 * fclose -> p_fio_fclose
 */

size_t p_fio_fread(void *ptr, size_t size, size_t nobj, FILE *stream)
{
    FIO_FILE      *ffp = (FIO_FILE *)stream;
    size_t        amount, amount1, amount2;
    int           err;
    size_t        ret;


    err = 0;
    amount = size * nobj;

    /* Allocate buffers on first file operation */
    if (ffp->buf_size == 0) {
        ffp->buf_size = ffp->buf_req;
        fio_fp_alloc_aligned(&ffp->buf_io, &ffp->buf_size);
        fio_fp_alloc_aligned(&ffp->buf_ap, &ffp->buf_size);
    }

    /* We have some restrictions - only cpfspd support, not full ansi-C */
    assert(amount < ffp->buf_size);

    /* Buffer not filled yet? then fill it. */
    if (ffp->buf_use_ap == 0) {
        /* Initiate first buffer read. */
        ffp->file_offset_io = ffp->file_offset_ap;
        err = fio_fp_read_buf(ffp);
        if (!err) {
            /* Move to next block and initiate next read. */
            ffp->file_offset_io = ffp->file_offset_ap + ffp->buf_size;
            err = fio_fp_read_buf(ffp);
        }
        ffp->dirty = 0;
    }

    /* No I/O operation on next buffer yet? Start it! */
    if (ffp->action == FIO_NONE && ffp->buf_use_io == 0 && !ffp->file_eof) {
        ffp->file_offset_io = ffp->file_offset_ap + ffp->buf_size;
        err = fio_fp_start_read(ffp);
    }

    if (amount > (ffp->buf_use_ap - ffp->buf_offset)) {
        amount1 = ffp->buf_use_ap - ffp->buf_offset;
        amount2 = amount - amount1;
    } else {
        amount1 = amount;
        amount2 = 0;
    }

    if (!err) {
        if (amount1 > 0) {
            (*ffp->memcpy_rtn)((unsigned char *)ptr, ffp->buf_ap+ffp->buf_offset, amount1);
            ffp->buf_offset += amount1;
        }
        if (amount2 > 0) {
            if (ffp->buf_use_ap > 0) {
                /* Move to next block */
                ffp->file_offset_ap += ffp->buf_size;
                ffp->file_offset_io += ffp->buf_size;
            }
            err = fio_fp_read_buf(ffp);
            if (amount2 > ffp->buf_use_ap)
                amount2 = ffp->buf_use_ap;
            (*ffp->memcpy_rtn)((unsigned char *)ptr+amount1, ffp->buf_ap, amount2);
            ffp->buf_offset = amount2;
            ffp->dirty      = 0;
        }
    }

    if (err) {
        ret = 0;
    } else {
        /* Amount of data actually read. */
        ret = amount1 + amount2;
    }
    return(ret);
} /* end of p_fio_fread */


size_t p_fio_fwrite(void *ptr, size_t size, size_t nobj, FILE *stream)
{
    FIO_FILE  *ffp = (FIO_FILE *)stream;
    size_t    amount, amount1, amount2;
    int       err;
    size_t    ret;
    size_t    amt;
    unsigned char *buf;


    err = 0;
    amount = size * nobj;

    /* Allocate buffers on first file operation */
    if (ffp->buf_size == 0) {
        ffp->buf_size = ffp->buf_req;
        fio_fp_alloc_aligned(&ffp->buf_io, &ffp->buf_size);
        fio_fp_alloc_aligned(&ffp->buf_ap, &ffp->buf_size);
    }

    /* We have some restrictions - only cpfspd support, not full ansi-C */
    assert(amount < ffp->buf_size);

    if ((!ffp->dirty) && (ffp->buf_offset != 0)) {
        /* Start of writing this buffer is not at beginning.
         * Fill the missing part of the buffer before doing anything
         */
        ffp->file_offset_io = ffp->file_offset_ap;
        err = fio_fp_read_buf(ffp);
        err = err || fio_fp_wait(ffp);
        /* Reverse swapped buffers, io buffer is filled, we need it as ap buffer. */
        buf = ffp->buf_io;
        ffp->buf_io = ffp->buf_ap;
        ffp->buf_ap = buf;
        ffp->dirty  = 0;
        ffp->buf_use_ap = ffp->buf_use_io;
    }

    if (amount > (ffp->buf_size - ffp->buf_offset)) {
        amount1 = ffp->buf_size - ffp->buf_offset;
        amount2 = amount - amount1;
    } else {
        amount1 = amount;
        amount2 = 0;
    }

    if (!err) {
        if (amount1 > 0) {
            if (ffp->buf_use_ap < ffp->buf_offset) {
                /* There is a hole in the buffer due to a previous forward fseek.
                 * First fill the hole, then update admin to move buf_use_ap index
                 * to file offset position.
                 * Maybe the data was written to the file previously, then the hole needs
                 * to be filled with data from the file.
                 * If there was no data in the file, we fill the hole with zero (to avoid
                 * undefined data in the file).
                 *
                 * Load data into the io buffer; then copy into ap buffer to fill the hole.
                 */
                ffp->file_offset_io = ffp->file_offset_ap;
                err = fio_fp_wait(ffp);
                err = err || fio_fp_start_read(ffp);
                err = err || fio_fp_wait(ffp);
                /* Io buffer filled now.
                 * Figure out how much data there is in the io buffer.
                 * If sufficient data is available, copy it.
                 * Maybe it is sufficient to fill the hole completely,
                 * maybe partly, or maybe there is no data at all.
                 */
                amt = 0;
                if (ffp->buf_use_io > ffp->buf_offset) {
                    amt = ffp->buf_offset - ffp->buf_use_ap;
                } else {
                    if (ffp->buf_use_io > ffp->buf_use_ap) {
                        amt = ffp->buf_use_io - ffp->buf_use_ap;
                    }
                }
                if (amt > 0) {
                    (*ffp->memcpy_rtn)(ffp->buf_ap+ffp->buf_use_ap, ffp->buf_io+ffp->buf_use_ap, amt);
                }

                ffp->buf_use_ap += amt;

                if (ffp->buf_use_ap < ffp->buf_offset) {
                    /* Fill the (remaining) hole with zero values. */
                    memset(ffp->buf_ap+ffp->buf_use_ap, 0, ffp->buf_offset-ffp->buf_use_ap);
                }


                ffp->buf_use_ap = ffp->buf_offset;
            }

            (*ffp->memcpy_rtn)(ffp->buf_ap+ffp->buf_offset, (unsigned char *)ptr, amount1);
            ffp->buf_offset += amount1;
            if (ffp->buf_offset > ffp->buf_use_ap) {
                ffp->buf_use_ap = ffp->buf_offset;
            }
            ffp->dirty = 1;
        }
        if (amount2 > 0) {
            if (ffp->dirty) {
                ffp->file_offset_io = ffp->file_offset_ap;
                err = fio_fp_write_buf(ffp);
            }
            ffp->file_offset_ap += ffp->buf_size;
            (*ffp->memcpy_rtn)(ffp->buf_ap, ((unsigned char *)ptr)+amount1, amount2);
            ffp->buf_offset = amount2;
            ffp->buf_use_ap = amount2;
            ffp->dirty = 1;
        }
    }

    if (err) {
        ret = 0;
    } else {
        ret = amount;
    }

    return(ret);
} /* end of p_fio_fwrite */


int p_fio_fseek(FILE *stream, fio_offset_t offset, int origin)
{
    FIO_FILE      *ffp = (FIO_FILE *)stream;
    fio_offset_t  new_pos;
    fio_offset_t  new_file_offset;
    long          new_buf_offset;
    int           err;
    unsigned char *buf;


    /* We have some restrictions - only cpfspd support, not full ansi-C */
#ifdef  _DEBUG
    assert(origin == SEEK_SET);
#else
    (void) origin;
#endif

    /* Allocate buffers on first file operation */
    if (ffp->buf_size == 0) {
        ffp->buf_size = ffp->buf_req;
        fio_fp_alloc_aligned(&ffp->buf_io, &ffp->buf_size);
        fio_fp_alloc_aligned(&ffp->buf_ap, &ffp->buf_size);
    }

    err = 0;
    new_pos = offset;
    new_file_offset = (new_pos / ffp->buf_size) * ffp->buf_size;
    new_buf_offset  = (long)(new_pos - new_file_offset);

    if (new_file_offset == ffp->file_offset_ap) {
        /* Jumped to a location in current appl. buffer. */
        if (ffp->dirty && (ffp->buf_offset > ffp->buf_use_ap)) {
            ffp->buf_use_ap = ffp->buf_offset;
        }
        ffp->buf_offset = new_buf_offset;
    } else {
        ffp->file_eof = 0;    /* reset eof */

        /* Jumped out of current appl. buffer: finish all I/O activity. */
        if (ffp->dirty) {
            err = fio_fp_flush(ffp);
        } else {
            err = fio_fp_wait(ffp);
        }

        if (new_file_offset == ffp->file_offset_io
            && ffp->file_offset_io > ffp->file_offset_ap) {
            /* Jumped to a location in io buffer
             * and we are reading (because the io buffer is ahead of the ap buffer). */
            if (ffp->buf_use_ap > 0) {
                /* Move to next block */
                ffp->file_offset_ap += ffp->buf_size;
                ffp->file_offset_io += ffp->buf_size;
            }

            /* Swap buffers, application buffer becomes io buffer. */
            buf = ffp->buf_io;
            ffp->buf_io = ffp->buf_ap;
            ffp->buf_ap = buf;
            ffp->buf_use_ap = ffp->buf_use_io;
            ffp->buf_use_io = 0;

            /* Set proper location in this new ap buffer. */
            ffp->buf_offset = new_buf_offset;
        } else {
            /* Jumped to any other location; really flush buffer. */
            ffp->file_offset_ap = new_file_offset;
            ffp->buf_offset = new_buf_offset;
            ffp->buf_use_ap = 0;
            ffp->buf_use_io = 0;
            ffp->dirty   = 0;
        }
    }

    return(err);
} /* end of p_fio_fseek */


int p_fio_feof(FILE *stream)
{
    FIO_FILE      *ffp = (FIO_FILE *)stream;
    int           ret;


    if (ffp->buf_offset < ffp->buf_use_ap) {
        /* More data in our buffer: no eof */
        ret = 0;
    } else {
        /* eof was returned by one of the system functions */
        ret = ffp->file_eof;
    }

    return(ret);
} /* end of p_fio_feof */


/*
 * Function SetPrivilege is copied from:
 *    http://msdn.microsoft.com/en-us/library/aa446619(VS.85).aspx
 *
 * Privilege name list:
 *    http://msdn.microsoft.com/en-us/library/bb530716(VS.85).aspx
 */
static BOOL SetPrivilege(
    FIO_FILE  *ffp,              /* for tracing */
    HANDLE    hToken,            /* access token handle */
    LPCTSTR   lpszPrivilege,     /* name of privilege to enable/disable */
    BOOL      bEnablePrivilege)  /* to enable or disable privilege */
{
    TOKEN_PRIVILEGES tp;
    LUID luid;

    if (!LookupPrivilegeValue(NULL, lpszPrivilege, &luid)) {
#ifdef FIO_TRACE
        FIO_TRC_EVT(ffp);
        sprintf(FIO_TRC_LIN(ffp), "SetPrivilege: LookupPrivilegeValue error: %d\n", (int)GetLastError() );
#else
        (void) ffp;
#endif
        return FALSE;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = bEnablePrivilege ? SE_PRIVILEGE_ENABLED : 0;

    // Enable the privilege or disable all privileges.
    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES) NULL, (PDWORD) NULL)) {
#ifdef FIO_TRACE
        FIO_TRC_EVT(ffp);
        sprintf(FIO_TRC_LIN(ffp), "SetPrivilege: AdjustTokenPrivileges error: %d\n", (int)GetLastError() ); 
#endif
        return FALSE;
    }

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
#ifdef FIO_TRACE
        FIO_TRC_EVT(ffp);
        sprintf(FIO_TRC_LIN(ffp), "SetPrivilege: The token does not have the specified privilege.\n");
#endif
        return FALSE;
    }

    return TRUE;
} /* end of SetPrivilege */


/* Note: keep these lists of open mode strings en related enum value consistent! */
static char   *fio_mode_str[] = {"rb", "wb", "ab", "r+b", "rb+", "w+b", "wb+", "a+b", "ab+", NULL};
typedef enum {MODE_RB_STD,MODE_WB_STD,MODE_AB_STD,MODE_RB_UPD1,MODE_RB_UPD2,MODE_WB_UPD1,MODE_WB_UPD2,MODE_AB_UPD1,MODE_AB_UPD2,MODE_NULL} fio_mode_val_t;

FILE * p_fio_fopen(const char *filename, const char *mode, fio_offset_t size)
{
    DWORD         dwDesiredAccess;
    DWORD         dwCreationDisposition;
    DWORD         dwFlagsAndAtrributes;
    HANDLE        hAccessToken;
    LARGE_INTEGER liPos;
    FIO_FILE      *ffp;
    int           truncate;
    int           append;
    int           i;
    int           err;

    ffp = fio_fp_alloc();
    err = (ffp == NULL);

#ifdef FIO_TRACE
    FIO_TRC_EVT(ffp);
    sprintf(FIO_TRC_LIN(ffp), "p_fio_fopen: entry %s %s", filename, mode);
#endif

    /* Avoid compiler warnings - assure all are assigned. */
    dwDesiredAccess       = GENERIC_WRITE | GENERIC_READ;
    dwCreationDisposition = OPEN_ALWAYS;
    truncate              = 0;
    append                = 0;

    if (!err) {
#ifdef FIO_CYGWIN
        /* Explicit conversion of cygwin pathnames to win32 pathnames. */
        cygwin_conv_to_full_win32_path(filename, ffp->win32_path);
#else
        strcpy(ffp->win32_path, filename);
#endif

        err = 1;
        for (i=0; i<MODE_NULL; i++) {
            if (strcmp(mode, fio_mode_str[i]) == 0) {
                err = 0; /* Found a valid open mode */
                switch (i) {
                case MODE_RB_STD:
                    dwDesiredAccess = GENERIC_READ;
                    dwCreationDisposition = OPEN_EXISTING;
                    break;
                case MODE_WB_STD:
                    dwDesiredAccess = GENERIC_WRITE | GENERIC_READ;
                    dwCreationDisposition = OPEN_ALWAYS;
                    truncate = 1;
                    break;
                case MODE_AB_STD:
                    dwDesiredAccess = GENERIC_WRITE | GENERIC_READ;
                    dwCreationDisposition = OPEN_ALWAYS;
                    append = 1;
                    break;
                case MODE_RB_UPD1:
                case MODE_RB_UPD2:
                    dwDesiredAccess = GENERIC_WRITE | GENERIC_READ;
                    dwCreationDisposition = OPEN_EXISTING;
                    break;
                case MODE_WB_UPD1:
                case MODE_WB_UPD2:
                    dwDesiredAccess = GENERIC_WRITE | GENERIC_READ;
                    dwCreationDisposition = OPEN_ALWAYS;
                    truncate = 1;
                    break;
                case MODE_AB_UPD1:
                case MODE_AB_UPD2:
                    dwDesiredAccess = GENERIC_WRITE | GENERIC_READ;
                    dwCreationDisposition = OPEN_ALWAYS;
                    append = 1;
                    break;
                default:
                    /* We have some restrictions - only cpfspd support, not full ansi-C */
                    assert((i>=MODE_RB_STD) && (i<MODE_NULL));
                    break;
                }
            }
        }
    }

    if ((!err) && (size > 0)) {
        /* Here we know that a new file must be created with a known size.
         * As a performance optimization, we will attempt to allocate the
         * file at once, without writing actual data. Advantages:
         *   - improved write performance, since overlapped I/O writes only
         *     occur for pre-allocated file
         *   - minimal fragmentation of the NTFS file system, see
         *     "Best Practices To Avoid NTFS Fragmentation While Writing To A File"
         *     http://msftmvp.com/Documents/NTFSFrag.pdf
         * Therefore, we will create the file using regular (i.e. synchronous,
         * non-overlapped) I/O, then close it, and reopen with overlapped I/O
         * for maximum performance.
         */

        /* Enable elevated privilege SE_MANAGE_VOLUME_NAME during CreateFile()
         * This is required for SetFileValidData() later
         */
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hAccessToken)) {
            SetPrivilege(ffp, hAccessToken, SE_MANAGE_VOLUME_NAME, TRUE);
        } else {
#ifdef FIO_TRACE
            FIO_TRC_EVT(ffp);
            sprintf(FIO_TRC_LIN(ffp), "p_fio_fopen: OpenProcessToken() error: %d\n", (int)GetLastError());
#endif
            hAccessToken = INVALID_HANDLE_VALUE;
        }

#ifdef FIO_TRACE
        FIO_TRC_EVT(ffp);
        sprintf(FIO_TRC_LIN(ffp), "p_fio_fopen: start open 1st\n");
#endif
        truncate = 0;
        ffp->hFile = CreateFile(
                ffp->win32_path,                    /* lpFileName */
                GENERIC_WRITE,                      /* dwDesiredAccess */
                FILE_SHARE_WRITE,                   /* dwShareMode */
                NULL,                               /* lpSecurityAttributes */
                CREATE_ALWAYS,                      /* dwCreationDisposition */
                FILE_ATTRIBUTE_NORMAL,              /* dwFlagsAndAtrributes */
                NULL);                              /* hTemplateFile */
        if (ffp->hFile == INVALID_HANDLE_VALUE) {
            err = 1;
        } else {
            /* Set the file size */
            liPos.QuadPart = size;
            err = !SetFilePointerEx(ffp->hFile, liPos, NULL, FILE_BEGIN);
            if (!err) {
                err = !SetEndOfFile(ffp->hFile);
            }

            if (!err) {
                if (!SetFileValidData(ffp->hFile, size)) {
#ifdef FIO_TRACE
                    FIO_TRC_EVT(ffp);
                    sprintf(FIO_TRC_LIN(ffp), "SetFileValidData() error: %d\n", (int)GetLastError());
#endif
                }
                /* Ignore potential error from SetFileValidData().
                 * This may be caused by a failure to obtain the elevated privilege
                 * SE_MANAGE_VOLUME_NAME, since that depends on user privileges.
                 * Furthermore, the feature is not supported on all file systems
                 * (NTFS is our main target, and that supports SetFileValidData).
                 * Finally, I found that SetFileValidData() fails on cygwin
                 * version 1.5.25-15. It returns a failure and GetLastError()
                 * eports error 1314 "A required priviledge is not held by the client".
                 * An upgrade to version 1.7.1-1 resolved this.
                 * Seems related to the run-time support, since an old exec in the new
                 * environment works.
                 */
            }
            /* Close the file, so it can be reopened in overlapped mode */
            CloseHandle(ffp->hFile);
        }

        /* Disable elevated privilege SE_MANAGE_VOLUME_NAME for minimal risk */
        if (hAccessToken != INVALID_HANDLE_VALUE) {
            SetPrivilege(ffp, hAccessToken, SE_MANAGE_VOLUME_NAME, FALSE);
            CloseHandle(hAccessToken);
        }

        /* Prepare for re-open in update mode */
        dwDesiredAccess = GENERIC_WRITE | GENERIC_READ;
        dwCreationDisposition = OPEN_EXISTING;
    }

    if (!err) {
#ifdef FIO_TRACE
        FIO_TRC_EVT(ffp);
        sprintf(FIO_TRC_LIN(ffp), "p_fio_fopen: start open 2nd\n");
#endif
        if (ffp->use_olap) {
            dwFlagsAndAtrributes = FILE_FLAG_NO_BUFFERING|FILE_FLAG_OVERLAPPED;
        } else {
            dwFlagsAndAtrributes = FILE_FLAG_NO_BUFFERING;
        }

        ffp->hFile = CreateFile(
            ffp->win32_path,                    /* lpFileName */
            dwDesiredAccess,                    /* dwDesiredAccess */
            FILE_SHARE_READ|FILE_SHARE_WRITE,   /* dwShareMode */
            NULL,                               /* lpSecurityAttributes */
            dwCreationDisposition,              /* dwCreationDisposition */
            dwFlagsAndAtrributes,               /* dwFlagsAndAtrributes */
            NULL);                              /* hTemplateFile */

        if (ffp->hFile == INVALID_HANDLE_VALUE) {
            err = 1;
        } else {
            if (truncate) {
                /* Truncate file; i.e. set end-of-file at beginning. */
                if (!SetEndOfFile(ffp->hFile)) {
                    err = 1;
                }
            }

            if (!err && append) {
                /* Set file pointer at end-of-file. */
                err = p_fio_fseek((FILE *)ffp, fio_fp_get_filesize(ffp), SEEK_SET);
            }

            if (err) {
                CloseHandle(ffp->hFile);
            }
        }
    }

    if (err && (ffp != NULL)) {
        fio_fp_free(ffp);
        ffp = NULL;
    }

#ifdef FIO_TRACE
    FIO_TRC_EVT(ffp);
    sprintf(FIO_TRC_LIN(ffp), "p_fio_fopen: exit 0x%x\n", (unsigned int)ffp);
#endif
    return((FILE *)ffp);
} /* end of p_fio_fopen */


int p_fio_fclose(FILE *stream)
{
    FIO_FILE  *ffp = (FIO_FILE *)stream;
    int       ret;

    ret = 0;
    if (ffp->dirty) {
        if (fio_fp_flush(ffp))
            ret = EOF;
        if (fio_fp_wait(ffp))
            ret = EOF;
    }

    if (CloseHandle(ffp->hFile) == 0) {
        ret = EOF;
    }
    fio_fp_free(ffp);

    return(ret);
} /* end of p_fio_fclose */


int p_fio_bufsize(FILE *stream, size_t size)
{
    FIO_FILE  *ffp = (FIO_FILE *)stream;

    ffp->buf_req = size;

    return(0);
} /* end of p_fio_bufsize */


int p_fio_set_end_of_file(const char* filename, fio_offset_t offset)
{
    FILE          *stream;
    FIO_FILE      *ffp;
    LARGE_INTEGER liPos;
    int           err;

#ifdef FIO_TRACE
    return(0);
#endif

    stream = p_fio_fopen(filename, "rb+", -1);
    err = (stream == NULL);
    if (! err) {
        ffp = (FIO_FILE *)stream;
        liPos.QuadPart = offset;
        err = !SetFilePointerEx(ffp->hFile, liPos, NULL, FILE_BEGIN);
        err = err || !SetEndOfFile(ffp->hFile);
        if (p_fio_fclose(stream) == EOF)
            err = 1;
    }

    return(err);
} /* end of p_fio_set_end_of_file */

#else  /* not FIO_WIN32_FILE */

/*
 * Functions that can be overwritten by compile time options.
 * This way, platform specific conditional compilation is possible.
 */
#ifndef FIO_FUNC_FOPEN
#define FIO_FUNC_FOPEN(filename,mode) fopen(filename,mode)
#endif
#ifndef FIO_FUNC_FCLOSE
#define FIO_FUNC_FCLOSE(stream) fclose(stream)
#endif
#ifndef FIO_FUNC_FREAD
#define FIO_FUNC_FREAD(ptr,size,nobj,stream) fread(ptr,size,nobj,stream)
#endif
#ifndef FIO_FUNC_FWRITE
#define FIO_FUNC_FWRITE(ptr,size,nobj,stream) fwrite(ptr,size,nobj,stream)
#endif
#ifndef FIO_FUNC_FSEEK
#define FIO_FUNC_FSEEK(stream,offset,origin) fseek(stream,offset,origin)
#endif
#ifndef FIO_FUNC_FEOF
#define FIO_FUNC_FEOF(stream) feof(stream)
#endif


FILE * p_fio_fopen(const char *filename, const char *mode, fio_offset_t size)
{
    char buf = 0;
    FILE * fp = NULL;
       
    fp = FIO_FUNC_FOPEN( filename, mode );

    if ((size > 0) && (fp != NULL)) {
        /* allocate disk space by writing the last byte of the file */
        p_fio_fseek( fp, size-1, SEEK_SET );
        p_fio_fwrite( &buf, 1, 1, fp );
        p_fio_fseek( fp, 0, SEEK_SET );
    }
    
    return fp;
} /* end of p_fio_fopen */


int p_fio_fclose(FILE *stream)
{
    return(FIO_FUNC_FCLOSE(stream));
} /* end of p_fio_fclose */


size_t p_fio_fread(void *ptr, size_t size, size_t nobj, FILE *stream)
{
    return(FIO_FUNC_FREAD(ptr, size, nobj, stream));
} /* end of p_fio_fread */


size_t p_fio_fwrite(void *ptr, size_t size, size_t nobj, FILE *stream)
{
    return(FIO_FUNC_FWRITE(ptr, size, nobj, stream));
} /* end of p_fio_fwrite */


int p_fio_fseek(FILE *stream, fio_offset_t offset, int origin)
{
    return(FIO_FUNC_FSEEK(stream, offset, origin));
} /* end of p_fio_fseek */

int p_fio_feof(FILE *stream)
{
    return(FIO_FUNC_FEOF(stream));
} /* end of p_fio_feof */

int p_fio_bufsize(FILE *stream, size_t size)
{
    /* Function only available in win32 system */
    return((stream == NULL) || (size == 0));      /* Dummy operation to get rid of sgi compiler warnings on unused parameters */
} /* end of p_fio_bufsize */

int p_fio_set_end_of_file(const char* filename, fio_offset_t offset)
{
    /* Function only available in win32 system.
     * Not required on *nix systems, since it is only used to
     * chop off unused data at the end of the file caused
     * by full-buffer file access on win32.
     * If we need this later on, check the truncate() function
     * (... and be aware of large files :-)
     */
    return((filename == NULL) || (offset == 0));  /* Dummy operation to get rid of sgi compiler warnings on unused parameters */
} /* end of p_fio_set_end_of_file */

#endif  /* not FIO_WIN32_FILE */
