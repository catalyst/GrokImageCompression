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
 */
#include <T1Part1.h>
#include "grok_includes.h"
#include "testing.h"
#include <algorithm>
using namespace std;

namespace grk {
namespace t1_part1{

T1Part1::T1Part1(bool isEncoder, grk_tcp *tcp, uint32_t maxCblkW,
		uint32_t maxCblkH) : t1(nullptr){
	(void) tcp;
	t1 = t1_create(isEncoder);
	if (!isEncoder) {
	   t1->cblkdatabuffersize = maxCblkW * maxCblkH * (uint32_t)sizeof(int32_t);
	   t1->cblkdatabuffer = (uint8_t*)grk_malloc(t1->cblkdatabuffersize);
   }
}
T1Part1::~T1Part1() {
	t1_destroy( t1);
}

/**
 Multiply two fixed-point numbers.
 @param  a 13-bit precision fixed point number
 @param  b 11-bit precision fixed point number
 @return a * b in T1_NMSEDEC_FRACBITS-bit precision fixed point
 */
static inline int32_t int_fix_mul_t1(int32_t a, int32_t b) {
#if defined(_MSC_VER) && (_MSC_VER >= 1400) && !defined(__INTEL_COMPILER) && defined(_M_IX86)
	int64_t temp = __emul(a, b);
#else
	int64_t temp = (int64_t) a * (int64_t) b;
#endif
	temp += 1<<(13 + 11 - T1_NMSEDEC_FRACBITS - 1);
	assert((temp >> (13 + 11 - T1_NMSEDEC_FRACBITS)) <= (int64_t)0x7FFFFFFF);
	assert(
			(temp >> (13 + 11 - T1_NMSEDEC_FRACBITS)) >= (-(int64_t)0x7FFFFFFF - (int64_t)1));
	return (int32_t) (temp >> (13 + 11 - T1_NMSEDEC_FRACBITS));
}

void T1Part1::preEncode(encodeBlockInfo *block, grk_tcd_tile *tile,
		uint32_t &maximum) {
	auto cblk = block->cblk;
	auto w = cblk->x1 - cblk->x0;
	auto h = cblk->y1 - cblk->y0;
	if (!t1_allocate_buffers(t1, w,h))
		return;
	t1->data_stride = w;
	uint32_t tile_width = (tile->comps + block->compno)->width();
	auto tileLineAdvance = tile_width - w;
	auto tiledp = block->tiledp;
	uint32_t tileIndex = 0;
	uint32_t cblk_index = 0;
	maximum = 0;
	if (block->qmfbid == 1) {
		for (auto j = 0U; j < h; ++j) {
			for (auto i = 0U; i < w; ++i) {
				int32_t temp = (block->tiledp[tileIndex] *= (1<< T1_NMSEDEC_FRACBITS));
				maximum = max((uint32_t)abs(temp), maximum);
				t1->data[cblk_index] = temp;
				tileIndex++;
				cblk_index++;
			}
			tileIndex += tileLineAdvance;
		}
	} else {
		for (auto j = 0U; j < h; ++j) {
			for (auto i = 0U; i < w; ++i) {
				int32_t temp = int_fix_mul_t1(tiledp[tileIndex], block->inv_step);
				maximum = max((uint32_t)abs(temp), maximum);
				t1->data[cblk_index] = temp;
				tileIndex++;
				cblk_index++;
			}
			tileIndex += tileLineAdvance;
		}
	}
}
double T1Part1::encode(encodeBlockInfo *block, grk_tcd_tile *tile,
		uint32_t max, bool doRateControl) {
	auto cblk = block->cblk;
	tcd_cblk_enc_t cblkopj;
	memset(&cblkopj, 0, sizeof(tcd_cblk_enc_t));

	cblkopj.x0 = block->x;
	cblkopj.y0 = block->y;
	cblkopj.x1 = block->x + cblk->x1 - cblk->x0;
	cblkopj.y1 = block->y + cblk->y1 - cblk->y0;
	assert(cblk->x1 - cblk->x0 > 0);
	assert(cblk->y1 - cblk->y0 > 0);

	cblkopj.data = cblk->data;
	cblkopj.data_size = cblk->data_size;

	auto disto = t1_encode_cblk(t1, &cblkopj, max, block->bandno,
			block->compno,
			(tile->comps + block->compno)->numresolutions - 1 - block->resno,
			block->qmfbid, block->stepsize, block->cblk_sty,
			block->mct_norms, block->mct_numcomps, doRateControl);

	cblk->num_passes_encoded = cblkopj.totalpasses;
	cblk->numbps = cblkopj.numbps;
	for (uint32_t i = 0; i < cblk->num_passes_encoded; ++i) {
		auto passopj = cblkopj.passes + i;
		auto passgrk = cblk->passes + i;
		passgrk->distortiondec = passopj->distortiondec;
		passgrk->len = passopj->len;
		passgrk->rate = passopj->rate;
		passgrk->term = passopj->term;
	}

	t1_code_block_enc_deallocate(&cblkopj);
	cblkopj.data = nullptr;

 	return disto;
}

bool T1Part1::decode(decodeBlockInfo *block) {
	auto cblk = block->cblk;
	bool ret;

  	if (!cblk->seg_buffers.get_len())
		return true;

	auto min_buf_vec = &cblk->seg_buffers;
	size_t total_seg_len = min_buf_vec->get_len() + GRK_FAKE_MARKER_BYTES;
	if (t1->cblkdatabuffersize < total_seg_len) {
		uint8_t *new_block = (uint8_t*) grk_realloc(t1->cblkdatabuffer,
				total_seg_len);
		if (!new_block)
			return false;
		t1->cblkdatabuffer = new_block;
		t1->cblkdatabuffersize = (uint32_t)total_seg_len;
	}
	size_t offset = 0;
	// note: min_buf_vec only contains segments of non-zero length
	for (size_t i = 0; i < min_buf_vec->size(); ++i) {
		grk_buf *seg = (grk_buf*) min_buf_vec->get(i);
		memcpy(t1->cblkdatabuffer + offset, seg->buf, seg->len);
		offset += seg->len;
	}
	tcd_seg_data_chunk_t chunk;
	chunk.len = t1->cblkdatabuffersize;
	chunk.data = t1->cblkdatabuffer;

	tcd_cblk_dec_t cblkopj;
	memset(&cblkopj, 0, sizeof(tcd_cblk_dec_t));
	cblkopj.numchunks = 1;
	cblkopj.chunks = &chunk;
	cblkopj.x0 = block->x;
	cblkopj.y0 = block->y;
	cblkopj.x1 = block->x + cblk->x1 - cblk->x0;
	cblkopj.y1 = block->y + cblk->y1 - cblk->y0;
	assert(cblk->x1 - cblk->x0 > 0);
	assert(cblk->y1 - cblk->y0 > 0);
	cblkopj.real_num_segs = cblk->numSegments;
	auto segs = new tcd_seg_t[cblk->numSegments];
	for (uint32_t i = 0; i < cblk->numSegments; ++i){
		auto sopj = segs + i;
		memset(sopj, 0, sizeof(tcd_seg_t));
		auto sgrk = cblk->segs + i;
		sopj->len = sgrk->len;
		assert(sopj->len <= total_seg_len);
		sopj->real_num_passes = sgrk->numpasses;
	}
	cblkopj.segs = segs;
	// subtract roishift as it was added when packet was parsed
	// and opj uses subtracted value
	cblkopj.numbps = cblk->numbps - block->roishift;

    ret =t1_decode_cblk(t1,
    				&cblkopj,
    				block->bandno,
					block->roishift,
					block->cblk_sty,
					false);

	delete[] segs;
	return ret;
}

void T1Part1::postDecode(decodeBlockInfo *block) {

	auto cblk = block->cblk;
	if (!cblk->seg_buffers.get_len())
		return;

	tcd_cblk_dec_t cblkopj;
	memset(&cblkopj, 0, sizeof(tcd_cblk_dec_t));
	cblkopj.x0 = block->x;
	cblkopj.y0 = block->y;
	cblkopj.x1 = block->x + cblk->x1 - cblk->x0;
	cblkopj.y1 = block->y + cblk->y1 - cblk->y0;
    post_decode(t1,
    		&cblkopj,
			block);
}

void T1Part1::post_decode(t1_info *t1,
						tcd_cblk_dec_t *cblk,
						decodeBlockInfo *block) {
	uint32_t roishift = block->roishift;
	uint32_t qmfbid = block->qmfbid;
	float stepsize = block->stepsize;
	int32_t *tilec_data = block->tiledp;
	uint32_t tile_w = block->tilec->width();
	bool whole_tile_decoding = block->tilec->whole_tile_decoding;
	auto src = t1->data;
	uint32_t dest_width = tile_w;
	uint32_t cblk_w = (uint32_t) (cblk->x1 - cblk->x0);
	uint32_t cblk_h = (uint32_t) (cblk->y1 - cblk->y0);

	if (roishift) {
		if (roishift >= 31) {
			for (uint16_t j = 0; j < cblk_h; ++j) {
				for (uint16_t i = 0; i < cblk_w; ++i) {
					src[(j * cblk_w) + i] = 0;
				}
			}
		} else {
			int32_t thresh = 1 << roishift;
			for (int j = 0; j < cblk_h; ++j) {
				for (int i = 0; i < cblk_w; ++i) {
					int32_t val = src[(j * cblk_w) + i];
					int32_t mag = abs(val);
					if (mag >= thresh) {
						mag >>= roishift;
						src[(j * cblk_w) + i] = val < 0 ? -mag : mag;
					}
				}
			}
		}
	}

	if (!whole_tile_decoding) {
    	if (qmfbid == 1) {
    		for (uint32_t j = 0; j < cblk_h; ++j) {
    			uint32_t i = 0;
    			for (; i < (cblk_w & ~(uint32_t) 3U); i += 4U) {
    				src[(j * cblk_w) + i + 0U] /= 2;
    				src[(j * cblk_w) + i + 1U] /= 2;
    				src[(j * cblk_w) + i + 2U] /= 2;
    				src[(j * cblk_w) + i + 3U] /= 2;
    			}
    			for (; i < cblk_w; ++i)
    				src[(j * cblk_w) + i] /= 2;
     		}
    	} else {
			float *GRK_RESTRICT tiledp = (float*) src;
			for (int j = 0; j < cblk_h; ++j) {
				float *GRK_RESTRICT tiledp2 = tiledp;
				for (int i = 0; i < cblk_w; ++i) {
					float tmp = (float) (*src) * stepsize;
					*tiledp2 = tmp;
					src++;
					tiledp2++;
				}
				tiledp += cblk_w;
			}
    	}
		// write directly from t1 to sparse array
        if (!block->tilec->m_sa->write(block->x,
					  block->y,
					  block->x + cblk_w,
					  block->y + cblk_h,
					  t1->data,
					  1,
					  cblk_w,
					  true)) {
			  return;
		  }
	} else {
		auto dest = tilec_data;
		if (qmfbid == 1) {
			int32_t *GRK_RESTRICT tiledp = dest;
			for (uint32_t j = 0; j < cblk_h; ++j) {
				uint32_t i = 0;
				for (; i < (cblk_w & ~(uint32_t) 3U); i += 4U) {
					int32_t tmp0 = src[(j * cblk_w) + i + 0U];
					int32_t tmp1 = src[(j * cblk_w) + i + 1U];
					int32_t tmp2 = src[(j * cblk_w) + i + 2U];
					int32_t tmp3 = src[(j * cblk_w) + i + 3U];
					((int32_t*) tiledp)[(j * (size_t) dest_width) + i + 0U] = tmp0
							/ 2;
					((int32_t*) tiledp)[(j * (size_t) dest_width) + i + 1U] = tmp1
							/ 2;
					((int32_t*) tiledp)[(j * (size_t) dest_width) + i + 2U] = tmp2
							/ 2;
					((int32_t*) tiledp)[(j * (size_t) dest_width) + i + 3U] = tmp3
							/ 2;
				}
				for (; i < cblk_w; ++i) {
					int32_t tmp = src[(j * cblk_w) + i];
					((int32_t*) tiledp)[(j * (size_t) dest_width) + i] = tmp / 2;
				}
			}
		} else {
			float *GRK_RESTRICT tiledp = (float*) dest;
			for (uint32_t j = 0; j < cblk_h; ++j) {
				float *GRK_RESTRICT tiledp2 = tiledp;
				for (uint32_t i = 0; i < cblk_w; ++i) {
					float tmp = (float) (*src) * stepsize;
					*tiledp2 = tmp;
					src++;
					tiledp2++;
				}
				tiledp += dest_width;
			}
		}

	}
}

}
}
