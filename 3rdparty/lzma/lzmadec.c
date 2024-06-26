#include "lzmadec.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// private.h//////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

/******************************************************************************

    Internal defines and typedefs for liblzmadec

    Copyright (C) 1999-2005 Igor Pavlov (http://7-zip.org/)
    Copyright (C) 2005 Lasse Collin <lasse.collin@tukaani.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

******************************************************************************/


/***********
  Constants
 ***********/

/* uint16_t would be enough for CProb. uint_fast16_t will give a little
   extra speed but wastes memory. On 32-bit architechture the amount
   of wasted memory is usually only a few kilobytes but the theoretical
   maximum is about 1.5 megabytes (4.5 on 64-bit). */
#define CProb uint_fast16_t

#define LZMA_BASE_SIZE 1846
#define LZMA_LIT_SIZE 768
#define LZMA_IN_BUFFER_SIZE 4096

#define LZMA_MINIMUM_COMPRESSED_FILE_SIZE 18

/* Decoder status */
#define LZMADEC_STATUS_UNINITIALIZED 0
#define LZMADEC_STATUS_RUNNING       1
#define LZMADEC_STATUS_FINISHING     2
#define LZMADEC_STATUS_STREAM_END    3
#define LZMADEC_STATUS_ERROR       (-1)


#define LZMA_NUM_TOP_BITS 24
#define LZMA_TOP_VALUE ((uint32_t)1 << LZMA_NUM_TOP_BITS)

#define LZMA_NUM_BIT_MODEL_TOTAL_BITS 11
#define LZMA_BIT_MODEL_TOTAL (1 << LZMA_NUM_BIT_MODEL_TOTAL_BITS)
#define LZMA_NUM_MOVE_BITS 5

#define LZMA_NUM_POS_BITS_MAX 4
#define LZMA_NUM_POS_STATES_MAX (1 << LZMA_NUM_POS_BITS_MAX)

#define LZMA_LEN_NUM_LOW_BITS 3
#define LZMA_LEN_NUM_LOW_SYMBOLS (1 << LZMA_LEN_NUM_LOW_BITS)
#define LZMA_LEN_NUM_MID_BITS 3
#define LZMA_LEN_NUM_MID_SYMBOLS (1 << LZMA_LEN_NUM_MID_BITS)
#define LZMA_LEN_NUM_HIGH_BITS 8
#define LZMA_LEN_NUM_HIGH_SYMBOLS (1 << LZMA_LEN_NUM_HIGH_BITS)

#define LZMA_LEN_CHOICE 0
#define LZMA_LEN_CHOICE2 (LZMA_LEN_CHOICE + 1)
#define LZMA_LEN_LOW (LZMA_LEN_CHOICE2 + 1)
#define LZMA_LEN_MID (LZMA_LEN_LOW + (LZMA_NUM_POS_STATES_MAX << LZMA_LEN_NUM_LOW_BITS))
#define LZMA_LEN_HIGH (LZMA_LEN_MID + (LZMA_NUM_POS_STATES_MAX << LZMA_LEN_NUM_MID_BITS))
#define LZMA_NUM_LEN_PROBS (LZMA_LEN_HIGH + LZMA_LEN_NUM_HIGH_SYMBOLS)

#define LZMA_NUM_STATES 12
#define LZMA_NUM_LIT_STATES 7

#define LZMA_START_POS_MODEL_INDEX 4
#define LZMA_END_POS_MODEL_INDEX 14
#define LZMA_NUM_FULL_DISTANCES (1 << (LZMA_END_POS_MODEL_INDEX >> 1))

#define LZMA_NUM_POS_SLOT_BITS 6
#define LZMA_NUM_LEN_TO_POS_STATES 4

#define LZMA_NUM_ALIGN_BITS 4
#define LZMA_ALIGN_TABLE_SIZE (1 << LZMA_NUM_ALIGN_BITS)

#define LZMA_MATCH_MIN_LEN 2
#define LZMA_IS_MATCH 0
#define LZMA_IS_REP (LZMA_IS_MATCH + (LZMA_NUM_STATES << LZMA_NUM_POS_BITS_MAX))
#define LZMA_IS_REP_G0 (LZMA_IS_REP + LZMA_NUM_STATES)
#define LZMA_IS_REP_G1 (LZMA_IS_REP_G0 + LZMA_NUM_STATES)
#define LZMA_IS_REP_G2 (LZMA_IS_REP_G1 + LZMA_NUM_STATES)
#define LZMA_IS_REP0_LONG (LZMA_IS_REP_G2 + LZMA_NUM_STATES)
#define LZMA_POS_SLOT (LZMA_IS_REP0_LONG + (LZMA_NUM_STATES << LZMA_NUM_POS_BITS_MAX))
#define LZMA_SPEC_POS (LZMA_POS_SLOT + (LZMA_NUM_LEN_TO_POS_STATES << LZMA_NUM_POS_SLOT_BITS))
#define LZMA_ALIGN (LZMA_SPEC_POS + LZMA_NUM_FULL_DISTANCES - LZMA_END_POS_MODEL_INDEX)
#define LZMA_LEN_CODER (LZMA_ALIGN + LZMA_ALIGN_TABLE_SIZE)
#define LZMA_REP_LEN_CODER (LZMA_LEN_CODER + LZMA_NUM_LEN_PROBS)
#define LZMA_LITERAL (LZMA_REP_LEN_CODER + LZMA_NUM_LEN_PROBS)

/* LZMA_REQUIRED_IN_BUFFER_SIZE = number of required input bytes for worst case:
   longest match with longest distance.
   kLzmaInBufferSize must be larger than LZMA_REQUIRED_IN_BUFFER_SIZE
   23 bits = 2 (match select) + 10 (len) + 6 (distance) + 4 (align) + 1 (RC_NORMALIZE)
*/
#define LZMA_REQUIRED_IN_BUFFER_SIZE ((23 * (LZMA_NUM_BIT_MODEL_TOTAL_BITS \
        - LZMA_NUM_MOVE_BITS + 1) + 26 + 9) / 8)


