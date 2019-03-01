/*
 *  Copyright (c) KONINKLIJKE PHILIPS ELECTRONICS N.V. 1997-2010
 *  All rights reserved.
 *  For licensing and warranty information, see the file COPYING.LGPLv21.txt
 *  in the root directory of this package.
 *
 *  Philips Research Laboratories
 *  Eindhoven, The Netherlands
 *
 *  CVS id      :  $Id: cpfspd_hdr.h,v 2.12 2010/01/05 14:18:32 riemens Exp $
 *
 *  Name        :  cpfspd_hdr.h
 *
 *  Author      :  Robert Jan Schutten
 *
 *  Function    :  Header file for cpfspd_hdr.c
 *
 */

/******************************************************************************/

#ifndef CPFSPD_HDR_H
#define CPFSPD_HDR_H

/* mandatory include of cpfspd.h; because of used typedefs */
#include "cpfspd.h"

/* functions of cpfspd_hdr that are also used in other files */
extern pT_data_fmt p_get_comp_data_format (const pT_header *header,
                                           int comp_nr);

extern pT_status p_check_file_data_format (const pT_header *header, 
                                           pT_color color_format,
                                           pT_data_fmt *file_data_fmt);

extern pT_status p_check_color_format (const pT_header *header, 
                                       pT_color *color_format);

extern pT_status p_create_free_header (pT_header *header, 
                                       pT_color color,
                                       double ima_freq, 
                                       double lin_freq, 
                                       double pix_freq,
                                       int act_lines, 
                                       int act_pixel,
                                       int interlace_factor,
                                       int h_ratio, 
                                       int v_ratio
                                       /*add data format*/);

#endif /* end of #ifndef CPFSPD_HDR_H */

/******************************************************************************/
