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
	{ 0x77414029, "__kmalloc" },
	{ 0x59e12e37, "ieee80211_crypt_quiescing" },
	{ 0xc9befb9c, "__kfree_skb" },
	{ 0x89b301d4, "param_get_int" },
	{ 0x7c50e156, "alloc_etherdev" },
	{ 0x7ac3e50f, "del_timer" },
	{ 0x3de88ff0, "malloc_sizes" },
	{ 0x934a6bc, "remove_proc_entry" },
	{ 0xd8c152cd, "raise_softirq_irqoff" },
	{ 0x98bd6f46, "param_set_int" },
	{ 0x1d26aa98, "sprintf" },
	{ 0x7d11c268, "jiffies" },
	{ 0x5b8b98f2, "netif_rx" },
	{ 0x2bc95bd4, "memset" },
	{ 0x726cd05, "proc_mkdir" },
	{ 0x50cd3878, "proc_net" },
	{ 0x1b7d4074, "printk" },
	{ 0x859204af, "sscanf" },
	{ 0x5152e605, "memcmp" },
	{ 0xc4e99917, "alloc_skb" },
	{ 0xd81c20a8, "free_netdev" },
	{ 0x279e265f, "ieee80211_crypt_deinit_handler" },
	{ 0x484fef3f, "skb_over_panic" },
	{ 0x2642591c, "kmem_cache_alloc" },
	{ 0xc5a4e281, "skb_under_panic" },
	{ 0xb6c89f26, "eth_type_trans" },
	{ 0x84d6a182, "create_proc_entry" },
	{ 0x411593a2, "wake_up_process" },
	{ 0x3e182214, "ieee80211_crypt_delayed_deinit" },
	{ 0x37a0cba, "kfree" },
	{ 0x98adfde2, "request_module" },
	{ 0x25da070, "snprintf" },
	{ 0x94b9661d, "ieee80211_crypt_deinit_entries" },
	{ 0xf2a644fb, "copy_from_user" },
	{ 0x54fd19dd, "ieee80211_get_crypto_ops" },
	{ 0x982ab0c8, "per_cpu__softnet_data" },
};

static const char __module_depends[]
__attribute_used__
__attribute__((section(".modinfo"))) =
"depends=ieee80211_crypt";


MODULE_INFO(srcversion, "2679B77B51A3FE8E217823C");
