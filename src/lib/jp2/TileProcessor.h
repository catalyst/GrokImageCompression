/*
 *    Copyright (C) 2016-2020 Grok Image Compression Inc.
 *
 *    This source code is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This source code is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *    This source code incorporates work covered by the following copyright and
 *    permission notice:
 *
 * The copyright in this software is being made available under the 2-clauses
 * BSD License, included below. This software may be subject to other third
 * party and contributor rights, including patent rights, and no such rights
 * are granted under this license.
 *
 * Copyright (c) 2002-2014, Universite catholique de Louvain (UCL), Belgium
 * Copyright (c) 2002-2014, Professor Benoit Macq
 * Copyright (c) 2001-2003, David Janssens
 * Copyright (c) 2002-2003, Yannick Verschueren
 * Copyright (c) 2003-2007, Francois-Olivier Devaux
 * Copyright (c) 2003-2014, Antonin Descampe
 * Copyright (c) 2005, Herve Drolon, FreeImage Team
 * Copyright (c) 2008, 2011-2012, Centre National d'Etudes Spatiales (CNES), FR
 * Copyright (c) 2012, CS Systemes d'Information, France
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once
#include "testing.h"
#include <vector>

namespace grk {

// code segment (code block can be encoded into multiple segments)
struct grk_tcd_seg {
	grk_tcd_seg() {
		clear();
	}
	void clear() {
		dataindex = 0;
		numpasses = 0;
		len = 0;
		maxpasses = 0;
		numPassesInPacket = 0;
		numBytesInPacket = 0;
	}
	uint32_t dataindex;		      // segment data offset in contiguous memory block
	uint32_t numpasses;		    	// number of passes in segment
	uint32_t len;               // total length of segment
	uint32_t maxpasses;			  	// maximum number of passes in segment
	uint32_t numPassesInPacket;	  // number of passes contributed by current packet
	uint32_t numBytesInPacket;      // number of bytes contributed by current packet
};

struct grk_packet_length_info {
	grk_packet_length_info(uint32_t mylength, uint32_t bits) :
			len(mylength), len_bits(bits) {
	}
	grk_packet_length_info() :
			len(0), len_bits(0) {
	}
	bool operator==(grk_packet_length_info &rhs) const {
		return (rhs.len == len && rhs.len_bits == len_bits);
	}
	uint32_t len;
	uint32_t len_bits;
};

// encoding/decoding pass
struct grk_tcd_pass {
	grk_tcd_pass() :
			rate(0), distortiondec(0), len(0), term(0), slope(0) {
	}
	uint32_t rate;
	double distortiondec;
	uint32_t len;
	uint8_t term;
	uint16_t slope;  //ln(slope) in 8.8 fixed point
};

//quality layer
struct grk_tcd_layer {
	grk_tcd_layer() :
			numpasses(0), len(0), disto(0), data(nullptr) {
	}
	uint32_t numpasses; /* Number of passes in the layer */
	uint32_t len; /* len of information */
	double disto; /* add for index (Cfr. Marcela) */
	uint8_t *data; /* data buffer (points to code block data) */
};

const uint8_t cblk_compressed_data_pad_left = 2;

// encoder code block
struct grk_tcd_cblk_enc {
	grk_tcd_cblk_enc() :
			actualData(nullptr), data(nullptr), data_size(0), owns_data(false), layers(
					nullptr), passes(nullptr), x0(0), y0(0), x1(0), y1(0), numbps(
					0), numlenbits(0), num_passes_included_in_current_layer(0), num_passes_included_in_previous_layers(
					0), num_passes_encoded(0),
#ifdef DEBUG_LOSSLESS_T2
							included(0),
							packet_length_info(nullptr),
#endif
					contextStream(nullptr) {
	}
	~grk_tcd_cblk_enc();
	bool alloc();
	bool alloc_data(size_t nominalBlockSize);
	void cleanup();
	uint8_t *actualData;
	uint8_t *data; /* data buffer*/
	uint32_t data_size; /* size of allocated data buffer */
	bool owns_data;	// true if code block manages data buffer, otherwise false
	grk_tcd_layer *layers;
	grk_tcd_pass *passes;
	uint32_t x0, y0, x1, y1; /* dimension of the code block : left upper corner (x0, y0) right low corner (x1,y1) */
	uint32_t numbps;
	uint32_t numlenbits;
	uint32_t num_passes_included_in_current_layer; /* number of passes encoded in current layer */
	uint32_t num_passes_included_in_previous_layers; /* number of passes in previous layers */
	uint32_t num_passes_encoded; /* number of passes encoded */
	uint32_t *contextStream;
#ifdef DEBUG_LOSSLESS_T2
	uint32_t included;
	std::vector<grk_packet_length_info>* packet_length_info;
#endif
};

