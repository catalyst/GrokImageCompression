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

#ifdef _WIN32
#include <windows.h>
#else /* _WIN32 */
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#endif
#include <fcntl.h>
#include "grok_includes.h"
using namespace grk;

/**
 * Main codec handler used for compression or decompression.
 */
struct grk_codec_private {
	/** FIXME DOC */
	union {
		/**
		 * Decompression handler.
		 */
		struct decompression {
			/** Main header reading function handler */
			bool (*read_header)(BufferedStream *cio, void *p_codec,
					 grk_header_info  *header_info, grk_image **p_image);

			/** Decoding function */
			bool (*decode)(void *p_codec, grk_plugin_tile *tile,
					BufferedStream *p_cio, grk_image *p_image);

			/** FIXME DOC */
			bool (*read_tile_header)(void *p_codec, uint16_t *tile_index,
					uint64_t *data_size, uint32_t *p_tile_x0,
					uint32_t *p_tile_y0, uint32_t *p_tile_x1,
					uint32_t *p_tile_y1, uint32_t *p_nb_comps,
					bool *p_should_go_on, BufferedStream *p_cio);

			/** FIXME DOC */
			bool (*decode_tile_data)(void *p_codec, uint16_t tile_index,
					uint8_t *p_data, uint64_t data_size, BufferedStream *p_cio);

			/** Reading function used after codestream if necessary */
			bool (*end_decompress)(void *p_codec, BufferedStream *cio);

			/** Codec destroy function handler */
			void (*destroy)(void *p_codec);

			/** Setup decoder function handler */
			void (*setup_decoder)(void *p_codec,  grk_dparameters  *p_param);

			/** Set decode area function handler */
			bool (*set_decode_area)(void *p_codec, grk_image *p_image,
					uint32_t start_x, uint32_t end_x, uint32_t start_y,
					uint32_t end_y);

			/** Get tile function */
			bool (*get_decoded_tile)(void *p_codec, BufferedStream *p_cio,
					grk_image *p_image,
					uint16_t tile_index);

		} m_decompression;

		/**
		 * Compression handler. FIXME DOC
		 */
		struct compression {
			bool (*start_compress)(void *p_codec, BufferedStream *cio,
					grk_image *p_image);

			bool (*encode)(void *p_codec, grk_plugin_tile*,
					BufferedStream *p_cio);

			bool (*write_tile)(void *p_codec, uint16_t tile_index,
					uint8_t *p_data, uint64_t data_size, BufferedStream *p_cio);

			bool (*end_compress)(void *p_codec, BufferedStream *p_cio);

			void (*destroy)(void *p_codec);

			bool (*setup_encoder)(void *p_codec,  grk_cparameters  *p_param,
					grk_image *p_image);
		} m_compression;
	} m_codec_data;
	/** FIXME DOC*/
	void *m_codec;
	 grk_stream  *m_stream;
	/** Flag to indicate if the codec is used to decode or encode*/
	bool is_decompressor;
	void (*grk_dump_codec)(void *p_codec, int32_t info_flag,
			FILE *output_stream);
	 grk_codestream_info_v2  *  (*get_codec_info)(void *p_codec);
	 grk_codestream_index  *  (*grk_get_codec_index)(void *p_codec);
};

ThreadPool* ThreadPool::singleton = nullptr;
std::mutex ThreadPool::singleton_mutex;

static bool is_plugin_initialized = false;
bool GRK_CALLCONV grk_initialize(const char *plugin_path, uint32_t numthreads) {
	ThreadPool::instance(numthreads);
	if (!is_plugin_initialized) {
		grok_plugin_load_info info;
		info.plugin_path = plugin_path;
		is_plugin_initialized = grok_plugin_load(info);
	}
	return is_plugin_initialized;
}

GRK_API void GRK_CALLCONV grk_deinitialize() {
	grok_plugin_cleanup();
	ThreadPool::release();
}

/* ---------------------------------------------------------------------- */
/* Functions to set the message handlers */

bool GRK_CALLCONV grk_set_info_handler( grk_msg_callback p_callback,
		void *p_user_data) {
	logger::m_logger.info_handler = p_callback;
	logger::m_logger.m_info_data = p_user_data;

	return true;
}
bool GRK_CALLCONV grk_set_warning_handler( 	grk_msg_callback p_callback,
		void *p_user_data) {
	logger::m_logger.warning_handler = p_callback;
	logger::m_logger.m_warning_data = p_user_data;
	return true;
}
bool GRK_CALLCONV grk_set_error_handler( grk_msg_callback p_callback,
		void *p_user_data) {
	logger::m_logger.error_handler = p_callback;
	logger::m_logger.m_error_data = p_user_data;
	return true;
}
/* ---------------------------------------------------------------------- */

static size_t grk_read_from_file(void *p_buffer, size_t nb_bytes,
		FILE *p_file) {
	size_t l_nb_read = fread(p_buffer, 1, nb_bytes, p_file);
	return l_nb_read;
}

static uint64_t grk_get_data_length_from_file(FILE *p_file) {
	GROK_FSEEK(p_file, 0, SEEK_END);
	int64_t file_length = (int64_t) GROK_FTELL(p_file);
	GROK_FSEEK(p_file, 0, SEEK_SET);
	return (uint64_t) file_length;
}
static size_t grk_write_from_file(void *p_buffer, size_t nb_bytes,
		FILE *p_file) {
	return fwrite(p_buffer, 1, nb_bytes, p_file);
}
static bool grok_seek_from_file(int64_t nb_bytes, FILE *p_user_data) {
	return GROK_FSEEK(p_user_data, nb_bytes, SEEK_SET) ? false : true;
}

