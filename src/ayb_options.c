/**
 * \file ayb_options.c
 * AYB specific Options.
 *//*
 *  Created : 23 Feb 2010
 *  Author  : Hazel Marsden
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "ayb_model.h"
#include "ayb_options.h"
#include "ayb_version.h"
#include "call_bases.h"
#include "datablock.h"
#include "dirio.h"
#include "message.h"


/* constants */
/* none */

/* private functions that output bulk text */

/** Print help information. Includes text from ayb_help.h. */
void print_help(FILE *fp) {
    validate(NULL!=fp,);
    fputs(""
#include "ayb_help.h"
          , fp);
}

/** Print licence information. Includes text from copyright.h. */
void print_licence(FILE *fp) {
    validate(NULL!=fp,);
    fputs("\n"
PROGNAME " Advanced Base Calling for Next-Generation Sequencing Machines\n"
#include "copyright.h"
          , fp);
}

/** Print usage information. Includes text from ayb_usage.h. */
void print_usage(FILE *fp) {
    validate(NULL!=fp,);
    fputs(""
#include "ayb_usage.h"
          , fp);
}


/* members */
/* none */

/** Options with no short form. */
enum {OPT_HELP, OPT_LICENCE, OPT_VERSION};

/** Long option structure used by getopt_long. */
static struct option Longopts[] = {
    {"blockstring", required_argument,  NULL, 'b'},
    {"composition", required_argument,  NULL, 'c'},
    {"dataformat",  required_argument,  NULL, 'd'},
    {"logfile",     required_argument,  NULL, 'e'},
    {"format",      required_argument,  NULL, 'f'},
    {"input",       required_argument,  NULL, 'i'},
    {"loglevel",    required_argument,  NULL, 'l'},
    {"mu"   ,       required_argument,  NULL, 'm'},
    {"niter",       required_argument,  NULL, 'n'},
    {"output",      required_argument,  NULL, 'o'},
    {"simdata",     required_argument,  NULL, 's'},   // Note!! index identified as E_SIMDATA = 10 in header file
    {"working",     no_argument,        NULL, 'w'},
    {"M",           required_argument,  NULL, 'M'},
    {"N",           required_argument,  NULL, 'N'},
    {"P",           required_argument,  NULL, 'P'},
    {"qualtab",     required_argument,  NULL, 'Q'},
    {"solver",      required_argument,  NULL, 'S'},
    {"help",        no_argument,        NULL, OPT_HELP },
    {"licence",     no_argument,        NULL, OPT_LICENCE },
    {"license",     no_argument,        NULL, OPT_LICENCE },
    {"version",     no_argument,        NULL, OPT_VERSION },
    {0,0,0,0}
};


/* private functions */

/** Set default values for ayb options defined in this module. */
static void init_options() {
}


/* public functions */

/**
 * Read options from command line arguments. Uses getopt_long to allow long and short forms.
 * Returns index to first non-option argument as reference parameter.
 * Returns whether to continue, stop and indicate error or just stop.
 */
RETOPT read_options(const int argc, char ** const argv, int *nextarg) {
    RETOPT status = E_CONTINUE;

    /* set default values */
    init_options();

    /* act on each option in turn */
    int ch;

    while ((ch = getopt_long(argc, argv, "b:c:d:e:f:i:l:m:n:o:s:wM:N:P:Q:S:", Longopts, NULL)) != -1){

        switch(ch){
            case 'b':
                /* pattern of data blocks */
                if (!parse_blockopt(optarg)) {
                    status = E_FAIL;
                }
                break;

            case 'c':
                /* Genome composition of reference sequence */
                if(!set_composition(optarg)) {
                    fprintf(stderr, "Fatal: Invalid genome composition: \'%s\'\n\n", optarg);
                    status = E_FAIL;
                }
                break;

            case 'd':
                /* input format */
                if (!set_input_format(optarg)) {
                    fprintf(stderr, "Fatal: Unrecognised input format option: \'%s\'\n\n", optarg);
                    status = E_FAIL;
                }
                break;

            case 'e':
                /* message file location */
                set_message_path(optarg);
                break;

            case 'f':
                /* output format */
                if (!set_output_format(optarg)) {
                    fprintf(stderr, "Fatal: Unrecognised output format option: \'%s\'\n\n", optarg);
                    status = E_FAIL;
                }
                break;

            case 'i':
                /* input file location */
                set_location(optarg, E_INPUT);
                break;

            case 'l':
                /* message output level */
                if (!set_message_level(optarg)) {
                    fprintf(stderr, "Fatal: Unrecognised error level option: \'%s\'\n\n", optarg);
                    status = E_FAIL;
                }
                break;

            case 'm':
                /* phredchar calculation */
                if (!set_mu(optarg)) {
                    fprintf(stderr, "Fatal: Mu must be a positive value; \'%s\' supplied\n\n", optarg);
                    status = E_FAIL;
                }
                break;

            case 'n':
                /* number of cycles */
                set_niter(optarg);
                break;

            case 'o':
                /* output file location */
                set_location(optarg, E_OUTPUT);
                break;

            case 's':
                 /* output file location */
                 set_simdata(optarg);
                 break;

            case 'w':
                /* show working flag */
                set_show_working();
                break;

            case 'M':
                /* crosstalk file name */
                set_location(optarg, E_CROSSTALK);
                break;

            case 'N':
                /* crosstalk file name */
                set_location(optarg, E_NOISE);
                break;

            case 'P':
                /* crosstalk file name */
                set_location(optarg, E_PHASING);
                break;

            case 'Q':
                /* quality calibration conversion table file location */
                set_location(optarg, E_QUALTAB);
                break;

            case 'S':
                /* Which solver to use for P estimation */
                if(!set_solver(optarg)){
                    fprintf(stderr,"Fatal: Unrecognised solver option: \'%s\'\n\n",optarg);
                    status = E_FAIL;
                }
                break;

            case OPT_HELP:
                print_usage(stderr);
                print_help(stderr);
            	status = E_STOP;
                break;

            case OPT_LICENCE:
                print_licence(stderr);
                status = E_STOP;
                break;

            case OPT_VERSION:
                fprintf(stderr, "\n" PROGNAME " Version %0.2f  %u\n\n", get_version(), get_version_date());

                status = E_STOP;
                break;

            default:
            	// getopt_long outputs an error message
                print_usage(stderr);
            	status = E_FAIL;
        }
    }

    /* return index to non-option arguments */
    *nextarg = optind;
    return status;
}

/**
 * Return true if supplied string matches long or short form of supplied option structure index.
 * Only some indexes identified in OptIndexT enum.
 */
bool match_option(const char *string, const OPTINDEX index) {

    bool ret = false;

    /* must start with option indicator */
    if (*string == '-') {
        if (*(string + 1) == '-') {
            /* long form */
            if (strcmp(string + 2, Longopts[index].name) == 0) {
                ret = true;
            }
        }
        else {
            /* short form */
            if (*(string + 1) == Longopts[index].val) {
                ret = true;
            }
        }
    }
    return ret;
}
