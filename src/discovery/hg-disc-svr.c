#include <abt.h>
#include <abt-snoozer.h>
#include <margo.h>
#include <hg-sv-disc.h>
#include <assert.h>
#include "init.h"

int init_rpcs(hg_class_t *class, hg_id_t *rpcid_p);

int main(int argc, char **argv)
{
    int ret;
    hg_class_t *class;
    hg_context_t *context;
    ABT_pool pool;
    margo_instance_id mid;
    hg_id_t rpcid;
    char localhost[] = "bmi+tcp://localhost:9000\0";

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

    /*** LOOP FOREVER ***/
    margo_wait_for_finalize(mid);

    /*** FINALIZE ***/
    finalize_margo(mid);
    finalize_argobots();
    finalize_mercury(class, context);

    return 0;
}

int init_rpcs(hg_class_t *class, hg_id_t *rpcid_p)
{
    *rpcid_p = MERCURY_REGISTER(class, "HG_svrec_add_ult",
				sv_rec_add_in_t,
				sv_rec_add_out_t,
				HG_svrec_add_ult_handler);

    return 0;
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