/* ---------------------------------------------------------------------- */

/* ---------------------------------------------------------------------- */

#ifdef _WIN32
#ifndef GRK_STATIC
BOOL APIENTRY
DllMain(HINSTANCE hModule, DWORD ul_reason_for_call, LPVOID lpReserved){
    ARG_NOT_USED(lpReserved);
    ARG_NOT_USED(hModule);
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH :
        break;
    case DLL_PROCESS_DETACH :
        break;
    case DLL_THREAD_ATTACH :
    case DLL_THREAD_DETACH :
        break;
    }
    return TRUE;
}
#endif /* GRK_STATIC */
#endif /* _WIN32 */

/* ---------------------------------------------------------------------- */

const char* GRK_CALLCONV grk_version(void) {
	return GRK_PACKAGE_VERSION;
}

/* ---------------------------------------------------------------------- */
/* DECOMPRESSION FUNCTIONS*/

 grk_codec  *  GRK_CALLCONV grk_create_decompress(GRK_CODEC_FORMAT p_format,
		 grk_stream  *p_stream) {
	grk_codec_private *l_codec = nullptr;
	l_codec = (grk_codec_private*) grk_calloc(1, sizeof(grk_codec_private));
	if (!l_codec) {
		return nullptr;
	}
	l_codec->is_decompressor = 1;
	l_codec->m_stream = p_stream;

	switch (p_format) {
	case GRK_CODEC_J2K:
		l_codec->grk_dump_codec = (void (*)(void*, int32_t, FILE*)) j2k_dump;

		l_codec->get_codec_info =
				( grk_codestream_info_v2  *  (*)(void*)) j2k_get_cstr_info;

		l_codec->grk_get_codec_index =
				( grk_codestream_index  *  (*)(void*)) j2k_get_cstr_index;

		l_codec->m_codec_data.m_decompression.decode =
				(bool (*)(void*, grk_plugin_tile*, BufferedStream*, grk_image * )) j2k_decode;

		l_codec->m_codec_data.m_decompression.end_decompress = (bool (*)(void*,
				BufferedStream*)) j2k_end_decompress;

		l_codec->m_codec_data.m_decompression.read_header = (bool (*)(
				BufferedStream*, void*,  grk_header_info  *header_info,
				grk_image **)) j2k_read_header;

		l_codec->m_codec_data.m_decompression.destroy =
				(void (*)(void*)) j2k_destroy;

		l_codec->m_codec_data.m_decompression.setup_decoder = (void (*)(void*,
				 grk_dparameters  * )) j2k_setup_decoder;

		l_codec->m_codec_data.m_decompression.read_tile_header =
				(bool (*)(void*, uint16_t*, uint64_t*, uint32_t*, uint32_t*,
						uint32_t*, uint32_t*, uint32_t*, bool*, BufferedStream*)) j2k_read_tile_header;

		l_codec->m_codec_data.m_decompression.decode_tile_data =
				(bool (*)(void*, uint16_t, uint8_t*, uint64_t, BufferedStream*)) j2k_decode_tile;

		l_codec->m_codec_data.m_decompression.set_decode_area = (bool (*)(void*,
				grk_image * , uint32_t, uint32_t, uint32_t, uint32_t)) j2k_set_decode_area;

		l_codec->m_codec_data.m_decompression.get_decoded_tile = (bool (*)(
				void *p_codec, BufferedStream *p_cio, grk_image *p_image, uint16_t tile_index)) j2k_get_tile;

		l_codec->m_codec = j2k_create_decompress();

		if (!l_codec->m_codec) {
			grok_free(l_codec);
			return nullptr;
		}
		break;
	case GRK_CODEC_JP2:
		/* get a JP2 decoder handle */
		l_codec->grk_dump_codec = (void (*)(void*, int32_t, FILE*)) jp2_dump;
		l_codec->get_codec_info =
				( grk_codestream_info_v2  *  (*)(void*)) jp2_get_cstr_info;
		l_codec->grk_get_codec_index =
				( grk_codestream_index  *  (*)(void*)) jp2_get_cstr_index;
		l_codec->m_codec_data.m_decompression.decode =
				(bool (*)(void*, grk_plugin_tile*, BufferedStream*, grk_image * )) jp2_decode;
		l_codec->m_codec_data.m_decompression.end_decompress = (bool (*)(void*,
				BufferedStream*)) jp2_end_decompress;
		l_codec->m_codec_data.m_decompression.read_header = (bool (*)(
				BufferedStream*, void*,  grk_header_info  *header_info,
				grk_image **)) jp2_read_header;
		l_codec->m_codec_data.m_decompression.read_tile_header =
				(bool (*)(void*, uint16_t*, uint64_t*, uint32_t*, uint32_t*,
						uint32_t*, uint32_t*, uint32_t*, bool*, BufferedStream*)) jp2_read_tile_header;
		l_codec->m_codec_data.m_decompression.decode_tile_data =
				(bool (*)(void*, uint16_t, uint8_t*, uint64_t, BufferedStream*)) jp2_decode_tile;

		l_codec->m_codec_data.m_decompression.destroy =
				(void (*)(void*)) jp2_destroy;
		l_codec->m_codec_data.m_decompression.setup_decoder = (void (*)(void*,
				 grk_dparameters  * )) jp2_setup_decoder;
		l_codec->m_codec_data.m_decompression.set_decode_area = (bool (*)(void*,
				grk_image * , uint32_t, uint32_t, uint32_t, uint32_t)) jp2_set_decode_area;

		l_codec->m_codec_data.m_decompression.get_decoded_tile = (bool (*)(
				void *p_codec, BufferedStream *p_cio, grk_image *p_image, uint16_t tile_index)) jp2_get_tile;
		l_codec->m_codec = jp2_create(true);
		if (!l_codec->m_codec) {
			grok_free(l_codec);
			return nullptr;
		}
		break;
	case GRK_CODEC_UNKNOWN:
	default:
		grok_free(l_codec);
		return nullptr;
	}
	return ( grk_codec  * ) l_codec;
}
void GRK_CALLCONV grk_set_default_decoder_parameters(
		 grk_dparameters  *parameters) {
	if (parameters) {
		memset(parameters, 0, sizeof( grk_dparameters) );
	}
}
bool GRK_CALLCONV grk_setup_decoder( grk_codec  *p_codec,
		 grk_dparameters  *parameters) {
	if (p_codec && parameters) {
		grk_codec_private *l_codec = (grk_codec_private*) p_codec;
		if (!l_codec->is_decompressor) {
			GROK_ERROR(
					"Codec provided to the grk_setup_decoder function is not a decompressor handler.");
			return false;
		}
		l_codec->m_codec_data.m_decompression.setup_decoder(l_codec->m_codec,
				parameters);
		return true;
	}
	return false;
}
bool GRK_CALLCONV grk_read_header(
		 grk_codec  *p_codec,  grk_header_info  *header_info,
		grk_image **p_image) {
	if (p_codec) {
		grk_codec_private *l_codec = (grk_codec_private*) p_codec;
		BufferedStream *l_stream = (BufferedStream*) l_codec->m_stream;
		if (!l_codec->is_decompressor) {
			GROK_ERROR(
					"Codec provided to the grk_read_header function is not a decompressor handler.");
			return false;
		}

		return l_codec->m_codec_data.m_decompression.read_header(l_stream,
				l_codec->m_codec, header_info, p_image);
	}
	return false;
}

