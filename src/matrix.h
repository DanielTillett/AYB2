/**
 * \file matrix.h
 * Public parts of Matrix Class.
 *//* 
 *  Created : 2010
 *  Authors : Tim Massingham/Hazel Marsden
 *
 *  Copyright (C) 2010 by Tim Massingham, European Bioinformatics Institute
 *  tim.massingham@ebi.ac.uk
 *
 *  This file is part of the AYB base-calling software.
 *
 *  AYB is free software: you can redistribute it and/or modify
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

#ifndef MATRIX_H_
#define MATRIX_H_

#include <stdint.h>
#include <stdbool.h>
#include "xio.h"
#include "utility.h"

/*  Specification did not specify what type of input should be taken. Use a
 * typedef so it can be easily changed, although care must be taken with the
 * input and output routines (printf etc) to make sure types match.
 */

struct _matrix_str {
    int    nrow, ncol;
    real_t    * x;
    };

// Make future abstraction easier
typedef struct _matrix_str * MAT;

// standard functions
MAT new_MAT( const int nrow, const int ncol );
void free_MAT( MAT mat );
MAT copy_MAT( const MAT mat);
void show_MAT( XFILE * fp, const MAT mat, const uint32_t mrow, const uint32_t mcol);

// standard variations
MAT new_MAT_from_array( const uint32_t nrow, const uint32_t ncol, const real_t * x);
MAT new_MAT_from_line(const int nrow, int *ncol, char *ptr);
MAT identity_MAT( const int nrow);
MAT copyinto_MAT( MAT matout, const MAT matin);
MAT set_MAT( MAT mat, const real_t x);

// Input from stream
MAT read_MAT_from_column_file(XFILE * fp);

// Identities
bool is_square(const MAT mat);

// Operations
MAT vectranspose( const MAT mat, const unsigned int p );
MAT reshape_MAT( MAT mat, const int nrow);
MAT cholesky( MAT mat);
MAT invert_cholesky( MAT mat);
MAT trim_MAT( MAT mat, const int mrow, const int mcol, const bool forwards);
MAT * block_diagonal_MAT( const MAT mat, const int n);
MAT scale_MAT(MAT mat, const real_t f);
MAT transpose_inplace( MAT mat);
MAT invert(const MAT mat);

#endif /* MATRIX_H_ */
