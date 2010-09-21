/** 
 * \file call_bases.c
 * Call Bases.
 *//* 
 *  Created : 20 Apr 2010
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

#include <assert.h>
#include <math.h>
#include "call_bases.h"

/* constants */
/* None      */


/* members */

static real_t Mu = 1e-5;                        ///< Tolerance used in quality score calculation?


/* private functions */

/** Maximum of n real_ts. Should possibly be moved into utility.h library. */
static inline int max_real_t(const real_t * restrict p, const uint32_t n){
    validate(NULL!=p,-1);
    real_t m = p[0];
    int idx = 0;
    for ( uint32_t i=1 ; i<n ; i++){
        if(p[i]>m){ m=p[i]; idx=i;}
    }
    return idx;
}


/* public functions */

/**
 * Call base from processed intensities using maximum intensity.
 * Used for initial base call.
 */
NUC call_base_simple( const real_t * restrict p){
    return max_real_t(p,NBASE);
}

/** Return a null base call, used when insufficient data available. */
struct basequal call_base_null(void){
    struct basequal b = {0, MIN_PHRED};
    return b;
}

/**
 * Call base from processed intensities using minimum Least Squares.
 * Also returns a quality score.
 * - p:       Processed intensities for given cycle
 * - lambda:  Cluster brightness
 * - omega:   Cycle specific inverse covariance matrix
 */
struct basequal call_base( const real_t * restrict p, const real_t lambda, const MAT omega){
    assert(NULL!=p);
    assert(NULL!=omega);
    assert(NBASE==omega->nrow);
    assert(NBASE==omega->ncol);

    if(0==lambda){
        #warning "Should call bases should return random base when lambda=0?"
//        struct basequal b = {0,33};
//        return b;
        return call_base_null();
        // Special case, return random value
        //return (int)(NBASE*runif());
    }

    int call = 0;
    real_t minstat = HUGE_VAL;
    real_t stat[NBASE] = { 0.,0.,0.,0.};
    for ( int i=0 ; i<NBASE ; i++){
        stat[i] = lambda * omega->x[i*NBASE+i];
        for ( int j=0 ; j<NBASE ; j++){
            stat[i] -= 2.0 * p[j] * omega->x[i*NBASE+j];
        }
        if(stat[i]<minstat){
            minstat = stat[i];
            call = i;
        }
    }
    /* Summation of probabilities for normalisation,
     * having removed factor exp(-0.5*(K+lambda*minstat))
     */
    real_t tot = 0.;
    for ( int i=0 ; i<NBASE ; i++){
        tot += exp(-0.5*lambda*(stat[i]-minstat));
    }

    real_t K = xMy(p,omega,p);
    real_t maxprob = exp(-0.5*(K+lambda*minstat));

    /* Calculate posterior probability in numerically stable fashion
     * Note that maxp can be extremely small.
     */
    real_t post_prob = (maxprob<Mu) ?
       // Case probabilities small compared to mu
       (Mu + maxprob ) / (4*Mu + maxprob*tot) :
       // Case probabilities large compared to mu
       (Mu/maxprob + 1.) / (4.0*Mu/maxprob + tot);

    struct basequal b = {call,phredchar_from_prob(post_prob)};
    return b;
}

/** Set value for Mu. */
bool set_mu(const char *mu_str) {

    char *endptr;
    Mu = strtor(mu_str, &endptr);

    return (Mu > 0);
}