bool GRK_CALLCONV grk_decode( grk_codec  *p_codec, grk_plugin_tile *tile,
		 grk_image *p_image) {
	if (p_codec) {
		grk_codec_private *l_codec = (grk_codec_private*) p_codec;
		BufferedStream *l_stream = (BufferedStream*) l_codec->m_stream;
		if (!l_codec->is_decompressor) {
			return false;
		}
		return l_codec->m_codec_data.m_decompression.decode(l_codec->m_codec,
				tile, l_stream, p_image);
	}
	return false;
}
bool GRK_CALLCONV grk_set_decode_area( grk_codec  *p_codec,
		grk_image *p_image, uint32_t start_x, uint32_t start_y,
		uint32_t end_x, uint32_t end_y) {
	if (p_codec) {
		grk_codec_private *l_codec = (grk_codec_private*) p_codec;
		if (!l_codec->is_decompressor) {
			return false;
		}
		return l_codec->m_codec_data.m_decompression.set_decode_area(
				l_codec->m_codec, p_image, start_x, start_y, end_x,
				end_y);
	}
	return false;
}
bool GRK_CALLCONV grk_read_tile_header( grk_codec  *p_codec,
		 uint16_t *tile_index, uint64_t *data_size,
		uint32_t *p_tile_x0, uint32_t *p_tile_y0, uint32_t *p_tile_x1,
		uint32_t *p_tile_y1, uint32_t *p_nb_comps, bool *p_should_go_on) {
	if (p_codec && data_size && tile_index) {
		grk_codec_private *l_codec = (grk_codec_private*) p_codec;
		BufferedStream *l_stream = (BufferedStream*) l_codec->m_stream;
		if (!l_codec->is_decompressor) {
			return false;
		}
		return l_codec->m_codec_data.m_decompression.read_tile_header(
				l_codec->m_codec, tile_index, data_size, p_tile_x0,
				p_tile_y0, p_tile_x1, p_tile_y1, p_nb_comps, p_should_go_on,
				l_stream);
	}
	return false;
}
bool GRK_CALLCONV grk_decode_tile_data( grk_codec  *p_codec,
		uint16_t tile_index, uint8_t *p_data, uint64_t data_size) {
	if (p_codec && p_data) {
		grk_codec_private *l_codec = (grk_codec_private*) p_codec;
		BufferedStream *l_stream = (BufferedStream*) l_codec->m_stream;

		if (!l_codec->is_decompressor) {
			return false;
		}

		return l_codec->m_codec_data.m_decompression.decode_tile_data(
				l_codec->m_codec, tile_index, p_data, data_size, l_stream);
	}
	return false;
}
bool GRK_CALLCONV grk_get_decoded_tile( grk_codec  *p_codec,
		 grk_image *p_image, uint16_t tile_index) {
	if (p_codec) {
		grk_codec_private *l_codec = (grk_codec_private*) p_codec;
		BufferedStream *l_stream = (BufferedStream*) l_codec->m_stream;

		if (!l_codec->is_decompressor) {
			return false;
		}

		return l_codec->m_codec_data.m_decompression.get_decoded_tile(
				l_codec->m_codec, l_stream, p_image,
				tile_index);
	}
	return false;
}

