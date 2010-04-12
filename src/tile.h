/** 
 * \file tile.h
 * Public parts of Tile Class.
 *//* 
 *  Created : 2010
 *  Authors : Tim Massingham/Hazel Marsden
 *
 *  Copyright (C) 2010 by Tim Massingham, European Bioinformatics Institute
 *  tim.massingham@ebi.ac.uk
 *
 *  This file is part of the AYB base calling software.
 *
 *  AYB is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  AYB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with AYB.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TILE_H_
#define TILE_H_

#include "cluster.h"
#include "xio.h"

#define X(A) A ## CLUSTER
    #include "list.def"
#undef X

struct _struct_tile {
    unsigned int lane,tile,ncluster;
    LIST(CLUSTER) clusterlist;
};
typedef struct _struct_tile * TILE;

// Standard funcions
TILE new_TILE(void);
void free_TILE(TILE tile);
TILE copy_TILE(const TILE tile);
void show_TILE(XFILE * fp, const TILE tile, const unsigned int n);

// Read tile from file in Illumina int.txt format, reverse order
/*hmhm*/
//TILE read_known_TILE( XFILE * fp, unsigned int ncycle);
TILE read_known_TILE( XFILE * fp, unsigned int *ncycle) __attribute__((deprecated));

// Read tile from file in Illumina int.txt format, forwards order
/*hmhm*/
//TILE read_TILE( XFILE * fp, unsigned int ncycle);
TILE read_TILE( XFILE * fp, unsigned int *ncycle);

#endif /* TILE_H_ */
