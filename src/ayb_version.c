/** 
 * \file ayb_version.c
 * AYB version number.
 * Before making a release, ensure this file is updated.
 *//* 
 *  Created : 22 Oct 2010
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

#include "ayb_version.h"


/* constants */

/** Program Version. */
static const float VERSION = 1.07;      // Quality score calibration, missing data handling,
                                        // GC content, sim output, argument defaults, optimisation
/** Version release date. */
static const unsigned int VERSION_DATE = 20101115;

/* keep a history */
// Version 1.06;    20100922    Final working values, mu arg, bad data handling
// Version 1.05;    20100730    P Solver, MPN unit test, CIF format
// Version 1.04;    20100617    MPN, Quality output and Datablocks
// Version 1.03;    20100520    AYB Base Call Loop
// Version 1.02;    20100427    AYB Initialisation
// Version 1.01     20100406    File I/O and Data Structures
// Version 1.00     20100312    Initial with basic infrastructure

/* members */
/* None    */


/* private functions */
/* None              */

/* public functions */

/** Return the program version. */
float get_version(void) {
    return VERSION;
}

/** Return the program version date as an integer representing yyyymmdd. */
unsigned int get_version_date(void) {
    return VERSION_DATE;
}