/***************
  Sanity checks
 ***************/

#if LZMA_LITERAL != LZMA_BASE_SIZE
#error BUG: LZMA_LITERAL != LZMA_BASE_SIZE
#endif

#if LZMA_IN_BUFFER_SIZE <= LZMA_REQUIRED_IN_BUFFER_SIZE
#error LZMA_IN_BUFFER_SIZE <= LZMA_REQUIRED_IN_BUFFER_SIZE
#error Fix by increasing LZMA_IN_BUFFER_SIZE.
#endif



/********
  Macros
 ********/

#define MIN(x,y) ((x) < (y) ? (x) : (y))


/**********
  typedefs
 **********/

typedef struct {
    /* LZMA_REQUIRED_IN_BUFFER_SIZE is added to LZMA_IN_BUFFER_SIZE for
       buffer overflow protection. I'm not 100% if it is really needed
       (I haven't studied the details enough) but allocating a few extra
       bytes shouldn't harm anyone. --Larhzu */
    unsigned char buffer[LZMA_IN_BUFFER_SIZE + LZMA_REQUIRED_IN_BUFFER_SIZE];

    /* Pointer to the current position in buffer[] */
    unsigned char *buffer_position;

    /* In the original version from LZMA SDK buffer_size had
       to be signed. In liblzmadec this should be unsigned. */
    size_t buffer_size;

    /* We don't know the properties of the stream we are going to
       decode in lzmadec_decompressInit. The needed memory
       will be allocated on first call to lzmadec_decode.
       status is used to check if we have parsed the header and
       allocated the memory needed by the LZMA decoder engine. */
    int_fast8_t status;

    uint_fast32_t dictionary_size;
    uint8_t *dictionary;

    uint_fast64_t uncompressed_size;
    int_fast8_t streamed; /* boolean */

    uint32_t pos_state_mask;
    uint32_t literal_pos_mask;
    uint_fast8_t pb;
    uint_fast8_t lp;
    uint_fast8_t lc;

    CProb *probs;

    uint32_t range;
    uint32_t code;
    uint_fast32_t dictionary_position;
    uint32_t distance_limit;
    uint32_t rep0, rep1, rep2, rep3;
    int state;
    int len;
} lzmadec_state;

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// lzma_main.c //////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

/******************************************************************************

    LZMA decoder library with a zlib like API

    Copyright (C) 1999-2005 Igor Pavlov (http://7-zip.org/)
    Copyright (C) 2005 Lasse Collin <lasse.collin@tukaani.org>
    Based on zlib.h and bzlib.h. FIXME

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

******************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef _MSC_VER
#pragma warning( disable: 4100 )
#endif

/* FIXME DEBUG */
#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "lzmadec.h"
//#include "private.h"

#ifndef UINT64_MAX
#define UINT64_MAX (~(uint64_t)0)
#endif

/* Cleaner way to refer to strm->state */
#define STATE ((lzmadec_state*)(strm->state))

static void *lzmadec_alloc (void *opaque, size_t nmemb, size_t size);
static void lzmadec_free (void *opaque, void *addr);
static int_fast8_t lzmadec_internal_init (lzmadec_stream *strm);
static /*inline*/ int_fast8_t lzmadec_decode_main (
        lzmadec_stream *strm,
        const int_fast8_t finish_decoding);
static int_fast8_t lzmadec_header_properties (
        uint_fast8_t *pb,
        uint_fast8_t *lp,
        uint_fast8_t *lc,
        const uint8_t c);
static int_fast8_t lzmadec_header_dictionary (
        uint_fast32_t *size,
        const uint8_t *buffer);
static void lzmadec_header_uncompressed (
        uint_fast64_t *size,
        int_fast8_t *is_streamed,
        const uint8_t *buffer);


/******************
  extern functions
 ******************/

/* This function doesn't do much but it's here to be as close to zlib
   as possible. See lzmadec_internal_init for actual initialization. */
extern int_fast8_t
lzmadec_init (lzmadec_stream *strm)
{
    /* Set the functions */
    if (strm->lzma_alloc == NULL)
        strm->lzma_alloc = lzmadec_alloc;
    if (strm->lzma_free == NULL)
        strm->lzma_free = lzmadec_free;
    strm->total_in = 0;
    strm->total_out = 0;

    /* Allocate memory for internal state structure */
    strm->state = (lzmadec_state*)((strm->lzma_alloc)(strm->opaque, 1,
            sizeof (lzmadec_state)));
    if (strm->state == NULL)
        return LZMADEC_MEM_ERROR;
    /* We will allocate memory and put the pointers in probs and
       dictionary later. Before that, make it clear that they contain
       no valid pointer yet. */
    STATE->probs = NULL;
    STATE->dictionary = NULL;

    /* Mark that the decoding engine is not yet initialized. */
    STATE->status = LZMADEC_STATUS_UNINITIALIZED;

    /* Initialize the internal data if there is enough input available */
    if (strm->avail_in >= LZMA_MINIMUM_COMPRESSED_FILE_SIZE) {
        return (lzmadec_internal_init (strm));
    }

    return LZMADEC_OK;
}

extern int_fast8_t
lzmadec_decode (lzmadec_stream *strm, const int_fast8_t finish_decoding)
{
    switch (STATE->status) {
        case LZMADEC_STATUS_UNINITIALIZED:
            if (strm->avail_in < LZMA_MINIMUM_COMPRESSED_FILE_SIZE)
                return LZMADEC_BUF_ERROR;
            if (lzmadec_internal_init (strm) != LZMADEC_OK)
                return LZMADEC_HEADER_ERROR;
            /* Fall through */
        case LZMADEC_STATUS_RUNNING:
            /* */
            if (strm->total_out < STATE->uncompressed_size)
                break;
            if (strm->total_out > STATE->uncompressed_size)
                return LZMADEC_DATA_ERROR;
            STATE->status = LZMADEC_STATUS_STREAM_END;
            /* Fall through */
        case LZMADEC_STATUS_FINISHING:
            /* Sanity check */
            if (!finish_decoding)
                return LZMADEC_SEQUENCE_ERROR;
            if (strm->total_out > STATE->uncompressed_size)
                return LZMADEC_DATA_ERROR;
            if (strm->total_out <  STATE->uncompressed_size)
                break;
            /* Fall through */
        case LZMADEC_STATUS_STREAM_END:
            return LZMADEC_STREAM_END;
        case LZMADEC_STATUS_ERROR:
        default:
            return LZMADEC_SEQUENCE_ERROR;
    }
    /* Let's decode! */
    return (lzmadec_decode_main(strm, finish_decoding));
}

