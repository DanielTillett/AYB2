/** 
 * \file ayb_model.c
 * Top Level Modelling.
 * Used as a singleton class with local functions accessing global member data.
 *//* 
 *  Created : 14 Apr 2010
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

#include <math.h>
#include "ayb_model.h"
#include "call_bases.h"
#include "datablock.h"
#include "dirio.h"
#include "intensities.h"
#include "lambda.h"
#include "matrix.h"
#include "message.h"
#include "mpn.h"
#include "nuc.h"
#include "statistics.h"
#include "tile.h"


/** AYB structure contains the data required for modelling. */
struct AybT {
    uint32_t ncluster;
    uint32_t ncycle;
    TILE tile;
    ARRAY(NUC) bases;
    ARRAY(PHREDCHAR) quals;
    MAT M, P, N;
    MAT lambda;
    MAT we, cycle_var;
};

/* constants */
static const unsigned int AYB_NITER = 20;       ///< Number of parameter estimation loops.

/** Initial Crosstalk matrix if not read in, fixed values of approximately the right shape. */
static real_t INITIAL_CROSSTALK[] = {
    2.0114300, 1.7217841, 0.06436576, 0.1126401,
    0.6919319, 1.8022413, 0.06436576, 0.0804572,
    0.2735545, 0.2252802, 1.39995531, 0.9976693,
    0.2896459, 0.2413716, 0.11264008, 1.3194981
};

/** Possible output format text, used to match program argument. */
static const char *OUTFORM_TEXT[] = {"FASTA", "FASTQ"};
/** Name text for matrix messages. */
static const char *MATRIX_TEXT[] = {"Crosstalk", "Noise", "Phasing"};

/* members */

/** Possible output formats. */
typedef enum OutFormT {E_FASTA, E_FASTQ, E_OUTFORM_NUM} OUTFORM;

static OUTFORM OutputFormat = E_FASTA;          ///< Selected output format.
static unsigned int NIter = 5;                  ///< Number of iterations in base call loop.
static MAT Matrix[E_NMATRIX];                   ///< Predetermined matrices.
static AYB Ayb = NULL;                          ///< The model data.


/* private functions */

/**
 * Read in any external crosstalk (M), Phasing (P) and Noise (N) matrices.
 * Returns false if failed to read a supplied matrix file.
 */
static bool read_matrices() {
    XFILE *fpmat = NULL;
    bool found = true;

    for (IOTYPE idx = 0; idx < E_NMATRIX; idx++) {
        if (matrix_from_file(idx)) {
            fpmat = open_matrix(idx);
            if (fpmat == NULL) {
                found = false;
            }
            else {
                /* read in matrix */
                Matrix[idx] = read_MAT_from_column_file(fpmat);
                if (Matrix[idx] == NULL) {
                    found = false;
                }
            }
            fpmat = xfclose(fpmat);
        }
        else {
            /* no input file specified, initialise using internal method later */
            Matrix[idx] = NULL;
        }

        /* exit if this iteration failed */
        if (!found) {
            break;
        }
    }
    if (found) {
        /* Crosstalk is always the same size so create from default array */
        if (Matrix[E_CROSSTALK] == NULL) {
            Matrix[E_CROSSTALK] = new_MAT_from_array(NBASE, NBASE, INITIAL_CROSSTALK);
        }
    }

    return found;
}

/**
 * Read and store an intensities input file.
 * Return flag indicating not to continue if not sufficient cycles in file.
 */
static TILE read_intensities(XFILE *fp, unsigned int ncycle, bool *goon) {

    TILE tile = read_TILE(fp, ncycle);
    if (tile != NULL) {
        if (tile->ncycle < ncycle) {
            /* not enough data */
            message(E_CYCLESIZE_DD, MSG_FATAL, tile->ncycle, ncycle);
            tile = free_TILE(tile);
            *goon = false;
        }
    }

    return tile;
}

/** Create the sub-tile datablocks to be analysed. */
static TILE * create_datablocks(TILE maintile, const unsigned int numblock) {

    /* create the sub-tiles with an array of pointers */
    TILE * tileblock = calloc(numblock, sizeof(*tileblock));
    if(tileblock == NULL) {return NULL;}

    int blk = 0;
    int colstart = 0;
    int colend = 0;

    /* block specification already decoded */
    DATABLOCK datablock = get_next_block();
    while (datablock != NULL) {
        colend = colstart + datablock->num - 1;
        switch (datablock->type) {
        case E_READ :
            /* new block if not first */
            if (tileblock[blk] != NULL) {
                blk++;
            }

        /* no break; fall through to case CONCAT */
        case E_CONCAT :
            tileblock[blk] = copy_append_TILE(tileblock[blk], maintile, colstart, colend);
            break;

        case E_IGNORE :
            /* just increment column pointers, done outside of switch */
            break;

        default :;
        }

        colstart = colend + 1;
        datablock = get_next_block();
    }

    return tileblock;
}

