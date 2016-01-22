/*
 * Copyright (c) 1994 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

/*
 * Written  by cheang 9/94  for Prospero Configurations
 * Documentation proofreading and augmentation by swa 8/16/95
 */

#ifndef	PCONFIG_H
#define	PCONFIG_H

/* These constants are used when returning values from functions or setting
   variables explicitly.  They shouldn't be tested against (at least TRUE
   shouldn't), since C true is any non-zero value. */
/* These are used by pconfig.h */
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif



#define PRESOURCE                       "/.ProsperoResources"


/* Uninitialized value may happen if some options are ruled out by the
   compilation #define flags but actually used in the program, definitely
   an inconsistency in the program.
 */
#define UNINITIALIZED_PATH 		\
	"THIS SHOUND NEVER HAPPEN: A PATH WAS UNINITIALIZED"
#define UNINITIALIZED_STRING		\
	"THIS SHOUND NEVER HAPPEN: A STRING WAS UNINITIALIZED"
#define UNINITIALIZED_INT		0
#define UNINITIALIZED_BOOLEAN		FALSE



/* Supported Value Types for Prospero Configurations */
typedef enum {
    P_CONFIG_STRING,	/* prospero string 				*/
    P_CONFIG_PATH,      /* prospero string interpreted as a native host
			   filename (we call this PATH froms the UNIX term
			   "pathname") */ 
                        /* ~/ will be translated to $HOME/.  In other words,
			   ~/ will work like it does in modern editors and
			   shells. */ 
    P_CONFIG_INT,	/* integer         				*/
    P_CONFIG_BOOLEAN	/* integer with predefined value TRUE/FALSE	*/
} p_config_value_type;

/* Prospero Configuration Query Record */
typedef struct {
    const char			*config_title; /* name of the configuration
						  option.  */
    p_config_value_type		value_type; /* type of the option's value */
    void			*default_value;	/* default value if
						   uninitialized.  */
} p_config_query_type;


/* Prospero Configuration Routines */
void p_command_line_preparse(int *argcp, char **argv);
int p_command_line_config(char *config_line);
int p_read_config(const char *prefix, void *p_config_struct, 
	p_config_query_type *p_config_table);
extern char     *p_user_file;	/* These default to NULL; they can be set by
				   the programmer ... so any program can use
				   any name for its configuration files.  */
extern char     *p_system_file;

#endif /* PCONFIG_H */

















