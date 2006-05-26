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
	{ 0x63faba60, "__mod_timer" },
	{ 0x3de88ff0, "malloc_sizes" },
	{ 0x7d11c268, "jiffies" },
	{ 0x1b7d4074, "printk" },
	{ 0x2642591c, "kmem_cache_alloc" },
	{ 0x411593a2, "wake_up_process" },
	{ 0x37a0cba, "kfree" },
};

static const char __module_depends[]
__attribute_used__
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "15DC4B5C15AD041C867861E");
