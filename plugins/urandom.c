/* Setup and save random seed at boot/shutdown
 *
 * Copyright (c) 2012-2024  Joachim Wiberg <troglobit@gmail.com>
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
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>		/* gettimeofday() */
#include <sys/types.h>
#ifdef _LIBITE_LITE
# include <libite/lite.h>
#else
# include <lite/lite.h>
#endif
#include <linux/random.h>

#include "config.h"
#include "finit.h"
#include "helpers.h"
#include "plugin.h"

#ifndef RANDOM_BYTES
#define RANDOM_BYTES (32*1024)
#endif

#ifdef RANDOMSEED
static void fallback(FILE *fp)
{
	struct timeval tv;
	int iter = 128;

	gettimeofday(&tv, NULL);
	srandom(tv.tv_sec % 3600);
	while (iter--) {
		uint32_t i, prng = random();

		for (i = 0; i < sizeof(prng); i++)
			fputc((prng >> (i * CHAR_BIT)) & UCHAR_MAX, fp);
	}
}
#endif

static void setup(void *arg)
{
#ifdef RANDOMSEED
	struct rand_pool_info *rpi;
	ssize_t len = 0;
	struct stat st;
	int rc = -1;
	int fd, err;

	if (rescue) {
		dbg("Skipping %s plugin in rescue mode.", __FILE__);
		return;
	}

	if (stat(RANDOMSEED, &st) || st.st_size < 512) {
		int ret = 1;
		mode_t prev;
		FILE *fp;

		print_desc("Bootstrapping random seed", NULL);
		prev = umask(077);
		fp = fopen(RANDOMSEED, "w");
		if (fp) {
			const char *hwrng = "/dev/hwrng";
			FILE *hw;

			hw = fopen(hwrng, "r");
			if (hw) {
				char buf[512];
				size_t len;

				len = fread(buf, sizeof(buf[0]), sizeof(buf), hw);
				if (len == 0)
					fallback(fp);
				else
					len = fwrite(buf, sizeof(buf[0]), len, fp);
				fclose(hw);
			} else {
				fallback(fp);
			}
			ret = fclose(fp);
		}
		print_result(ret);
		umask(prev);
	}

	print_desc("Seeding random number generator", NULL);

	/*
	 * Simply copying our saved entropy to /dev/urandom doesn't
	 * increment the kernel "entropy count", instead we must use
	 * the ioctl below.
	 */
	fd = open(RANDOMSEED, O_RDONLY);
	if (fd < 0)
		goto fallback;

	rpi = malloc(sizeof(*rpi) + RANDOM_BYTES);
	if (!rpi) {
		close(fd);
		goto fallback;
	}

	do {
		ssize_t num;

		num = read(fd, &rpi->buf[len], RANDOM_BYTES - len);
		if (num <= 0) {
			if (num == -1 && errno == EINTR)
				continue;
			if (len > 0)
				break;

			close(fd);
			free(rpi);
			goto fallback;
		}

		len += num;
	} while (len < RANDOM_BYTES);
	close(fd);

	fd = open("/dev/urandom", O_WRONLY);
	if (fd < 0) {
		free(rpi);
		goto fallback;
	}

	rpi->buf_size = len;
	rpi->entropy_count = len * 8;
	rc = ioctl(fd, RNDADDENTROPY, rpi);
	err = errno;
	close(fd);
	free(rpi);
	if (rc < 0)
		logit(LOG_ERR, "Failed adding entropy to kernel random pool: %s", strerror(err));
	print_result(rc < 0);
	return;
fallback:
	print_result(512 > copyfile(RANDOMSEED, "/dev/urandom", 0, 0));
#endif
}

static void save(void *arg)
{
#ifdef RANDOMSEED
	mode_t prev;

	if (rescue) {
		dbg("Skipping %s plugin in rescue mode.", __FILE__);
		return;
	}

	prev = umask(077);

	print_desc("Saving random seed", NULL);
	print_result(RANDOM_BYTES != copyfile("/dev/urandom", RANDOMSEED, RANDOM_BYTES, 0));

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