/* ---------------------------------------------------------------------- */
/* COMPRESSION FUNCTIONS*/

 grk_codec  *  GRK_CALLCONV grk_create_compress(GRK_CODEC_FORMAT p_format,
		 grk_stream  *p_stream) {
	grk_codec_private *l_codec = nullptr;
	l_codec = (grk_codec_private*) grk_calloc(1, sizeof(grk_codec_private));
	if (!l_codec) {
		return nullptr;
	}
	l_codec->m_stream = p_stream;
	l_codec->is_decompressor = 0;

	switch (p_format) {
	case GRK_CODEC_J2K:
		l_codec->m_codec_data.m_compression.encode =
				(bool (*)(void*, grk_plugin_tile*, BufferedStream*)) j2k_encode;
		l_codec->m_codec_data.m_compression.end_compress = (bool (*)(void*,
				BufferedStream*)) j2k_end_compress;
		l_codec->m_codec_data.m_compression.start_compress =
				(bool (*)(void*, BufferedStream*, grk_image * )) j2k_start_compress;
		l_codec->m_codec_data.m_compression.write_tile =
				(bool (*)(void*, uint16_t, uint8_t*, uint64_t, BufferedStream*)) j2k_write_tile;
		l_codec->m_codec_data.m_compression.destroy =
				(void (*)(void*)) j2k_destroy;
		l_codec->m_codec_data.m_compression.setup_encoder =
				(bool (*)(void*,  grk_cparameters  * , grk_image * )) j2k_setup_encoder;
		l_codec->m_codec = j2k_create_compress();
		if (!l_codec->m_codec) {
			grok_free(l_codec);
			return nullptr;
		}
		break;
	case GRK_CODEC_JP2:
		/* get a JP2 decoder handle */
		l_codec->m_codec_data.m_compression.encode =
				(bool (*)(void*, grk_plugin_tile*, BufferedStream*)) jp2_encode;
		l_codec->m_codec_data.m_compression.end_compress = (bool (*)(void*,
				BufferedStream*)) jp2_end_compress;
		l_codec->m_codec_data.m_compression.start_compress = (bool (*)(void*,
				BufferedStream*, grk_image * )) jp2_start_compress;
		l_codec->m_codec_data.m_compression.write_tile =
				(bool (*)(void*, uint16_t, uint8_t*, uint64_t, BufferedStream*)) jp2_write_tile;
		l_codec->m_codec_data.m_compression.destroy =
				(void (*)(void*)) jp2_destroy;
		l_codec->m_codec_data.m_compression.setup_encoder =
				(bool (*)(void*,  grk_cparameters  * , grk_image * )) jp2_setup_encoder;

		l_codec->m_codec = jp2_create(false);
		if (!l_codec->m_codec) {
			grok_free(l_codec);
			return nullptr;
		}
		break;
	case GRK_CODEC_UNKNOWN:
	default:
		grok_free(l_codec);
		return nullptr;
	}
	return ( grk_codec  * ) l_codec;
}
void GRK_CALLCONV grk_set_default_encoder_parameters(
		 grk_cparameters  *parameters) {
	if (parameters) {
		memset(parameters, 0, sizeof( grk_cparameters) );
		/* default coding parameters */
		parameters->rsiz = GRK_PROFILE_NONE;
		parameters->max_comp_size = 0;
		parameters->numresolution = GRK_COMP_PARAM_DEFAULT_NUMRESOLUTION;
		parameters->cblockw_init = GRK_COMP_PARAM_DEFAULT_CBLOCKW;
		parameters->cblockh_init = GRK_COMP_PARAM_DEFAULT_CBLOCKH;
		parameters->prog_order = GRK_COMP_PARAM_DEFAULT_PROG_ORDER;
		parameters->roi_compno = -1; /* no ROI */
		parameters->subsampling_dx = 1;
		parameters->subsampling_dy = 1;
		parameters->tp_on = 0;
		parameters->decod_format = GRK_UNKNOWN_FORMAT;
		parameters->cod_format = GRK_UNKNOWN_FORMAT;
		parameters->tcp_rates[0] = 0;
		parameters->tcp_numlayers = 0;
		parameters->cp_disto_alloc = 0;
		parameters->cp_fixed_quality = 0;
		if (!parameters->numThreads)
			parameters->numThreads = ThreadPool::hardware_concurrency();
		parameters->deviceId = 0;
		parameters->repeats = 1;
	}
}
bool GRK_CALLCONV grk_setup_encoder( grk_codec  *p_codec,
		 grk_cparameters  *parameters, grk_image *p_image) {
	if (p_codec && parameters && p_image) {
		grk_codec_private *l_codec = (grk_codec_private*) p_codec;
		if (!l_codec->is_decompressor) {
			return l_codec->m_codec_data.m_compression.setup_encoder(
					l_codec->m_codec, parameters, p_image);
		}
	}
	return false;
}
bool GRK_CALLCONV grk_start_compress( grk_codec  *p_codec, grk_image *p_image) {
	if (p_codec) {
		grk_codec_private *l_codec = (grk_codec_private*) p_codec;
		BufferedStream *l_stream = (BufferedStream*) l_codec->m_stream;
		if (!l_codec->is_decompressor) {
			return l_codec->m_codec_data.m_compression.start_compress(
					l_codec->m_codec, l_stream, p_image	);
		}
	}
	return false;
}
bool GRK_CALLCONV grk_encode( grk_codec  *p_codec) {
	return grk_encode_with_plugin(p_codec, nullptr);
}
bool GRK_CALLCONV grk_encode_with_plugin( grk_codec  *p_info,
		grk_plugin_tile *tile) {
	if (p_info) {
		grk_codec_private *l_codec = (grk_codec_private*) p_info;
		BufferedStream *l_stream = (BufferedStream*) l_codec->m_stream;
		if (!l_codec->is_decompressor) {
			return l_codec->m_codec_data.m_compression.encode(l_codec->m_codec,
					tile, l_stream);
		}
	}
	return false;
}
bool GRK_CALLCONV grk_end_compress( grk_codec  *p_codec) {
	if (p_codec) {
		grk_codec_private *l_codec = (grk_codec_private*) p_codec;
		BufferedStream *l_stream = (BufferedStream*) l_codec->m_stream;
		if (!l_codec->is_decompressor) {
			return l_codec->m_codec_data.m_compression.end_compress(
					l_codec->m_codec, l_stream);
		}
	}
	return false;
}
bool GRK_CALLCONV grk_end_decompress( grk_codec  *p_codec) {
	if (p_codec) {
		grk_codec_private *l_codec = (grk_codec_private*) p_codec;
		BufferedStream *l_stream = (BufferedStream*) l_codec->m_stream;
		if (!l_codec->is_decompressor) {
			return false;
		}
		return l_codec->m_codec_data.m_decompression.end_decompress(
				l_codec->m_codec, l_stream);
	}
	return false;
}

