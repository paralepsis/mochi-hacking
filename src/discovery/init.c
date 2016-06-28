/* rdma.c -- test the RDMA data movement. Many comments stripped for better
 *           readability over startup.c.
 *
 * (C) 2016 The University of Chicago
 */

#include <abt.h>
#include <abt-snoozer.h>
#include <margo.h>
#include <assert.h>
#include "init.h"

int init_mercury(char *addr,
		 int listen,
		 hg_class_t **class_p,
		 hg_context_t **context_p);
int init_argobots(int argc, char **argv, ABT_pool *pool_p);
int init_margo(hg_context_t *context, ABT_pool prog_pool,
	       ABT_pool handler_pool, margo_instance_id *mid_p);
void finalize_mercury(hg_class_t *class, hg_context_t *context);
void finalize_argobots();
void finalize_margo(margo_instance_id mid);


/********** INITIALIZE **********/

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

    ABT_info_print_all_xstreams(stdout);

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
