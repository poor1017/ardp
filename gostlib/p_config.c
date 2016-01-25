/*
 * Copyright (c) 1994 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>
/*
 * Written  by cheang 9/94  for Prospero Configurations
 *
 * The current configuration routines is error tolerant and takes the
 * liberty to report errors directly to stderr. 
 */

#include <stdio.h>
#include <string.h>		/* strncmp() prototype */

#include <gostlib.h>
#include <gl_strings.h>
#include <gl_parse.h>
#include <sys/stat.h>
#include <stdlib.h>		/* malloc() prototype */
#include <ctype.h>		/* isspace() prototype */
#if 0
#include "../lib/ardp/ardp.h"		/* XXX for ARDP_MAX_PRI.  This is the only spot
				   where gostlib depends on ardp.  This
				   special-case code testing for this can be
				   revised later. --swa */
#endif
#include <errno.h>
#include <pconfig.h>

#include <perrno.h>

#define RETURN(rv) do { retval = rv ; goto cleanup ; } while (0)


/* Definition of the P_CONFIG_LINE structure                            */
/* Note:                                                                */
/*     This structure constitutes the configuration database and is     */
/*       not to be freed.                                               */
/*     It is not sorted currently.                                      */

struct p_config_line {
    char                        *prefix;
    char                        *config_title;
    char                        *config_value;
/*    struct p_config_line      *previous; */
    struct p_config_line        *next;
};

typedef struct p_config_line *P_CONFIG_LINE;
typedef struct p_config_line P_CONFIG_LINE_ST;

/* P_CONFIG_LINE */


/* Definition of the P_COMMAND_LINE structure                           */
/* Note:                                                                */
/*     This structure is used as a temporary holder of the command      */
/*       line config options before prospero is initialized.            */
/*     Once prospero is initialized, it will be converted into          */
/*       P_CONFIG_LINE.                                                 */
struct p_command_line {
    char                        *config_line;
    struct p_command_line       *next;
};


typedef struct p_command_line *P_COMMAND_LINE;
typedef struct p_command_line P_COMMAND_LINE_ST;

/* P_COMMAND_LINE */


/* MAJOR ASSUMPTION :                                                   */
/*     These variables are MODIFIED BEFORE any MULTI-THREADED           */
/*        operations and remain readonly afterward.                     */
/* THREAD-SAFE NOW. -- 5/23/96 */

static int      p_config_loaded         = FALSE;
char     *p_user_file            = NULL;
char     *p_system_file          = NULL;
static P_CONFIG_LINE    p_config_list = NULL;
static P_COMMAND_LINE   p_command_list = NULL;


static char *p_parse_path_stcopyr(const char *inpath, char *outpath);
static char *match_config(const char *prefix, const char *config_title);
static int p_load_config_files(void);
static int p_load_command_line_config(void);
static int p_load_config_file(const char *p_config_file, const char *default_dir);

static int in_config_line_GSP(INPUT in, char **linep);
static int p_insert_config_line(const char *config_line);
static P_CONFIG_LINE clalloc(void);



/* swa wrote this one.  give me my 10 or so lines of credit :)  My fault too.
   :)*/ 
void
p_command_line_preparse(int *argcp, char **argv)
{
    int num_args_used = 0;
    char **scanning; char **writing;

    /* modifies global data */
    for (scanning = writing = argv; *scanning; ++scanning) {
        /* If a - by itself, or --, then no more arguments */
        if (strequal(*scanning, "-") ||
                strnequal(*scanning, "--", 2)) {
            break;
        }
        if (strnequal(*scanning, "-D", 2)) {
            pfs_debug = 1; /* Default debug level */
            qsscanf(*scanning,"-D%d",&pfs_debug);
	    ardp_debug = pfs_debug;
            /* eat this argument */
            ++num_args_used;
	    
        }
#ifdef ARDP_MAX_PRI
        else if (strnequal(*scanning, "-N", 2)) {
            ardp_priority = ARDP_MAX_PRI; /* Use this if no # given */
            qsscanf(*scanning,"-N%d", &ardp_priority);
            if(ardp_priority > ARDP_MAX_SPRI) 
                ardp_priority = ARDP_MAX_PRI;
            if(ardp_priority < ARDP_MIN_PRI) 
                ardp_priority = ARDP_MIN_PRI;
            /* eat this argument */
            ++num_args_used;
        }
#endif
        else if (strnequal(*scanning, "-pc", 3)) {
            p_command_line_config(*scanning + 3);
            ++num_args_used;
        }
        else {
            /* shift the arguments forward */
            assert (scanning >= writing);
            if (scanning > writing) {
                *writing = *scanning;
            }
            writing++;
        }
    }
    /* shift remaining arguments */
    while (*scanning)
        *writing++ = *scanning++;

    *writing = 0;               /* finish off the written list */
    *argcp -= num_args_used;    /* reset caller's ARGC appropriately, by
                                   removing all consumed arguments. */
}