extern int_fast8_t
lzmadec_end (lzmadec_stream *strm)
{
    (strm->lzma_free)(strm->opaque, STATE->dictionary);
    STATE->dictionary = NULL;
    (strm->lzma_free)(strm->opaque, STATE->probs);
    STATE->probs = NULL;
    (strm->lzma_free)(strm->opaque, strm->state);
    strm->state = NULL;
    return LZMADEC_OK;
}

extern int_fast8_t
lzmadec_buffer_info (lzmadec_info *info, const uint8_t *buffer,
    const size_t len)
{
    /* LZMA header is 13 bytes long. */
    if (len < 13)
        return LZMADEC_BUF_ERROR;
    if (lzmadec_header_properties (&info->pb, &info->lp, &info->lc,
            buffer[0]) != LZMADEC_OK)
        return LZMADEC_HEADER_ERROR;
    if (LZMADEC_OK != lzmadec_header_dictionary (
            &info->dictionary_size, buffer + 1))
        return LZMADEC_HEADER_ERROR;
    lzmadec_header_uncompressed (&info->uncompressed_size,
            &info->is_streamed, buffer + 5);
    return LZMADEC_OK;
}


/*******************
  Memory allocation
 *******************/

/* Default function for allocating memory */
static void *
lzmadec_alloc (__attribute__ ((unused)) void *opaque,
    size_t nmemb, size_t size)
{
    return (malloc (nmemb * size)); /* No need to zero the memory. */
}

/* Default function for freeing memory */
static void
lzmadec_free (__attribute__ ((unused)) void *opaque, void *addr)
{
    free (addr);
}


/****************
  Header parsing
 ****************/

/* Parse the properties byte */
static int_fast8_t
lzmadec_header_properties (
    uint_fast8_t *pb, uint_fast8_t *lp, uint_fast8_t *lc, const uint8_t c)
{
    /* pb, lp and lc are encoded into a single byte. */
    if (c > (9 * 5 * 5))
        return LZMADEC_HEADER_ERROR;
    *pb = c / (9 * 5);        /* 0 <= pb <= 4 */
    *lp = (c % (9 * 5)) / 9;  /* 0 <= lp <= 4 */
    *lc = c % 9;              /* 0 <= lc <= 8 */
    assert (*pb < 5 && *lp < 5 && *lc < 9);
    return LZMADEC_OK;
}

/* Parse the dictionary size (4 bytes, little endian) */
static int_fast8_t
lzmadec_header_dictionary (uint_fast32_t *size, const uint8_t *buffer)
{
    uint_fast32_t i;
    *size = 0;
    for (i = 0; i < 4; i++)
        *size += (uint_fast32_t)(*buffer++) << (i * 8);
    /* The dictionary size is limited to 256 MiB (checked from
       LZMA SDK 4.30) */
    if (*size > (1 << 28))
        return LZMADEC_HEADER_ERROR;
    return LZMADEC_OK;
}

/* Parse the uncompressed size field (8 bytes, little endian) */
static void
lzmadec_header_uncompressed (uint_fast64_t *size, int_fast8_t *is_streamed,
    const uint8_t *buffer)
{
    uint_fast32_t i;
    /* Streamed files have all 64 bits set in the size field.
       We don't know the uncompressed size beforehand. */
    *is_streamed = 1; /* Assume streamed. */
    *size = 0;
    for (i = 0; i < 8; i++) {
        *size += (uint_fast64_t)buffer[i] << (i * 8);
        if (buffer[i] != 255)
            *is_streamed = 0;
    }
    assert ((*is_streamed == 1 && *size == UINT64_MAX)
            || (*is_streamed == 0 && *size < UINT64_MAX));
}

/* Because the LZMA decoder cannot be initialized in practice by
   lzmadec_decode_init(), lzmadec_internal_init()
   is run when lzmadec_decompress() is called the first time.
   lzmadec_decompress() provides the FIXME FIXME FIXME
   is because initialization needs to know how much to allocate memory.
   This function reads the first 18 (LZMA_MINIMUM_COMPRESSED_FILE_SIZE)
   bytes of an LZMA stream, parses it, allocates the required memory and
   initializes the internal variables to a good values. 18 bytes is also
   the size of the smallest possible LZMA encoded stream. */
