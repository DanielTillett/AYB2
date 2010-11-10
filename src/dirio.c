/**
 * \file dirio.c
 * I/O Environment.
 *   - Input and output file locations (parameters).
 *   - Input file pattern match by input type (parameter), 
 *     prefix (parameter), substring (fixed) and suffix (fixed).
 *   - Search input directory for pattern match files.
 *   - Generate output file name.
 *   - File open and close.
 *
 * Used as a singleton class with local functions accessing global member data.
 *//*
 *  Created : 16 Mar 2010
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

//#define __USE_GNU
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>          // for toupper
#include <dirent.h>
#include <sys/stat.h>
#include "dirio.h"
#include "message.h"


/* constants */

static const char *DEFAULT_PATH = "./";         ///< Use current directory if none supplied.
static const char *PATH_DELIMSTR = "/";         ///< Path delimiter as string. For linux, other OS?
static const char PATH_DELIM = '/';             ///< Path delimiter as char. For linux, other OS?
static const char PREFIXCHAR = '+';             ///< Indicates pattern to be treated as a prefix.
static const char DOT = '.';                    ///< Before file extension, used for tag location.
static const char DELIM = '_';                  ///< Before tag, used for tag location.
static const char BLOCKCHAR = 'a';              ///< Start for additional block suffix.

/**
 * Possible input format text. Match to INFORM enum. Used to match program argument.
 * - Illumina files match name template <prefix>*_int.txt*[.<zip ext>]
 * - cif files match name template <prefix>*.cif
 */
static const char *INFORM_TEXT[] = {"TXT", "CIF"};
/** Text for input format messages. Match to INFORM enum. */
static const char *INFORM_MESS_TEXT[] = {"standard illumina txt", "cif"};
static const char *INTEN_TAG[] = {"int", ""};       ///< Fixed Intensities file tags.
static const char *INTEN_SUF[] = {"txt", "cif"};    ///< Fixed Intensities file suffixes.

/**
 * Permission flags for a directory; owner/group all plus other read/execute.
 * Used by output directory create but seems to be ignored in favour of parent adoption.
 */
static const mode_t DIR_MODE = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;

/* members */

/* the I/O settings for this run */
static INFORM Input_Format = E_CIF;             ///< Selected input format.
static CSTRING Input_Path = NULL;               ///< Input path, default or program argument.
static CSTRING Output_Path = NULL;              ///< Output path, default or program argument.
static CSTRING IntenSubstr = NULL;              ///< Substring an intensities file must contain.
/**
 * File pattern match. Currently a prefix or the whole part of the filename before the substring.
 * Supplied as non-option program argument. Any partial path moved to pattern path.
 */
static CSTRING Pattern = NULL;
static CSTRING Pattern_Path = NULL;             ///< Input path and any pattern partial path.
static size_t Pattern_Len = 0;                  ///< Length of pattern match, program exits if not > 0.
static CSTRING Matrix[E_NMATRIX];               ///< Predetermined matrix input file locations.

static struct dirent **Dir_List = NULL;         ///< The pattern matched directory list.
static int Dir_Num = 0;                         ///< Number of pattern matched files found.
static int Index = -1;                          ///< Current index to pattern matched files.

static CSTRING Current = NULL;                  ///< Current input file, used to create output filename.


/* private functions */

/** Clear structures and values associated with a pattern. */
static void clear_pattern(void) {

    Pattern = free_CSTRING(Pattern);
    Pattern_Path = free_CSTRING(Pattern_Path);
    Pattern_Len = 0;
    Index = -1;

    if ((Dir_Num > 0) && (Dir_List != NULL)) {
        for (int i = 0; i < Dir_Num; i++) {
            free(Dir_List[i]);
        }
        free(Dir_List);
        Dir_List = NULL;
        Dir_Num = 0;
    }
}

/**
 * Create a full path name from a directory and filename. Places a path delimiter between them;
 * delimiter may change according to target system.
 * Return false if supplied filename is null or cannot allocate memory for name string.
 */
static bool full_path(const CSTRING dir, const CSTRING filename, CSTRING *filepath) {
/*  Parameters: dir - the directory, may be null
                filename - the filename
                filepath - full pathname (returned)
    Returns:    whether succeeded
*/
    if (filename == NULL) {return false;}

    /* determine length required, allow for delimiter */
    size_t len = strlen(filename);
    if (dir != NULL) {
        len += strlen(dir) + strlen(PATH_DELIMSTR);
    }

    *filepath = new_CSTRING(len);
    if (*filepath == NULL) {
        message(E_NOMEM_S, MSG_FATAL, "file path creation");
        return false;
    }
    else {
        if (dir != NULL) {
            strcpy(*filepath, dir);
            strcat(*filepath, PATH_DELIMSTR);
        }
        strcat(*filepath, filename);
//        message(E_DEBUG_SSD_S, MSG_DEBUG, __func__, __FILE__, __LINE__, *filepath);
        return true;
    }
}

