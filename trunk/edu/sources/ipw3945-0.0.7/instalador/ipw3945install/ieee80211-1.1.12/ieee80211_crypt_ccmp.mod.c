#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

#undef unix
struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = __stringify(KBUILD_MODNAME),
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
};

static const struct modversion_info ____versions[]
__attribute_used__
__attribute__((section("__versions"))) = {
	{ 0x2720b98e, "struct_module" },
	{ 0x906f19cd, "ieee80211_unregister_crypto_ops" },
	{ 0xb6a90b48, "ieee80211_register_crypto_ops" },
	{ 0x1d26aa98, "sprintf" },
	{ 0x99a4a69e, "___pskb_trim" },
	{ 0x5152e605, "memcmp" },
	{ 0xf6ebc03b, "net_ratelimit" },
	{ 0x484fef3f, "skb_over_panic" },
	{ 0x8235805b, "memmove" },
	{ 0xc5a4e281, "skb_under_panic" },
	{ 0x37a0cba, "kfree" },
	{ 0x5d2783c1, "crypto_free_tfm" },
	{ 0x1b7d4074, "printk" },
	{ 0x4006d157, "crypto_alloc_tfm" },
	{ 0x2642591c, "kmem_cache_alloc" },
	{ 0x3de88ff0, "malloc_sizes" },
	{ 0x6fbdb419, "mem_map" },
};

static const char __module_depends[]
__attribute_used__
__attribute__((section(".modinfo"))) =
"depends=ieee80211_crypt";


MODULE_INFO(srcversion, "B7C96B2B654B8FD4FA1AEEE");
