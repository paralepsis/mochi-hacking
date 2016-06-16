/*
 *
 */

#include <abt.h>
#include <abt-snoozer.h>
#include <margo.h>
#include <assert.h>
#include <sys/time.h>

/*
  MERCURY_GEN_PROC: Macro used to define simple payloads and the
  associated encode/decode functions.

  HG RPCs have exactly one input struct and one output struct.
  MERCURY_GEN_PROC can generate handlers, etc., assuming that you only
  use types that Mercury understands. It will not compile if it
  doesn't understand a type.

  MERCURY_GEN_PROC also typedefs the struct and declares the functions
  for processing. These are then used by HG_Get_input/output(), which we
  believe uses the handle to figure out which of these functions to
  call.
*/
MERCURY_GEN_PROC(rtt_in_t,((int64_t) (sec)) ((int32_t) (usec)))
MERCURY_GEN_PROC(rtt_out_t, ((int64_t) (sec)) ((int32_t) (usec)))

/*
 DECLARE_MARGO_RPC_HANDLER(fn): Defines a function that wraps "fn",
 called "fn_handler", to be called by HG as the callback. This does the
 spawn of the ULT, etc. It's up to the programmer to pass the _handler
 version to the register function (below).
*/
DECLARE_MARGO_RPC_HANDLER(rtt_ult)

int init_mercury(char *addr,
		 int listen,
		 hg_class_t **class_p,
		 hg_context_t **context_p);
int init_argobots(int argc, char **argv, ABT_pool *pool_p);
int init_margo(hg_context_t *context, ABT_pool prog_pool,
	       ABT_pool handler_pool, margo_instance_id *mid_p);
int init_rpcs(hg_class_t *class, hg_id_t *rpcid_p);

void finalize_mercury(hg_class_t *class, hg_context_t *context);
void finalize_argobots();
void finalize_margo(margo_instance_id mid);

int main(int argc, char **argv)
{
    int ret;
    hg_class_t *class;
    hg_context_t *context;
    ABT_pool pool;
    margo_instance_id mid;
    char localhost[] = "bmi+tcp://localhost:9000\0";

    hg_id_t rpcid;
    hg_handle_t h;
    hg_addr_t svr;
    rtt_in_t in;
    rtt_out_t out;
    struct timeval t;

    /*** INITIALIZE ***/

    ret = init_mercury((argc > 1) ? argv[1] : localhost, 1, &class, &context);
    assert(!ret);

    ret = init_argobots(argc, argv, &pool);
    assert(!ret);

    /* Note: Passing same pool twice, once for progress and once for 
     * handler.
     */
    ret = init_margo(context, pool, pool, &mid);
    assert(!ret);

    ret = init_rpcs(class, &rpcid);
    assert(!ret);

    /*** DO FUN STUFF ***/

    ret = margo_addr_lookup(mid, (argc > 1) ? argv[1] : localhost, &svr);
    assert(!ret);

    ret = HG_Create(context, svr, rpcid, &h);
    assert(!ret);

    ret = gettimeofday(&t, NULL);
    assert(!ret);
    in.sec  = (int64_t) t.tv_sec;
    in.usec = (int32_t) t.tv_usec;
    ret = margo_forward(mid, h, &in);
    assert(!ret);

    ret = HG_Get_output(h, &out);
    assert(!ret);

    /*** FINALIZE ***/

    finalize_margo(mid);

    /* TODO: xstream join/free? would it be better to do in
     * finalize_argobots()?
     */

    finalize_argobots();
    finalize_mercury(class, context);

    return 0;
}

/********** INITIALIZE **********/

/* init_rpcs()
 */
int init_rpcs(hg_class_t *class, hg_id_t *rpcid_p) 
{
    /* TODO: What assumptions are made about RPC initialization on both sides?
     *
     * TODO: What's the NULL in MERCURY_REGISTER?
     */
    *rpcid_p = MERCURY_REGISTER(class, "rtt", rtt_in_t, rtt_out_t, NULL);

    return 0;
}

/* init_mercury()
 *
 * Returns 0 on success, -1 on failure.
 */