/** Create the fixed tag and suffix string that an intensities file must contain. */
static void make_substring(void) {

    /* check if anything to match */
    size_t taglen = strlen(INTEN_TAG[Input_Format]);
    size_t suflen = strlen(INTEN_SUF[Input_Format]);
    size_t len = 0;

    if (taglen > 0) {
        len += taglen + 1;
    }
    if (suflen > 0) {
        len += suflen + 1;
    }
    if (len > 0) {
        /* add room for terminator */
        IntenSubstr = new_CSTRING(++len);
        const char *pnext;
        int pos = 0;

        if (taglen > 0) {
            /* add delimiter and tag */
            IntenSubstr[pos++] = DELIM;
            for (pnext = INTEN_TAG[Input_Format]; pnext < INTEN_TAG[Input_Format] + taglen; pnext++) {
                IntenSubstr[pos++] = *pnext;
            }
        }
        if (suflen > 0) {
            /* add dot and suffix */
            IntenSubstr[pos++] = DOT;
            for (pnext = INTEN_SUF[Input_Format]; pnext < INTEN_SUF[Input_Format] + suflen; pnext++) {
                IntenSubstr[pos++] = *pnext;
            }
        }
        IntenSubstr[pos] = '\0';
    }
}

//#ifndef __USE_GNU
/* substring search ignoring case, needed hmhm? */
char *strcasestr(const char *s1, const char *s2) {

    CSTRING lookin = copy_CSTRING((CSTRING)s1);
    CSTRING lookfor = copy_CSTRING((CSTRING)s2);
    char *ret;
    int pos;

    for (pos = 0; pos < strlen(lookin); pos++) {
        lookin[pos] = toupper(lookin[pos]);
    }
    for (pos = 0; pos < strlen(lookfor); pos++) {
        lookfor[pos] = toupper(lookfor[pos]);
    }

    ret = strstr(lookin, lookfor);
    if (ret != NULL) {
        /* move pointer to original string */
        ret = (char*)s1 + (int)(ret - lookin);
    }
    free_CSTRING(lookin);
    free_CSTRING(lookfor);

    return ret;
}
//#endif


/**
 * Selector function for scandir. Matches to a prefix and fixed tag and suffix.
 * If pattern ends with prefix indicator then allows inclusion of additional characters between.
 * Assumes match substring already set up.
 * Returns non-zero value if match found as required by scandir.
 */
static int match_pattern(const struct dirent *list) {

    int ret = 0;

    if (Pattern[Pattern_Len - 1] == PREFIXCHAR) {
        /* check if prefix matches beginning */
        if (strncasecmp(list->d_name, Pattern, Pattern_Len - 1) == 0) {
            /* check if contains _input tag and suffix  */
            if (IntenSubstr != NULL) {
                if (strcasestr(list->d_name, IntenSubstr) != NULL) {
                    ret = 1;
                }
            }
            else {
                /* nothing to match */
                ret = 1;
            }
        }
    }

    else {
        /* create whole filename */
        size_t len = Pattern_Len;
        if (IntenSubstr != NULL) {
            len += strlen(IntenSubstr);
        }
        CSTRING filename = new_CSTRING(len);
        strcpy(filename, Pattern);
        if (IntenSubstr != NULL) {
            strcat(filename, IntenSubstr);
        }
        /* check for whole filename match, still allows for compression suffix */
        if (strncasecmp(list->d_name, filename, len) == 0) {
            ret = 1;
        }
        free_CSTRING(filename);
    }

    return ret;
}

/** Make a new input path by adding any partial path from filename, and remove from filename. */
static CSTRING move_partial_path(const CSTRING filepath, CSTRING *filename) {

    if (filepath == NULL) {return NULL;}
    if (*filename == NULL) {return NULL;}

    CSTRING newpath = NULL;
    size_t oldlen = strlen(filepath);

    /* find the beginning of the name */
    char *pdlm = strrchr(*filename, PATH_DELIM);
    if (pdlm == NULL) {
        /* no partial path, return filepath unchanged */
        newpath = copy_CSTRING(filepath);
    }
    else {
        /* add the partial path to a new path */
        size_t sublen = pdlm - *filename;
        newpath = new_CSTRING(oldlen + sublen + strlen(PATH_DELIMSTR));
        strcpy(newpath, filepath);
        if (newpath[oldlen - 1] != PATH_DELIM) {
            strcat(newpath, PATH_DELIMSTR);
        }
        strncat(newpath, *filename, sublen);

        /* remove the partial path from the filename */
        CSTRING old = *filename;
        *filename = copy_CSTRING(old + sublen + 1);
        free_CSTRING(old);
    }

    return newpath;
}

