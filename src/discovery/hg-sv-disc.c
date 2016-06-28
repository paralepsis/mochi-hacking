#include <abt.h>
#include <abt-snoozer.h>
#include <margo.h>
#include <hg-sv-disc.h>
#include <assert.h>

static hg_sv_rec_t *rec_list=NULL;
static int rec_ct;
static int rec_list_sz = 10;

/* HG_svrec_init() -- server-local call to allocate space, etc.
 */
int HG_svrec_init()
{
    assert(!rec_list);
    rec_list = malloc(rec_list_sz*sizeof(hg_sv_rec_t));
    rec_ct = 0;

    return 0;
}

/* HG_svrec_add_ult() -- handler
 */
void HG_svrec_add_ult(hg_handle_t h) {
    int ret;
    sv_rec_remove_in_t in;
    sv_rec_remove_out_t out;
    struct hg_info *hgi;
    margo_instance_id mid;
    hg_size_t sz;
    hg_bulk_t lbh; /* local bulk handle */
    void *p;

    ret = HG_Get_input(h, &in);
    assert(ret == HG_SUCCESS);

    hgi = HG_Get_info(h);
    mid = margo_hg_class_to_instance(hgi->hg_class);

    assert(rec_list != NULL);
    assert(rec_ct < rec_list_sz);
    p = &rec_list[rec_ct];
    sz = sizeof(hg_sv_rec_t);
    ret = HG_Bulk_create(hgi->hg_class, 1, (void **) &p, &sz,
			 HG_BULK_READWRITE, &lbh);
    assert(ret == HG_SUCCESS);

    ret = margo_bulk_transfer(mid, HG_BULK_PULL, hgi->addr, in.b, 0,
			      lbh, 0, sz);
    assert(!ret);

    printf("  added %s\n", rec_list[rec_ct].addr);
    rec_ct++;

    out.ret = 0;
    ret = margo_respond(mid, h, &out);
    assert(!ret);

    ret = HG_Destroy(h);
    assert(ret == HG_SUCCESS);

    return;
}
DEFINE_MARGO_RPC_HANDLER(HG_svrec_add_ult)

/* HG_svrec_remove_ult() -- handler
 */
void HG_svrec_remove_ult() {

    return;
}
DEFINE_MARGO_RPC_HANDLER(HG_svrec_remove_ult)

/*
 * Local variables:
 *  mode: C
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 expandtab
 */