bool GRK_CALLCONV grk_set_MCT( grk_cparameters  *parameters,
		float *pEncodingMatrix, int32_t *p_dc_shift, uint32_t pNbComp) {
	uint32_t l_matrix_size = pNbComp * pNbComp * (uint32_t) sizeof(float);
	uint32_t l_dc_shift_size = pNbComp * (uint32_t) sizeof(int32_t);
	uint32_t l_mct_total_size = l_matrix_size + l_dc_shift_size;

	/* add MCT capability */
	if (GRK_IS_PART2(parameters->rsiz)) {
		parameters->rsiz |= GRK_EXTENSION_MCT;
	} else {
		parameters->rsiz = ((GRK_PROFILE_PART2) | (GRK_EXTENSION_MCT));
	}
	parameters->irreversible = true;

	/* use array based MCT */
	parameters->tcp_mct = 2;
	parameters->mct_data = grk_malloc(l_mct_total_size);
	if (!parameters->mct_data) {
		return false;
	}
	memcpy(parameters->mct_data, pEncodingMatrix, l_matrix_size);
	memcpy(((uint8_t*) parameters->mct_data) + l_matrix_size, p_dc_shift,
			l_dc_shift_size);
	return true;
}
bool GRK_CALLCONV grk_write_tile( grk_codec  *p_codec, uint16_t tile_index,
		uint8_t *p_data, uint64_t data_size) {
	if (p_codec && p_data) {
		grk_codec_private *l_codec = (grk_codec_private*) p_codec;
		BufferedStream *l_stream = (BufferedStream*) l_codec->m_stream;
		if (l_codec->is_decompressor) {
			return false;
		}
		return l_codec->m_codec_data.m_compression.write_tile(l_codec->m_codec,
				tile_index, p_data, data_size, l_stream	);
	}
	return false;
}

/* ---------------------------------------------------------------------- */

void GRK_CALLCONV grk_destroy_codec( grk_codec  *p_codec) {
	if (p_codec) {
		grk_codec_private *l_codec = (grk_codec_private*) p_codec;
		if (l_codec->is_decompressor) {
			l_codec->m_codec_data.m_decompression.destroy(l_codec->m_codec);
		} else {
			l_codec->m_codec_data.m_compression.destroy(l_codec->m_codec);
		}
		l_codec->m_codec = nullptr;
		grok_free(l_codec);
	}
}

/* ---------------------------------------------------------------------- */

void GRK_CALLCONV grk_dump_codec( grk_codec  *p_codec, int32_t info_flag,
		FILE *output_stream) {
	if (p_codec) {
		grk_codec_private *l_codec = (grk_codec_private*) p_codec;
		l_codec->grk_dump_codec(l_codec->m_codec, info_flag, output_stream);
		return;
	}
	/* TODO return error */
	GROK_ERROR("Input parameter of the dump_codec function are incorrect.");
	return;
}
 grk_codestream_info_v2  *  GRK_CALLCONV grk_get_cstr_info( grk_codec  *p_codec) {
	if (p_codec) {
		grk_codec_private *l_codec = (grk_codec_private*) p_codec;
		return l_codec->get_codec_info(l_codec->m_codec);
	}
	return nullptr;
}
void GRK_CALLCONV grk_destroy_cstr_info( grk_codestream_info_v2  **cstr_info) {
	if (cstr_info) {
		if ((*cstr_info)->m_default_tile_info.tccp_info) {
			grok_free((*cstr_info)->m_default_tile_info.tccp_info);
		}

		if ((*cstr_info)->tile_info) {
			/* FIXME not used for the moment*/
		}
		grok_free((*cstr_info));
		(*cstr_info) = nullptr;
	}
}

 grk_codestream_index  *  GRK_CALLCONV grk_get_cstr_index( grk_codec  *p_codec) {
	if (p_codec) {
		grk_codec_private *l_codec = (grk_codec_private*) p_codec;
		return l_codec->grk_get_codec_index(l_codec->m_codec);
	}
	return nullptr;
}
void GRK_CALLCONV grk_destroy_cstr_index(
		 grk_codestream_index  **p_cstr_index) {
	if (*p_cstr_index) {
		j2k_destroy_cstr_index(*p_cstr_index);
		(*p_cstr_index) = nullptr;
	}
}

