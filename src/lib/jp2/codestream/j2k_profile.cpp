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
 * Copyright (c) 2008, Jerome Fimes, Communications & Systemes <jerome.fimes@c-s.fr>
 * Copyright (c) 2006-2007, Parvatha Elangovan
 * Copyright (c) 2010-2011, Kaori Hagihara
 * Copyright (c) 2011-2012, Centre National d'Etudes Spatiales (CNES), France
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

#include "grok_includes.h"

namespace grk {


/****************************
 * Cinema/IMF Profile Methods
 ***************************/

uint32_t j2k_initialise_4K_poc( grk_poc  *POC, uint32_t numres) {
	assert(numres > 0);
	POC[0].tile = 1;
	POC[0].resno0 = 0;
	POC[0].compno0 = 0;
	POC[0].layno1 = 1;
	POC[0].resno1 = (uint32_t) (numres - 1);
	POC[0].compno1 = 3;
	POC[0].prg1 = GRK_CPRL;
	POC[1].tile = 1;
	POC[1].resno0 = (uint32_t) (numres - 1);
	POC[1].compno0 = 0;
	POC[1].layno1 = 1;
	POC[1].resno1 = numres;
	POC[1].compno1 = 3;
	POC[1].prg1 = GRK_CPRL;
	return 2;
}

void j2k_set_cinema_parameters( grk_cparameters  *parameters,
		grk_image *image) {
	/* Configure cinema parameters */
	uint32_t i;

	/* No tiling */
	parameters->tile_size_on = false;
	parameters->cp_tdx = 1;
	parameters->cp_tdy = 1;

	/* One tile part for each component */
	parameters->tp_flag = 'C';
	parameters->tp_on = 1;

	/* Tile and Image shall be at (0,0) */
	parameters->cp_tx0 = 0;
	parameters->cp_ty0 = 0;
	parameters->image_offset_x0 = 0;
	parameters->image_offset_y0 = 0;

	/* Codeblock size= 32*32 */
	parameters->cblockw_init = 32;
	parameters->cblockh_init = 32;

	/* Codeblock style: no mode switch enabled */
	parameters->cblk_sty = 0;

	/* No ROI */
	parameters->roi_compno = -1;

	/* No subsampling */
	parameters->subsampling_dx = 1;
	parameters->subsampling_dy = 1;

	/* 9-7 transform */
	parameters->irreversible = true;

	/* Number of layers */
	if (parameters->tcp_numlayers > 1) {
		GROK_WARN(
				"JPEG 2000 profiles 3 and 4 (2k and 4k digital cinema) require:\n"
						"1 single quality layer"
						"-> Number of layers forced to 1 (rather than %d)\n"
						"-> Rate of the last layer (%3.1f) will be used",
				parameters->tcp_numlayers,
				parameters->tcp_rates[parameters->tcp_numlayers - 1]);
		parameters->tcp_rates[0] =
				parameters->tcp_rates[parameters->tcp_numlayers - 1];
		parameters->tcp_numlayers = 1;
	}

	/* Resolution levels */
	switch (parameters->rsiz) {
	case GRK_PROFILE_CINEMA_2K:
		if (parameters->numresolution > 6) {
			GROK_WARN(
					"JPEG 2000 profile 3 (2k digital cinema) requires:\n"
							"Number of decomposition levels <= 5\n"
							"-> Number of decomposition levels forced to 5 (rather than %d)\n",
					parameters->numresolution + 1);
			parameters->numresolution = 6;
		}
		break;
	case GRK_PROFILE_CINEMA_4K:
		if (parameters->numresolution < 2) {
			GROK_WARN(
					"JPEG 2000 profile 4 (4k digital cinema) requires:\n"
							"Number of decomposition levels >= 1 && <= 6\n"
							"-> Number of decomposition levels forced to 1 (rather than %d)\n",
					parameters->numresolution + 1);
			parameters->numresolution = 1;
		} else if (parameters->numresolution > 7) {
			GROK_WARN(
					"JPEG 2000 profile 4 (4k digital cinema) requires:\n"
							"Number of decomposition levels >= 1 && <= 6\n"
							"-> Number of decomposition levels forced to 6 (rather than %d)\n",
					parameters->numresolution + 1);
			parameters->numresolution = 7;
		}
		break;
	default:
		break;
	}

	/* Precincts */
	parameters->csty |= J2K_CP_CSTY_PRT;
	parameters->res_spec = parameters->numresolution - 1;
	for (i = 0; i < parameters->res_spec; i++) {
		parameters->prcw_init[i] = 256;
		parameters->prch_init[i] = 256;
	}

	/* The progression order shall be CPRL */
	parameters->prog_order = GRK_CPRL;

	/* Progression order changes for 4K, disallowed for 2K */
	if (parameters->rsiz == GRK_PROFILE_CINEMA_4K) {
		parameters->numpocs = j2k_initialise_4K_poc(parameters->POC,
				parameters->numresolution);
	} else {
		parameters->numpocs = 0;
	}

	/* Limit bit-rate */
	parameters->cp_disto_alloc = 1;
	if (parameters->max_cs_size == 0) {
		/* No rate has been introduced for code stream, so 24 fps is assumed */
		parameters->max_cs_size = GRK_CINEMA_24_CS;
		GROK_WARN(
				"JPEG 2000 profiles 3 and 4 (2k and 4k digital cinema) require:\n"
						"Maximum 1302083 compressed bytes @ 24fps for code stream.\n"
						"As no rate has been given for entire code stream, this limit will be used.");
	} else if (parameters->max_cs_size > GRK_CINEMA_24_CS) {
		GROK_WARN(
				"JPEG 2000 profiles 3 and 4 (2k and 4k digital cinema) require:\n"
						"Maximum 1302083 compressed bytes @ 24fps for code stream.\n"
						"The specified rate exceeds this limit, so rate will be forced to 1302083 bytes.");
		parameters->max_cs_size = GRK_CINEMA_24_CS;
	}

	if (parameters->max_comp_size == 0) {
		/* No rate has been introduced for each component, so 24 fps is assumed */
		parameters->max_comp_size = GRK_CINEMA_24_COMP;
		GROK_WARN(
				"JPEG 2000 profiles 3 and 4 (2k and 4k digital cinema) require:\n"
						"Maximum 1041666 compressed bytes @ 24fps per component.\n"
						"As no rate has been given, this limit will be used.");
	} else if (parameters->max_comp_size > GRK_CINEMA_24_COMP) {
		GROK_WARN(
				"JPEG 2000 profiles 3 and 4 (2k and 4k digital cinema) require:\n"
						"Maximum 1041666 compressed bytes @ 24fps per component.\n"
						"The specified rate exceeds this limit, so rate will be forced to 1041666 bytes.");
		parameters->max_comp_size = GRK_CINEMA_24_COMP;
	}

	parameters->tcp_rates[0] = ((double) image->numcomps * image->comps[0].w
			* image->comps[0].h * image->comps[0].prec)
			/ ((double) parameters->max_cs_size * 8 * image->comps[0].dx
					* image->comps[0].dy);

}

bool j2k_is_cinema_compliant(grk_image *image, uint16_t rsiz) {
	uint32_t i;

	/* Number of components */
	if (image->numcomps != 3) {
		GROK_WARN(
				"JPEG 2000 profile 3 (2k digital cinema) requires:\n"
						"3 components"
						"-> Number of components of input image (%d) is not compliant\n"
						"-> Non-profile-3 codestream will be generated\n",
				image->numcomps);
		return false;
	}

	/* Bitdepth */
	for (i = 0; i < image->numcomps; i++) {
		if ((image->comps[i].prec != 12) | (image->comps[i].sgnd)) {
			char signed_str[] = "signed";
			char unsigned_str[] = "unsigned";
			char *tmp_str = image->comps[i].sgnd ? signed_str : unsigned_str;
			GROK_WARN(
					"JPEG 2000 profile 3 (2k digital cinema) requires:\n"
							"Precision of each component shall be 12 bits unsigned"
							"-> At least component %d of input image (%d bits, %s) is not compliant\n"
							"-> Non-profile-3 codestream will be generated\n",
					i, image->comps[i].prec, tmp_str);
			return false;
		}
	}

	/* Image size */
	switch (rsiz) {
	case GRK_PROFILE_CINEMA_2K:
		if (((image->comps[0].w > 2048) | (image->comps[0].h > 1080))) {
			GROK_WARN(
					"JPEG 2000 profile 3 (2k digital cinema) requires:\n"
							"width <= 2048 and height <= 1080\n"
							"-> Input image size %d x %d is not compliant\n"
							"-> Non-profile-3 codestream will be generated\n",
					image->comps[0].w, image->comps[0].h);
			return false;
		}
		break;
	case GRK_PROFILE_CINEMA_4K:
		if (((image->comps[0].w > 4096) | (image->comps[0].h > 2160))) {
			GROK_WARN(
					"JPEG 2000 profile 4 (4k digital cinema) requires:\n"
							"width <= 4096 and height <= 2160\n"
							"-> Image size %d x %d is not compliant\n"
							"-> Non-profile-4 codestream will be generated\n",
					image->comps[0].w, image->comps[0].h);
			return false;
		}
		break;
	default:
		break;
	}

	return true;
}

static int j2k_get_imf_max_NL(grk_cparameters *parameters,
                                  grk_image *image)
{
    /* Decomposition levels */
    const uint16_t rsiz = parameters->rsiz;
    const uint16_t profile = GRK_GET_IMF_PROFILE(rsiz);
    const uint32_t XTsiz = parameters->tile_size_on ? (uint32_t)
                             parameters->cp_tdx : image->x1;
    switch (profile) {
    case GRK_PROFILE_IMF_2K:
        return 5;
    case GRK_PROFILE_IMF_4K:
        return 6;
    case GRK_PROFILE_IMF_8K:
        return 7;
    case GRK_PROFILE_IMF_2K_R: {
        if (XTsiz >= 2048) {
            return 5;
        } else if (XTsiz >= 1024) {
            return 4;
        }
        break;
    }
    case GRK_PROFILE_IMF_4K_R: {
        if (XTsiz >= 4096) {
            return 6;
        } else if (XTsiz >= 2048) {
            return 5;
        } else if (XTsiz >= 1024) {
            return 4;
        }
        break;
    }
    case GRK_PROFILE_IMF_8K_R: {
        if (XTsiz >= 8192) {
            return 7;
        } else if (XTsiz >= 4096) {
            return 6;
        } else if (XTsiz >= 2048) {
            return 5;
        } else if (XTsiz >= 1024) {
            return 4;
        }
        break;
    }
    default:
        break;
    }
    return -1;
}

void j2k_set_imf_parameters(grk_cparameters *parameters, grk_image *image)
{
    const uint16_t rsiz = parameters->rsiz;
    const uint16_t profile = GRK_GET_IMF_PROFILE(rsiz);

    /* Override defaults set by set_default_encoder_parameters */
    if (parameters->cblockw_init == GRK_COMP_PARAM_DEFAULT_CBLOCKW &&
            parameters->cblockh_init == GRK_COMP_PARAM_DEFAULT_CBLOCKH) {
        parameters->cblockw_init = 32;
        parameters->cblockh_init = 32;
    }

    /* One tile part for each component */
    parameters->tp_flag = 'C';
    parameters->tp_on = 1;

    if (parameters->prog_order == GRK_COMP_PARAM_DEFAULT_PROG_ORDER) {
        parameters->prog_order = GRK_CPRL;
    }

    if (profile == GRK_PROFILE_IMF_2K ||
            profile == GRK_PROFILE_IMF_4K ||
            profile == GRK_PROFILE_IMF_8K) {
        /* 9-7 transform */
        parameters->irreversible = true;
    }

    /* Adjust the number of resolutions if set to its defaults */
    if (parameters->numresolution == GRK_COMP_PARAM_DEFAULT_NUMRESOLUTION &&
            image->x0 == 0 &&
            image->y0 == 0) {
        const int max_NL = j2k_get_imf_max_NL(parameters, image);
        if (max_NL >= 0 && parameters->numresolution > (uint32_t)max_NL) {
            parameters->numresolution = (uint32_t)(max_NL + 1);
        }

        /* Note: below is generic logic */
        if (!parameters->tile_size_on) {
            while (parameters->numresolution > 0) {
                if (image->x1 < (1U << ((uint32_t)parameters->numresolution - 1U))) {
                    parameters->numresolution --;
                    continue;
                }
                if (image->y1 < (1U << ((uint32_t)parameters->numresolution - 1U))) {
                    parameters->numresolution --;
                    continue;
                }
                break;
            }
        }
    }

    /* Set defaults precincts */
    if (parameters->csty == 0) {
        parameters->csty |= J2K_CP_CSTY_PRT;
        if (parameters->numresolution == 1) {
            parameters->res_spec = 1;
            parameters->prcw_init[0] = 128;
            parameters->prch_init[0] = 128;
        } else {
            parameters->res_spec = parameters->numresolution - 1;
            for (uint32_t i = 0; i < parameters->res_spec; i++) {
                parameters->prcw_init[i] = 256;
                parameters->prch_init[i] = 256;
            }
        }
    }
}

/* Table A.53 from JPEG2000 standard */
static const uint16_t tabMaxSubLevelFromMainLevel[] = {
    15, /* unspecified */
    1,
    1,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9
};

bool j2k_is_imf_compliant(grk_cparameters *parameters,
        grk_image *image)
{
	assert(parameters->numresolution > 0);
	if (parameters->numresolution == 0)
		return false;
    uint32_t i;
    const uint16_t rsiz = parameters->rsiz;
    const uint16_t profile = GRK_GET_IMF_PROFILE(rsiz);
    const uint16_t mainlevel = GRK_GET_IMF_MAINLEVEL(rsiz);
    const uint16_t sublevel = GRK_GET_IMF_SUBLEVEL(rsiz);
    const uint32_t NL = (uint32_t)(parameters->numresolution - 1);
    const uint32_t XTsiz = parameters->tile_size_on ? (uint32_t)
                             parameters->cp_tdx : image->x1;
    bool ret = true;

    /* Validate mainlevel */
    if (mainlevel > GRK_MAINLEVEL_MAX) {
        GROK_WARN(   "IMF profile require mainlevel <= 11.\n"
                      "-> %d is thus not compliant\n"
                      "-> Non-IMF codestream will be generated\n",
                      mainlevel);
        ret = false;
    }

    /* Validate sublevel */
    assert(sizeof(tabMaxSubLevelFromMainLevel) ==
           (GRK_MAINLEVEL_MAX + 1) * sizeof(tabMaxSubLevelFromMainLevel[0]));
    if (sublevel > tabMaxSubLevelFromMainLevel[mainlevel]) {
    	GROK_WARN(   "IMF profile require sublevel <= %d for mainlevel = %d.\n"
                      "-> %d is thus not compliant\n"
                      "-> Non-IMF codestream will be generated\n",
                      tabMaxSubLevelFromMainLevel[mainlevel],
                      mainlevel,
                      sublevel);
        ret = false;
    }

    /* Number of components */
    if (image->numcomps > 3) {
    	GROK_WARN(    "IMF profiles require at most 3 components.\n"
                      "-> Number of components of input image (%d) is not compliant\n"
                      "-> Non-IMF codestream will be generated\n",
                      image->numcomps);
        ret = false;
    }

    if (image->x0 != 0 || image->y0 != 0) {
        GROK_WARN(   "IMF profiles require image origin to be at 0,0.\n"
                      "-> %d,%d is not compliant\n"
                      "-> Non-IMF codestream will be generated\n",
                      image->x0, image->y0 != 0);
        ret = false;
    }

    if (parameters->cp_tx0 != 0 || parameters->cp_ty0 != 0) {
        GROK_WARN(   "IMF profiles require tile origin to be at 0,0.\n"
                      "-> %d,%d is not compliant\n"
                      "-> Non-IMF codestream will be generated\n",
                      parameters->cp_tx0, parameters->cp_ty0);
        ret = false;
    }

    if (parameters->tile_size_on) {
        if (profile == GRK_PROFILE_IMF_2K ||
                profile == GRK_PROFILE_IMF_4K ||
                profile == GRK_PROFILE_IMF_8K) {
            if ((uint32_t)parameters->cp_tdx < image->x1 ||
                    (uint32_t)parameters->cp_tdy < image->y1) {
                GROK_WARN(   "IMF 2K/4K/8K single tile profiles require tile to be greater or equal to image size.\n"
                              "-> %d,%d is lesser than %d,%d\n"
                              "-> Non-IMF codestream will be generated\n",
                              parameters->cp_tdx,
                              parameters->cp_tdy,
                              image->x1,
                              image->y1);
                ret = false;
            }
        } else {
            if ((uint32_t)parameters->cp_tdx >= image->x1 &&
                    (uint32_t)parameters->cp_tdy >= image->y1) {
                /* ok */
            } else if (parameters->cp_tdx == 1024 &&
                       parameters->cp_tdy == 1024) {
                /* ok */
            } else if (parameters->cp_tdx == 2048 &&
                       parameters->cp_tdy == 2048 &&
                       (profile == GRK_PROFILE_IMF_4K ||
                        profile == GRK_PROFILE_IMF_8K)) {
                /* ok */
            } else if (parameters->cp_tdx == 4096 &&
                       parameters->cp_tdy == 4096 &&
                       profile == GRK_PROFILE_IMF_8K) {
                /* ok */
            } else {
                GROK_WARN(    "IMF 2K_R/4K_R/8K_R single/multiple tile profiles "
                              "require tile to be greater or equal to image size,\n"
                              "or to be (1024,1024), or (2048,2048) for 4K_R/8K_R "
                              "or (4096,4096) for 8K_R.\n"
                              "-> %d,%d is non conformant\n"
                              "-> Non-IMF codestream will be generated\n",
                              parameters->cp_tdx,
                              parameters->cp_tdy);
                ret = false;
            }
        }
    }

    /* Bitdepth */
    for (i = 0; i < image->numcomps; i++) {
        if (!(image->comps[i].prec >= 8 && image->comps[i].prec <= 16) ||
                (image->comps[i].sgnd)) {
            char signed_str[] = "signed";
            char unsigned_str[] = "unsigned";
            char *tmp_str = image->comps[i].sgnd ? signed_str : unsigned_str;
            GROK_WARN(   "IMF profiles require precision of each component to b in [8-16] bits unsigned"
                          "-> At least component %d of input image (%d bits, %s) is not compliant\n"
                          "-> Non-IMF codestream will be generated\n",
                          i, image->comps[i].prec, tmp_str);
            ret = false;
        }
    }

    /* Sub-sampling */
    for (i = 0; i < image->numcomps; i++) {
        if (i == 0 && image->comps[i].dx != 1) {
        	GROK_WARN(   "IMF profiles require XRSiz1 == 1. Here it is set to %d.\n"
                          "-> Non-IMF codestream will be generated\n",
                          image->comps[i].dx);
            ret = false;
        }
        if (i == 1 && image->comps[i].dx != 1 && image->comps[i].dx != 2) {
            GROK_WARN(   "IMF profiles require XRSiz2 == 1 or 2. Here it is set to %d.\n"
                          "-> Non-IMF codestream will be generated\n",
                          image->comps[i].dx);
            ret = false;
        }
        if (i > 1 && image->comps[i].dx != image->comps[i - 1].dx) {
            GROK_WARN(   "IMF profiles require XRSiz%d to be the same as XRSiz2. "
                          "Here it is set to %d instead of %d.\n"
                          "-> Non-IMF codestream will be generated\n",
                          i + 1, image->comps[i].dx, image->comps[i - 1].dx);
            ret = false;
        }
        if (image->comps[i].dy != 1) {
            GROK_WARN(    "IMF profiles require YRsiz == 1. "
                          "Here it is set to %d for component i.\n"
                          "-> Non-IMF codestream will be generated\n",
                          image->comps[i].dy, i);
            ret = false;
        }
    }

    /* Image size */
    switch (profile) {
    case GRK_PROFILE_IMF_2K:
    case GRK_PROFILE_IMF_2K_R:
        if (((image->comps[0].w > 2048) | (image->comps[0].h > 1556))) {
            GROK_WARN(    "IMF 2K/2K_R profile require:\n"
                          "width <= 2048 and height <= 1556\n"
                          "-> Input image size %d x %d is not compliant\n"
                          "-> Non-IMF codestream will be generated\n",
                          image->comps[0].w, image->comps[0].h);
            ret = false;
        }
        break;
    case GRK_PROFILE_IMF_4K:
    case GRK_PROFILE_IMF_4K_R:
        if (((image->comps[0].w > 4096) | (image->comps[0].h > 3112))) {
            GROK_WARN(   "IMF 4K/4K_R profile require:\n"
                          "width <= 4096 and height <= 3112\n"
                          "-> Input image size %d x %d is not compliant\n"
                          "-> Non-IMF codestream will be generated\n",
                          image->comps[0].w, image->comps[0].h);
            ret = false;
        }
        break;
    case GRK_PROFILE_IMF_8K:
    case GRK_PROFILE_IMF_8K_R:
        if (((image->comps[0].w > 8192) | (image->comps[0].h > 6224))) {
            GROK_WARN(    "IMF 8K/8K_R profile require:\n"
                          "width <= 8192 and height <= 6224\n"
                          "-> Input image size %d x %d is not compliant\n"
                          "-> Non-IMF codestream will be generated\n",
                          image->comps[0].w, image->comps[0].h);
            ret = false;
        }
        break;
    default :
        assert(0);
        return false;
    }

    if (parameters->roi_compno != -1) {
        GROK_WARN(   "IMF profile forbid RGN / region of interest marker.\n"
                      "-> Compression parameters specify a ROI\n"
                      "-> Non-IMF codestream will be generated\n");
        ret = false;
    }

    if (parameters->cblockw_init != 32 || parameters->cblockh_init != 32) {
    	GROK_WARN(    "IMF profile require code block size to be 32x32.\n"
                      "-> Compression parameters set it to %dx%d.\n"
                      "-> Non-IMF codestream will be generated\n",
                      parameters->cblockw_init,
                      parameters->cblockh_init);
        ret = false;
    }

    if (parameters->prog_order != GRK_CPRL) {
    	GROK_WARN(   "IMF profile require progression order to be CPRL.\n"
                      "-> Compression parameters set it to %d.\n"
                      "-> Non-IMF codestream will be generated\n",
                      parameters->prog_order);
        ret = false;
    }

    if (parameters->numpocs != 0) {
    	GROK_WARN(    "IMF profile forbid POC markers.\n"
                      "-> Compression parameters set %d POC.\n"
                      "-> Non-IMF codestream will be generated\n",
                      parameters->numpocs);
        ret = false;
    }

    /* Codeblock style: no mode switch enabled */
    if (parameters->cblk_sty != 0) {
    	GROK_WARN(   "IMF profile forbid mode switch in code block style.\n"
                      "-> Compression parameters set code block style to %d.\n"
                      "-> Non-IMF codestream will be generated\n",
                      parameters->cblk_sty);
        ret = false;
    }

    if (profile == GRK_PROFILE_IMF_2K ||
            profile == GRK_PROFILE_IMF_4K ||
            profile == GRK_PROFILE_IMF_8K) {
        /* Expect 9-7 transform */
        if (!parameters->irreversible) {
        	GROK_WARN(    "IMF 2K/4K/8K profiles require 9-7 Irreversible Transform.\n"
                          "-> Compression parameters set it to reversible.\n"
                          "-> Non-IMF codestream will be generated\n");
            ret = false;
        }
    } else {
        /* Expect 5-3 transform */
        if (parameters->irreversible) {
        	GROK_WARN(    "IMF 2K/4K/8K profiles require 5-3 reversible Transform.\n"
                          "-> Compression parameters set it to irreversible.\n"
                          "-> Non-IMF codestream will be generated\n");
            ret = false;
        }
    }

    /* Number of layers */
    if (parameters->tcp_numlayers != 1) {
    	GROK_WARN(    "IMF 2K/4K/8K profiles require 1 single quality layer.\n"
                      "-> Number of layers is %d.\n"
                      "-> Non-IMF codestream will be generated\n",
                      parameters->tcp_numlayers);
        ret = false;
    }

    /* Decomposition levels */
    switch (profile) {
    case GRK_PROFILE_IMF_2K:
        if (!(NL >= 1 && NL <= 5)) {
        	GROK_WARN(    "IMF 2K profile requires 1 <= NL <= 5:\n"
                          "-> Number of decomposition levels is %d.\n"
                          "-> Non-IMF codestream will be generated\n",
                          NL);
            ret = false;
        }
        break;
    case GRK_PROFILE_IMF_4K:
        if (!(NL >= 1 && NL <= 6)) {
        	GROK_WARN(    "IMF 4K profile requires 1 <= NL <= 6:\n"
                          "-> Number of decomposition levels is %d.\n"
                          "-> Non-IMF codestream will be generated\n",
                          NL);
            ret = false;
        }
        break;
    case GRK_PROFILE_IMF_8K:
        if (!(NL >= 1 && NL <= 7)) {
        	GROK_WARN(    "IMF 8K profile requires 1 <= NL <= 7:\n"
                          "-> Number of decomposition levels is %d.\n"
                          "-> Non-IMF codestream will be generated\n",
                          NL);
            ret = false;
        }
        break;
    case GRK_PROFILE_IMF_2K_R: {
        if (XTsiz >= 2048) {
            if (!(NL >= 1 && NL <= 5)) {
            	GROK_WARN(    "IMF 2K_R profile requires 1 <= NL <= 5 for XTsiz >= 2048:\n"
                              "-> Number of decomposition levels is %d.\n"
                              "-> Non-IMF codestream will be generated\n",
                              NL);
                ret = false;
            }
        } else if (XTsiz >= 1024) {
            if (!(NL >= 1 && NL <= 4)) {
            	GROK_WARN(    "IMF 2K_R profile requires 1 <= NL <= 4 for XTsiz in [1024,2048[:\n"
                              "-> Number of decomposition levels is %d.\n"
                              "-> Non-IMF codestream will be generated\n",
                              NL);
                ret = false;
            }
        }
        break;
    }
    case GRK_PROFILE_IMF_4K_R: {
        if (XTsiz >= 4096) {
            if (!(NL >= 1 && NL <= 6)) {
            	GROK_WARN(    "IMF 4K_R profile requires 1 <= NL <= 6 for XTsiz >= 4096:\n"
                              "-> Number of decomposition levels is %d.\n"
                              "-> Non-IMF codestream will be generated\n",
                              NL);
                ret = false;
            }
        } else if (XTsiz >= 2048) {
            if (!(NL >= 1 && NL <= 5)) {
            	GROK_WARN(    "IMF 4K_R profile requires 1 <= NL <= 5 for XTsiz in [2048,4096[:\n"
                              "-> Number of decomposition levels is %d.\n"
                              "-> Non-IMF codestream will be generated\n",
                              NL);
                ret = false;
            }
        } else if (XTsiz >= 1024) {
            if (!(NL >= 1 && NL <= 4)) {
            	GROK_WARN(    "IMF 4K_R profile requires 1 <= NL <= 4 for XTsiz in [1024,2048[:\n"
                              "-> Number of decomposition levels is %d.\n"
                              "-> Non-IMF codestream will be generated\n",
                              NL);
                ret = false;
            }
        }
        break;
    }
    case GRK_PROFILE_IMF_8K_R: {
        if (XTsiz >= 8192) {
            if (!(NL >= 1 && NL <= 7)) {
                GROK_WARN(
                              "IMF 4K_R profile requires 1 <= NL <= 7 for XTsiz >= 8192:\n"
                              "-> Number of decomposition levels is %d.\n"
                              "-> Non-IMF codestream will be generated\n",
                              NL);
                ret = false;
            }
        } else if (XTsiz >= 4096) {
            if (!(NL >= 1 && NL <= 6)) {
                GROK_WARN(
                              "IMF 4K_R profile requires 1 <= NL <= 6 for XTsiz in [4096,8192[:\n"
                              "-> Number of decomposition levels is %d.\n"
                              "-> Non-IMF codestream will be generated\n",
                              NL);
                ret = false;
            }
        } else if (XTsiz >= 2048) {
            if (!(NL >= 1 && NL <= 5)) {
                GROK_WARN(
                              "IMF 4K_R profile requires 1 <= NL <= 5 for XTsiz in [2048,4096[:\n"
                              "-> Number of decomposition levels is %d.\n"
                              "-> Non-IMF codestream will be generated\n",
                              NL);
                ret = false;
            }
        } else if (XTsiz >= 1024) {
            if (!(NL >= 1 && NL <= 4)) {
                GROK_WARN(
                              "IMF 4K_R profile requires 1 <= NL <= 4 for XTsiz in [1024,2048[:\n"
                              "-> Number of decomposition levels is %d.\n"
                              "-> Non-IMF codestream will be generated\n",
                              NL);
                ret = false;
            }
        }
        break;
    }
    default:
        break;
    }

    if (parameters->numresolution == 1) {
        if (parameters->res_spec != 1 ||
                parameters->prcw_init[0] != 128 ||
                parameters->prch_init[0] != 128) {
            GROK_WARN(
                          "IMF profiles require PPx = PPy = 7 for NLLL band, else 8.\n"
                          "-> Supplied values are different from that.\n"
                          "-> Non-IMF codestream will be generated\n",
                          NL);
            ret = false;
        }
    } else {
        for (uint32_t i = 0; i < parameters->res_spec; i++) {
            if (parameters->prcw_init[i] != 256 ||
                    parameters->prch_init[i] != 256) {
                GROK_WARN(
                              "IMF profiles require PPx = PPy = 7 for NLLL band, else 8.\n"
                              "-> Supplied values are different from that.\n"
                              "-> Non-IMF codestream will be generated\n",
                              NL);
                ret = false;
            }
        }
    }

    return ret;
}



}
