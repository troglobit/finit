/* Setup random seed
 *
 * Copyright (c) 2012-2021  Joachim Wiberg <troglobit@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>		/* gettimeofday() */
#include <sys/types.h>
#include <lite/lite.h>

#include "config.h"
#include "finit.h"
#include "helpers.h"
#include "plugin.h"

static void setup(void *arg)
{
#ifdef RANDOMSEED
	if (!fexist(RANDOMSEED)) {
		int ret = 1;
		mode_t prev;
		FILE *fp;

		print_desc("Bootstrapping random seed", NULL);
		prev = umask(077);
		fp = fopen(RANDOMSEED, "w");
		if (fp) {
			int iter = 128;
			struct timeval tv;

			gettimeofday(&tv, NULL);
			srandom(tv.tv_sec % 3600);
			while (iter--) {
				uint32_t i, prng = random();

				for (i = 0; i < sizeof(prng); i++)
					fputc((prng >> (i * CHAR_BIT)) & UCHAR_MAX, fp);
			}
			ret = fclose(fp);
		}
		print_result(ret);
		umask(prev);
	}

	print_desc("Initializing random number generator", NULL);
	print_result(512 != copyfile(RANDOMSEED, "/dev/urandom", 0, 0));
#endif
}

static void save(void *arg)
{
#ifdef RANDOMSEED
	mode_t prev;

	prev = umask(077);

	print_desc("Saving random seed", NULL);
	print_result(512 != copyfile("/dev/urandom", RANDOMSEED, 512, 0));

	umask(prev);
#endif
}

static plugin_t plugin = {
	.name = __FILE__,
	.hook[HOOK_BASEFS_UP] = { .cb  = setup },
	.hook[HOOK_SHUTDOWN]  = { .cb  = save  },
	.depends = { "bootmisc", }
};

PLUGIN_INIT(plugin_init)
{
	plugin_register(&plugin);
}

PLUGIN_EXIT(plugin_exit)
{
	plugin_unregister(&plugin);
}

/**
 * Local Variables:
 *  indent-tabs-mode: t
 *  c-file-style: "linux"
 * End:
 */
