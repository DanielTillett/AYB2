/** 
 * \file cluster.c
 * Cluster Class.
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

#include "cluster.h"
#include "nuc.h"
#include "utility.h"


/* constants */
/* None      */

/* members */
/* below    */


/* private functions */
/* undetermined */

/* public functions */
/* undetermined */


/*
 * Standard functions for cluster structure
 */
CLUSTER new_CLUSTER ( void){
    CLUSTER cl = calloc(1,sizeof(*cl));
    return cl;
}

CLUSTER free_CLUSTER(CLUSTER cluster){
    if(NULL==cluster){return NULL;}
    free_MAT(cluster->signals);
    xfree(cluster);
    return NULL;
}

CLUSTER copy_CLUSTER(const CLUSTER cluster){
    if(NULL==cluster){ return NULL;}
    CLUSTER cl = new_CLUSTER();
    if(NULL==cl){ return NULL;}
    cl->signals = copy_MAT(cluster->signals);
    if(NULL==cl->signals && NULL!=cluster->signals){ goto cleanup; }
    cl->x = cluster->x;
    cl->y = cluster->y;

    return cl;

cleanup:
    free_MAT(cl->signals);
    xfree(cl);
    return NULL;
}

void show_CLUSTER(XFILE * fp, const CLUSTER cluster){
    if(NULL==fp){ return;}
    if(NULL==cluster){ return;}
    xfprintf(fp,"Cluster coordinates: (%u,%u)\n",cluster->x,cluster->y);
    show_MAT(fp,cluster->signals,4,5);
}

/**
 * Create a new cluster from the supplied array of values.
 * It is the responsibility of the caller to ensure the supplied array is large enough
 * as no size checking can be done. Ignores x and y.
 * Returns a pointer to the following element in the array
 * (which may be beyond its range).
 */
CLUSTER coerce_CLUSTER_from_array(const unsigned int ncycle, real_t * x, real_t ** next){
    if(NULL==x){ return NULL;}
    CLUSTER cl = new_CLUSTER();
    if(NULL==cl){ return NULL;}
    cl->signals = coerce_MAT_from_array(NBASE, ncycle, x);
    if(NULL==cl->signals){ goto cleanup;}
    /* return the next element in the array */
    *next = x + NBASE * ncycle;
    return cl;

cleanup:
    free_MAT(cl->signals);
    xfree(cl);
    return NULL;
}

/** 
 * Append clustin onto clustout, selecting data columns. 
 * clustout may be null, in which case it is created using details from clustin.
 * See matrix() append_columns for error handling.
 */
CLUSTER copy_append_CLUSTER(CLUSTER clustout, const CLUSTER clustin, int colstart, int colend){

    /* validate parameters */
    if(NULL==clustin) {return clustout;}
    
    if (NULL==clustout){
        clustout = new_CLUSTER();
        if(NULL==clustout){ return NULL;}
        clustout->x = clustin->x;
        clustout->y = clustin->y;
        clustout->signals = NULL;
    }
    /* append the selected matrix columns */
    clustout->signals = append_columns(clustout->signals, clustin->signals, colstart, colend);
    
    /* hmhm any error checking required?? */
    return clustout;
}

/*
 * Basic input functions from file
 */
/*  Reads a cluster from file pointer, assuming that the number of cycles is known.
 * Format should be that of Illumina's _int.txt
 * A little validation of the file format is performed.
 */
CLUSTER read_known_CLUSTER( XFILE * fp, unsigned int *ncycle, bool moredata){
    char * line = NULL;
    MAT signals = NULL;
    CLUSTER cluster = NULL;

    if(NULL==fp){return NULL;}
    // Get line from file
    size_t len = 0;
    line = xfgetln(fp,&len);
    char * ptr = line;

    /* Read lane, tile and coordinate information */
    unsigned long int lane,tile,x,y;
    lane = strtoul(ptr,&ptr,0);
    if('\t'!=ptr[0]){ goto cleanup;}
    tile = strtoul(ptr,&ptr,0);
    if('\t'!=ptr[0]){ goto cleanup;}
    x = strtoul(ptr,&ptr,0);
    if('\t'!=ptr[0]){ goto cleanup;}
    y = strtoul(ptr,&ptr,0);

    /* read cycle data */
    int nc = *ncycle;
    signals = new_MAT_from_line(NBASE, &nc, ptr, moredata);
    if((NULL==signals) || (nc == 0)) {goto cleanup;}

    /* store all the data in the cluster */
    cluster = new_CLUSTER();
    if(NULL==cluster){goto cleanup;}
    cluster->x = x;
    cluster->y = y;
    cluster->signals = signals;

    xfree(line);
    *ncycle = nc;
    return cluster;

cleanup:
    free_CLUSTER(cluster);
    free_MAT(signals);
    xfree(line);
    return NULL;
}

// Stub function
CLUSTER read_unknown_CLUSTER( XFILE * fp){
    return NULL;
}

#ifdef TEST
//#include <stdio.h>
int main(int argc, char * argv[]){
    if(argc!=3){
        xfputs("Usage: test ncycle filename\n", xstdout);
        return EXIT_FAILURE;
    }

    unsigned int ncycle = 0;
    sscanf(argv[1], "%u", &ncycle);

    /* test read from file */
    XFILE * fp = xfopen(argv[2], XFILE_UNKNOWN, "r");
    /* don't want return ncycle value */
    unsigned int nc = ncycle;
    CLUSTER cl = read_known_CLUSTER(fp, &nc, true);
    show_CLUSTER(xstdout, cl);
    
    xfputs("Copy cluster\n", xstdout);
    CLUSTER cl2 = copy_CLUSTER(cl);
    show_CLUSTER(xstdout, cl2);

    xfputs("Copy append cluster, from second column to half way\n", xstdout);
    cl = copy_append_CLUSTER(cl, cl2, 1, cl2->signals->ncol/2);
    show_CLUSTER(xstdout, cl);

    xfputs("Copy append cluster to null, from second column to half way\n", xstdout);
    CLUSTER cl3 = NULL;
    cl3 = copy_append_CLUSTER(cl3, cl2, 1, cl2->signals->ncol/2);
    show_CLUSTER(xstdout, cl3);

    xfputs("Create an array\n", xstdout);
    real_t arry[NBASE * ncycle];
    for (unsigned int idx = 0; idx < NBASE * ncycle; idx++) {
        arry[idx] = cl->signals->x[idx];
    }
    xfputs("array values:", xstdout);
    for (unsigned int idx = 0; idx < NBASE * ncycle; idx++) {
        xfprintf(xstdout, " %#8.2f", arry[idx]);
    }
    xfputs("\n", xstdout);

    xfputs("Coerce cluster from array, ignores x,y\n", xstdout);
    real_t *x = arry;
    CLUSTER cl4 = coerce_CLUSTER_from_array(ncycle, x, &x);
    show_CLUSTER(xstdout, cl4);

    free_CLUSTER(cl);
    free_CLUSTER(cl2);
    free_CLUSTER(cl3);
    /* do not free cl4 as it points to arry */
    xfclose(fp);
    return EXIT_SUCCESS;
}
#endif