/**
 * Initialise crosstalk (M), Phasing (P) and Noise (N) matrices.
 * May use read in values or initialise using an internal method.
 */
static MAT init_matrix(MAT mat, const IOTYPE idx) {

    if (mat == NULL) {return NULL;}

    if (Matrix[idx] == NULL) {
        /* initialise internally */
        unsigned int nrow = mat->nrow;
        switch (idx) {
            case E_CROSSTALK:
                /* shouldn't reach this as crosstalk always set up in read_matrices */
                /* initial crosstalk from default array */
                mat = free_MAT(mat);
                mat = new_MAT_from_array(NBASE, NBASE, INITIAL_CROSSTALK);
                break;

            case E_NOISE:
                /* initial noise is zero */
                set_MAT(mat, 0.0);
                break;

            case E_PHASING:
                /* initial phasing */
                #warning "Phasing not properly initialised yet"
                mat = free_MAT(mat);
                mat = identity_MAT(nrow);
                break;

            default: ;
        }
    }

    else {
        /* use read in matrix */
        mat = copyinto_MAT(mat, Matrix[idx]);
        if (mat == NULL) {
            message (E_MATRIXINIT_SDD, MSG_ERR, MATRIX_TEXT[idx], mat->ncol, Matrix[idx]->ncol);
        }
    }
    return mat;
}

/**
 * Set initial values for the model.
 * Returns false if one of the initial matrices is wrong dimension.
 */
static bool initialise_model() {

    /* initial M, P, N */
    Ayb->M = init_matrix(Ayb->M, E_CROSSTALK);
    if (Ayb->M == NULL) {
        message (E_MATRIX_FAIL_S, MSG_ERR, MATRIX_TEXT[E_CROSSTALK]);
        return false;
    }
    Ayb->N = init_matrix(Ayb->N, E_NOISE);
    if (Ayb->N == NULL) {
        message (E_MATRIX_FAIL_S, MSG_ERR, MATRIX_TEXT[E_NOISE]);
        return false;
    }
    Ayb->P = init_matrix(Ayb->P, E_PHASING);
    if (Ayb->P == NULL) {
        message (E_MATRIX_FAIL_S, MSG_ERR, MATRIX_TEXT[E_PHASING]);
        return false;
    }

    /* Initial weights and cycles are all equal. Arbitrarily one */
    set_MAT(Ayb->we, 1.0);
    set_MAT(Ayb->cycle_var, 1.0);

#ifndef NDEBUG
    XFILE *fpout = NULL;
    fpout = open_output("inv");
#endif

    /* process intensities then call initial bases and lambda for each cluster */
    MAT pcl_int = NULL;                     // Shell for processed intensities
    MAT Minv_t = transpose_inplace(invert(Ayb->M));
    MAT Pinv_t = transpose_inplace(invert(Ayb->P));

#ifndef NDEBUG
    if (!xfisnull(fpout)) {
        show_MAT(fpout, Minv_t, Minv_t->nrow, Minv_t->ncol);
        show_MAT(fpout, Pinv_t, Pinv_t->nrow, Pinv_t->ncol);
    }
    fpout = xfclose(fpout);
    fpout = open_output("pi");
#endif

    unsigned int cl = 0;
    LIST(CLUSTER) node = Ayb->tile->clusterlist;
    while (NULL != node && cl < Ayb->ncluster){
        pcl_int = process_intensities(node->elt->signals, Minv_t, Pinv_t, Ayb->N, pcl_int);

#ifndef NDEBUG
        if (!xfisnull(fpout)) {
            show_MAT(fpout, pcl_int, pcl_int->nrow, pcl_int->ncol);
        }
#endif

        /* call initial bases for each cycle */
        NUC * cl_bases = Ayb->bases.elt + cl * Ayb->ncycle;
        PHREDCHAR * cl_quals = Ayb->quals.elt + cl * Ayb->ncycle;
        for ( uint32_t cy = 0; cy < Ayb->ncycle; cy++){
            cl_bases[cy] = call_base_simple(pcl_int->x + cy * NBASE);
            cl_quals[cy] = MIN_PHRED;
        }
        /* initial lambda */
        Ayb->lambda->x[cl] = estimate_lambdaOLS(pcl_int, cl_bases);

        /* next cluster */
        node = node->nxt;
        cl++;
    }

#ifndef NDEBUG
    fpout = xfclose(fpout);
#endif

    free_MAT(pcl_int);
    free_MAT(Pinv_t);
    free_MAT(Minv_t);
    return true;
}

