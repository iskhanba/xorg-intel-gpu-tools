/*
 * Copyright © 2018 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "igt_psr.h"
#include "igt_sysfs.h"
#include <errno.h>

bool psr_active(int fd, bool check_active)
{
	bool active;
	char buf[512];

	igt_debugfs_read(fd, "i915_edp_psr_status", buf);
	active = strstr(buf, "HW Enabled & Active bit: yes\n") &&
		(strstr(buf, "SRDENT") || strstr(buf, "SLEEP"));
	return check_active ? active : !active;
}

bool psr_wait_entry(int fd)
{
	return igt_wait(psr_active(fd, true), 500, 1);
}

static ssize_t psr_write(int fd, const char *buf)
{
	return igt_sysfs_write(fd, "i915_edp_psr_debug", buf, strlen(buf));
}

static int has_psr_debugfs(int fd)
{
	int ret;

	/*
	 * Check if new PSR debugfs api is usable by writing an invalid value.
	 * Legacy mode will return OK here, debugfs api will return -EINVAL.
	 * -ENODEV is returned when PSR is unavailable.
	 */
	ret = psr_write(fd, "0xf");
	if (ret == -EINVAL)
		return 0;
	else if (ret < 0)
		return ret;

	/* legacy debugfs api, we enabled irqs by writing, disable them. */
	psr_write(fd, "0");
	return -EINVAL;
}

static bool psr_modparam_set(int val)
{
	static int oldval = -1;

	igt_set_module_param_int("enable_psr", val);

	if (val == oldval)
		return false;

	oldval = val;
	return true;
}

static int psr_restore_debugfs_fd = -1;

static void restore_psr_debugfs(int sig)
{
	psr_write(psr_restore_debugfs_fd, "0");
}

static bool psr_set(int fd, bool enable)
{
	int ret;

	ret = has_psr_debugfs(fd);
	if (ret == -ENODEV) {
		igt_skip_on_f(enable, "PSR not available\n");
		return false;
	}

	if (ret == -EINVAL) {
		ret = psr_modparam_set(enable);
	} else {
		ret = psr_write(fd, enable ? "0x3" : "0x1");
		igt_assert(ret > 0);
	}

	/* Restore original value on exit */
	if (psr_restore_debugfs_fd == -1) {
		psr_restore_debugfs_fd = dup(fd);
		igt_assert(psr_restore_debugfs_fd >= 0);
		igt_install_exit_handler(restore_psr_debugfs);
	}

	return ret;
}

bool psr_enable(int fd)
{
	return psr_set(fd, true);
}

bool psr_disable(int fd)
{
	return psr_set(fd, false);
}