//decoder code block
struct grk_tcd_cblk_dec {
	grk_tcd_cblk_dec() {
		init();
	}

	grk_tcd_cblk_dec(const grk_tcd_cblk_enc &rhs) :
					segs(nullptr), x0(rhs.x0), y0(rhs.y0), x1(
					rhs.x1), y1(rhs.y1), numbps(rhs.numbps), numlenbits(
					rhs.numlenbits), numPassesInPacket(0), numSegments(0),
#ifdef DEBUG_LOSSLESS_T2
														 included(false),
														packet_length_info(nullptr),
#endif
					numSegmentsAllocated(0){
	}
	/**
	 * Allocates memory for a decoding code block (but not data)
	 */
	void init();
	bool alloc();
	void cleanup();
	grk_buf compressedData;
	grk_vec seg_buffers;
	grk_tcd_seg *segs; /* information on segments */
	uint32_t x0, y0, x1, y1; /* position: left upper corner (x0, y0) right low corner (x1,y1) */
	uint32_t numbps;
	uint32_t numlenbits;
	uint32_t numPassesInPacket; /* number of passes added by current packet */
	uint32_t numSegments; /* number of segment in block*/
	uint32_t numSegmentsAllocated; // number of segments allocated for segs array
#ifdef DEBUG_LOSSLESS_T2
	uint32_t included;
	std::vector<grk_packet_length_info>* packet_length_info;
#endif

};

// precinct
struct grk_tcd_precinct {
	grk_tcd_precinct() :
			x0(0), y0(0), x1(0), y1(0), cw(0), ch(0), block_size(0), incltree(
					nullptr), imsbtree(nullptr) {
		cblks.blocks = nullptr;
	}

	void initTagTrees();
	void deleteTagTrees();

	void cleanupEncodeBlocks() {
		if (!cblks.enc)
			return;
		for (uint64_t i = 0; i < (uint64_t) cw * ch; ++i)
			(cblks.enc + i)->cleanup();
		grok_free(cblks.blocks);
	}
	void cleanupDecodeBlocks() {
		if (!cblks.dec)
			return;
		grok_free(cblks.blocks);
	}

	uint32_t x0, y0, x1, y1; /* dimension of the precinct : left upper corner (x0, y0) right low corner (x1,y1) */
	uint32_t cw, ch; /* number of precinct in width and height */
	union { /* code-blocks information */
		grk_tcd_cblk_enc *enc;
		grk_tcd_cblk_dec *dec;
		void *blocks;
	} cblks;
	uint64_t block_size; /* size taken by cblks (in bytes) */
	TagTree *incltree; /* inclusion tree */
	TagTree *imsbtree; /* IMSB tree */
};

// band
struct grk_tcd_band {
	grk_tcd_band() :
			x0(0), y0(0), x1(0), y1(0), bandno(0), precincts(nullptr), numPrecincts(
					0), numAllocatedPrecincts(0), numbps(0), stepsize(0), inv_step(0) {
	}

	grk_tcd_band(const grk_tcd_band &rhs) :
			x0(rhs.x0), y0(rhs.y0), x1(rhs.x1), y1(rhs.y1), bandno(rhs.bandno), precincts(
					nullptr), numPrecincts(rhs.numPrecincts), numAllocatedPrecincts(
					rhs.numAllocatedPrecincts), numbps(rhs.numbps), stepsize(
					rhs.stepsize), inv_step(rhs.inv_step) {
	}
	bool isEmpty() {
		return ((x1 - x0 == 0) || (y1 - y0 == 0));
	}
	/* dimension of the subband : left upper corner (x0, y0) right low corner (x1,y1) */
	uint32_t x0, y0, x1, y1;
	// 0 for first band of lowest resolution, otherwise equal to 1,2 or 3
	uint8_t bandno;
	grk_tcd_precinct *precincts; /* precinct information */
	size_t numPrecincts;
	size_t numAllocatedPrecincts;
	uint32_t numbps;
	float stepsize;
	// inverse step size in 13 bit fixed point
	uint32_t inv_step;

};

