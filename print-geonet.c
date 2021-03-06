/*
 * Copyright (c) 2013 The TCPDUMP project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code
 * distributions retain the above copyright notice and this paragraph
 * in its entirety, and (2) distributions including binary code include
 * the above copyright notice and this paragraph in its entirety in
 * the documentation or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND
 * WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * Original code by Ola Martin Lykkja (ola.lykkja@q-free.com)
 */

/* \summary: ISO CALM FAST and ETSI GeoNetworking printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include "netdissect.h"
#include "extract.h"
#include "addrtoname.h"


/*
   ETSI TS 102 636-5-1 V1.1.1 (2011-02)
   Intelligent Transport Systems (ITS); Vehicular Communications; GeoNetworking;
   Part 5: Transport Protocols; Sub-part 1: Basic Transport Protocol

   ETSI TS 102 636-4-1 V1.1.1 (2011-06)
   Intelligent Transport Systems (ITS); Vehicular communications; GeoNetworking;
   Part 4: Geographical addressing and forwarding for point-to-point and point-to-multipoint communications;
   Sub-part 1: Media-Independent Functionality
*/

#define GEONET_ADDR_LEN 8

static const struct tok msg_type_values[] = {
	{   0, "CAM" },
	{   1, "DENM" },
	{ 101, "TPEGM" },
	{ 102, "TSPDM" },
	{ 103, "VPM" },
	{ 104, "SRM" },
	{ 105, "SLAM" },
	{ 106, "ecoCAM" },
	{ 107, "ITM" },
	{ 150, "SA" },
	{   0, NULL }
};

static void
print_btp_body(netdissect_options *ndo,
	       const u_char *bp)
{
	u_int version;
	u_int msg_type;
	const char *msg_type_str;

	/* Assuming ItsDpuHeader */
	/* 2 bytes ND_TCHECKed in geonet_print() */
	version = EXTRACT_U_1(bp);
	msg_type = EXTRACT_U_1(bp + 1);
	msg_type_str = tok2str(msg_type_values, "unknown (%u)", msg_type);

	ND_PRINT("; ItsPduHeader v:%u t:%u-%s", version, msg_type, msg_type_str);
}

static void
print_btp(netdissect_options *ndo,
	  const u_char *bp)
{
	/* 4 bytes ND_TCHECKed in geonet_print() */
	uint16_t dest = EXTRACT_BE_U_2(bp + 0);
	uint16_t src = EXTRACT_BE_U_2(bp + 2);
	ND_PRINT("; BTP Dst:%u Src:%u", dest, src);
}

static int
print_long_pos_vector(netdissect_options *ndo,
		      const u_char *bp)
{
	uint32_t lat, lon;

	ND_TCHECK_LEN(bp, GEONET_ADDR_LEN);
	ND_PRINT("GN_ADDR:%s ", linkaddr_string (ndo, bp, 0, GEONET_ADDR_LEN));

	ND_TCHECK_8(bp + 12);
	lat = EXTRACT_BE_U_4(bp + 12);
	ND_PRINT("lat:%u ", lat);
	lon = EXTRACT_BE_U_4(bp + 16);
	ND_PRINT("lon:%u", lon);
	return (0);
trunc:
	return -1;
}


/*
 * This is the top level routine of the printer.  'p' points
 * to the geonet header of the packet.
 */
