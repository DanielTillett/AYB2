/** 
 * \file intensities.c
 * Intensities Processing.
 *//* 
 *  Created : 19 Apr 2010
 *  Author  : Tim Massingham/Hazel Marsden
 *
 *  Copyright (C) 2010 European Bioinformatics Institute
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

#include <string.h>
#include "intensities.h"
#include "nuc.h"

/* constants */
/* None      */


/* members */
/* None    */


/* private functions */
/* None              */


/* public functions */

/**
 * Process intensities.
 * ip = Minv %*% (Intensities-N) %*% Pinv
 *   - Uses identity: Vec(ip) = ( Pinv^t kronecker Minv) Vec(Intensities-N)
 *   - Storing Intensities-N as an intermediate saved < 3%
 *   - Calculating ip^t rather than ip (pcol loop is over minor index) made no difference
 *   - Using Pinv rather than Pinv^t makes little appreciable difference
 */
MAT process_intensities(const MAT intensities,
                        const MAT Minv_t, const MAT Pinv_t, const MAT N, MAT ip) {

    validate(NULL != intensities, NULL);
    validate(NULL != Minv_t, NULL);
    validate(NULL != Pinv_t, NULL);
    validate(NULL != N, NULL);

    const uint_fast32_t ncycle = Pinv_t->nrow;
    if (NULL==ip){
        ip = new_MAT(NBASE, ncycle);
        validate(NULL != ip, NULL);
    }
    memset(ip->x, 0, ip->nrow * ip->ncol * sizeof(real_t));

    for (uint_fast32_t icol = 0; icol < ncycle; icol++) {    // Columns of Intensity
        for (uint_fast32_t base = 0; base < NBASE; base++) { // Bases (rows of Minv, cols of Minv_t)
            real_t dp = 0;
            for (uint_fast32_t chan = 0; chan < NBASE; chan++) {  // Channels
                dp += Minv_t->x[base * NBASE + chan] *
                        (intensities->x[icol * NBASE + chan] - N->x[icol * NBASE + chan]);
            }
            for (uint_fast32_t pcol = 0; pcol < ncycle; pcol++) { // Columns of ip
                ip->x[pcol * NBASE + base] += Pinv_t->x[icol * ncycle + pcol] * dp;
            }
        }
    }

    return ip;
}

MAT expected_intensities(const real_t lambda, const NUC * bases,
                         const MAT M, const MAT P, const MAT N, MAT e){
    validate(lambda>=0,NULL);
    validate(NULL!=bases,NULL);
    validate(NULL!=M,NULL);
    validate(NULL!=P,NULL);
    validate(NULL!=N,NULL);
    const uint_fast32_t ncycle = P->nrow;
    if(NULL==e){
        e = new_MAT(NBASE,ncycle);
        validate(NULL!=e,NULL);
    }
    memset(e->x, 0, NBASE*ncycle*sizeof(real_t));

    if(has_ambiguous_base(bases,ncycle)){
        for(uint_fast32_t cy2=0 ; cy2<ncycle ; cy2++){
            for(uint_fast32_t cy=0 ; cy<ncycle ; cy++){
                const uint_fast32_t base = bases[cy];
                if(!isambig(base)){
                    for ( uint_fast32_t ch=0 ; ch<NBASE ; ch++){
                        e->x[cy2*NBASE+ch] += M->x[base*NBASE+ch] * P->x[cy2*ncycle+cy];
		    }
                }
            }
        }
    } else {
       for(uint_fast32_t cy2=0 ; cy2<ncycle ; cy2++){
            for(uint_fast32_t cy=0 ; cy<ncycle ; cy++){
                const uint_fast32_t base = bases[cy];
                for ( uint_fast32_t ch=0 ; ch<NBASE ; ch++){
                    e->x[cy2*NBASE+ch] += M->x[base*NBASE+ch] * P->x[cy2*ncycle+cy];
                }
            }
        }
    }

    // Multiply by brightness;
    scale_MAT(e,lambda);
    // Add noise
    for ( uint_fast32_t i=0 ; i<(NBASE*ncycle) ; i++){
        e->x[i] += N->x[i];
    }
    return e;
}
