/***********************license start***************
 *  Copyright (c) 2003-2008 Cavium Networks (support@cavium.com). All rights
 *  reserved.
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials provided
 *        with the distribution.
 *
 *      * Neither the name of Cavium Networks nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written
 *        permission.
 *
 *  TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 *  AND WITH ALL FAULTS AND CAVIUM NETWORKS MAKES NO PROMISES, REPRESENTATIONS
 *  OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
 *  RESPECT TO THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY
 *  REPRESENTATION OR DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT
 *  DEFECTS, AND CAVIUM SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES
 *  OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR
 *  PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET
 *  POSSESSION OR CORRESPONDENCE TO DESCRIPTION.  THE ENTIRE RISK ARISING OUT
 *  OF USE OR PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
 *
 *
 *  For any questions regarding licensing please contact marketing@caviumnetworks.com
 *
 ***********************license end**************************************/


/**
 * @file
 *
 * Main Octeon executive header file (This should be the second header
 * file included by an application).
 *
 * <hr>$Revision: 41586 $<hr>
*/
#ifndef __CVMX_H__
#define __CVMX_H__

/* Control whether simple executive applications use 1-1 TLB mappings to access physical
** memory addresses.  This must be disabled to allow large programs that use more than
** the 0x10000000 - 0x20000000 virtual address range.
*/
#ifndef CVMX_USE_1_TO_1_TLB_MAPPINGS
#define CVMX_USE_1_TO_1_TLB_MAPPINGS 1
#endif

#ifndef CVMX_ENABLE_PARAMETER_CHECKING
#define CVMX_ENABLE_PARAMETER_CHECKING 1
#endif

#ifndef CVMX_ENABLE_DEBUG_PRINTS
#define CVMX_ENABLE_DEBUG_PRINTS 1
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#include "cvmx-platform.h"
#include "cvmx-access.h"
#include "cvmx-address.h"
#include "cvmx-asm.h"
#include "cvmx-packet.h"
#include "cvmx-warn.h"
#include "cvmx-sysinfo.h"
#include "octeon-model.h"
#include "cvmx-csr.h"
#include "cvmx-utils.h"

#if defined(__mips__) && !defined(CVMX_BUILD_FOR_LINUX_HOST)
#include "cvmx-access-native.h"
#endif

#ifdef	__cplusplus
}
#endif

#endif  /*  __CVMX_H__  */