/** Calculate new weights. */
static real_t update_cluster_weights(){
    validate(NULL!=Ayb,NAN);
    const uint32_t ncluster = Ayb->ncluster;
    const uint32_t ncycle   = Ayb->ncycle;
    real_t sumLSS = 0.;

    MAT e = NULL;
    /*  Calculate least squares error, using Ayb->we as temporary storage */
    unsigned int cl = 0;
    LIST(CLUSTER) node = Ayb->tile->clusterlist;
    while (NULL != node && cl < ncluster){
        Ayb->we->x[cl] = 0.;
        MAT cycle_ints = node->elt->signals;
        NUC * cycle_bases = Ayb->bases.elt + cl*ncycle;
        e = expected_intensities(Ayb->lambda->x[cl],cycle_bases,Ayb->M,Ayb->P,Ayb->N,e);
        for( uint32_t idx=0 ; idx<NBASE*ncycle ; idx++){
            real_t tmp = cycle_ints->x[idx] - e->x[idx];
            Ayb->we->x[cl] += tmp*tmp;
        }
        sumLSS += Ayb->we->x[cl];

        /* next cluster */
        node = node->nxt;
        cl++;
    }
    free_MAT(e);

    /* Calculate weight for each cluster */
    real_t meanLSSi = mean(Ayb->we->x,ncluster);
    real_t varLSSi = variance(Ayb->we->x,ncluster);
    for ( uint32_t cl=0 ; cl<ncluster ; cl++){
        const real_t d = Ayb->we->x[cl]-meanLSSi;
        Ayb->we->x[cl] = cauchy(d*d,varLSSi);
    }
    //xfputs("Cluster weights:\n",xstderr);
    //show_MAT(xstderr,Ayb->we,8,1);
    return sumLSS;
}

/**
 * Code for parameter estimation loop.
 * Equivalent to parameter_estimation_loop in old AYB.
 * - Updates: M, P, N
 * - Recalculates weights
 * - Scales all lambda by factor
 */
static real_t estimate_MPN(){
    validate(NULL!=Ayb,NAN);
    const uint32_t ncycle = Ayb->ncycle;
    /*  Calculate new weights */
    //timestamp("Updating weights\n",stderr);
    real_t sumLSS = update_cluster_weights(Ayb);

    /*  Precalculate terms for iteration */
    //timestamp("Calculating matrices\n",stderr);
    //timestamp("J\t",stderr);
    MAT J = calculateJ(Ayb->lambda,Ayb->we,Ayb->bases,ncycle,NULL);
    MAT Jt = transpose(J);
    //timestamp("K\t",stderr);
    MAT K = calculateK(Ayb->lambda,Ayb->we,Ayb->bases,Ayb->tile,ncycle,NULL);
    //timestamp("Others\n",stderr);
    MAT Kt = transpose(K);
    MAT Sbar = calculateSbar(Ayb->lambda,Ayb->we,Ayb->bases,ncycle,NULL);
    MAT SbarT = transpose(Sbar);
    MAT Ibar = calculateIbar(Ayb->tile,Ayb->we,NULL);
    MAT IbarT = transpose(Ibar);
    real_t Wbar = calculateWbar(Ayb->we);
    real_t lambdaf = 1.;
    real_t * tmp = calloc(ncycle*ncycle*NBASE*NBASE,sizeof(real_t));
    /* Convenience terms for M, P and N */
    MAT matMt = transpose_inplace(Ayb->M);
    MAT matP = Ayb->P;
    MAT matN = Ayb->N;

    //xfputs("Staring main loop",xstderr);
    /*  Main iteration */
    MAT mlhs=NULL,mrhs=NULL,plhs=NULL,prhs=NULL;
    const uint32_t lda = NBASE + ncycle;
    real_t det=0.;
    //xfprintf(xstderr,"Starting\tdelta = %e\n",calculateDeltaLSE( matMt, matP, matN, J, K, tmp));
    for( uint32_t i=0 ; i<AYB_NITER ; i++){
        /*
         *  Solution for phasing and constant noise
         */
        plhs = calculatePlhs(Wbar,Sbar,matMt,J,tmp,plhs);
        prhs = calculatePrhs(Ibar,matMt,K,tmp,prhs);
        solverSVD(plhs,prhs,tmp);
        for ( uint32_t i=0 ; i<ncycle ; i++){
            for(uint32_t j=0 ; j<ncycle ; j++){
                matP->x[i*ncycle+j] = prhs->x[i*lda+j];
            }
        }
        for ( uint32_t i=0 ; i<ncycle ; i++){
            for(uint32_t j=0 ; j<NBASE ; j++){
                matN->x[i*NBASE+j] = prhs->x[i*lda+ncycle+j];
            }
        }
        // Scaling so det(P) = 1
        det = normalise_MAT(matP,3e-8);
        scale_MAT(J,det*det); scale_MAT(Jt,det*det);
        scale_MAT(K,det);     scale_MAT(Kt,det);
        scale_MAT(Sbar,det);  scale_MAT(SbarT,det);
        lambdaf *= det;

        /*
         *  Solution for cross-talk and constant noise
         */
        mlhs = calculateMlhs(Ayb->cycle_var,Wbar,SbarT,matP,Jt,tmp,mlhs);
        mrhs = calculateMrhs(Ayb->cycle_var,IbarT,matP,Kt,tmp,mrhs);
        solverSVD(mlhs,mrhs,tmp);

        for(uint32_t i=0 ; i<NBASE ; i++){
            for(uint32_t j=0 ; j<NBASE ; j++){
                matMt->x[i*NBASE+j] = mrhs->x[i*lda+j];
            }
        }
        for(uint32_t i=0 ; i<NBASE ; i++){
            for(uint32_t j=0 ; j<ncycle ; j++){
                matN->x[j*NBASE+i] = mrhs->x[i*lda+j+NBASE];
            }
        }
        // Scaling so det(M) = 1
        det = normalise_MAT(matMt,3e-8);
        scale_MAT(J,det*det); scale_MAT(Jt,det*det);
        scale_MAT(K,det);     scale_MAT(Kt,det);
        scale_MAT(Sbar,det);  scale_MAT(SbarT,det);
        lambdaf *= det;
        //xfprintf(xstderr,"... done %u\tdelta = %e\n",i,calculateDeltaLSE( matMt, matP, matN, J, K, tmp));
    }
    real_t delta = calculateDeltaLSE( matMt, matP, matN, J, K, tmp);

    // Transpose Mt back to normal form
    matMt = transpose_inplace(matMt);
    // Scale lambdas by factor
    scale_MAT(Ayb->lambda,lambdaf);

    // Clean-up memory
    free_MAT(prhs);
    free_MAT(plhs);
    free_MAT(mrhs);
    free_MAT(mlhs);
    xfree(tmp);
    free_MAT(IbarT);
    free_MAT(Ibar);
    free_MAT(SbarT);
    free_MAT(Sbar);
    free_MAT(Kt);
    free_MAT(K);
    free_MAT(Jt);
    free_MAT(J);

    //xfprintf(xstderr,"Initial %e\tImprovement %e\t = %e\n",sumLSS,delta,sumLSS-delta);
    //xfprintf(xstderr,"Updated weights %e\n", update_cluster_weights(ayb));
    return sumLSS-delta;
}