/* ---------------------------------------------------------------------- */

static void grok_free_file(void *p_user_data) {
	if (p_user_data)
		fclose((FILE*) p_user_data);
}

 grk_stream  *  GRK_CALLCONV grk_stream_create_file_stream(const char *fname,
		size_t p_size, bool p_is_read_stream) {
	bool stdin_stdout = !fname || !fname[0];
	 grk_stream  *l_stream = nullptr;
	FILE *p_file = nullptr;
	if (!stdin_stdout && (!fname || !fname[0])) {
		return nullptr;
	}
	if (stdin_stdout) {
		p_file = p_is_read_stream ? stdin : stdout;
	} else {
		const char *mode = (p_is_read_stream) ? "rb" : "wb";
		p_file = fopen(fname, mode);
		if (!p_file) {
			return nullptr;
		}
	}
	l_stream = grk_stream_create(p_size, p_is_read_stream);
	if (!l_stream) {
		if (!stdin_stdout)
			fclose(p_file);
		return nullptr;
	}
	grk_stream_set_user_data(l_stream, (void*) p_file,
			(grk_stream_free_user_data_fn) (
					stdin_stdout ? nullptr : grok_free_file));
	if (p_is_read_stream)
		grk_stream_set_user_data_length(l_stream,
				grk_get_data_length_from_file(p_file));
	grk_stream_set_read_function(l_stream,
			(grk_stream_read_fn) grk_read_from_file);
	grk_stream_set_write_function(l_stream,
			(grk_stream_write_fn) grk_write_from_file);
	grk_stream_set_seek_function(l_stream,
			(grk_stream_seek_fn) grok_seek_from_file);
	return l_stream;
}
/* ---------------------------------------------------------------------- */
GRK_API size_t GRK_CALLCONV grk_stream_get_write_mem_stream_length(
		 grk_stream  *stream) {
	if (!stream)
		return 0;
	return get_mem_stream_offset(stream);
}
 grk_stream  *  GRK_CALLCONV grk_stream_create_mem_stream(uint8_t *buf,
		size_t len, bool ownsBuffer, bool p_is_read_stream) {
	return create_mem_stream(buf, len, ownsBuffer, p_is_read_stream);
}
 grk_stream  *  GRK_CALLCONV grk_stream_create_mapped_file_read_stream(
		const char *fname) {
	return create_mapped_file_read_stream(fname);
}
/* ---------------------------------------------------------------------- */
void GRK_CALLCONV grk_image_all_components_data_free(grk_image *image) {
	uint32_t i;
	if (!image || !image->comps)
		return;
	for (i = 0; i < image->numcomps; ++i) {
		grk_image_single_component_data_free(image->comps + i);
	}
}
bool GRK_CALLCONV grk_image_single_component_data_alloc(
		 grk_image_comp  *comp) {
	int32_t *data = nullptr;
	if (!comp)
		return false;
	data = (int32_t*) grk_aligned_malloc(
			(uint64_t) comp->w * comp->h * sizeof(uint32_t));
	if (!data)
		return false;
	grk_image_single_component_data_free(comp);
	comp->data = data;
	comp->owns_data = true;
	return true;
}
void GRK_CALLCONV grk_image_single_component_data_free( grk_image_comp  *comp) {
	if (!comp || !comp->data || !comp->owns_data)
		return;
	grk_aligned_free(comp->data);
	comp->data = nullptr;
	comp->owns_data = false;
}

uint8_t* GRK_CALLCONV grk_buffer_new(size_t len) {
	return new uint8_t[len];
}
void GRK_CALLCONV grk_buffer_delete(uint8_t *buf) {
	delete[] buf;
}

/**********************************************************************
 Plugin interface implementation
 ***********************************************************************/

static const char *plugin_get_debug_state_method_name = "plugin_get_debug_state";
static const char *plugin_init_method_name = "plugin_init";
static const char *plugin_encode_method_name = "plugin_encode";
static const char *plugin_batch_encode_method_name = "plugin_batch_encode";
static const char *plugin_stop_batch_encode_method_name =
		"plugin_stop_batch_encode";
static const char *plugin_is_batch_complete_method_name =
		"plugin_is_batch_complete";
static const char *plugin_decode_method_name = "plugin_decode";
static const char *plugin_init_batch_decode_method_name =
		"plugin_init_batch_decode";
static const char *plugin_batch_decode_method_name = "plugin_batch_decode";
static const char *plugin_stop_batch_decode_method_name =
		"plugin_stop_batch_decode";

static const char* get_path_separator() {
#ifdef _WIN32
	return "\\";
#else
	return "/";
#endif
}