static int_fast8_t
lzmadec_internal_init (lzmadec_stream *strm)
{
    uint_fast32_t i;
    uint32_t num_probs;
    size_t lzmadec_num_probs;

    /* Make sure we have been called sanely */
    if (STATE->probs != NULL || STATE->dictionary != NULL
            || STATE->status != LZMADEC_STATUS_UNINITIALIZED)
        return LZMADEC_SEQUENCE_ERROR;

    /* Check that we have enough input */
    if (strm->avail_in < LZMA_MINIMUM_COMPRESSED_FILE_SIZE)
        return LZMADEC_BUF_ERROR;

    /* Parse the header (13 bytes) */
    /* - Properties (the first byte) */
    if (lzmadec_header_properties (&STATE->pb, &STATE->lp, &STATE->lc,
            *strm->next_in) != LZMADEC_OK)
        return LZMADEC_HEADER_ERROR;

    strm->next_in++;
    strm->avail_in--;
    /* - Calculate these right away: */
    STATE->pos_state_mask = (1 << STATE->pb) - 1;
    STATE->literal_pos_mask = (1 << STATE->lp) - 1;
    /* - Dictionary size */
    lzmadec_header_dictionary (&STATE->dictionary_size, strm->next_in);
    strm->next_in += 4;
    strm->avail_in -= 4;
    /* - Uncompressed size */
    lzmadec_header_uncompressed (&STATE->uncompressed_size,
            &STATE->streamed, strm->next_in);
    strm->next_in += 8;
    strm->avail_in -= 8;

    /* Allocate memory for internal data */
    lzmadec_num_probs = (LZMA_BASE_SIZE
            + (LZMA_LIT_SIZE << (STATE->lc + STATE->lp)));
    STATE->probs = (CProb *)((strm->lzma_alloc)(strm->opaque, 1,
            lzmadec_num_probs * sizeof(CProb)));
    if (STATE->probs == NULL)
        return LZMADEC_MEM_ERROR;

    /* When dictionary_size == 0, it must be set to 1. */
    if (STATE->dictionary_size == 0)
        STATE->dictionary_size = 1;
    /* Allocate dictionary */
    STATE->dictionary = (unsigned char*)((strm->lzma_alloc)(
            strm->opaque, 1, STATE->dictionary_size));
    if (STATE->dictionary == NULL) {
        /* First free() the memory allocated for internal data */
        (strm->lzma_free)(strm->opaque, STATE->probs);
        return LZMADEC_MEM_ERROR;
    }

    /* Initialize the internal data */
    num_probs = LZMA_BASE_SIZE
            + ((CProb)LZMA_LIT_SIZE << (STATE->lc + STATE->lp));
    for (i = 0; i < num_probs; i++)
        STATE->probs[i] = 1024; /* LZMA_BIT_MODEL_TOTAL >> 1; */

    /* Read the first five bytes of data and initialize STATE->code */
    STATE->code = 0;
    for (i = 0; i < 5; i++)
        STATE->code = (STATE->code << 8) | (uint32_t)(*strm->next_in++);
    strm->avail_in -= 5;

    /* Zero the buffer[] */
    memset (STATE->buffer, 0,
            LZMA_IN_BUFFER_SIZE + LZMA_REQUIRED_IN_BUFFER_SIZE);

    /* Set the initial static values */
    STATE->rep0 = 1;
    STATE->rep1 = 1;
    STATE->rep2 = 1;
    STATE->rep3 = 1;
    STATE->state = 0;
    strm->total_out = 0;
    STATE->distance_limit = 0;
    STATE->dictionary_position = 0;
    STATE->dictionary[STATE->dictionary_size - 1] = 0;
    STATE->buffer_size = 0;
    STATE->buffer_position = STATE->buffer;
    STATE->len = 0;
    STATE->range = 0xFFFFFFFF;

    /* Mark that initialization has been done */
    STATE->status = LZMADEC_STATUS_RUNNING;

    return LZMADEC_OK;
}


/*********************
  LZMA decoder engine
 *********************/

/* Have a nice day! */

#define RC_NORMALIZE \
        if (range < LZMA_TOP_VALUE) { \
            range <<= 8; \
            code = (code << 8) | *buffer++; \
        }

#define IfBit0(p) \
        RC_NORMALIZE; \
        bound = (range >> LZMA_NUM_BIT_MODEL_TOTAL_BITS) * *(p); \
        if (code < bound)

#define UpdateBit0(p) \
        range = bound; \
        *(p) += (LZMA_BIT_MODEL_TOTAL - *(p)) >> LZMA_NUM_MOVE_BITS;

#define UpdateBit1(p) \
        range -= bound; \
        code -= bound; \
        *(p) -= (*(p)) >> LZMA_NUM_MOVE_BITS;

#define RC_GET_BIT2(p, mi, A0, A1) \
        IfBit0(p) { \
            UpdateBit0(p); \
            mi <<= 1; \
            A0; \
        } else { \
            UpdateBit1(p); \
            mi = (mi + mi) + 1; \
            A1; \
        }

#define RC_GET_BIT(p, mi) RC_GET_BIT2(p, mi, ; , ;)

#define RangeDecoderBitTreeDecode(probs, numLevels, res) \
        { \
            int _i = numLevels; \
            res = 1; \
            do { \
                CProb *_p = probs + res; \
                RC_GET_BIT(_p, res) \
            } while(--_i != 0); \
            res -= (1 << numLevels); \
        }