/*
 * Routines to calculate covariance of errors, using the "fast approach".
 * Working with processed intensities.
 */

/**
 * Accumulate required variances (inner summation of variance calculation).
 * \n Note: If V is NULL, the required memory is allocated.
 * - p:        Matrix of processed intensities
 * - lambda:   Brightness of cluster
 * - base:     Current base call
 * - V:        Array of covariance matrices into which accumulation occurs
 */
static MAT * accumulate_covariance( const real_t we, const MAT p, const real_t lambda, const NUC * base, MAT * V){
    validate(NULL!=p,NULL);
    validate(NBASE==p->nrow,NULL);
    validate(lambda>=0.,NULL);
    const uint32_t ncycle = p->ncol;
    // Allocate memory for V if necessary
    if ( NULL==V ){
        V = calloc(ncycle+1,sizeof(*V));
        if(NULL==V){ return NULL;}
        for ( uint32_t cycle=0 ; cycle<ncycle ; cycle++){
            V[cycle] = new_MAT(NBASE,NBASE);
            if(NULL==V[cycle]){ goto cleanup;}
        }
    }
    // Perform accululation. V += R R^t
    // Note: R = P - \lambda I_b, where I_b is unit vector with b'th elt = 1
    //  so R R^t = P P^t - \lambda I_b P^t
    //                   - \lambda P I_b^t + \lambda^2 I_b I_b^2
    for ( uint32_t cycle=0 ; cycle<ncycle ; cycle++){
        const int cybase = base[cycle];
        validate(cybase<NBASE,NULL);
        // P P^t
        for ( uint32_t i=0 ; i<NBASE ; i++){
            for ( uint32_t j=0 ; j<NBASE ; j++){
                V[cycle]->x[i*NBASE+j] += we * p->x[cycle*NBASE+i] * p->x[cycle*NBASE+j];
            }
        }
        // \lambda I_b P^t and \lambda P I_b^t
        for ( uint32_t i=0 ; i<NBASE ; i++){
            V[cycle]->x[cybase*NBASE+i] -= we * lambda * p->x[cycle*NBASE+i];
            V[cycle]->x[i*NBASE+cybase] -= we * lambda * p->x[cycle*NBASE+i];
        }
        // \lambda^2 I_b I_b^t
        V[cycle]->x[cybase*NBASE+cybase] += we * lambda*lambda;
    }
    // Create residuals
    for ( uint32_t cy=0 ; cy<ncycle ; cy++){
        p->x[cy*NBASE+base[cy]] -= lambda;
    }

    return V;

cleanup:
    for ( uint32_t cycle=0 ; cycle<ncycle ; cycle++){
        free_MAT(V[cycle]);
    }
    xfree(V);
    return NULL;
}

