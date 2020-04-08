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

#include "grok_includes.h"
#include  <stdexcept>

namespace grk {

TagTree::TagTree(uint64_t mynumleafsh, uint64_t mynumleafsv) :
		numleafsh(mynumleafsh), numleafsv(mynumleafsv), numnodes(0), nodes(
				nullptr), nodes_size(0) {
	int64_t nplh[32];
	int64_t nplv[32];
	TagTreeNode *node = nullptr;
	TagTreeNode *parent_node = nullptr;
	TagTreeNode *parent_node0 = nullptr;
	uint64_t i;
	int64_t j, k;
	uint64_t numlvls;
	uint64_t n;

	numlvls = 0;
	nplh[0] = (int64_t) numleafsh;
	nplv[0] = (int64_t) numleafsv;
	numnodes = 0;
	do {
		n = (uint64_t) (nplh[numlvls] * nplv[numlvls]);
		nplh[numlvls + 1] = (nplh[numlvls] + 1) / 2;
		nplv[numlvls + 1] = (nplv[numlvls] + 1) / 2;
		numnodes += n;
		++numlvls;
	} while (n > 1);

	if (numnodes == 0) {
		GROK_WARN(
				"tgt_create numnodes == 0, no tree created.");
		throw std::runtime_error("tgt_create numnodes == 0, no tree created");
	}

	nodes = new TagTreeNode[numnodes];
	nodes_size = numnodes * sizeof(TagTreeNode);

	node = nodes;
 parent_node = &nodes[numleafsh * numleafsv];
 parent_node0 = parent_node;

	for (i = 0; i < numlvls - 1; ++i) {
		for (j = 0; j < nplv[i]; ++j) {
			k = nplh[i];
			while (--k >= 0) {
				node->parent = parent_node;
				++node;
				if (--k >= 0) {
					node->parent = parent_node;
					++node;
				}
				++parent_node;
			}
			if ((j & 1) || j == nplv[i] - 1) {
			 parent_node0 = parent_node;
			} else {
			 parent_node = parent_node0;
			 parent_node0 += nplh[i];
			}
		}
	}
	node->parent = 0;
	reset();
}

TagTree::~TagTree() {
	delete[] nodes;
}

/**
 * Reinitialises a tag tree from an existing one.
 *
 * @param       num_leafs_h           the width of the array of leafs of the tree
 * @param       num_leafs_v           the height of the array of leafs of the tree
 * @return      a new tag tree if successful, nullptr otherwise
 */
bool TagTree::init(uint64_t num_leafs_h, uint64_t num_leafs_v) {
	
	int64_t nplh[32];
	int64_t nplv[32];
	TagTreeNode *node = nullptr;
	TagTreeNode *parent_node = nullptr;
	TagTreeNode *parent_node0 = nullptr;
	uint64_t i;
	int64_t j, k;
	uint64_t num_levels;
	uint64_t n;
	uint64_t node_size;

	if ((numleafsh != num_leafs_h) || (numleafsv != num_leafs_v)) {
		numleafsh = num_leafs_h;
		numleafsv = num_leafs_v;

	 num_levels = 0;
	 nplh[0] = (int64_t) num_leafs_h;
	 nplv[0] = (int64_t) num_leafs_v;
		numnodes = 0;
		do {
			n = (uint64_t) (nplh[num_levels] * nplv[num_levels]);
		 nplh[num_levels + 1] = (nplh[num_levels] + 1) / 2;
		 nplv[num_levels + 1] = (nplv[num_levels] + 1) / 2;
			numnodes += n;
			++num_levels;
		} while (n > 1);

		if (numnodes == 0) {
			return false;
		}
	 node_size = numnodes * sizeof(TagTreeNode);

		if (node_size > nodes_size) {
			TagTreeNode *new_nodes = new TagTreeNode[numnodes];
			for (i = 0; i < nodes_size / sizeof(TagTreeNode); ++i)
				new_nodes[i] = nodes[i];
			delete[] nodes;
			nodes = new_nodes;
			nodes_size = node_size;
		}
	 node = nodes;
	 parent_node = &nodes[numleafsh * numleafsv];
	 parent_node0 = parent_node;

		for (i = 0; i < num_levels - 1; ++i) {
			for (j = 0; j < nplv[i]; ++j) {
				k = nplh[i];
				while (--k >= 0) {
				 node->parent = parent_node;
					++node;
					if (--k >= 0) {
					 node->parent = parent_node;
						++node;
					}
					++parent_node;
				}
				if ((j & 1) || j == nplv[i] - 1) {
				 parent_node0 = parent_node;
				} else {
				 parent_node = parent_node0;
				 parent_node0 += nplh[i];
				}
			}
		}
	 node->parent = 0;
	}
	reset();
	return true;
}

void TagTree::reset() {
	uint64_t i;
	auto current_node = nodes;

	for (i = 0; i < numnodes; ++i) {
	 current_node->value = tag_tree_uninitialized_node_value;
	 current_node->low = 0;
	 current_node->known = 0;
		++current_node;
	}
}

void TagTree::setvalue(uint64_t leafno, int64_t value) {
	TagTreeNode *node;
	node = &nodes[leafno];

	while (node && node->value > value) {
		node->value = value;
		node = node->parent;
	}
}

void TagTree::encode(BitIO *bio, uint64_t leafno, int64_t threshold) {
	TagTreeNode *stk[31];
	TagTreeNode **stkptr;
	TagTreeNode *node;
	int64_t low;

	stkptr = stk;
	node = &nodes[leafno];
	while (node->parent) {
		*stkptr++ = node;
		node = node->parent;
	}

	low = 0;
	for (;;) {
		if (low > node->low) {
			node->low = low;
		} else {
			low = node->low;
		}

		while (low < threshold) {
			if (low >= node->value) {
				if (!node->known) {
					bio->write(1, 1);
					node->known = 1;
				}
				break;
			}
			bio->write(0, 1);
			++low;
		}

		node->low = low;
		if (stkptr == stk)
			break;
		node = *--stkptr;
	}
}

bool TagTree::decode(BitIO *bio, uint64_t leafno, int64_t threshold,
		uint8_t *decoded) {
	uint64_t value;

	if (!decodeValue(bio, leafno, threshold, &value))
		return false;
	*decoded = (value < (uint32_t) threshold) ? 1 : 0;
	return true;
}

bool TagTree::decodeValue(BitIO *bio, uint64_t leafno, int64_t threshold,
		uint64_t *value) {
	TagTreeNode *stk[31];
	TagTreeNode **stkptr;
	TagTreeNode *node;
	int64_t low;

	*value = tag_tree_uninitialized_node_value;
	stkptr = stk;
	node = &nodes[leafno];
	while (node->parent) {
		*stkptr++ = node;
		node = node->parent;
	}
	low = 0;
	for (;;) {
		if (low > node->low) {
			node->low = low;
		} else {
			low = node->low;
		}
		while (low < threshold && low < node->value) {
			uint32_t temp = 0;
			if (!bio->read(&temp, 1))
				return false;
			if (temp) {
				node->value = low;
			} else {
				++low;
			}
		}
		node->low = low;
		if (stkptr == stk) {
			break;
		}
		node = *--stkptr;
	}
	*value = (uint64_t)node->value;
	return true;
}

}