/* p_command_line_config(char *config_line)                             */
/*     char *config_line -- command line config option                  */
/*                                                                      */
/*     Record the command line config options in p_command_list for     */
/*       later configuration use.                                       */
/*     Recognize two special configurations:                            */
/*              p_system_file:  system_file_path                        */
/*              p_user_file:    user_file_path                          */
/*     Assumption:                                                      */
/*       config_line passed in are command line arguments, and won't be */
/*       freed.                                                         */
/*                                                                      */
int
p_command_line_config(char *config_line)
{
    P_COMMAND_LINE p_command_line;

    if (!strncmp(config_line, "p_system_file:", 14)) {
        p_system_file = p_parse_path_stcopyr(config_line+14, p_system_file);
    }
    else if (!strncmp(config_line, "p_user_file:", 12)) {
        p_user_file = p_parse_path_stcopyr(config_line+12, p_user_file);
    }
    else {
        p_command_line = malloc(sizeof(*p_command_line));
        if (!p_command_line) out_of_memory();
        p_command_line->config_line = config_line;
        p_command_line->next = p_command_list;
        p_command_list = p_command_line;
    }

    return PSUCCESS;
}


/* Convert any leading ~ in inpath to users home directory if any and   */
/*   stcopyr the whole path into outpath.                               */
/* special case: if HOME undefined, file name won't be expanded; the ~ is
	passed through literally.
	(cheang wonders if this behavior is the best choice.  swa agrees.)
*/
static char *
p_parse_path_stcopyr(const char *inpath, char *outpath)
{
    char *home = NULL;

    if (!inpath)
        return NULL;
    if (*inpath == '~')
        home = getenv("HOME");
    if (home)
         return qsprintf_stcopyr(outpath, "%s%s", home, inpath+1);
    else
        return stcopyr(inpath, outpath);
}



/* p_read_config(const char *prefix, void *p_config_struct,             */
/*               p_config_query_type *p_config_table)                   */
/*     char *prefix -- class of configurations like "prospero", "ardp", */
/*                      or application name like "vls", "menu", etc.    */
/*     char *p_config_struct -- structure to fill in configuration      */
/*                      values.                                         */
/*     p_config_query_type *p_config_table -- table specifying the      */
/*                      names and value types of configurations needed, */
/*                      and corresponding default values.               */
/*                                                                      */
/*     Load the configuration values from the configuration database    */
/*     into a configuration structure with matching specifications.     */
/*                                                                      */
int 
p_read_config(const char *prefix, void *p_config_struct, 
        p_config_query_type *p_config_table)
{
    char *value_string;

    if (!p_config_struct || !p_config_table)
        return PSUCCESS;

    p_load_config_files();

    while (p_config_table->config_title) {
        value_string = match_config(prefix, p_config_table->config_title);
        if (value_string) {
            switch (p_config_table->value_type) {
            case P_CONFIG_INT:
            {
                int* temp = (int*)p_config_struct;
                sscanf(value_string, "%d", temp);
                ++temp;
                p_config_struct = temp;
                break;
            }
            case P_CONFIG_BOOLEAN:
            {
                int* temp = (int*)p_config_struct;
                *temp = !strcasecmp(value_string, "true") ? TRUE :
                        !strcasecmp(value_string, "false") ? FALSE :
                        (int)p_config_table->default_value;
                ++temp;
                p_config_struct = temp;
                break;
            }
            case P_CONFIG_PATH:
            {
                char** temp = (char**)p_config_struct;
                *temp = p_parse_path_stcopyr(value_string, *temp);
                ++temp;
                p_config_struct = temp;
                break;
            }
            case P_CONFIG_STRING:
            {
                char** temp = (char**)p_config_struct;
                *temp = stcopyr(value_string, *temp);
                ++temp;
                p_config_struct = temp;
                break;
            }
            default:
                internal_error("Invalid Configuration Value Type");
            }
        }
        else {
            switch (p_config_table->value_type) {
            case P_CONFIG_INT:
            case P_CONFIG_BOOLEAN:
            {
                int* temp = (int*)p_config_struct;
                *temp = (int)p_config_table->default_value;
                ++temp;
                p_config_struct = temp;
                break;
            }
            case P_CONFIG_PATH:
            {
                char** temp = (char**)p_config_struct;
                *temp = p_parse_path_stcopyr((char *)p_config_table->default_value, *temp);
                ++temp;
                p_config_struct = temp;
                break;
            }
            case P_CONFIG_STRING:
            {
                char** temp = (char**)p_config_struct;
                *temp = stcopyr((char *)p_config_table->default_value, *temp);
                ++temp;
                p_config_struct = temp;
                break;
            }
            default:
                internal_error("Invalid Configuration Value Type");
            }
        }
        p_config_table++;
    }
    return PSUCCESS;
}