/**
 * Calculate covariance of (processed) residuals.
 * Returns a pointer to array of matrices, one per cycle.
 */
static MAT * calculate_covariance(){

    const uint32_t ncluster = Ayb->ncluster;
    const uint32_t ncycle = Ayb->ncycle;

    MAT * V = NULL;                     // memory allocated in accumulate
    MAT pcl_int = NULL;                 // Shell for processed intensities
    /* Create t(inverse(M)) and t(inverse(P)) */
    MAT Minv_t = transpose_inplace(invert(Ayb->M));
    MAT Pinv_t = transpose_inplace(invert(Ayb->P));

    real_t wesum = 0.;

#ifndef NDEBUG
    XFILE *fpout = NULL;
    fpout = open_output("cov_add");
#endif

    unsigned int cl = 0;
    LIST(CLUSTER) node = Ayb->tile->clusterlist;
    while (NULL != node && cl < ncluster){
        const NUC * cl_bases = Ayb->bases.elt + cl * ncycle;
        pcl_int = process_intensities(node->elt->signals, Minv_t, Pinv_t, Ayb->N, pcl_int);
        validate(NULL != pcl_int, NULL);

        /* add this cluster values */
        V = accumulate_covariance(Ayb->we->x[cl], pcl_int, Ayb->lambda->x[cl], cl_bases, V);
        validate(NULL != V, NULL);

        /* sum denominator */
        wesum += Ayb->we->x[cl];

        /* next cluster */
        node = node->nxt;
        cl++;

#ifndef NDEBUG
    if (!xfisnull(fpout)) {
        show_MAT(fpout, V[0], NBASE, NBASE);
    }
#endif

    }

    free_MAT(pcl_int);
    free_MAT(Pinv_t);
    free_MAT(Minv_t);

    /* scale sum of squares to make covariance */
    for ( uint32_t cy = 0; cy < ncycle; cy++){
        for ( uint32_t i = 0; i < NBASE * NBASE; i++){
            V[cy]->x[i] /= wesum;
        }
    }

#ifndef NDEBUG
    fpout = xfclose(fpout);
#endif

    return V;
}