bool pluginLoaded = false;
bool GRK_CALLCONV grok_plugin_load(grok_plugin_load_info info) {
	if (!info.plugin_path)
		return false;

	// form plugin name
	std::string pluginName = "";
#if !defined(_WIN32)
	pluginName += "lib";
#endif
	pluginName += std::string(GROK_PLUGIN_NAME) + "."
			+ minpf_get_dynamic_library_extension();

	// form absolute plugin path
	auto pluginPath = std::string(info.plugin_path) + get_path_separator()
			+ pluginName;
	int32_t rc = minpf_load_from_path(pluginPath.c_str(), nullptr);

	// if fails, try local path
	if (rc) {
		std::string localPlugin = std::string(".") + get_path_separator()
				+ pluginName;
		rc = minpf_load_from_path(localPlugin.c_str(), nullptr);
	}
	pluginLoaded = !rc;
	if (!pluginLoaded)
		minpf_cleanup_plugin_manager();
	return pluginLoaded;
}
uint32_t GRK_CALLCONV grok_plugin_get_debug_state() {
	minpf_plugin_manager *mgr = nullptr;
	PLUGIN_GET_DEBUG_STATE func = nullptr;
	uint32_t rc = GROK_PLUGIN_STATE_NO_DEBUG;
	if (!pluginLoaded)
		return rc;
	mgr = minpf_get_plugin_manager();
	if (mgr && mgr->num_libraries > 0) {
		func = (PLUGIN_GET_DEBUG_STATE) minpf_get_symbol(
				mgr->dynamic_libraries[0], plugin_get_debug_state_method_name);
		if (func) {
			rc = func();
		}
	}
	return rc;
}
void GRK_CALLCONV grok_plugin_cleanup(void) {
	minpf_cleanup_plugin_manager();
	pluginLoaded = false;
}
GRK_API bool GRK_CALLCONV grok_plugin_init(grok_plugin_init_info initInfo) {
	minpf_plugin_manager *mgr = nullptr;
	PLUGIN_INIT func = nullptr;
	if (!pluginLoaded)
		return false;
	mgr = minpf_get_plugin_manager();
	if (mgr && mgr->num_libraries > 0) {
		func = (PLUGIN_INIT) minpf_get_symbol(mgr->dynamic_libraries[0],
				plugin_init_method_name);
		if (func) {
			return func(initInfo);
		}
	}
	return false;
}

/*******************
 Encode Implementation
 ********************/

GROK_PLUGIN_ENCODE_USER_CALLBACK userEncodeCallback = 0;

/* wrapper for user's encode callback */
void grok_plugin_internal_encode_callback(
		plugin_encode_user_callback_info *info) {
	/* set code block data etc on code object */
	grk_plugin_encode_user_callback_info grk_info;
	memset(&grk_info, 0, sizeof(grk_plugin_encode_user_callback_info));
	grk_info.input_file_name = info->input_file_name;
	grk_info.outputFileNameIsRelative = info->outputFileNameIsRelative;
	grk_info.output_file_name = info->output_file_name;
	grk_info.encoder_parameters = ( grk_cparameters  * ) info->encoder_parameters;
	grk_info.image = (grk_image * ) info->image;
	grk_info.tile = (grk_plugin_tile*) info->tile;
	if (userEncodeCallback)
		userEncodeCallback(&grk_info);
}
int32_t GRK_CALLCONV grok_plugin_encode( grk_cparameters  *encode_parameters,
		GROK_PLUGIN_ENCODE_USER_CALLBACK callback) {
	minpf_plugin_manager *mgr = nullptr;
	PLUGIN_ENCODE func = nullptr;
	if (!pluginLoaded)
		return -1;

	userEncodeCallback = callback;
	mgr = minpf_get_plugin_manager();
	if (mgr && mgr->num_libraries > 0) {
		func = (PLUGIN_ENCODE) minpf_get_symbol(mgr->dynamic_libraries[0],
				plugin_encode_method_name);
		if (func) {
			return func(( grk_cparameters  * ) encode_parameters,
					grok_plugin_internal_encode_callback);
		}
	}
	return -1;
}
int32_t GRK_CALLCONV grok_plugin_batch_encode(const char *input_dir,
		const char *output_dir,  grk_cparameters  *encode_parameters,
		GROK_PLUGIN_ENCODE_USER_CALLBACK callback) {
	minpf_plugin_manager *mgr = nullptr;
	PLUGIN_BATCH_ENCODE func = nullptr;
	if (!pluginLoaded)
		return -1;

	userEncodeCallback = callback;
	mgr = minpf_get_plugin_manager();
	if (mgr && mgr->num_libraries > 0) {
		func = (PLUGIN_BATCH_ENCODE) minpf_get_symbol(mgr->dynamic_libraries[0],
				plugin_batch_encode_method_name);
		if (func) {
			return func(input_dir, output_dir,
					( grk_cparameters  * ) encode_parameters,
					grok_plugin_internal_encode_callback);
		}
	}
	return -1;
}

