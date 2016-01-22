#define SAMPLE_DEFAULT_SERVER_PORT 4008
/* Can be overridden with arguments. */
/* myhostname() is a function in the ARDP library. */
#define SAMPLE_DEFAULT_SUPER_SERVER_HOST (myhostname())
/* Don't touch this stuff; this is needed by the program. */
extern void get_data(RREQ req, FILE *out);