/**
 * Return a new file name created from an original.
 * Replaces the part between the last delimiter and the first dot with a new tag.
 * Removes any compression suffix.
 */
static CSTRING output_name(const CSTRING oldname, const CSTRING tag, int blk) {

    CSTRING newname = NULL;
    size_t oldlen = strlen(oldname);
    size_t taglen = strlen(tag);

    /* find last delimiter */
    char *pdlm = strrchr(oldname, DELIM);
    if (pdlm == NULL) {
        /* no delimiter, replace whole name */
        pdlm = oldname - 1;
    }

    /* find first dot */
    char *pdot = strchr(oldname, DOT);
    if (pdot == NULL) {
        /* no dot, replace to end of name */
        pdot = oldname + oldlen;
    }

    /* get a string of size for new name */
    newname = new_CSTRING(oldlen - (pdot - pdlm - 1) + taglen + ((blk >= 0)?1:0));

    if (newname == NULL) {
        message(E_NOMEM_S, MSG_FATAL, " output name creation");
    }
    else {
        /* copy the component parts to the new name */
        char *pnext;
        int pos = 0;
        for (pnext = oldname; pnext < pdlm; pnext++) {
            newname[pos++] = *pnext;
        }
        /* add block suffix before delimeter */
        if (blk >= 0) {
            newname[pos++] = BLOCKCHAR + blk;
        }
        newname[pos++] = *pdlm;
        for (pnext = tag; pnext < tag + taglen; pnext++) {
            newname[pos++] = *pnext;
        }
        for (pnext = pdot; pnext < oldname + oldlen; pnext++) {
            newname[pos++] = *pnext;
        }
        /* finish with string terminator */
        newname[pos] = '\0';

        /* Remove any compression suffix */
        XFILE_MODE mode = guess_mode_from_filename (newname);
        if (mode != XFILE_RAW) {
            char *psuf = strrchr(newname, DOT);
            *psuf = '\0';
        }
    }
//    message(E_DEBUG_SSD_S, MSG_DEBUG, __func__, __FILE__, __LINE__, newname);
    return newname;
}

/**
 * Return a new file name created from an original cif.
 * Replaces the suffix with a new tag.
 */
static CSTRING output_name_cif(const CSTRING oldname, const CSTRING tag, int blk) {

    CSTRING newname = NULL;
    size_t oldlen = strlen(oldname);
    size_t taglen = strlen(tag);

    /* find the suffix */
    char *pdot = strrchr(oldname, DOT);
    if (pdot == NULL) {
        /* no dot, add to name */
        pdot = oldname + oldlen;
    }

    /* get a string of size for new name */
    newname = new_CSTRING(pdot - oldname + taglen + 1 + ((blk >= 0)?1:0));

    if (newname == NULL) {
        message(E_NOMEM_S, MSG_FATAL, " output name creation");
    }
    else {
        /* copy the component parts to the new name */
        char *pnext;
        int pos = 0;
        for (pnext = oldname; pnext < pdot; pnext++) {
            newname[pos++] = *pnext;
        }
        /* add block suffix before dot */
        if (blk >= 0) {
            newname[pos++] = BLOCKCHAR + blk;
        }
        newname[pos++] = DOT;
        for (pnext = tag; pnext < tag + taglen; pnext++) {
            newname[pos++] = *pnext;
        }
        /* finish with string terminator */
        newname[pos] = '\0';
    }

//    message(E_DEBUG_SSD_S, MSG_DEBUG, __func__, __FILE__, __LINE__, newname);
    return newname;
}

/**
 * Scan the input directory for any files that match the specified pattern.
 * Result placed in Dir_List. Return the number found.
 */
static int scan_inputs(void) {

    /* hmhm new message func to return a message string */
    static const char *ERRMESS = "Error: Couldn't open the directory: \'%s\'";

    int num = scandir (Pattern_Path, &Dir_List, match_pattern, alphasort);

    if (num < 0) {
        CSTRING msg = new_CSTRING(strlen(ERRMESS) + strlen(Pattern_Path));
        sprintf(msg, ERRMESS, Pattern_Path);
        perror (msg);
        free_CSTRING(msg);
    }
    else {
        for (int cnt = 0; cnt < num; ++cnt) {
//            message(E_DEBUG_SSD_S, MSG_DEBUG, __func__, __FILE__, __LINE__, Dir_List[cnt]->d_name);
        }
    }

    return num;
}