int init_mercury(char *addr,
		 int listen,
		 hg_class_t **class_p,
		 hg_context_t **context_p)
{
    hg_addr_t self_addr;
    char addr_buf[64];
    hg_size_t bufsz=63;

    /* NOTE: HG_Init chooses your NA (underlying network module) */
    *class_p = HG_Init(addr, listen);
    if (*class_p == NULL) return -1;

    /* [ From John, 06/14/2016 ]
       A context is akin to a queue for RPCs and bulk transfers (and
       their callbacks) whereas a class is a handle for the network
       resource, but we've never used more than one, and the network
       progression model is actually bound to contexts rather than
       classes.
    */
    *context_p = HG_Context_create(*class_p);
    if (*context_p == NULL) return -1;

    printf("Mercury Initialization:\n");
    printf("  HG Class: %s\n", HG_Class_get_name(*class_p));
    printf("  HG Protocol: %s\n", HG_Class_get_protocol(*class_p));

    if (listen) {
	/* NOTE: These functions won't return valid results if not listening. */

	HG_Addr_self(*class_p, &self_addr);
	HG_Addr_to_string(*class_p, addr_buf, &bufsz, self_addr);
	printf("  HG Self Address (listening): %s\n", addr_buf);
    }

    return 0;
}

/* init_argobots(argc, argv, pool_p)
 *
 * Notes:
 * - According to Sangmin (6/14/2016) Argobots ignores the argc 
 *   and argv parameters to ABT_init() at this time.
 *
 * Returns 0 on success, non-zero on failure.
 */
int init_argobots(int argc, char **argv, ABT_pool *pool_p)
{
    int ret;
    ABT_xstream xstream;

    ret = ABT_init(argc, argv);
    if (ret != 0) {
	fprintf(stderr, "ABT_init: ?\n");
	fflush(stderr);
	return ret;
    }

    /* [ From Phil ]
       An ES (execution stream, or xstream), is an OS-level thread or
       process that is capable of running user level threads or
       tasklets. The "primary ES" is the implicit main() thread that
       you have as soon as execution starts.

       Setting an ES to "idle without polling" means activating the
       abt-snoozer scheduler for that ES so that when that ES runs out
       of execution work to do (either because there are no threads to
       run, or because all of the threads are blocked on I/O) it will
       sleep gracefully rather than just busy while() loop looking for
       work.

       It has its own explicit API function because it is kind of a
       special case to find and identify the main() thread.
    */
    ret = ABT_snoozer_xstream_self_set();
    if (ret != 0) {
	fprintf(stderr, "ABT_snoozer_xstream_self_set: ?\n");
	fflush(stderr);
	return ret;
    }

    ret = ABT_xstream_self(&xstream);
    if (ret != 0) {
	fprintf(stderr, "ABT_xstream_self: ?\n");
	fflush(stderr);
	return ret;
    }

    ret = ABT_xstream_get_main_pools(xstream, 1, pool_p);
    if (ret != 0) {
	fprintf(stderr, "ABT_xstream_get_main_pools: ?\n");
	fflush(stderr);
	return ret;
    }

    /* TODO: What diagnostics are available from argobots? */
    printf("Argobots Initialization:\n");
    ABT_info_print_config(stdout);

#if 0
    ABT_info_print_all_xstreams(stdout);
#endif

    return 0;
}

/* init_margo(context, pool, mid)
 *
 * Note:
 * - Using the margo_init_pool() version rather than the higher-level 
 *   margo_init() function in order to force myself to understand what is
 *   going on in Argobots. Notionally I don't have to see the details.
 */
int init_margo(hg_context_t *context, ABT_pool prog_pool, ABT_pool handler_pool,
	       margo_instance_id *mid_p)
{
    /*
      First parameter is a progress pool, second a "handler" pool. The second
      pool is unnecessary if one isn't going to receive incoming RPCs, and
      ABT_POOL_NULL can be passed in in that case.
     */
    *mid_p = margo_init_pool(prog_pool, handler_pool, context);
    if (*mid_p == MARGO_INSTANCE_NULL) return -1;

    printf("Margo Initialization:\n  <done>\n");

    return 0;
}

/********** FINALIZE **********/

/* finalize margo()
 */
void finalize_margo(margo_instance_id mid)
{
    /* TODO: ADD MID PARAMETER! */
    margo_finalize(mid);
}

/* finalize_argobots()
 */
void finalize_argobots()
{
    ABT_finalize();
}

/* finalize_mercury(class, context)
 */
void finalize_mercury(hg_class_t *class, hg_context_t *context)
{
    HG_Context_destroy(context);
    HG_Finalize(class);
}

/*
 * Local variables:
 *  mode: C
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 expandtab
 */
