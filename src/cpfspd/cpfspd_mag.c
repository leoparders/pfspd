/*
 *  Copyright (c) KONINKLIJKE PHILIPS ELECTRONICS N.V. 2000-2010
 *  All rights reserved.
 *  For licensing and warranty information, see the file COPYING.LGPLv21.txt
 *  in the root directory of this package.
 *
 *  Philips Research Laboratories
 *  Eindhoven, The Netherlands
 *
 *  CVS id      :  $Id: cpfspd_mag.c,v 2.18 2013/09/18 12:42:43 gerk59 Exp $
 *
 *  Description :  Default way of handling magic numbers
 *
 *  Comment     :  See magic.h for an explanation
 */

#include "cpfspd_mag.h"

/* Intermediate file to simulate running mtk within Visual Studio solution  */
//#ifdef __VSTUDIO__
//#include "mtk_windows.h"
//#endif

/*
 * The preprocessor variables MTK_APPL_VERSION, MTK_APPL_NAME
 * MTK_APPL_MAGIC and MTK_APPL_README may be defined
 * on the command line. If they are defined, they do NOT contain
 * double quotes. This simplified command building in makefiles,
 * shell scripts etc. The macro QUOTE adds these in the
 * during token parsing. Therefore, it can even be used
 * in an #include directive to constuct the quoted filename.
 *
 * Macro QUOTE1 replaces a string abc into a quoted string "abc".
 * See "The C programming language, Kernighan and Ritchie,
 * ANSI-C version", appendix A12.3. Note that this macro does
 * NOT do macro expansion.
 * Therefore, macro QUOTE is used to activate QUOTE1 with its
 * expanded argument.
 */
#define QUOTE1(str) #str        /* add double quote around it */
#define QUOTE(str) QUOTE1(str)  /* substitute str definition */

/*
 * Beware, the character sequence "DollarRevision: string Dollar"
 * is modified by cvs. Therefore, we define these two to assure
 * such sequence does not occur in this file.
 */
#define REV_PREFIX  "@(#) $Revision:"
#define REV_POSTFIX "$"

#ifdef MTK_APPL_VERSION
#define MTK_VERSION_STRING QUOTE(MTK_APPL_VERSION)
#else
#define MTK_VERSION_STRING "unknown-version"
#endif

#ifdef MTK_APPL_NAME
#define MTK_NAME_STRING QUOTE(MTK_APPL_NAME)
#else
#define MTK_NAME_STRING "unknown-appl-name"
#endif

#ifdef MTK_APPL_MAGIC
#include QUOTE(MTK_APPL_MAGIC)
#else
#define MTK_MAGIC_STRING "unknown-magic-nr"
#endif


char *p_revision_str = REV_PREFIX " " MTK_NAME_STRING " " MTK_VERSION_STRING " " MTK_MAGIC_STRING " " REV_POSTFIX;

char *p_get_version(void)
{
    return(MTK_VERSION_STRING);
}

char *p_get_magic(void)
{
    return(MTK_MAGIC_STRING);
}