/* Find a P_CONFIG_LINE in p_config_list which matches the requested    */
/*   prefix and config_title and return the associated value if found.  */
static char *
match_config(const char *prefix, const char *config_title)
{
    P_CONFIG_LINE cl = p_config_list;

    while (cl) {
        if (!strcasecmp(prefix, cl->prefix) && 
            !strcasecmp(config_title, cl->config_title))
            return cl->config_value;
        cl = cl->next;
    }
    return NULL;
}


/* Load all the configuration lines from command line options and       */
/*   configuration files into p_config_list                             */
/* Priority: 1 - command line; 2 - user file; 3 - system file           */
static int 
p_load_config_files(void)
{
    const char *home;

    if (p_config_loaded)
        return PSUCCESS;

    p_config_loaded = TRUE;
    p_config_list = NULL;

    p_load_command_line_config();
    home = getenv("HOME");
    if (home)
        p_load_config_file(p_user_file, home);
    p_load_config_file(p_system_file, P_SYSTEM_DIR);

    return PSUCCESS;
}


/* Load command line options into p_config_list                         */
static int 
p_load_command_line_config(void)
{
    P_COMMAND_LINE p_command_line = p_command_list;

    while (p_command_line) {
        p_insert_config_line(p_command_line->config_line);
        p_command_line = p_command_line->next;
        free(p_command_list);
        p_command_list = p_command_line;
    }
    return PSUCCESS;
}


/* Load configuration lines from a configuration file into p_config_list */
static int 
p_load_config_file(const char *p_config_file, const char *default_dir)
{
    int retval = PSUCCESS;
    char *p_file = NULL;
    struct stat st;
    FILE *f_rc;
    INPUT_ST in_st;
    INPUT in = &in_st;
    char *line = NULL;		/* GOSTLIB string; must be freed */

    if (p_config_file) {
        p_file = stcopy(p_config_file);
        if (stat(p_file, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
		p_file = qsprintf_stcopyr(p_file, "%s%s", p_file, PRESOURCE);
	   }
        }
        else {
            /* perror(p_file); */
            stfree(p_file);
            return PFAILURE;
        }
    }
    else {
        if (default_dir)
	     p_file = qsprintf_stcopyr(p_file, "%s%s", default_dir, PRESOURCE);
        /* This strange behavior will happen if there is no HOME directory. 
           Happens when called in Web CGI Script. */
        else
            return PFAILURE;
    }
    f_rc = fopen(p_file, "r");
    if (!f_rc) {
        /* perror(p_file); */
	stfree(p_file);
        return PFAILURE;
    }

    retval = wholefiletoin(f_rc, in);
    if (retval) {
	RETURN(retval);
    }

    while (!(retval = in_config_line_GSP(in, &line))) {
        /* lines beginning with '!' */
        if (*line == '!')
            continue;
        p_insert_config_line(line);
    }
    if (retval != EOF) {
        RETURN(retval);
    }
cleanup:
    fclose(f_rc);
    stfree(p_file);
    GL_STFREE(line);
    return retval;
}


/* get a line from configuration file buffer and skip blank lines */
/* Modifies *linep.
 * When this function is called, *linep should be a Gostlib String.
 */
