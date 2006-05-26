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
	{ 0x89b301d4, "param_get_int" },
	{ 0x98bd6f46, "param_set_int" },
	{ 0x3be17469, "driver_remove_file" },
	{ 0x97a40785, "pci_unregister_driver" },
	{ 0x2bc7e786, "driver_create_file" },
	{ 0xcabc9e14, "pci_register_driver" },
	{ 0x5ac606bb, "pci_restore_state" },
	{ 0x8e1432a2, "pci_set_power_state" },
	{ 0x8986a2f2, "pci_save_state" },
	{ 0x3e537332, "unregister_netdev" },
	{ 0x25328354, "free_ieee80211" },
	{ 0xc93c8b91, "pci_disable_device" },
	{ 0x2cd1eb5c, "pci_release_regions" },
	{ 0xedc03953, "iounmap" },
	{ 0x33bd90e0, "destroy_workqueue" },
	{ 0xf20dabd8, "free_irq" },
	{ 0xdb689e21, "sysfs_remove_group" },
	{ 0x11d3f792, "sysfs_create_group" },
	{ 0x26e96637, "request_irq" },
	{ 0x3762cb6e, "ioremap_nocache" },
	{ 0x43c9fc51, "pci_bus_write_config_byte" },
	{ 0x141a4196, "pci_request_regions" },
	{ 0x672518e2, "pci_set_consistent_dma_mask" },
	{ 0x224f6330, "pci_set_dma_mask" },
	{ 0x8eb56110, "pci_set_master" },
	{ 0xaba2e613, "pci_enable_device" },
	{ 0x22e381dd, "alloc_ieee80211" },
	{ 0xdb9e8e86, "pci_bus_read_config_word" },
	{ 0xa5808bbf, "tasklet_init" },
	{ 0x6d3dce6b, "__create_workqueue" },
	{ 0x44657b04, "__netdev_watchdog_up" },
	{ 0xed5c73bf, "__tasklet_schedule" },
	{ 0x329a1c7b, "iw_handler_get_thrspy" },
	{ 0x4ad1ae73, "iw_handler_set_thrspy" },
	{ 0xb0a1f841, "iw_handler_get_spy" },
	{ 0xbd5e8918, "iw_handler_set_spy" },
	{ 0x350da043, "ieee80211_wx_get_encode" },
	{ 0x2ce96f08, "ieee80211_wx_set_encode" },
	{ 0x861e8745, "ieee80211_wx_get_scan" },
	{ 0xbabae347, "ieee80211_freq_to_channel" },
	{ 0xd4dfc984, "ieee80211_rx" },
	{ 0xa49afb5, "ieee80211_rx_mgt" },
	{ 0xf6ebc03b, "net_ratelimit" },
	{ 0x484fef3f, "skb_over_panic" },
	{ 0x2da418b5, "copy_to_user" },
	{ 0xf2a644fb, "copy_from_user" },
	{ 0x98adfde2, "request_module" },
	{ 0x54fd19dd, "ieee80211_get_crypto_ops" },
	{ 0x3e182214, "ieee80211_crypt_delayed_deinit" },
	{ 0x2bc95bd4, "memset" },
	{ 0x79aa04a2, "get_random_bytes" },
	{ 0x490fa1e9, "ieee80211_channel_to_index" },
	{ 0xa9fb135f, "escape_essid" },
	{ 0x1b17ee7, "ieee80211_txb_free" },
	{ 0x33934162, "release_firmware" },
	{ 0xdb1111ff, "request_firmware" },
	{ 0xa7ca53f3, "pci_bus_read_config_byte" },
	{ 0xdcc8d932, "pci_bus_read_config_dword" },
	{ 0xf95cab08, "dma_alloc_coherent" },
	{ 0xd59d208f, "dma_free_coherent" },
	{ 0xc4e99917, "alloc_skb" },
	{ 0x5152e605, "memcmp" },
	{ 0x3a379b9, "ieee80211_is_valid_channel" },
	{ 0x3bad31ee, "ieee80211_set_geo" },
	{ 0x8738f45d, "linkwatch_fire_event" },
	{ 0xead1fe5d, "register_netdev" },
	{ 0xd21d196d, "ieee80211_tx_frame" },
	{ 0x51c47591, "mod_timer" },
	{ 0xb958656b, "ieee80211_get_channel_flags" },
	{ 0x91807d50, "ieee80211_get_channel" },
	{ 0x5a41c034, "wireless_send_event" },
	{ 0x7ac3e50f, "del_timer" },
	{ 0x7d11c268, "jiffies" },
	{ 0x859204af, "sscanf" },
	{ 0x1d3bfa11, "ieee80211_get_geo" },
	{ 0xd05e28f2, "queue_work" },
	{ 0x2642591c, "kmem_cache_alloc" },
	{ 0x3de88ff0, "malloc_sizes" },
	{ 0x20000329, "simple_strtoul" },
	{ 0x1d26aa98, "sprintf" },
	{ 0x60a4461c, "__up_wakeup" },
	{ 0x96b27088, "__down_failed" },
	{ 0x3e211f08, "queue_delayed_work" },
	{ 0xc9befb9c, "__kfree_skb" },
	{ 0xd8c152cd, "raise_softirq_irqoff" },
	{ 0x982ab0c8, "per_cpu__softnet_data" },
	{ 0x77414029, "__kmalloc" },
	{ 0x98e4e879, "finish_wait" },
	{ 0x17d59d01, "schedule_timeout" },
	{ 0x98d23e12, "prepare_to_wait" },
	{ 0x2e60bace, "memcpy" },
	{ 0x4e45624a, "autoremove_wake_function" },
	{ 0xbce89f77, "__wake_up" },
	{ 0x37a0cba, "kfree" },
	{ 0xeae3dfd6, "__const_udelay" },
	{ 0x1b7d4074, "printk" },
	{ 0x8d3894f2, "_ctype" },
	{ 0x25da070, "snprintf" },
};

static const char __module_depends[]
__attribute_used__
__attribute__((section(".modinfo"))) =
"depends=ieee80211,ieee80211_crypt,ieee80211,firmware_class";

MODULE_ALIAS("pci:v00008086d00004222sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d00004227sv*sd*bc*sc*i*");

MODULE_INFO(srcversion, "46B89197931462830745065");