// resolution
struct grk_tcd_resolution {
	grk_tcd_resolution() :
			x0(0),
			y0(0),
			x1(0),
			y1(0),
			pw(0),
			ph(0),
			numbands(0),
			win_x0(0),
			win_y0(0),
			win_x1(0),
			win_y1(0)
	{}

	/* dimension of the resolution level in tile coordinates */
	uint32_t x0, y0, x1, y1;
	uint32_t pw, ph;
	uint32_t numbands; /* number sub-band for the resolution level */
	grk_tcd_band bands[3]; /* subband information */

    /* dimension of the resolution limited to window of interest.
     *  Only valid if tcd->whole_tile_decoding is set */
	uint32_t win_x0;
	uint32_t win_y0;
	uint32_t win_x1;
	uint32_t win_y1;

};

struct TileComponent;

// tile
struct grk_tcd_tile {
	uint32_t x0, y0, x1, y1; /* dimension of the tile : left upper corner (x0, y0) right low corner (x1,y1) */
	uint32_t numcomps; /* number of components in tile */
	TileComponent *comps; /* Components information */
	uint64_t numpix;
	double distotile;
	double distolayer[100];
	uint64_t packno; /* packet number */
};

/**
 Tile coder/decoder
 */
struct TileProcessor {

	TileProcessor(bool isDecoder) ;
	~TileProcessor();

	/**
	 * Initialize the tile coder and may reuse some memory.
	 * @param	p_image		raw image.
	 *
	 * @return true if the encoding values could be set (false otherwise).
	 */
	bool init(grk_image *p_image, grk_coding_parameters *p_cp);

	/**
	 * Allocates memory for decoding a specific tile.
	 *
	 * @param	output_image output image - stores the decode region of interest
	 * @param	tile_no	the index of the tile received in sequence. This not necessarily lead to the
	 * tile at index tile_no.
	 *
	 * @return	true if the remaining data is sufficient.
	 */
	bool init_decode_tile(grk_image *output_image,
			uint16_t tile_no);

	/**
	 * Sets the given area to be decoded. This function should be called right after grk_read_header
	 * and before any tile header reading.
	 *
	 * @param	p_j2k		the jpeg2000 codec.
	 * @param	p_image     FIXME DOC
	 * @param	start_x		the left position of the rectangle to decode (in image coordinates).
	 * @param	start_y		the up position of the rectangle to decode (in image coordinates).
	 * @param	end_x		the right position of the rectangle to decode (in image coordinates).
	 * @param	end_y		the bottom position of the rectangle to decode (in image coordinates).

	 *
	 * @return	true			if the area could be set.
	 */
	bool set_decode_area(grk_j2k *p_j2k,
						grk_image *p_image,
						uint32_t start_x,
						uint32_t start_y,
						uint32_t end_x,
						uint32_t end_y);

	/**
	 * Encodes a tile from the raw image into the given buffer.
	 * @param	tile_no		Index of the tile to encode.
	 * @param	p_dest			Destination buffer
	 * @param	p_data_written	pointer to an int that is incremented by the number of bytes really written on p_dest
	 * @param	p_len			Maximum length of the destination buffer
	 * @param	p_cstr_info		Codestream information structure
	 * @return  true if the coding is successful.
	 */
	bool encode_tile(uint16_t tile_no, BufferedStream *p_stream,
			uint64_t *p_data_written, uint64_t len,
			 grk_codestream_info  *p_cstr_info);

	/**
	 Decode a tile from a buffer into a raw image
	 @param src Source buffer
	 @param len Length of source buffer
	 @param tileno Number that identifies one of the tiles to be decoded
	 @param cstr_info  FIXME DOC
	 */
	bool decode_tile(ChunkBuffer *src_buf, uint16_t tileno);

	/**
	 * Copies tile data from the system onto the given memory block.
	 */
	bool update_tile_data(uint8_t *p_dest,
			uint64_t dest_length);


	uint64_t get_tile_size(bool reduced);