/** Call bases. Includes calculate covariance and estimate lambda. */
static void estimate_bases() {

    validate(NULL != Ayb,);
    const uint32_t ncluster = Ayb->ncluster;
    const uint32_t ncycle = Ayb->ncycle;

#ifndef NDEBUG
    XFILE *fpout = NULL;
    fpout = open_output("ayb2");
    if (!xfisnull(fpout)) {
        show_AYB(fpout, Ayb);
    }
    fpout = xfclose(fpout);
#endif

    /* calculate covariance */
    MAT * V = calculate_covariance();

#ifndef NDEBUG
    fpout = open_output("cov");
    if (!xfisnull(fpout)) {
        xfputs("covariance:\n", fpout);
        for (uint32_t cy = 0; cy < ncycle; cy++){
            show_MAT(fpout, V[cy], NBASE, NBASE);
        }
    }
    fpout = xfclose(fpout);
#endif

    /* scale is variance of residuals; get from V matrices */
    for (uint32_t cy = 0; cy < ncycle; cy++){
        Ayb->cycle_var->x[cy] = 0.;
        for (uint32_t b = 0; b < NBASE; b++){
            Ayb->cycle_var->x[cy] += V[cy]->x[b * NBASE + b];
        }
        /* hmhm I have no idea why a reciprocal is taken here */
//        Ayb->cycle_var->x[cy] = 1.0/Ayb->cycle_var->x[cy];
    }

    /* invert variance matrices to get omega */
    for (uint32_t cy = 0; cy < ncycle; cy++){
        MAT a = V[cy];
        V[cy] = invert(a);
        free_MAT(a);
    }

#ifndef NDEBUG
    fpout = open_output("om");
    if (!xfisnull(fpout)) {
        xfputs("omega:\n", fpout);
        for (uint32_t cy = 0; cy < ncycle; cy++){
            show_MAT(fpout, V[cy], NBASE, NBASE);
        }
    }
    fpout = xfclose(fpout);
#endif

    /* process intensities then estimate lambda and call bases for each cluster */
    MAT pcl_int = NULL;                 // Shell for processed intensities
    MAT Minv_t = transpose_inplace(invert(Ayb->M));
    MAT Pinv_t = transpose_inplace(invert(Ayb->P));

#ifndef NDEBUG
    fpout = open_output("lam2");
    if (!xfisnull(fpout)) {
        xfputs("lambda:\n", fpout);
    }
#endif

    unsigned int cl = 0;
    LIST(CLUSTER) node = Ayb->tile->clusterlist;
    while (NULL != node && cl < ncluster){
        NUC * cl_bases = Ayb->bases.elt + cl * ncycle;
        PHREDCHAR * cl_quals = Ayb->quals.elt + cl * ncycle;
        pcl_int = process_intensities(node->elt->signals, Minv_t, Pinv_t, Ayb->N, pcl_int);

        /* estimate lambda using Weighted Least Squares */
//        Ayb->lambda->x[cl] = estimate_lambdaGWLS(pcl_int, cl_bases, Ayb->lambda->x[cl], Ayb->cycle_var->x, V);
        Ayb->lambda->x[cl] = estimate_lambdaWLS(pcl_int, cl_bases, Ayb->lambda->x[cl], Ayb->cycle_var->x);

#ifndef NDEBUG
    if (!xfisnull(fpout)) {
        xfprintf(fpout, "%d: %#12.6f\n", cl + 1, Ayb->lambda->x[cl]);
    }
#endif

        /* call bases for each cycle */
        for (uint32_t cy = 0; cy < ncycle; cy++){
            struct basequal bq = call_base(pcl_int->x+cy * NBASE, Ayb->lambda->x[cl], V[cy]);
            cl_bases[cy] = bq.base;
            cl_quals[cy] = bq.qual;
        }

        /* repeat estimate lambda with the new bases */
        Ayb->lambda->x[cl] = estimate_lambdaWLS(pcl_int, cl_bases, Ayb->lambda->x[cl], Ayb->cycle_var->x);

        /* next cluster */
        node = node->nxt;
        cl++;
    }

#ifndef NDEBUG
    fpout = xfclose(fpout);
    fpout = open_output("ayb3");
    if (!xfisnull(fpout)) {
        show_AYB(fpout, Ayb);
    }
    fpout = xfclose(fpout);
#endif

    free_MAT(pcl_int);
    free_MAT(Pinv_t);
    free_MAT(Minv_t);
    for(uint32_t cy = 0; cy < ncycle; cy++){
        free_MAT(V[cy]);
    }
    xfree(V);
}

/**
 * Output the results of the base calling.
 * Returns true if output file opened ok.
 */
static bool output_results (int blk) {

    if (Ayb == NULL) {return false;}

    XFILE *fpout = NULL;
    fpout = open_output_blk("seq", blk);

    if (xfisnull(fpout)) {return false;}

    const uint32_t ncluster = Ayb->ncluster;
    const uint32_t ncycle = Ayb->ncycle;

    int symbol = (OutputFormat == E_FASTQ)?'@':'>';

    for (uint32_t cl = 0; cl < ncluster; cl++){
        xfprintf(fpout, "%ccluster_%03u\n", symbol, cl + 1);
        for (uint32_t cy = 0; cy < ncycle; cy++){
            show_NUC(fpout, Ayb->bases.elt[cl * ncycle + cy]);
        }
        /* quality score */
        if (OutputFormat == E_FASTQ) {
            xfputs("\n+\n", fpout);
            for (uint32_t cy = 0; cy < ncycle; cy++){
                show_PHREDCHAR(fpout, Ayb->quals.elt[cl * ncycle + cy]);
            }
        }
//        xfputs("\n", fpout);
        xfputc('\n', fpout);
    }
    fpout = xfclose(fpout);
    return true;
}


/* public functions */

/* standard functions */

AYB new_AYB(const uint32_t ncycle, const uint32_t ncluster){
    AYB ayb = malloc(sizeof(*ayb));
    if(NULL==ayb){ return NULL;}
    ayb->ncycle = ncycle;
    ayb->ncluster = ncluster;
    ayb->tile = new_TILE();
    ayb->bases = new_ARRAY(NUC)(ncluster*ncycle);
    ayb->quals = new_ARRAY(PHREDCHAR)(ncluster*ncycle);
    ayb->M = new_MAT(NBASE,NBASE);
    ayb->P = new_MAT(ncycle,ncycle);
    ayb->N = new_MAT(NBASE,ncycle);
    ayb->lambda = new_MAT(ncluster,1);
    ayb->we = new_MAT(ncluster,1);
    ayb->cycle_var = new_MAT(ncycle,1);
    if( NULL==ayb->tile || NULL==ayb->bases.elt || NULL==ayb->quals.elt
            || NULL==ayb->M || NULL==ayb->P || NULL==ayb->N
            || NULL==ayb->lambda || NULL==ayb->we || NULL==ayb->cycle_var){
        goto cleanup;
    }

    return ayb;

cleanup:
    free_AYB(ayb);
    return NULL;
}

