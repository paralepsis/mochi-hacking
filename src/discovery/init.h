
#include <abt.h>
#include <abt-snoozer.h>
#include <margo.h>
#include <assert.h>

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

/*
 * Local variables:
 *  mode: C
 *  c-indent-level: 4
 *  c-basic-offset: 4
 * End:
 *
 * vim: ts=8 sts=4 sw=4 expandtab
 */
