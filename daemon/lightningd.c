#include "configdir.h"
#include "lightningd.h"
#include "log.h"
#include <ccan/err/err.h>
#include <ccan/opt/opt.h>
#include <ccan/tal/str/str.h>
#include <ccan/tal/tal.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <version.h>

static struct lightningd_state *lightningd_state(void)
{
	struct lightningd_state *state = tal(NULL, struct lightningd_state);

	state->log_record = new_log_record(state, 20 * 1024 * 1024, LOG_INFORM);
	state->base_log = new_log(state, state->log_record,
				  "lightningd(%u):", (int)getpid());

	return state;
}

/* Tal wrappers for opt. */
static void *opt_allocfn(size_t size)
{
	return tal_alloc_(NULL, size, false, TAL_LABEL("opt_allocfn", ""));
}

static void *tal_reallocfn(void *ptr, size_t size)
{
	if (!ptr)
		return opt_allocfn(size);
	tal_resize_(&ptr, 1, size, false);
	return ptr;
}

static void tal_freefn(void *ptr)
{
	tal_free(ptr);
}

int main(int argc, char *argv[])
{
	struct lightningd_state *state = lightningd_state();

	err_set_progname(argv[0]);
	opt_set_alloc(opt_allocfn, tal_reallocfn, tal_freefn);

	opt_register_noarg("--help|-h", opt_usage_and_exit,
			   "\n"
			   "A bitcoin lightning daemon.",
			   "Print this message.");
	opt_register_logging(state->base_log);
	opt_register_version();

	configdir_register_opts(state,
				&state->config_dir, &state->rpc_filename);

	/* Get any configdir options first. */
	opt_early_parse(argc, argv, opt_log_stderr_exit);

	/* Move to config dir, to save ourselves the hassle of path manip. */
	if (chdir(state->config_dir) != 0) {
		log_unusual(state->base_log, "Creating lightningd dir %s"
			    " (because chdir gave %s)",
			    state->config_dir, strerror(errno));
		if (mkdir(state->config_dir, 0700) != 0)
			fatal("Could not make directory %s: %s",
			      state->config_dir, strerror(errno));
		if (chdir(state->config_dir) != 0)
			fatal("Could not change directory %s: %s",
			      state->config_dir, strerror(errno));
	}
	/* Activate crash log now we're in the right place. */
	crashlog_activate(state->base_log);

	/* Now look for config file */
	opt_parse_from_config(state);

	opt_parse(&argc, argv, opt_log_stderr_exit);
	if (argc != 1)
		errx(1, "no arguments accepted");

	log_info(state->base_log, "Hello world!");
	tal_free(state);
	opt_free_table();
	return 0;
}