PLUGIN_IS_BATCH_COMPLETE funcPluginIsBatchComplete = nullptr;
GRK_API bool GRK_CALLCONV grok_plugin_is_batch_complete(void) {
	minpf_plugin_manager *mgr = nullptr;
	if (!pluginLoaded)
		return true;
	mgr = minpf_get_plugin_manager();
	if (mgr && mgr->num_libraries > 0) {
		if (!funcPluginIsBatchComplete)
			funcPluginIsBatchComplete =
					(PLUGIN_IS_BATCH_COMPLETE) minpf_get_symbol(
							mgr->dynamic_libraries[0],
							plugin_is_batch_complete_method_name);
		if (funcPluginIsBatchComplete) {
			return funcPluginIsBatchComplete();
		}
	}
	return true;
}
void GRK_CALLCONV grok_plugin_stop_batch_encode(void) {
	minpf_plugin_manager *mgr = nullptr;
	PLUGIN_STOP_BATCH_ENCODE func = nullptr;
	if (!pluginLoaded)
		return;
	mgr = minpf_get_plugin_manager();
	if (mgr && mgr->num_libraries > 0) {
		func = (PLUGIN_STOP_BATCH_ENCODE) minpf_get_symbol(
				mgr->dynamic_libraries[0],
				plugin_stop_batch_encode_method_name);
		if (func) {
			func();
		}
	}
}

/*******************
 Decode Implementation
 ********************/

grok_plugin_decode_callback decodeCallback = 0;

/* wrapper for user's decode callback */
int32_t grok_plugin_internal_decode_callback(PluginDecodeCallbackInfo *info) {
	int32_t rc = -1;
	/* set code block data etc on code object */
	grk_plugin_decode_callback_info grokInfo;
	memset(&grokInfo, 0, sizeof(grk_plugin_decode_callback_info));
	grokInfo.init_decoders_func = info->init_decoders_func;
	grokInfo.input_file_name =
			info->inputFile.empty() ? nullptr : info->inputFile.c_str();
	grokInfo.output_file_name =
			info->outputFile.empty() ? nullptr : info->outputFile.c_str();
	grokInfo.decod_format = info->decod_format;
	grokInfo.cod_format = info->cod_format;
	grokInfo.decoder_parameters = info->decoder_parameters;
	grokInfo.l_stream = info->l_stream;
	grokInfo.l_codec = info->l_codec;
	grokInfo.image = info->image;
	grokInfo.plugin_owns_image = info->plugin_owns_image;
	grokInfo.tile = info->tile;
	grokInfo.decode_flags = info->decode_flags;
	if (decodeCallback) {
		rc = decodeCallback(&grokInfo);
	}
	//synch
	info->image = grokInfo.image;
	info->l_stream = grokInfo.l_stream;
	info->l_codec = grokInfo.l_codec;
	info->header_info = grokInfo.header_info;
	return rc;
}

int32_t GRK_CALLCONV grok_plugin_decode(
		grk_decompress_parameters *decode_parameters,
		grok_plugin_decode_callback callback) {
	minpf_plugin_manager *mgr = nullptr;
	PLUGIN_DECODE func = nullptr;
	if (!pluginLoaded)
		return -1;

	decodeCallback = callback;
	mgr = minpf_get_plugin_manager();
	func = nullptr;
	if (mgr && mgr->num_libraries > 0) {
		func = (PLUGIN_DECODE) minpf_get_symbol(mgr->dynamic_libraries[0],
				plugin_decode_method_name);
		if (func) {
			return func((grk_decompress_parameters*) decode_parameters,
					grok_plugin_internal_decode_callback);
		}
	}
	return -1;
}
int32_t GRK_CALLCONV grok_plugin_init_batch_decode(const char *input_dir,
		const char *output_dir, grk_decompress_parameters *decode_parameters,
		grok_plugin_decode_callback callback) {
	minpf_plugin_manager *mgr = nullptr;
	PLUGIN_INIT_BATCH_DECODE func = nullptr;
	if (!pluginLoaded)
		return -1;

	decodeCallback = callback;
	mgr = minpf_get_plugin_manager();
	if (mgr && mgr->num_libraries > 0) {
		func = (PLUGIN_INIT_BATCH_DECODE) minpf_get_symbol(
				mgr->dynamic_libraries[0],
				plugin_init_batch_decode_method_name);
		if (func) {
			return func(input_dir, output_dir,
					(grk_decompress_parameters*) decode_parameters,
					grok_plugin_internal_decode_callback);
		}
	}
	return -1;
}
int32_t GRK_CALLCONV grok_plugin_batch_decode(void) {
	minpf_plugin_manager *mgr = nullptr;
	PLUGIN_BATCH_DECODE func = nullptr;
	if (!pluginLoaded)
		return -1;
	mgr = minpf_get_plugin_manager();
	if (mgr && mgr->num_libraries > 0) {
		func = (PLUGIN_BATCH_DECODE) minpf_get_symbol(mgr->dynamic_libraries[0],
				plugin_batch_decode_method_name);
		if (func) {
			return func();
		}
	}
	return -1;
}
void GRK_CALLCONV grok_plugin_stop_batch_decode(void) {
	minpf_plugin_manager *mgr = nullptr;
	PLUGIN_STOP_BATCH_DECODE func = nullptr;
	if (!pluginLoaded)
		return;
	mgr = minpf_get_plugin_manager();
	if (mgr && mgr->num_libraries > 0) {
		func = (PLUGIN_STOP_BATCH_DECODE) minpf_get_symbol(
				mgr->dynamic_libraries[0],
				plugin_stop_batch_decode_method_name);
		if (func) {
			func();
		}
	}
}