static int_fast8_t
lzmadec_decode_main (lzmadec_stream *strm, const int_fast8_t finish_decoding)
{
    /* Split the *strm structure to separate _local_ variables.
       This improves readability a little. The major reason to do
       this is performance; at least with GCC 3.4.4 this makes
       the code about 30% faster! */
    /* strm-> */
    unsigned char *next_out = strm->next_out;
    unsigned char *next_in = strm->next_in;
    size_t avail_in = strm->avail_in;
    uint64_t total_out = strm->total_out;
    /* strm->state-> */
    const int_fast8_t lc = STATE->lc;
    const uint32_t pos_state_mask = STATE->pos_state_mask;
    const uint32_t literal_pos_mask = STATE->literal_pos_mask;
    const uint32_t dictionary_size = STATE->dictionary_size;
    unsigned char *dictionary = STATE->dictionary;
/*	int_fast8_t streamed;*/ /* boolean */
    CProb *p = STATE->probs;
    uint32_t range = STATE->range;
    uint32_t code = STATE->code;
    uint32_t dictionary_position = STATE->dictionary_position;
    uint32_t distance_limit = STATE->distance_limit;
    uint32_t rep0 = STATE->rep0;
    uint32_t rep1 = STATE->rep1;
    uint32_t rep2 = STATE->rep2;
    uint32_t rep3 = STATE->rep3;
    int state = STATE->state;
    int len = STATE->len;
    unsigned char *buffer_start = STATE->buffer;
    size_t buffer_size = STATE->buffer_size;
    /* Other variable initializations */
    int_fast8_t i; /* Temporary variable for loop indexing */
    unsigned char *next_out_end = next_out + strm->avail_out;
    unsigned char *buffer = STATE->buffer_position;

    /* This should have been verified in lzmadec_decode() already: */
    assert (STATE->uncompressed_size > total_out);
    /* With non-streamed LZMA stream the output has to be limited. */
    if (STATE->uncompressed_size - total_out < strm->avail_out) {
        next_out_end = next_out + (STATE->uncompressed_size - total_out);
    }

    /* The main loop */
    while (1) {
assert (len >= 0);
assert (state >= 0);
        /* Copy uncompressed data to next_out: */
        {
            unsigned char *foo = next_out;
            while (len != 0 && next_out != next_out_end) {
                uint32_t pos = dictionary_position - rep0;
                if (pos >= dictionary_size)
                    pos += dictionary_size;
                *next_out++ = dictionary[dictionary_position] = dictionary[pos];
                if (++dictionary_position == dictionary_size)
                    dictionary_position = 0;
                len--;
            }
            total_out += next_out - foo;
        }

        /* Fill the internal input buffer: */
        {
            size_t avail_buf;
            /* Check for overflow (invalid input) */
            if (buffer > buffer_start + LZMA_IN_BUFFER_SIZE)
                return LZMADEC_DATA_ERROR;
            /* Calculate how much data is unread in the buffer: */
            avail_buf = buffer_size - (buffer - buffer_start);

            /* Copy more data to the buffer if needed: */
            if (avail_buf < LZMA_REQUIRED_IN_BUFFER_SIZE) {
                const size_t copy_size = MIN (avail_in,
                        LZMA_IN_BUFFER_SIZE - avail_buf);
                if (avail_buf > 0)
                    memmove (buffer_start, buffer, avail_buf);
                memcpy (buffer_start + avail_buf,
                        next_in, copy_size);
                buffer = buffer_start;
                next_in += copy_size;
                avail_in -= copy_size;
                buffer_size = avail_buf + copy_size;
            }
        }

        /* Decoder cannot continue if there is
           - no output space available
           - less data in the input buffer than a single decoder pass
             could consume; decoding is still continued if the callee
             has marked that all available input data has been given. */
        if ((next_out == next_out_end)
                || (!finish_decoding
                && buffer_size < LZMA_REQUIRED_IN_BUFFER_SIZE))
            break;

        assert (STATE->status != LZMADEC_STATUS_FINISHING);

        /* The rest of the main loop can at maximum
           - read at maximum of LZMA_REQUIRED_IN_BUFFER_SIZE bytes
             from the buffer[]
           - write one byte to next_out. */
        {
            CProb *prob;
            uint32_t bound;
            int_fast32_t posState = (int_fast32_t)(total_out & pos_state_mask);
            prob = p + LZMA_IS_MATCH + (state << LZMA_NUM_POS_BITS_MAX) + posState;
            IfBit0(prob) {
                int_fast32_t symbol = 1;
                UpdateBit0(prob)
                prob = p + LZMA_LITERAL + (LZMA_LIT_SIZE *
                    (((total_out & literal_pos_mask) << lc)
                    + ((dictionary_position != 0
                    ? dictionary[dictionary_position - 1]
                    : dictionary[dictionary_size - 1])
                    >> (8 - lc))));
                if (state >= LZMA_NUM_LIT_STATES) {
                    int_fast32_t matchByte;
                    uint32_t pos = dictionary_position - rep0;
                    if (pos >= dictionary_size)
                        pos += dictionary_size;
                    matchByte = dictionary[pos];
                    do {
                        int_fast32_t bit;
                        CProb *probLit;
                        matchByte <<= 1;
                        bit = (matchByte & 0x100);
                        probLit = prob + 0x100 + bit + symbol;
                        RC_GET_BIT2(probLit, symbol,
                            if (bit != 0) break,
                            if (bit == 0) break)
                    } while (symbol < 0x100);
                }
                while (symbol < 0x100) {
                    CProb *probLit = prob + symbol;
                    RC_GET_BIT(probLit, symbol)
                }

                if (distance_limit < dictionary_size)
                    distance_limit++;

                /* Eliminate? */
                *next_out++ = dictionary[dictionary_position]
                        = (char)symbol;
                if (++dictionary_position == dictionary_size)
                    dictionary_position = 0;
                total_out++;

                if (state < 4)
                    state = 0;
                else if (state < 10)
                    state -= 3;
                else
                    state -= 6;

                continue;
            }

            UpdateBit1(prob);
            prob = p + LZMA_IS_REP + state;
            IfBit0(prob) {
                UpdateBit0(prob);
                rep3 = rep2;
                rep2 = rep1;
                rep1 = rep0;
                state = state < LZMA_NUM_LIT_STATES ? 0 : 3;
                prob = p + LZMA_LEN_CODER;
            } else {
                UpdateBit1(prob);
                prob = p + LZMA_IS_REP_G0 + state;
                IfBit0(prob) {
                    UpdateBit0(prob);
                    prob = p + LZMA_IS_REP0_LONG + (state
                            << LZMA_NUM_POS_BITS_MAX)
                            + posState;
                    IfBit0(prob) {
                        UpdateBit0(prob);
                        if (distance_limit == 0)
                            return LZMADEC_DATA_ERROR;
                        if (distance_limit < dictionary_size)
                            distance_limit++;
                        state = state < LZMA_NUM_LIT_STATES ? 9 : 11;
                        len++;
                        continue;
                    } else {
                        UpdateBit1(prob);
                    }
                } else {
                    uint32_t distance;
                    UpdateBit1(prob);
                    prob = p + LZMA_IS_REP_G1 + state;
                    IfBit0(prob) {
                        UpdateBit0(prob);
                        distance = rep1;
                    } else {
                        UpdateBit1(prob);
                        prob = p + LZMA_IS_REP_G2 + state;
                        IfBit0(prob) {
                            UpdateBit0(prob);
                            distance = rep2;
                        } else {
                            UpdateBit1(prob);
                            distance = rep3;
                            rep3 = rep2;
                        }
                        rep2 = rep1;
                    }
                    rep1 = rep0;
                    rep0 = distance;
                }
                state = state < LZMA_NUM_LIT_STATES ? 8 : 11;
                prob = p + LZMA_REP_LEN_CODER;
            }

            {
                int_fast32_t numBits, offset;
                CProb *probLen = prob + LZMA_LEN_CHOICE;
                IfBit0(probLen) {
                    UpdateBit0(probLen);
                    probLen = prob + LZMA_LEN_LOW
                            + (posState
                            << LZMA_LEN_NUM_LOW_BITS);
                    offset = 0;
                    numBits = LZMA_LEN_NUM_LOW_BITS;
                } else {
                    UpdateBit1(probLen);
                    probLen = prob + LZMA_LEN_CHOICE2;
                    IfBit0(probLen) {
                        UpdateBit0(probLen);
                        probLen = prob + LZMA_LEN_MID
                            + (posState
                            << LZMA_LEN_NUM_MID_BITS);
                        offset = LZMA_LEN_NUM_LOW_SYMBOLS;
                        numBits = LZMA_LEN_NUM_MID_BITS;
                    } else {
                        UpdateBit1(probLen);
                        probLen = prob + LZMA_LEN_HIGH;
                        offset = LZMA_LEN_NUM_LOW_SYMBOLS
                            + LZMA_LEN_NUM_MID_SYMBOLS;
                        numBits = LZMA_LEN_NUM_HIGH_BITS;
                    }
                }
                RangeDecoderBitTreeDecode(probLen, numBits, len);
                len += offset;
            }

            if (state < 4) {
                int_fast32_t posSlot;
                state += LZMA_NUM_LIT_STATES;
                prob = p + LZMA_POS_SLOT + (MIN (len,
                        LZMA_NUM_LEN_TO_POS_STATES - 1)
                        << LZMA_NUM_POS_SLOT_BITS);
                RangeDecoderBitTreeDecode(prob, LZMA_NUM_POS_SLOT_BITS, posSlot);
                if (posSlot >= LZMA_START_POS_MODEL_INDEX) {
                    int_fast32_t numDirectBits = ((posSlot >> 1) - 1);
                    rep0 = (2 | ((uint32_t)posSlot & 1));
                    if (posSlot < LZMA_END_POS_MODEL_INDEX) {
                        rep0 <<= numDirectBits;
                        prob = p + LZMA_SPEC_POS + rep0 - posSlot - 1;
                    } else {
                        numDirectBits -= LZMA_NUM_ALIGN_BITS;
                        do {
                            RC_NORMALIZE
                            range >>= 1;
                            rep0 <<= 1;
                            if (code >= range) {
                                code -= range;
                                rep0 |= 1;
                            }
                        } while (--numDirectBits != 0);
                        prob = p + LZMA_ALIGN;
                        rep0 <<= LZMA_NUM_ALIGN_BITS;
                        numDirectBits = LZMA_NUM_ALIGN_BITS;
                    }
                    {
                        int_fast32_t mi = 1;
                        i = 1;
                        do {
                            CProb *prob3 = prob + mi;
                            RC_GET_BIT2(prob3, mi, ; , rep0 |= i);
                            i <<= 1;
                        } while(--numDirectBits != 0);
                    }
                } else {
                    rep0 = posSlot;
                }
                if (++rep0 == (uint32_t)(0)) {
                    /* End of stream marker detected */
                    STATE->status = LZMADEC_STATUS_STREAM_END;
                    break;
                }
            }

            if (rep0 > distance_limit)
                return LZMADEC_DATA_ERROR;

            len += LZMA_MATCH_MIN_LEN;
            if (dictionary_size - distance_limit > (uint32_t)(len))
                distance_limit += len;
            else
                distance_limit = dictionary_size;
        }
    }
    RC_NORMALIZE;

    if (STATE->uncompressed_size < total_out) {
        STATE->status = LZMADEC_STATUS_ERROR;
        return LZMADEC_DATA_ERROR;
    }

    /* Store the saved values back to the lzmadec_stream structure. */
    strm->total_in += (strm->avail_in - avail_in);
    strm->total_out = total_out;
    strm->avail_in = avail_in;
    strm->avail_out -= (next_out - strm->next_out);
    strm->next_in = next_in;
    strm->next_out = next_out;
    STATE->range = range;
    STATE->code = code;
    STATE->rep0 = rep0;
    STATE->rep1 = rep1;
    STATE->rep2 = rep2;
    STATE->rep3 = rep3;
    STATE->state = state;
    STATE->len = len;
    STATE->dictionary_position = dictionary_position;
    STATE->distance_limit = distance_limit;
    STATE->buffer_size = buffer_size;
    STATE->buffer_position = buffer;

    if (STATE->status == LZMADEC_STATUS_STREAM_END
            || STATE->uncompressed_size == total_out) {
        STATE->status = LZMADEC_STATUS_STREAM_END;
        if (len == 0)
            return LZMADEC_STREAM_END;
    }
    return LZMADEC_OK;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// buffer.c //////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

/******************************************************************************

    Decode the whole source buffer at once

    Copyright (C) 2005 Lasse Collin <lasse.collin@tukaani.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

******************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include "lzmadec.h"
//#include "private.h"

extern int_fast8_t
lzmadec_buffer (uint8_t *dest, size_t *dest_len,
    uint8_t *source, const size_t source_len)
{
    lzmadec_stream strm;
    int8_t ret;

    /* Initialize the decoder */
    strm.next_in = source;
    strm.avail_in = source_len;
    strm.next_out = dest;
    strm.avail_out = *dest_len;
    strm.lzma_alloc = NULL;
    strm.lzma_free = NULL;
    strm.opaque = NULL;
    ret = lzmadec_init (&strm);
    if (ret != LZMADEC_OK)
        return ret;

    /* Check that the destination buffer is big enough. With streamed
       LZMA data we can only hope it is big enough before starting
       the decoding process; if it is too small, we will return
       LZMADEC_BUF_ERROR after decoding dest_len bytes. */
    if (strm.avail_out
            < ((lzmadec_state*)(strm.state))->uncompressed_size)
        return LZMADEC_BUF_ERROR; /* Too small destination buffer */

    /* Call the decoder. One pass is enough if everything is OK. */
    ret = lzmadec_decode (&strm, 1);

    /* Set *dest_len to amount of bytes actually decoded. */
    assert (*dest_len >= strm.avail_out);
    *dest_len -= strm.avail_out;

    /* Free the allocated memory no matter did the decoding
       go well or not. */
    lzmadec_end (&strm);

    /* Check the return value of lzmadec_decode() and return appropriate
       return value */
    switch (ret) {
        case LZMADEC_STREAM_END:
            /* Everything has been decoded and put to
               the destination buffer. */
            return LZMADEC_OK;
        case LZMADEC_OK:
            /* Decoding went fine so far but not all of the
               uncompressed data did fit to the destination
               buffer. This should happen only with streamed LZMA
               data (otherwise liblzmadec might have a bug). */
            assert (((lzmadec_state*)(strm.state))->streamed == 1);
            return LZMADEC_BUF_ERROR;
        default:
            assert (ret == LZMADEC_DATA_ERROR);
            return LZMADEC_DATA_ERROR;
    }
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// io.c //////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

/******************************************************************************

    LZMA decoder library with a zlib like API - lzma_FILE I/O functions

    Copyright (C) 1999-2005 Igor Pavlov (http://7-zip.org/)
    Copyright (C) 2005 Lasse Collin <lasse.collin@tukaani.org>
    Based on zlib.h and bzlib.h. FIXME

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

******************************************************************************/

#ifndef LZMADEC_NO_STDIO

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifdef HAVE_ERRNO_H
#include <errno.h>
#else
extern int errno
#endif

#ifndef SIZE_MAX
#define SIZE_MAX (~(size_t)0)
#endif

#define LZMADEC_NO_STDIO
#include "lzmadec.h"
#undef LZMADEC_NO_STDIO

//#include "private.h"

#define LZMADEC_BUFSIZE (LZMA_IN_BUFFER_SIZE - LZMA_REQUIRED_IN_BUFFER_SIZE)

#define LZMADEC_IO_STATUS_OK	0
#define LZMADEC_IO_STATUS_EOF	1
#define LZMADEC_IO_STATUS_ERROR	2

typedef struct {
    lzmadec_stream strm;
    FILE *file;
    uint8_t buffer[LZMADEC_BUFSIZE];
    int_fast8_t status;
} lzmadec_FILE;


/****************************
  Opening and closing a file
 ****************************/

/* This is used by lzmadec_open() and lzmadec_dopen(). */
static lzmadec_FILE *
lzmadec_open_init (lzmadec_FILE *lfile)
{
    /* Check if the file was opened successfully */
    if (lfile->file == NULL) {
        int saved_errno = errno;
        free (lfile);
        errno = saved_errno;
        return NULL; /* Caller can read errno */
    }
    /* Initialize the decoder */
    lfile->strm.lzma_alloc = NULL;
    lfile->strm.lzma_free = NULL;
    lfile->strm.opaque = NULL;
    lfile->strm.avail_in = 0;
    lfile->strm.avail_out = 0;
    if (lzmadec_init (&lfile->strm) != LZMADEC_OK) {
        fclose (lfile->file);
        free (lfile);
        /* Set errno like fopen(2) (and malloc(3)) would set it: */
        errno = ENOMEM;
        return NULL; /* Caller can see faked malloc()'s errno */
    }
    /* Not yet at the end of the stream. */
    lfile->status = LZMADEC_IO_STATUS_OK;
    return lfile;
}

extern lzmadec_FILE *
lzmadec_open (const char *path)
{
    /* Allocate memory for the lzmadec_FILE */
    lzmadec_FILE *lfile = malloc (sizeof (lzmadec_FILE));
    if (lfile == NULL)
        return NULL;
    /* Open the file */
    lfile->file = fopen (path, "rb");
    /* The rest is shared with lzmadec_open() */
    return lzmadec_open_init (lfile);
}

extern lzmadec_FILE *
lzmadec_dopen (int fd)
{
    /* Allocate memory for the lzmadec_FILE */
    lzmadec_FILE *lfile = malloc (sizeof (lzmadec_FILE));
    if (lfile == NULL)
        return NULL;
    /* Open the file */
    lfile->file = fdopen (fd, "rb");
    /* The rest is shared with lzmadec_open() */
    return lzmadec_open_init (lfile);
}

extern int_fast8_t
lzmadec_close (lzmadec_FILE *lfile)
{
    /* Simple check that lfile looks like a valid lzmadec_FILE. */
    if (lfile == NULL || lfile->strm.state == NULL)
        return -1;
    lzmadec_end (&lfile->strm);
    fclose (lfile->file);
    lfile->file = NULL;
    free (lfile);
    return 0;
}


/****************
  Reading a file
 ****************/

extern ssize_t
lzmadec_read (lzmadec_FILE *lfile, uint8_t *buf, const size_t len)
{
    int_fast8_t ret;
    /* Simple check that lfile looks like a valid lzmadec_FILE. */
    if (lfile == NULL || lfile->strm.state == NULL)
        return -1;
    /* Check status */
    if (lfile->status == LZMADEC_IO_STATUS_ERROR)
        return -1;
    if (lfile->status == LZMADEC_IO_STATUS_EOF)
        return 0;
    /* The return value is ssize_t so we limit the maximum read size. */
    lfile->strm.avail_out = MIN (len, SIZE_MAX / 2 - 1);
    lfile->strm.next_out = buf;
    do {
        if (lfile->strm.avail_in == 0) {
            lfile->strm.next_in = lfile->buffer;
            lfile->strm.avail_in = fread (lfile->buffer,
                    sizeof (uint8_t), LZMADEC_BUFSIZE,
                    lfile->file);
        }
        ret = lzmadec_decode (&lfile->strm, lfile->strm.avail_in == 0);
    } while (lfile->strm.avail_out != 0 && ret == LZMADEC_OK);
    if (ret == LZMADEC_STREAM_END)
        lfile->status = LZMADEC_IO_STATUS_EOF;
    if (ret < 0)
        return -1; /* FIXME: errno? */
    return (len - lfile->strm.avail_out);
}

/* Read until '\n' or '\0' or at maximum of len bytes.
   Slow implementation, similar to what is in zlib. */
extern uint8_t *
lzmadec_gets (lzmadec_FILE *lfile, uint8_t *buf, size_t len)
{
    int_fast8_t ret;
    uint8_t *buf_start = buf;
    /* Sanity checks */
    if (buf == NULL || len < 1)
        return NULL;
    if (lfile == NULL || lfile->strm.state == NULL)
        return NULL;
    /* Read byte by byte (sloooow) and stop when 1) buf is full
       2) end of file 3) '\n' or '\0' is found. */
    while (--len > 0) {
        ret = lzmadec_read (lfile, buf, 1);
        if (ret != 1) {
            /* Error checking: 1) decoding error or 2) end of file
               and no characters were read. */
            if (ret < 0 || buf == buf_start)
                return NULL;
            break;
        }
        if (*buf == '\0')
            return buf_start;
        if (*buf++ == '\n')
            break;
    }
    *buf = '\0';
    return buf_start;
}

extern int
lzmadec_getc (lzmadec_FILE *lfile)
{
    uint8_t c;
    if (lzmadec_read (lfile, &c, 1) == 0)
        return -1;
    return (int)(c);
}


/*******
  Other
 *******/

extern off_t
lzmadec_tell (lzmadec_FILE *lfile)
{
    /* Simple check that lfile looks like a valid lzmadec_FILE. */
    if (lfile == NULL || lfile->strm.state == NULL)
        return -1;
    return (off_t)(lfile->strm.total_out);
}

extern int_fast8_t
lzmadec_eof (lzmadec_FILE *lfile)
{
    /* Simple check that lfile looks like a valid lzmadec_FILE. */
    if (lfile == NULL || lfile->strm.state == NULL)
        return -1;
    return lfile->status == LZMADEC_IO_STATUS_EOF;
}

extern int_fast8_t
lzmadec_rewind (lzmadec_FILE *lfile)
{
    /* Simple check that lfile looks like a valid lzmadec_FILE. */
    if (lfile == NULL || lfile->strm.state == NULL)
        return -1;
    /* Rewinding is done by closing the old lzmadec_stream
       and reinitializing it. */
    if (lzmadec_end (&lfile->strm) != LZMADEC_OK) {
        lfile->status = LZMADEC_IO_STATUS_ERROR;
        return -1;
    }
    rewind (lfile->file);
    if (lzmadec_init (&lfile->strm) != LZMADEC_OK) {
        lfile->status = LZMADEC_IO_STATUS_ERROR;
        return -1;
    }
    lfile->status = LZMADEC_IO_STATUS_OK;
    return 0;
}

extern off_t
lzmadec_seek (lzmadec_FILE *lfile, off_t offset, int whence)
{
    off_t oldpos = (off_t)(lfile->strm.total_out);
    off_t newpos;
    /* Simple check that lfile looks like a valid lzmadec_FILE. */
    if (lfile == NULL || lfile->strm.state == NULL)
        return -1;
    /* Get the new absolute position. */
    switch (whence) {
        case SEEK_SET:
            /* Absolute position must be >= 0. */
            if (offset < 0)
                return -1;
            newpos = offset;
            break;
        case SEEK_CUR:
            /* Need to be careful to avoid integer overflows. */
            if ((offset < 0 && (off_t)(-1 * offset) > oldpos)
                    ||
                    (offset > 0 && (off_t)(offset) + oldpos
                    < oldpos))
                return (off_t)(-1);
            newpos = (off_t)(lfile->strm.total_out) + offset;
            break;
        case SEEK_END:
            /* zlib doesn't support SEEK_END. However, liblzmadec
               provides this as a way to find out uncompressed
               size of a streamed file (streamed files don't have
               uncompressed size in their header). */
            newpos = -1;
            break;
        default:
            /* Invalid whence */
            errno = EINVAL;
            return -1;
    }
    /* Seeking with a valid whence value always clears
       the end of file indicator. */
    lfile->status = LZMADEC_IO_STATUS_OK;
    /* If the new absolute position is backward from current position,
       we need to rewind and uncompress from the beginning of the file.
       This is usually slow and thus not recommended. */
    if (whence != SEEK_END && newpos < oldpos) {
        if (lzmadec_rewind (lfile))
            return -1;
        oldpos = 0;
        assert (lfile->strm.total_out == 0);
    }
    /* Maybe we are lucky and don't need to seek at all. ;-) */
    if (newpos == oldpos)
        return oldpos;
    assert (newpos > oldpos || newpos == -1);
    /* Read as many bytes as needed to reach the requested position. */
    {
        /* strm.next_out cannot be NULL so use a temporary buffer. */
        uint8_t buf[LZMADEC_BUFSIZE];
        size_t req_size;
        ssize_t got_size;
        while (newpos > oldpos || newpos == -1) {
            req_size = MIN (LZMADEC_BUFSIZE, newpos - oldpos);
            got_size = lzmadec_read (lfile, buf, req_size);
            if (got_size != (ssize_t)(req_size)) {
                if (got_size < 0) {
                    return -1; /* Stream error */
                } else {
                    /* End of stream */
                    newpos = oldpos + got_size;
                    break;
                }
            }
            oldpos += got_size;
        };
    }
    assert (newpos == oldpos);
    assert ((off_t)(lfile->strm.total_out) == newpos);
    return newpos;
}

#endif /* ifndef LZMADEC_NO_STDIO */
