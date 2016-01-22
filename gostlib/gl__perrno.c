/* -*-c-*- */
/*
 * Copyright (c) 1991-1998 by the University of Southern California
 *
 * For copying and distribution information, please see the file
 * <usc-license.h>.
 */

#include <usc-license.h>

#include <stdlib.h>
#include <pthread.h>

#include <gl_strings.h>
#include <perrno.h>

#ifdef GL_THREADS
typedef struct _PERROR {
    int _perrno;
    char *_p_err_string;
    int _pwarn;
    char *_p_warn_string;
} PERROR;

static void perror_destruct(void *arg);

static void perror_once_init_routine(void);

static pthread_once_t perror_once = PTHREAD_ONCE_INIT;
static pthread_key_t perror_key;

/* perror_destruct()
	Destroys the created 
 */
static void
perror_destruct(void *arg)
{
    PERROR *perror;

    perror = (PERROR *)arg;
    if (perror->_p_err_string) stfree(perror->_p_err_string);
    if (perror->_p_warn_string) stfree(perror->_p_warn_string);
    free(perror);
} /* perror_destruct */

/* perror_once_init_routine():
	The one-time initialization routine to create a key for TSD 'perror'.

	Input:	None
	Output:	None
	Return:	None
	Side:	Initializes perrno_key and assigns free() as the destructor of
		the TSD.
*/
static void 
perror_once_init_routine(void)
{
    int status;

    status = pthread_key_create(&perror_key, perror_destruct);
} /* perror_once_init_routine */

/* gl__perror()
	Returns a per-thread private structure, which contains error and
	warning information to the appropriate thread. The function allocates
	memory the first time it is called in each thread. 

	Input:	None
	Output:	None
	Return:	A pointer to a per-thread private integer.
	Side:	A one-time per thread memory allocation and
		pthread_setspecific() 
 */
static PERROR *
gl__perror(void)
{
    int status;
    PERROR *val;

    status = pthread_once(&perror_once, perror_once_init_routine);
    if (status) {
	internal_error("gostlib: pthread_once() failed: Cannot initialize"
		       " perrno -- this should never happen");
    } /* if */
    val = pthread_getspecific(perror_key);
    if (!val) {			/* First time we've called this function in
				   this thread */
	val = (PERROR *)malloc(sizeof (PERROR));
	if (!val)
	    out_of_memory();

	/* Initialize the error strucutre */
	val->_perrno = PSUCCESS;
	val->_pwarn = PNOWARN;
	val->_p_err_string = NULL;
	val->_p_warn_string = NULL;
	status = pthread_setspecific(perror_key, (void *)val);
	if (status) {
	    internal_error("gostlib: pthread_setspecific() failed:  Cannot"
			   " set the Thread Specific Data -- this should"
			   " never happen");
	} /* if */
    } /* if */
    return val;
} /* gl__perror */

/* gl__perrno()
	Returns the pointer to the thread-specific data 'perrno'.

	Input:	None
	Output:	None
	Return:	Pointer to the thread's private integer 'perrno'
	Side:	It calls local function gl__error() to retrieve the entire
		TSD structure.
 */
int *
gl__perrno(void)
{
    return &((gl__perror())->_perrno);
} /* gl__perrno */

/* gl__pwarn()
	Returns the pointer to the thread-specific data 'pwarn'.

	Input:	None
	Output:	None
	Return:	Pointer to the thread's private integer 'pwarn'
	Side:	It calls local function gl__error() to retrieve the entire
		TSD structure.
 */
int *
gl__pwarn(void)
{
    return &((gl__perror())->_pwarn);
} /* gl_pwarn */

/* gl__p_err_string()
	Returns the pointer to the thread-specific data 'p_err_string'.

	Input:	None
	Output:	None
	Return:	Pointer to the thread's private char * 'p_err_string'
	Side:	It calls local function gl__error() to retrieve the entire
		TSD structure.
 */
char **
gl__p_err_string(void)
{
    return &(gl__perror())->_p_err_string;
} /* gl__p_err_string */

/* gl__p_warn_string()
	Returns the pointer to the thread-specific data 'p_warn_string'.

	Input:	None
	Output:	None
	Return:	Pointer to the thread's private char * 'p_warn_string'
	Side:	It calls local function gl__error() to retrieve the entire
		TSD structure.
 */
char **
gl__p_warn_string(void)
{
    return &(gl__perror())->_p_warn_string;
} /* gl__p_warn_string */

#endif /* GL_THREADS */

