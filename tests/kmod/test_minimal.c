/* Minimal freestanding .ko test — only printk */
#include <ktypes.h>

extern int _printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#define KERN_ERR "\001" "3"

static int __attribute__((section(".init.text"))) test_min_init(void)
{
    _printk(KERN_ERR "MINIMAL_TEST: init called\n");
    return 0;
}

static void __attribute__((section(".exit.text"))) test_min_exit(void)
{
    _printk(KERN_ERR "MINIMAL_TEST: exit called\n");
}

/* Aliases */
int init_module(void) __attribute__((alias("test_min_init")));
void cleanup_module(void) __attribute__((alias("test_min_exit")));

/* modinfo */
#define __PASTE2(a, b) a##b
#define __PASTE(a, b) __PASTE2(a, b)
#define __UNIQUE_ID(prefix) __PASTE(__PASTE(__unique_, prefix), __COUNTER__)
#define __MODULE_INFO(tag, name, info) \
    static const char __UNIQUE_ID(name)[] \
        __attribute__((used, section(".modinfo"), aligned(1))) = #tag "=" info

__MODULE_INFO(license, license, "GPL");
__MODULE_INFO(vermagic, vermagic, VERMAGIC_STRING);
__MODULE_INFO(name, modulename, "test_min");

/* __versions */
struct modversion_info { unsigned int crc; unsigned int pad; char name[56]; };
static const struct modversion_info __modver_layout
    __attribute__((used, section("__versions"), aligned(8))) = {
        .crc = 0xea759d7f, .name = "module_layout"
    };
static const struct modversion_info __modver_printk
    __attribute__((used, section("__versions"), aligned(8))) = {
        .crc = 0x92997ed8, .name = "_printk"
    };

/* __this_module with init/exit at correct offsets */
struct module {
    char __pre[24];
    char name[56];
    char __pad1[0x170 - 24 - 56];
    int (*init)(void);
    char __pad2[0x3d8 - 0x170 - 8];
    void (*exit)(void);
    char __pad3[0x440 - 0x3d8 - 8];
};

struct module __this_module
    __attribute__((used, aligned(64), section(".gnu.linkonce.this_module"))) = {
        .name = "test_min",
        .init = init_module,
        .exit = cleanup_module,
    };
