#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x1e94b2a0, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0xc996d097, __VMLINUX_SYMBOL_STR(del_timer) },
	{ 0x56b07c90, __VMLINUX_SYMBOL_STR(proc_remove) },
	{ 0x593a99b, __VMLINUX_SYMBOL_STR(init_timer_key) },
	{ 0x7d11c268, __VMLINUX_SYMBOL_STR(jiffies) },
	{ 0x70cb5d65, __VMLINUX_SYMBOL_STR(proc_create_data) },
	{ 0xd34e90c6, __VMLINUX_SYMBOL_STR(proc_mkdir) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x83c15249, __VMLINUX_SYMBOL_STR(kmem_cache_create) },
	{ 0x219428ce, __VMLINUX_SYMBOL_STR(pid_task) },
	{ 0xf4aacda7, __VMLINUX_SYMBOL_STR(find_vpid) },
	{ 0x4f8b5ddb, __VMLINUX_SYMBOL_STR(_copy_to_user) },
	{ 0x28318305, __VMLINUX_SYMBOL_STR(snprintf) },
	{ 0x754d539c, __VMLINUX_SYMBOL_STR(strlen) },
	{ 0x91715312, __VMLINUX_SYMBOL_STR(sprintf) },
	{ 0xda13ea39, __VMLINUX_SYMBOL_STR(kmem_cache_alloc) },
	{ 0x20c55ae0, __VMLINUX_SYMBOL_STR(sscanf) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0xd7804f5e, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0xc6a0bd9, __VMLINUX_SYMBOL_STR(mutex_lock_interruptible) },
	{ 0x3d1f7a21, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0xda22cdde, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0x2276db98, __VMLINUX_SYMBOL_STR(kstrtoint) },
	{ 0x4f6b400b, __VMLINUX_SYMBOL_STR(_copy_from_user) },
	{ 0xd2b09ce5, __VMLINUX_SYMBOL_STR(__kmalloc) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "1B5D99EF5E79E7F10F8EABD");