void
geonet_print(netdissect_options *ndo, const u_char *bp, u_int length,
	     const struct lladdr_info *src)
{
	u_int version;
	u_int next_hdr;
	u_int hdr_type;
	u_int hdr_subtype;
	uint16_t payload_length;
	u_int hop_limit;
	const char *next_hdr_txt = "Unknown";
	const char *hdr_type_txt = "Unknown";
	int hdr_size = -1;

	ndo->ndo_protocol = "geonet";
	ND_PRINT("GeoNet ");
	if (src != NULL)
		ND_PRINT("src:%s", (src->addr_string)(ndo, src->addr));
	ND_PRINT("; ");

	/* Process Common Header */
	if (length < 36)
		goto invalid;

	ND_TCHECK_8(bp);
	version = EXTRACT_U_1(bp) >> 4;
	next_hdr = EXTRACT_U_1(bp) & 0x0f;
	hdr_type = EXTRACT_U_1(bp + 1) >> 4;
	hdr_subtype = EXTRACT_U_1(bp + 1) & 0x0f;
	payload_length = EXTRACT_BE_U_2(bp + 4);
	hop_limit = EXTRACT_U_1(bp + 7);

	switch (next_hdr) {
		case 0: next_hdr_txt = "Any"; break;
		case 1: next_hdr_txt = "BTP-A"; break;
		case 2: next_hdr_txt = "BTP-B"; break;
		case 3: next_hdr_txt = "IPv6"; break;
	}

	switch (hdr_type) {
		case 0: hdr_type_txt = "Any"; break;
		case 1: hdr_type_txt = "Beacon"; break;
		case 2: hdr_type_txt = "GeoUnicast"; break;
		case 3: switch (hdr_subtype) {
				case 0: hdr_type_txt = "GeoAnycastCircle"; break;
				case 1: hdr_type_txt = "GeoAnycastRect"; break;
				case 2: hdr_type_txt = "GeoAnycastElipse"; break;
			}
			break;
		case 4: switch (hdr_subtype) {
				case 0: hdr_type_txt = "GeoBroadcastCircle"; break;
				case 1: hdr_type_txt = "GeoBroadcastRect"; break;
				case 2: hdr_type_txt = "GeoBroadcastElipse"; break;
			}
			break;
		case 5: switch (hdr_subtype) {
				case 0: hdr_type_txt = "TopoScopeBcast-SH"; break;
				case 1: hdr_type_txt = "TopoScopeBcast-MH"; break;
			}
			break;
		case 6: switch (hdr_subtype) {
				case 0: hdr_type_txt = "LocService-Request"; break;
				case 1: hdr_type_txt = "LocService-Reply"; break;
			}
			break;
	}

	ND_PRINT("v:%u ", version);
	ND_PRINT("NH:%u-%s ", next_hdr, next_hdr_txt);
	ND_PRINT("HT:%u-%u-%s ", hdr_type, hdr_subtype, hdr_type_txt);
	ND_PRINT("HopLim:%u ", hop_limit);
	ND_PRINT("Payload:%u ", payload_length);
	if (print_long_pos_vector(ndo, bp + 8) == -1)
		goto trunc;

	/* Skip Common Header */
	length -= 36;
	bp += 36;

	/* Process Extended Headers */
	switch (hdr_type) {
		case 0: /* Any */
			hdr_size = 0;
			break;
		case 1: /* Beacon */
			hdr_size = 0;
			break;
		case 2: /* GeoUnicast */
			break;
		case 3: switch (hdr_subtype) {
				case 0: /* GeoAnycastCircle */
					break;
				case 1: /* GeoAnycastRect */
					break;
				case 2: /* GeoAnycastElipse */
					break;
			}
			break;
		case 4: switch (hdr_subtype) {
				case 0: /* GeoBroadcastCircle */
					break;
				case 1: /* GeoBroadcastRect */
					break;
				case 2: /* GeoBroadcastElipse */
					break;
			}
			break;
		case 5: switch (hdr_subtype) {
				case 0: /* TopoScopeBcast-SH */
					hdr_size = 0;
					break;
				case 1: /* TopoScopeBcast-MH */
					hdr_size = 68 - 36;
					break;
			}
			break;
		case 6: switch (hdr_subtype) {
				case 0: /* LocService-Request */
					break;
				case 1: /* LocService-Reply */
					break;
			}
			break;
	}

	/* Skip Extended headers */
	if (hdr_size >= 0) {
		if (length < (u_int)hdr_size)
			goto invalid;
		ND_TCHECK_LEN(bp, hdr_size);
		length -= hdr_size;
		bp += hdr_size;
		switch (next_hdr) {
			case 0: /* Any */
				break;
			case 1:
			case 2: /* BTP A/B */
				if (length < 4)
					goto invalid;
				ND_TCHECK_4(bp);
				print_btp(ndo, bp);
				length -= 4;
				bp += 4;
				if (length >= 2) {
					/*
					 * XXX - did print_btp_body()
					 * return if length < 2
					 * because this is optional,
					 * or was that just not
					 * reporting genuine errors?
					 */
					ND_TCHECK_2(bp);
					print_btp_body(ndo, bp);
				}
				break;
			case 3: /* IPv6 */
				break;
		}
	}

	/* Print user data part */
	if (ndo->ndo_vflag)
		ND_DEFAULTPRINT(bp, length);
	return;

invalid:
	ND_PRINT(" Malformed (small) ");
	/* XXX - print the remaining data as hex? */
	return;

trunc:
	nd_print_trunc(ndo);
}