AYB free_AYB(AYB ayb){
    if(NULL==ayb){ return NULL;}
    free_TILE(ayb->tile);
    free_ARRAY(NUC)(ayb->bases);
    free_ARRAY(PHREDCHAR)(ayb->quals);
    free_MAT(ayb->M);
    free_MAT(ayb->P);
    free_MAT(ayb->N);
    free_MAT(ayb->we);
    free_MAT(ayb->cycle_var);
    free_MAT(ayb->lambda);
    xfree(ayb);
    return NULL;
}

AYB copy_AYB(const AYB ayb){
    if(NULL==ayb){return NULL;}
    AYB ayb_copy = malloc(sizeof(*ayb));
    if(NULL==ayb_copy){ return NULL;}

    ayb_copy->ncycle = ayb->ncycle;
    ayb_copy->ncluster = ayb->ncluster;

    ayb_copy->tile = copy_TILE(ayb->tile);
    if(NULL!=ayb->tile && NULL==ayb_copy->tile){ goto cleanup;}

    ayb_copy->bases = copy_ARRAY(NUC)(ayb->bases);
    if(NULL!=ayb->bases.elt && NULL==ayb_copy->bases.elt){ goto cleanup;}

    ayb_copy->quals = copy_ARRAY(PHREDCHAR)(ayb->quals);
    if(NULL!=ayb->quals.elt && NULL==ayb_copy->quals.elt){ goto cleanup;}

    ayb_copy->M = copy_MAT(ayb->M);
    if(NULL!=ayb->M && NULL==ayb_copy->M){ goto cleanup;}

    ayb_copy->P = copy_MAT(ayb->P);
    if(NULL!=ayb->P && NULL==ayb_copy->P){ goto cleanup;}

    ayb_copy->N = copy_MAT(ayb->N);
    if(NULL!=ayb->N && NULL==ayb_copy->N){ goto cleanup;}

    ayb_copy->we = copy_MAT(ayb->we);
    if(NULL!=ayb->we && NULL==ayb_copy->we){ goto cleanup;}

    ayb_copy->cycle_var = copy_MAT(ayb->cycle_var);
    if(NULL!=ayb->cycle_var && NULL==ayb_copy->cycle_var){ goto cleanup;}

    ayb_copy->lambda = copy_MAT(ayb->lambda);
    if(NULL!=ayb->lambda && NULL==ayb_copy->lambda){ goto cleanup;}

    return ayb_copy;

cleanup:
    free_AYB(ayb_copy);
    return NULL;
}

void show_AYB(XFILE * fp, const AYB ayb){
    validate(NULL!=fp,);
    validate(NULL!=ayb,);
    xfprintf(fp,"%u cycles from %u clusters\n",ayb->ncycle,ayb->ncluster);
    xfputs("M:\n",fp); show_MAT(fp,ayb->M,NBASE,NBASE);
    xfputs("P:\n",fp); show_MAT(fp,ayb->P,ayb->ncycle,ayb->ncycle);
    xfputs("N:\n",fp); show_MAT(fp,ayb->N,NBASE,8);
    xfputs("we:\n",fp); show_MAT(fp,ayb->we,8,1);
    xfputs("cycle_var:\n",fp); show_MAT(fp,ayb->cycle_var,8,1);
    xfputs("lambda:\n",fp); show_MAT(fp,ayb->lambda,8,1);
    xfputs("Bases:\n",fp); show_ARRAY(NUC)(fp,ayb->bases,"",ayb->ncycle);
    xfputc('\n',fp);
    xfputs("Quality:\n",fp); show_ARRAY(PHREDCHAR)(fp,ayb->quals,"",ayb->ncycle*10);
    xfputc('\n',fp);
#ifdef NDEBUG
    xfputs("Tile:\n",fp); show_TILE(fp,ayb->tile,10);
#else
    xfputs("Tile:\n",fp); show_TILE(fp,ayb->tile,ayb->ncluster);
#endif
    xfputc('\n',fp);
}

/**
 * Analyse a single input file. File is already opened.
 * Returns true if analysis should continue to next file.
 */