/* public functions */

/** Return whether specified output directory exists or can be created. */
bool check_outdir(const CSTRING dirname, const char * typestr) {

    struct stat st;
    if (stat(dirname, &st) == 0) {
        /* check it is a directory; ISDIR returns non-zero for a directory */
        if (S_ISDIR (st.st_mode) == 0) {
            message(E_BAD_DIR_SS, MSG_FATAL, typestr, dirname);
            return false;
        }
    }
    else {
        /* try to create it; mkdir returns zero on success */
        if (mkdir (dirname, DIR_MODE) != 0){
            message(E_NOCREATE_DIR_SS, MSG_FATAL, typestr, dirname);
            return false;
        }
        else{
            message(E_CREATED_DIR_SS, MSG_INFO, typestr, dirname);
        }
    }
    return true;
}

/** Return the name of the current input file. */
CSTRING get_current_file(void) {

    if (Current == NULL) {
        return "";
    }
    else {
        return Current;
    }
}

/** Return the selected input format. */
INFORM get_input_format(void) {

    return Input_Format;
}

/** Return the file pattern match argument. */
CSTRING get_pattern(void) {

    if (Pattern == NULL) {
        return "";
    }
    else {
        return Pattern;
    }
}

/** Return if a predetermined matrix input file is specified */
bool matrix_from_file(IOTYPE idx) {
    return (Matrix[idx] != NULL);
}

/**
 * Open a predetermined input matrix file.
 * Return the file handle or NULL if failed to open.
 */
XFILE * open_matrix(IOTYPE idx) {

    CSTRING filepath = NULL;
    XFILE *fp = NULL;

    /* make a full path name */
    if (Matrix[idx] == NULL) {
        message(E_DEBUG_SSD_SD, MSG_DEBUG, __func__, __FILE__, __LINE__, "No Matrix file at position:", idx);
    }
    else {
        if (full_path(Input_Path, Matrix[idx], &filepath)) {
           fp =  xfopen(filepath, XFILE_UNKNOWN, "r" );

           if (xfisnull(fp)) {
               message(E_OPEN_FAIL_SS, MSG_ERR, "Input matrix", filepath);
               fp = xfclose(fp);
           }
           else {
               message(E_INPUT_FOUND_S, MSG_INFO, Matrix[idx] );
           }
        }
        free_CSTRING(filepath);
    }

    return fp;
}

/**
 * Open the next intensities file in the directory.
 * Output error and go to next if fails to open.
 * Return the file handle or NULL if no more files.
 * Also closes previous intensities file if any.
 */
XFILE * open_next(XFILE *fplast) {

    CSTRING filepath = NULL;
    XFILE *fp = NULL;

    if (fplast != NULL) {
        fplast = xfclose(fplast);
        Current = free_CSTRING(Current);
    }

    while ((fp == NULL) && (++Index < Dir_Num)) {
        Current = copy_CSTRING(Dir_List[Index]->d_name);
        message(E_DEBUG_SSD_S, MSG_DEBUG, __func__, __FILE__, __LINE__, Current);

        /* make a full path name */
        if (full_path(Pattern_Path, Current, &filepath)) {
           fp =  xfopen(filepath, XFILE_UNKNOWN, "r" );

           if (xfisnull(fp)) {
               message(E_OPEN_FAIL_SS, MSG_ERR, "Input", filepath);
               fp =xfclose(fp);
           }
           else {
               message(E_INPUT_FOUND_S, MSG_INFO, Current );
           }
        }
        free_CSTRING(filepath);
    }

    if ((fp == NULL) && (Current != NULL)) {
        Current = free_CSTRING(Current);
    }
    return fp;
}

/** Open an output file with no block suffix. */
XFILE * open_output(const CSTRING tag) {
    return open_output_blk(tag, BLK_SINGLE);
}

/**
 * Open an output file corresponding to current input file with supplied tag.
 * A non-negative blk indicates a block suffix should be added to the name.
 * BLK_SINGLE indicates no block suffix.
 * BLK_APPEND indicates open in append mode.
 * Return the file handle or NULL if failed to open.
 */
