/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (C) 2015-2018 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#include "version.h"
#include "device.h"
#include "noise.h"
#include "queueing.h"
#include "ratelimiter.h"
#include "netlink.h"
#include "uapi/wireguard.h"
#include "crypto/zinc.h"

#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/genetlink.h>
#include <net/rtnetlink.h>

static int __init mod_init(void)
{
	int ret;

	if ((ret = chacha20_mod_init()) || (ret = poly1305_mod_init()) ||
	    (ret = chacha20poly1305_mod_init()) || (ret = blake2s_mod_init()) ||
	    (ret = curve25519_mod_init()))
		return ret;

#ifdef DEBUG
	if (!allowedips_selftest() || !packet_counter_selftest() ||
	    !ratelimiter_selftest())
		return -ENOTRECOVERABLE;
#endif
	noise_init();

	ret = device_init();
	if (ret < 0)
		goto err_device;

	ret = genetlink_init();
	if (ret < 0)
		goto err_netlink;

	pr_info("WireGuard " WIREGUARD_VERSION " loaded. See www.wireguard.com for information.\n");
	pr_info("Copyright (C) 2015-2018 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.\n");

	return 0;

err_netlink:
	device_uninit();
err_device:
	return ret;
}

static void __exit mod_exit(void)
{
	genetlink_uninit();
	device_uninit();
	pr_debug("WireGuard unloaded\n");
}

module_init(mod_init);
module_exit(mod_exit);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Fast, modern, and secure VPN tunnel");
MODULE_AUTHOR("Jason A. Donenfeld <Jason@zx2c4.com>");
MODULE_VERSION(WIREGUARD_VERSION);
MODULE_ALIAS_RTNL_LINK(KBUILD_MODNAME);
MODULE_ALIAS_GENL_FAMILY(WG_GENL_NAME);