bool analyse_tile (XFILE *fp) {

    if (xfisnull(fp)) {return true;}
    bool goon = true;

    /* read intensity data from supplied file */
    TILE maintile = read_intensities(fp, get_totalcycle(), &goon);
    if (maintile == NULL) {
        if (goon) {
            message(E_BAD_INPUT_S, MSG_ERR, get_current_file());
        }
        return goon;
    }
    const unsigned int ncluster = maintile->ncluster;
    const unsigned int numblock = get_numblock();

    /* put the data into distinct blocks */
    TILE * tileblock = NULL;
    tileblock = create_datablocks(maintile, numblock);
    /* no longer need the raw data as read in */
    maintile = free_TILE(maintile);

    if (tileblock == NULL) {
        message(E_DATABLOCK_FAIL_S, MSG_FATAL, get_current_file());
        return false;
    }

    /* analyse each tile block separately */
    for (int blk = 0; blk < numblock; blk++) {

        Ayb = new_AYB(tileblock[blk]->ncycle, ncluster);
        if (Ayb == NULL) {
            message(E_NOMEM_S, MSG_FATAL, "model structure creation");
            message(E_INIT_FAIL_DD, MSG_ERR, blk + 1, tileblock[blk]->ncycle);
            goon = false;
            goto cleanup;
        }

        /* get next tile block of raw intensities */
        /* tile has already been allocated memory in new_AYB */
        free_TILE(Ayb->tile);
        Ayb->tile = copy_TILE(tileblock[blk]);

        /* set initial model values */
        if (initialise_model()) {
            message(E_PROCESS_DD, MSG_INFO, blk + 1, Ayb->ncycle);

#ifndef NDEBUG
            XFILE *fpout = NULL;
            fpout = open_output_blk("ayb1", (numblock > 1)?blk:-1);
            if (!xfisnull(fpout)) {
                show_AYB(fpout, Ayb);
            }
            fpout = xfclose(fpout);
#endif

            /* base calling loop */
            for (int i = 0; i < NIter; i++){
                estimate_MPN();
                estimate_bases();
            }

            /* output the results */
            goon = output_results((numblock > 1)?blk:-1);
        }
        else {
            message(E_INIT_FAIL_DD, MSG_ERR, blk + 1, Ayb->ncycle);
        }

        /* free the structure ready for next */
        Ayb = free_AYB(Ayb);
        if (!goon) {break;}
    }

cleanup:
    if (tileblock != NULL) {
        for (int blk = 0; blk < numblock; blk++)  {
            free_TILE(tileblock[blk]);
        }
        xfree(tileblock);
    }
    return goon;
}

/** Set the number of base call iterations. */
void set_niter(const char *niter_str) {

    char *endptr;
    NIter = strtoul(niter_str, &endptr, 0);
}

/**
 * Set the output format. Text must match one of the output format text list. Ignores case.
 * Returns true if match found.
 */
bool set_output_format(const char *outform_str) {

    /* match to one of the possible options */
    int matchidx = match_string(outform_str, OUTFORM_TEXT, E_OUTFORM_NUM);
    if (matchidx >= 0) {
        OutputFormat = matchidx;
        return true;
    }
    else {
        return false;
    }
}

/**
 * Start up; call at program start after options.
 * Returns true if cycle blocks and iterations parameters are ok
 * and M, N, P matrix initialisation is successful.
 */
bool startup_model(){

    /* check number of cycles and data blocks supplied */
    const unsigned int totalcycle = get_totalcycle();
    const unsigned int numblock = get_numblock();

    message(E_GENERIC_SD, MSG_DEBUG, "total cycles:", totalcycle);
    message(E_GENERIC_SD, MSG_DEBUG, "distinct blocks:", numblock);
    if ((totalcycle == 0) || (numblock == 0)) {
        message(E_NOBLOCKS, MSG_FATAL);
        return false;
    }
    else {
        message(E_OPT_SELECT_SD, MSG_INFO, "cycles total", totalcycle);
        message(E_OPT_SELECT_SD, MSG_INFO, "distinct data blocks", numblock);

        /* check number of iterations supplied - may be left at default */
        message(E_GENERIC_SD, MSG_DEBUG, "niter:", NIter);
        if (NIter == 0) {
            message(E_BADITER, MSG_FATAL);
            return false;
        }
        else {
            message(E_OPT_SELECT_SD, MSG_INFO, "iterations", NIter);
            message(E_OUTPUT_FORM_S, MSG_INFO, OUTFORM_TEXT[OutputFormat]);

            /* read any M, N, P */
            return read_matrices();
        }
    }
}

/** Tidy up; call at program shutdown. */
void tidyup_model(){

    /* free memory */
    for (IOTYPE idx = 0; idx < E_NMATRIX; idx++) {
        Matrix[idx] = free_MAT(Matrix[idx]);
    }
}