	/**
	 * Initialize the tile coder and may reuse some meory.
	 *
	 * @param	tile_no	current tile index to encode.
	 *
	 * @return true if the encoding values could be set (false otherwise).
	 */
	bool init_encode_tile(uint16_t tile_no);

	/**
	 * Copies tile data from the given memory block onto the system.
	 */
	bool copy_image_data_to_tile(uint8_t *p_src, uint64_t src_length);


	bool needs_rate_control();

	bool copy_decoded_tile_to_output_image(uint8_t *p_data,
			grk_image *p_output_image, bool clearOutputOnInit);

	void copy_image_to_tile();

	/** Index of the tile to decode (used in get_tile); initialized to -1 */
	int32_t m_tile_ind_to_dec;

	/** tile number being currently coded/decoded */
	uint16_t m_current_tile_number;

	/** Position of the tile part flag in progression order*/
	uint32_t tp_pos;

	/** Tile part number, regardless of poc.
	 *  for each new poc, tp is reset to 0*/
	uint8_t m_current_poc_tile_part_number;

	/** Tile part number currently coding, taking into account POC.
	 *  m_current_tile_part_number holds the total number of tile parts
	 *   while encoding the last tile part.*/
	uint8_t m_current_tile_part_number;

	/** TNsot correction : see issue 254 **/
	uint32_t m_nb_tile_parts_correction_checked :1;
	uint8_t m_nb_tile_parts_correction :1;

	uint32_t tile_part_data_length;

	/**
	 locate the start position of the TLM marker
	 after encoding the tilepart, a jump (in j2k_write_sod) is done
	 to the TLM marker to store the value of its length.
	 */
	uint64_t m_tlm_start;
	/**
	 * Stores the sizes of the tlm.
	 */
	uint8_t *m_tlm_sot_offsets_buffer;
	/**
	 * The current offset of the tlm buffer.
	 */
	uint8_t *m_tlm_sot_offsets_current;

	/** Total number of tile parts of the current tile*/
	uint8_t cur_totnum_tp;
	/** Current packet iterator number */
	uint32_t cur_pino;
	/** info on image tile */
	grk_tcd_tile *tile;
	/** image header */
	grk_image *image;
	grk_plugin_tile *current_plugin_tile;

    /** Only valid for decoding. Whether the whole tile is decoded, or just the region in win_x0/win_y0/win_x1/win_y1 */
    bool   whole_tile_decoding;

	uint8_t *m_marker_scratch;
	uint32_t m_marker_scratch_size;

	PL_MAP *plt_marker;

private:
	/** coding parameters */
	grk_coding_parameters *m_cp;
	/** coding/decoding parameters common to all tiles */
	grk_tcp *m_tcp;
	/** current encoded tile (not used for decode) */
	uint16_t m_tileno;
	/** indicate if the tcd is a decoder. */
	bool m_is_decoder;

	/**
	 * Initializes tile coding/decoding
	 */
	 inline bool init_tile(uint16_t tile_no,
			grk_image *output_image, bool isEncoder);

	/**
	 Free the memory allocated for encoding
	 */
	 void free_tile();

	 bool t2_decode(uint16_t tile_no, ChunkBuffer *src_buf,
			uint64_t *p_data_read);

	 bool is_whole_tilecomp_decoding( uint32_t compno);

	 bool mct_decode();

	 bool dc_level_shift_decode();

	 bool dc_level_shift_encode();

	 bool mct_encode();

	 bool dwt_encode();

	 bool t1_encode();

	 bool t2_encode(BufferedStream *p_stream,
			uint64_t *p_data_written, uint64_t max_dest_size,
			 grk_codestream_info  *p_cstr_info);

	 bool rate_allocate_encode(uint64_t max_dest_size,
			 grk_codestream_info  *p_cstr_info);

	 bool layer_needs_rate_control(uint32_t layno);

	 bool make_single_lossless_layer();

	 void makelayer_final(uint32_t layno);

	 bool pcrd_bisect_simple(uint64_t *p_data_written,
			uint64_t len);

	 void make_layer_simple(uint32_t layno, double thresh,
			bool final);

	 bool pcrd_bisect_feasible(uint64_t *p_data_written,
			uint64_t len);

	 void makelayer_feasible(uint32_t layno, uint16_t thresh,
			bool final);

};

}
