/*
 *	LiMon Monitor (LiMon) - Network.
 *
 *	Copyright 1994 - 2000 Neil Russell.
 *	(See License)
 *	SPDX-License-Identifier:	GPL-2.0
 *
 * History
 *	9/16/00	  bor  adapted to TQM823L/STK8xxL board, RARP/TFTP boot added
 */

#ifndef __NET_H__
#define __NET_H__

#include <net-common.h>

#if defined(CONFIG_NET_LWIP)
#include <net-lwip.h>
#else
#include <net-legacy.h>
#endif

#endif /* __NET_H__ */