XFILE * open_output_blk(const CSTRING tag, int blk) {

    CSTRING filename = NULL;
    CSTRING filepath = NULL;
    XFILE *fp = NULL;

    if (Current == NULL) {
        /* use the tag on its own */
        filename = copy_CSTRING(tag);
    }
    else {
        /* create output file name from current input */
        switch (Input_Format) {
            case E_TXT:
                filename = output_name(Current, tag, blk);
                break;

            case E_CIF:
                filename = output_name_cif(Current, tag, blk);
                break;

            default: ;
        }
    }

    if (filename != NULL) {
        if (full_path(Output_Path, filename, &filepath)) {
            const char *mode_str = ((blk == BLK_APPEND) ? "a" : "w");
            fp =  xfopen(filepath, XFILE_RAW, mode_str );

            if (xfisnull(fp)) {
                message(E_OPEN_FAIL_SS, MSG_ERR, "Output", filepath);
                fp = xfclose(fp);
            }
            else {
                message(E_DEBUG_SSD_S, MSG_DEBUG, __func__, __FILE__, __LINE__, filename);
            }
        }
    }
    free_CSTRING(filename);
    free_CSTRING(filepath);

    return fp;
}

/**
 * Set the input format. Text must match one of the input format text list. Ignores case.
 * Returns true if match found.
 */
bool set_input_format(const char *inform_str) {

    /* match to one of the possible options */
    int matchidx = match_string(inform_str, INFORM_TEXT, E_INFORM_NUM);
    if (matchidx >= 0) {
        Input_Format = matchidx;
        return true;
    }
    else {
        return false;
    }
}

/** Set file location information. */
void set_location(const CSTRING path, IOTYPE idx){

    switch(idx) {
        case E_INPUT:
            Input_Path = copy_CSTRING(path);
            break;
        case E_OUTPUT:
            Output_Path = copy_CSTRING(path);
            break;
        case E_CROSSTALK:
        case E_NOISE:
        case E_PHASING:
            Matrix[idx] = copy_CSTRING(path);
            break;
        default: ;
    }
}

/**
 * Set the input filename pattern to match to. Moves any partial path to pattern path.
 * Checks pattern argument supplied, and at least one input file found.
 */
bool set_pattern(const CSTRING pattern) {

    clear_pattern();
    Pattern = copy_CSTRING(pattern);

    /* move any partial path from pattern to path */
    Pattern_Path = move_partial_path(Input_Path, &Pattern);

    /* check pattern match supplied */
    if (Pattern != NULL) {
        Pattern_Len = strlen(Pattern);
    }

    if ((Pattern_Len == 0) || (Pattern_Path == NULL)) {
        message(E_NOPATTERN_FILE_S, MSG_ERR, pattern);
        return false;
    }

    /* scan for matching input files */
    Dir_Num = scan_inputs();
    if (Dir_Num < 0) {
        /* failed to access directory */
        return false;
    }

    if (Dir_Num == 0) {
        message (E_NOINPUT_SS, MSG_ERR, Pattern_Path, Pattern);
        return false;
    }
    else {
        /* at least one input file */
        message(E_PATTERN_MATCH_SD, MSG_INFO, pattern, Dir_Num);
        return true;
    }
}

/**
 * Start up; call at program start after options.
 * Checks output directory exists and creates the match substring.
 * Return true if no errors.
 */
bool startup_dirio(void) {

    /* set default i/o paths if not supplied */
    if (Input_Path == NULL) {
        Input_Path = copy_CSTRING((CSTRING)DEFAULT_PATH);
    }
    if (Output_Path == NULL) {
        Output_Path = copy_CSTRING((CSTRING)DEFAULT_PATH);
    }

    /* check specified output path exists */
    if (!check_outdir(Output_Path, "output")) {
         return false;
    }
    /* create the input filename match substring */
    make_substring();

    message(E_INPUT_DIR_S, MSG_INFO, Input_Path);
    message(E_OPT_SELECT_SS, MSG_INFO, "Input format" ,INFORM_MESS_TEXT[Input_Format]);
    message(E_OUTPUT_DIR_S, MSG_INFO, Output_Path);
    return true;
}

/** Tidy up; call at program shutdown. */
void tidyup_dirio(void) {

    /* clear any pattern info */
    clear_pattern();

    /* free string memory */
    Input_Path = free_CSTRING(Input_Path);
    Output_Path = free_CSTRING(Output_Path);
    IntenSubstr = free_CSTRING(IntenSubstr);
    for (IOTYPE idx = 0; idx < E_NMATRIX; idx++) {
        Matrix[idx] = free_CSTRING(Matrix[idx]);
    }
}
