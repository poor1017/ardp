/* Support infrastructure for standalone ARDP code; copies of stuff that would
   otherwise be provided by gostlib. 
   Used only when compiling with ARDP_STANDALONE defined. */
#ifdef ARDP_STANDALONE
/* gl__function_internal_error_helper(), 
   gl__is_out_of_memory, 
   gl__fout_of_memory() */
#include "../../gostlib/gl_internal_err.c" 
/* (*internal_error_handler)() */
#include "../../gostlib/ardp_int_err.c"	
#include "../../gostlib/ardp_perrno.c"
#include "../../gostlib/p_err_string.c"
#include "../../gostlib/usc_lic_str.c"
#endif
