/*****************************************************************************
 * Copyright (c) 2016 Kernel Labs Inc.
 *
 * Authors:
 *   Steven Toth <stoth@kernellabs.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 *****************************************************************************/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>

#include <bitstream/mpeg/ts.h>
#include <bitstream/mpeg/psi.h>
#include <bitstream/dvb/si.h>
#include <bitstream/dvb/si_print.h>
#include <bitstream/scte/35.h>
#include <bitstream/scte/35_print.h>

#include <libklscte35/scte35.h>

#define dprintf(level, fmt, arg...) \
do {\
  if (ctx->verbose >= level) printf(fmt, ## arg); \
} while(0);
 
static void hexdump(unsigned char *buf, unsigned int len, int bytesPerRow /* Typically 16 */)
{
	for (unsigned int i = 0; i < len; i++)
		printf("%02x%s", buf[i], ((i + 1) % bytesPerRow) ? " " : "\n");
	printf("\n");
}

static void print_wrapper(void *_unused, const char *fmt, ...)
{
	char v[strlen(fmt) + 2];
	va_list args;
	va_start(args, fmt);
	strcpy(v, fmt);
	strcat(v, "\n");
	vprintf(v, args);
	va_end(args);
}
//static inline void scte35_print(const uint8_t *p_scte35, print_wrapper, void *print_opaque, print_type_t i_print_type);
// eit_print(p_section, print_wrapper, NULL, iconv_wrapper, NULL, PRINT_XML);
//
//

#if 0
/*****************************************************************************
 * iconv_wrapper
 *****************************************************************************/
static char *iconv_append_null(const char *p_string, size_t i_length)
{
    char *psz_string = malloc(i_length + 1);
    memcpy(psz_string, p_string, i_length);
    psz_string[i_length] = '\0';
    return psz_string;
}

static char *iconv_wrapper(void *_unused, const char *psz_encoding,
                           char *p_string, size_t i_length)
{
    char *psz_string, *p;
    size_t i_out_length;

    if (!strcmp(psz_encoding, psz_native_encoding))
        return iconv_append_null(p_string, i_length);

    if (iconv_handle != (iconv_t)-1 &&
        strcmp(psz_encoding, psz_current_encoding)) {
        iconv_close(iconv_handle);
        iconv_handle = (iconv_t)-1;
    }

    if (iconv_handle == (iconv_t)-1)
        iconv_handle = iconv_open(psz_native_encoding, psz_encoding);
    if (iconv_handle == (iconv_t)-1) {
        fprintf(stderr, "couldn't convert from %s to %s (%m)\n", psz_encoding,
                psz_native_encoding);
        return iconv_append_null(p_string, i_length);
    }
    psz_current_encoding = psz_encoding;

    /* converted strings can be up to six times larger */
    i_out_length = i_length * 6;
    p = psz_string = malloc(i_out_length);
    if (iconv(iconv_handle, &p_string, &i_length, &p, &i_out_length) == -1) {
        fprintf(stderr, "couldn't convert from %s to %s (%m)\n", psz_encoding,
                psz_native_encoding);
        free(psz_string);
        return iconv_append_null(p_string, i_length);
    }
    if (i_length)
        fprintf(stderr, "partial conversion from %s to %s\n", psz_encoding,
                psz_native_encoding);

    *p = '\0';
    return psz_string;
}
#endif


/* Return the number of TS packets we've generated */
static int output_psi_section(struct scte35_context_s *ctx, uint8_t *section, uint16_t pid, uint8_t *cc)
{
	uint16_t section_length = psi_get_length(section) + PSI_HEADER_SIZE;
	uint16_t section_offset = 0;
	int count = 0;

	memcpy(&ctx->section[0], section, section_length);
	ctx->section_length = section_length;

	do {
		uint8_t ts_offset = 0;
		memset(ctx->pkt, 0xff, TS_SIZE);

		psi_split_section(ctx->pkt, &ts_offset, section, &section_offset);

		ts_set_pid(ctx->pkt, pid);
		ts_set_cc(ctx->pkt, *cc);
		(*cc)++;
		*cc &= 0xf;

		if (section_offset == section_length)
			psi_split_end(ctx->pkt, &ts_offset);

		count++;
		if (ctx->verbose >= 2) {
			hexdump(ctx->pkt, TS_SIZE, 16);
			scte35_print(section, print_wrapper, NULL, PRINT_XML);
 		}

	} while (section_offset < section_length);
	return count;
}

int scte35_generate_heartbeat(struct scte35_context_s *ctx)
{
	uint8_t *scte35 = psi_allocate();

/*
47 41 23 10 00
fc          table id
30 11       SSI / Sec Length
00          protocol version
00          encrypted packet / enc algo / pts 32:32
00 00 00 00 pts 31:0
00          cw_index
ff f        tier
 0 00       splice command length
00          splice command type (0 = NULL)
00 00       descriptor look length
7a 4f bf ff crc32
ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff 
*/
	/* Generate empty section */
	scte35_init(scte35);
	psi_set_length(scte35, PSI_MAX_SIZE);
	scte35_set_pts_adjustment(scte35, 0);
	scte35_null_init(scte35);
	scte35_set_desclength(scte35, 0);
	psi_set_length(scte35, scte35_get_descl(scte35) + PSI_CRC_SIZE - scte35 - PSI_HEADER_SIZE);
	psi_set_crc(scte35);
	int count = output_psi_section(ctx, scte35, ctx->outputPid, &ctx->cc);

	free(scte35);
	return count;
}

#if 0
static void scte35_generate_pointinout(struct scte35_context_s *ctx, int out_of_network_indicator)
{
	uint8_t *scte35 = psi_allocate();

/*
47 41 23 11 00
fc          table id
30 25       SSI / Section length
00          protocol version
00          encrypted packet / enc algo / pts 32:32
00 00 00 00 pts 31:0
00          cw_index
ff f        tier
 0 14       splice command length
05          splice command type (5 = splice insert)
            00 00 10 92   event id
            7f            splice event cancel indicator
            ef            out of network indicator / program splice / duration flag
            fe
10 17 df 80 fe
            01 9b fc c0
            09 78
            00            aval_num
            00            avails_expected
00 00       descriptor loop length
e9 7f f3 c0 crc32
ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff 
*/

	/* Generate insert section */
	scte35_init(scte35);
	psi_set_length(scte35, PSI_MAX_SIZE);
	scte35_set_pts_adjustment(scte35, 0);
	scte35_insert_init(scte35,
			   SCTE35_INSERT_HEADER2_SIZE +
			   SCTE35_SPLICE_TIME_HEADER_SIZE +
			   SCTE35_SPLICE_TIME_TIME_SIZE +
			   SCTE35_BREAK_DURATION_HEADER_SIZE +
			   SCTE35_INSERT_FOOTER_SIZE);
	scte35_insert_set_cancel(scte35, false);
	scte35_insert_set_event_id(scte35, 4242);
	scte35_insert_set_out_of_network(scte35, true);
	scte35_insert_set_program_splice(scte35, true);
	scte35_insert_set_duration(scte35, true);
	scte35_insert_set_splice_immediate(scte35, false);

	uint8_t *splice_time = scte35_insert_get_splice_time(scte35);
	scte35_splice_time_init(splice_time);
	scte35_splice_time_set_time_specified(splice_time, true);
	scte35_splice_time_set_pts_time(splice_time, 270000000);

	uint8_t *duration = scte35_insert_get_break_duration(scte35);
	scte35_break_duration_init(duration);
	scte35_break_duration_set_auto_return(duration, true);
	scte35_break_duration_set_duration(duration, 27000000);

	scte35_insert_set_unique_program_id(scte35, 2424);
	scte35_insert_set_avail_num(scte35, 0);
	scte35_insert_set_avails_expected(scte35, 0);
	scte35_set_desclength(scte35, 0);
	psi_set_length(scte35, scte35_get_descl(scte35) + PSI_CRC_SIZE - scte35 - PSI_HEADER_SIZE);
	psi_set_crc(scte35);
	output_psi_section(scte35, ctx->outputPid, &ctx->cc);

	free(scte35);
}
#else
static int scte35_generate_pointinout(struct scte35_context_s *ctx, int out_of_network_indicator)
{
	uint8_t *scte35 = psi_allocate();

/*
47 41 23 10 00   out of network
fc
30 25
00            protocol version
00 00 00 00 00
00            cw_index
ff f          tier
 0 14         slice command length
05            slice command type 5 (insert)
00 00 00 01   eventid
7f            cancel prior = false
df            out of network | program splice | splice_immediate
00 01         uniq program id 
00            avail_num
00            avails_expected
00 00         desc loop length
00
00 00 00 00 00 00 00 00 00
6b 97 98 28   crc32
ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff 

47 41 23 11 00   back into network
fc
30 25
00
00 00 00 00
00 00       cw_index
ff f
 0 14
05
00 00 00 02 event id
7f          cancel prior = false
5f 00 01 00 00 00 00 00
00 00 00 00 00 00 00 00 00 b7 68 65 22 ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff 
*/

/*
47 41 23 11 00
fc          table id
30 25       SSI / Section length
00          protocol version
00          encrypted packet / enc algo / pts 32:32
00 00 00 00 pts 31:0
00          cw_index
ff f        tier
 0 14       splice command length
05          splice command type (5 = splice insert)
            00 00 10 92   event id
            7f            splice event cancel indicator
            ef            out of network indicator / program splice / duration flag
            fe
10 17 df 80 fe
            01 9b fc c0
            09 78
            00            aval_num
            00            avails_expected
00 00       descriptor loop length
e9 7f f3 c0 crc32
ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff ff
ff ff ff ff ff ff ff ff ff ff ff ff 
*/

	/* Generate insert section */
	scte35_init(scte35);
	psi_set_length(scte35, PSI_MAX_SIZE);
	scte35_set_pts_adjustment(scte35, 0);
	scte35_insert_init(scte35, SCTE35_INSERT_HEADER2_SIZE + SCTE35_INSERT_FOOTER_SIZE);
	scte35_insert_set_cancel(scte35, false);
	scte35_insert_set_event_id(scte35, ctx->eventId++);
	scte35_insert_set_out_of_network(scte35, out_of_network_indicator);

	/* Slice the entire program, audio, video, everything */
	scte35_insert_set_program_splice(scte35, true);
	scte35_insert_set_duration(scte35, false);
	scte35_insert_set_splice_immediate(scte35, true);

	/* See SCTE 118-2 - for Unique Program Number */
	scte35_insert_set_unique_program_id(scte35, ctx->uniqueProgramId);
	scte35_insert_set_avail_num(scte35, 0);
	scte35_insert_set_avails_expected(scte35, 0);
	scte35_set_desclength(scte35, 0);
	psi_set_length(scte35, scte35_get_descl(scte35) + PSI_CRC_SIZE - scte35 - PSI_HEADER_SIZE);
	psi_set_crc(scte35);
	int count = output_psi_section(ctx, scte35, ctx->outputPid, &ctx->cc);

	free(scte35);
	return count;
}
#endif

void scte35_initialize(struct scte35_context_s *ctx, uint16_t outputPid)
{
	dprintf(1, "%s()\n", __func__);
static int count = 0;
	if (count++ > 0)
		return;

	memset(ctx, 0, sizeof(*ctx));
	ctx->verbose = 2;
	ctx->outputPid = outputPid;
	ctx->eventId = 1;
	ctx->uniqueProgramId = 1;
}

	/* Go into Ad, switch away from the network */
int scte35_generate_immediate_out_of_network(struct scte35_context_s *ctx, uint16_t uniqueProgramId)
{
	dprintf(1, "%s()\n", __func__);
	ctx->uniqueProgramId = uniqueProgramId;
	return scte35_generate_pointinout(ctx, 1);
}

int scte35_generate_immediate_in_to_network(struct scte35_context_s *ctx, uint16_t uniqueProgramId)
{
	dprintf(1, "%s()\n", __func__);
	/* Go to network, switch away from the ad slicer */
	ctx->uniqueProgramId = uniqueProgramId;
	return scte35_generate_pointinout(ctx, 0);
}

int scte35_set_next_event_id(struct scte35_context_s *ctx, uint32_t eventId)
{
	dprintf(1, "%s(%d)\n", __func__, eventId);
	ctx->eventId = eventId;
	return 0;
}

static uint8_t *parse_time(struct scte35_splice_time_s *st, uint8_t *p)
{
	st->time_specified_flag = *(p + 0) & 0x80 ? 1 : 0;
	if (st->time_specified_flag == 1) {
		st->pts_time = ((uint64_t)*(p + 0) << 32 |
			(uint64_t)*(p + 1) << 24 |
			(uint64_t)*(p + 2) << 16 |
			(uint64_t)*(p + 3) << 8 |
			(uint64_t)*(p + 4)) & 0x1ffffffffL;
		return p + 5;
	} else
		return p + 1;
}

static uint8_t *parse_component(struct scte35_splice_insert_s *si, struct scte35_splice_component_s *c, uint8_t *p)
{
	c->component_tag = *p; p++;
	if (si->splice_immediate_flag == 0)
		p = parse_time(&c->splice_time, p);

	return p;
}

struct scte35_splice_info_section_s *scte35_splice_info_section_parse(uint8_t *section, unsigned int byteCount)
{
	if (*(section + 0) != SCTE35_TABLE_ID)
		return 0;

	struct scte35_splice_info_section_s *s = calloc(1, sizeof(*s));

	uint8_t *p = section;
	s->table_id = *(section + 0);
	s->section_syntax_indicator = *(section + 1) & 0x80 ? 1 : 0;
	s->private_indicator = *(section + 1) & 0x40 ? 1 : 0;
        s->section_length = (*(section + 1) << 8 | *(section + 2)) & 0xfff;
	s->protocol_version = scte35_get_protocol(section);
	s->encrypted_packet = scte35_is_encrypted(section);
	s->encryption_algorithm = (*(section + 4) >> 1) & 0x3f;
	s->pts_adjustment = scte35_get_pts_adjustment(section);
	s->cw_index = *(section + 8);
        s->tier = (*(section + 9) << 8 | *(section + 10)) & 0xfff;
	s->splice_command_length = scte35_get_command_length(section);
	s->splice_command_type = scte35_get_command_type(section);

	/* null processing */

	/* insert processing */
	if (s->splice_command_type == 0x05) {
		struct scte35_splice_insert_s *si = &s->splice_insert;
		si->splice_event_id = *(section + 14) << 24 | *(section + 15) << 16 | *(section + 16) << 8 | *(section + 17);
		si->splice_event_cancel_indicator = *(section + 18) & 0x80 ? 1 : 0;
		if (si->splice_event_cancel_indicator == 0) {
			si->out_of_network_indicator = *(section + 19) & 0x80 ? 1 : 0;
			si->program_splice_flag = *(section + 19) & 0x40 ? 1 : 0;
			si->duration_flag = *(section + 19) & 0x20 ? 1 : 0;
			si->splice_immediate_flag = *(section + 19) & 0x10 ? 1 : 0;

			p = section + 20;
			if ((si->program_splice_flag == 1) && (si->splice_immediate_flag == 0)) {
				struct scte35_splice_time_s *st = &si->splice_time;
				p = parse_time(st, p);
			}

			if (si->program_splice_flag == 0) {
				/* We don't support Component counts */
				si->component_count = *p;
				p++;
				for (int i = 0; i < si->component_count; i++)
					p = parse_component(si, &si->components[i], p);
				
			}

			if (si->duration_flag == 1) {
				struct scte35_break_duration_s *d = &si->duration;
				d->auto_return = *(p + 0) & 0x80 ? 1 : 0;
				d->duration = ((uint64_t)*(p + 0) << 32 |
					(uint64_t)*(p + 1) << 24 |
					(uint64_t)*(p + 2) << 16 |
					(uint64_t)*(p + 3) << 8 |
					(uint64_t)*(p + 4)) & 0x1ffffffffL;

				p += 5;
			}

			si->unique_program_id = *(p + 0) << 8 | *(p + 1); p+= 2;
			si->avail_num = *p; p++;
			si->avails_expected = *p; p++;
			
		} /* si->splice_event_cancel_indicator == 0 */

	} /* s->splice_command_type == 0x05 */

	s->descriptor_loop_length = *(p + 0) << 8 | *(p + 1); p+= 2;

	/* TODO: We don't support descriptor parsing, yet. */
	for (int i = 0; i < s->descriptor_loop_length; i++) {
		uint8_t tag = *p; p++;
		uint8_t len = *p; p++;
		p += len;
	}

	/* TODO: Alignment stuff, we've never seen a frame with alignment stuffing */

	if (s->encrypted_packet) {
		s->e_crc_32 = 0;
	}

	s->crc_32 = 0;

	return s;
}

void scte35_splice_info_section_free(struct scte35_splice_info_section_s *s)
{
	free(s);
}
