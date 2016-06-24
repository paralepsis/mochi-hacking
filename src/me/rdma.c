/* rdma.c -- test the RDMA data movement. Many comments stripped for better
 *           readability over startup.c.
 *
 * (C) 2016 The University of Chicago
 */

#include <abt.h>
#include <abt-snoozer.h>
#include <margo.h>
#include <assert.h>
#include <sys/time.h>

int init_mercury(char *addr,
		 int listen,
		 hg_class_t **class_p,
		 hg_context_t **context_p);
int init_argobots(int argc, char **argv, ABT_pool *pool_p);
int init_margo(hg_context_t *context, ABT_pool prog_pool,
	       ABT_pool handler_pool, margo_instance_id *mid_p);
int init_rpcs(hg_class_t *class, hg_id_t *rpcid_p);

void list_membership_ult(hg_handle_t h);

void finalize_mercury(hg_class_t *class, hg_context_t *context);
void finalize_argobots();
void finalize_margo(margo_instance_id mid);

MERCURY_GEN_PROC(list_membership_in_t, \
		 ((int32_t) (ct)) ((int32_t) (cookie)) ((hg_bulk_t) (b)))
MERCURY_GEN_PROC(list_membership_out_t, \
		 ((int32_t) (ct)) ((int32_t) (cookie)) ((int32_t) (more)))

DECLARE_MARGO_RPC_HANDLER(list_membership_ult)

typedef struct member {
    char addr[60];
} member_t;

member_t list[3] = { {"bmi+tcp://localhost:8000\0"},
		     {"bmi+tcp://localhost:8001\0"},
		     {"bmi+tcp://localhost:8002\0"}};

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
    list_membership_in_t in;
    list_membership_out_t out;

    void *v;
    hg_size_t vsz;

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
    vsz = 2*sizeof(member_t);
    v = malloc(vsz);
    assert(v);

    ret = margo_addr_lookup(mid, (argc > 1) ? argv[1] : localhost, &svr);
    assert(!ret);

    ret = HG_Create(context, svr, rpcid, &h);
    assert(!ret);

    /* NOTE: BUG IN MERCURY PRECLUDES USING WRITE_ONLY HERE */
    ret = HG_Bulk_create(class, 1, &v, &vsz, HG_BULK_READWRITE, &in.b);
    assert(ret == 0);

    in.ct = 2;
    in.cookie = 0;

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
     * NOTE: Need to append "_handler" on the handler function name to account
     *       for margo function wrapper.
     */
    *rpcid_p = MERCURY_REGISTER(class, "list_membership",
				list_membership_in_t,
				list_membership_out_t,
				list_membership_ult_handler);



    return 0;
}

/* list_membership_ult
 */
void list_membership_ult(hg_handle_t h)
{
    int ret;
    list_membership_in_t in;
    list_membership_out_t out;
    struct hg_info *hgi;
    margo_instance_id mid;
    hg_size_t sz;
    hg_bulk_t lbh; /* local bulk handle */

    ret = HG_Get_input(h, &in);
    assert(ret == HG_SUCCESS);

    printf("req: ct = %d, cookie = %d\n", in.ct, in.cookie);

    out.ct = 2;
    out.cookie = 0;
    out.more = 0;

    hgi = HG_Get_info(h);
    mid = margo_hg_class_to_instance(hgi->hg_class);

    sz = 2*sizeof(member_t);
    ret = HG_Bulk_create(hgi->hg_class, 1, (void **) &list, &sz,
			 HG_BULK_READ_ONLY, &lbh);
    assert(!ret);

    /*
      margo_bulk_transfer(margo_instance_id mid,
                          hg_bulk_op_t op,
			  hg_addr_t origin_addr,
			  hg_bulk_t origin_handle,
			  size_t origin_offset,
			  hg_bulk_t local_handle,
			  size_t local_offset,
			  size_t size)
    */
    ret = margo_bulk_transfer(mid, HG_BULK_PUSH, hgi->addr, in.b, 0, 
			      lbh, 0, sz);

    ret = margo_respond(mid, h, &out);
    assert(!ret);

    HG_Destroy(h);
    return;
}
DEFINE_MARGO_RPC_HANDLER(list_membership_ult)

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