static int
in_config_line_GSP(INPUT in, char **linep)
{
    int tmp;
    INPUT_ST eol_st;          /* temp. variable for end of line. */
    INPUT eol = &eol_st;      /* temp. variable for end of line. */
    int	   eol_aux[10];		/* auxiliary storage area */

    int linelen;           /* length to copy into linebuf  */
    int i;                    /* temporary index */
    char *cp;            /* temp. index */
    
    do {
        /* Skip leading blanks and skip to next line */
        qscanf(in, "%~%R", in, in->u.c.dat, in->u.c.datsiz);
        if (in_eof(in))
            return EOF;
    } while ( isspace(*(in->u.s.s)) );   /* skip blank lines */

#if 0
    /* This piece can't support multiple lines of configuration with */
    /* Prospero quoting. Otherwise it is much cleaner. */
    qsscanf(in->s, "%&[^\n\r]", linep);
    qscanf(in, "%*[^\n\r]%r", in, in->u.c.dat, in->u.c.datsiz);
#else
    /* Too bad no way to read in quoted Prospero strings without */
    /* unquoting them by directly using qsscanf. */
    tmp = qscanf(in, "%'*(^\n\r)%r", eol, &eol_aux, sizeof eol_aux);
    if (tmp < 1) {
        rfprintf(stderr, "Unterminated quote: %s\n", in->u.s.s);
        return PARSE_ERROR;
    }

    linelen = eol->u.s.s - in->u.s.s;
    if (!*linep)
        assert(*linep = stalloc(linelen + 1));
    else if (p__bstsize(*linep) < linelen + 1) {
        stfree(*linep);
        assert(*linep = stalloc(linelen + 1));
    }
    /* Now copy the bytes from the input stream to the storage space pointed to
    by *line.  This preserves quoting, which is very important. */
    for(cp = *linep, i = 0; i < linelen; ++cp, ++i, in_incc(in))
        *cp = in_readc(in);
    *cp = '\0';

#endif
    return PSUCCESS;
}



/* Parse and insert a configuration line into the p_config_list if      */
/*   the same configuration does not already exist there.               */
/* The p_config_list is constructed in a higher-priority first fashion. */
/*   Higher priority entries like command line config will be inserted  */
/*   before lower priority ones, say, from the system-wide              */
/*   configuration file. In this fashion, there is no need to override  */
/*   low priority entries but just ignore them.                         */
/* This function should only be called during initialization; it should
   certainly not be called simultaneously by two threads.  It is only used
   during setup, and it contains static data.   This could be made re-entrant,
   but we won't bother, because it's not necessary.  --swa, salehi, 4/98*/
static int
p_insert_config_line(const char *config_line)
{
    int tmp;
    P_CONFIG_LINE cl;

    static char *prefix = NULL;
    char **prefixp = &prefix;
    static char *config_title = NULL;
    char **config_titlep = &config_title;
    static char *config_value;
    char **config_valuep = &config_value;

    assert(gl_is_this_thread_master());
    tmp = qsscanf(config_line, "%'&[^.].%'&[^: \t]%~:%~%&'s", 
            prefixp, config_titlep, config_valuep);
    if (tmp < 3) {
        rfprintf(stderr, "Invalid configuration: %s\n", config_line);
        return PARSE_ERROR;
    }
    /* if entry does not exist yet, append it */
    if (!match_config(*prefixp, *config_titlep)) {
        cl = clalloc();
        cl->prefix = stcopyr(*prefixp, cl->prefix);
        cl->config_title = stcopyr(*config_titlep, cl->config_title);
        cl->config_value = stcopyr(*config_valuep, cl->config_value);
        cl->next = p_config_list;
        p_config_list = cl;
    }

    return PSUCCESS;
}



/* Allocate a P_CONFIG_LINE structure */
static P_CONFIG_LINE 
clalloc(void)
{
    P_CONFIG_LINE cl;

    cl = (P_CONFIG_LINE) malloc(sizeof(P_CONFIG_LINE_ST));
    if (!cl) out_of_memory();

    /* Initialize and fill in default values */
    cl->prefix = NULL;
    cl->config_title = NULL;
    cl->config_value = NULL;
/*    cl->previous = NULL; */
    cl->next = NULL;
    return cl;
}


#if 0

/* Free a P_CONFIG_LINE structure */
static P_CONFIG_LINE 
clfree(void)
{
    P_CONFIG_LINE cl;

    cl = (P_CONFIG_LINE) malloc(sizeof(P_CONFIG_LINE_ST));
    if (!cl) out_of_memory();

    /* Initialize and fill in default values */
    cl->prefix = NULL;
    cl->config_title = NULL;
    cl->config_value = NULL;
/*    cl->previous = NULL; */
    cl->next = NULL;
    return cl;
}
#endif
