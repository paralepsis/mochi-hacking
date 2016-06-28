/* XYZ
 *
 * (C) 2016 University of Chicago
 */
#include <abt.h>
#include <abt-snoozer.h>
#include <margo.h>

typedef struct hg_sv_rec {
    char addr[60];
} hg_sv_rec_t;

/* add */
MERCURY_GEN_PROC(sv_rec_add_in_t, \
                 ((hg_bulk_t) (b)))
MERCURY_GEN_PROC(sv_rec_add_out_t, \
		 ((int32_t) (ret)))
DECLARE_MARGO_RPC_HANDLER(HG_svrec_add_ult)

/* remove */
MERCURY_GEN_PROC(sv_rec_remove_in_t, \
                 ((hg_bulk_t) (b)))
MERCURY_GEN_PROC(sv_rec_remove_out_t, \
		 ((int32_t) (ret)))

/* list */
MERCURY_GEN_PROC(sv_rec_list_in_t, \
                 ((int32_t) (ct)) ((int32_t) (cookie)) ((hg_bulk_t) (b)))

MERCURY_GEN_PROC(sv_rec_list_out_t, \
                 ((int32_t) (ct)) ((int32_t) (cookie)) ((int8_t) (more)))


/*
 * Local variables:
 *  mode: C
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 expandtab
 */
