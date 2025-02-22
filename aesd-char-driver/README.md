# AESD Char Driver

Template source code for the AESD char driver used with assignments 8 and later

# Implement a memory leak on purpose and test:
cd /lib/modules/6.1.44/extra/
mount -t debugfs nodev /sys/kernel/debug/
echo clear > /sys/kernel/debug/kmemleak
./aesdchar_load
Loading local built file aesdchar.ko
echo "Lior" > /dev/aesdchar 
./aesdchar_unload
echo scan > /sys/kernel/debug/kmemleak
cat /sys/kernel/debug/kmemleak
echo scan > /sys/kernel/debug/kmemleak
     kmemleak: 1 new suspected memory leaks (see /sys/kernel/debug/kmemleak)
cat /sys/kernel/debug/kmemleak
unreferenced object 0xffffff80029cef00 (size 128):
  comm "sh", pid 157, jiffies 4294916664 (age 60.492s)
  hex dump (first 32 bytes):
    4c 69 6f 72 0a 00 00 00 10 00 00 00 01 00 01 00  Lior............
    00 38 69 42 00 00 00 00 00 04 00 00 01 00 02 00  .8iB............
  backtrace:
    [<000000004b01d085>] __kmem_cache_alloc_node+0x2a8/0x400
    [<000000009671a9cc>] __kmalloc+0x54/0x90
    [<00000000a6bae6da>] 0xffffffc0007a1664
    [<000000006d0b3e1b>] vfs_write+0xc8/0x390
    [<0000000088b60643>] ksys_write+0x74/0x110
    [<0000000085354e69>] __arm64_sys_write+0x1c/0x30
    [<0000000016ea1317>] invoke_syscall+0x54/0x130
    [<00000000452e305d>] el0_svc_common.constprop.0+0x44/0xf0
    [<000000008e1f08a2>] do_el0_svc+0x2c/0xc0
    [<00000000b3a763d1>] el0_svc+0x2c/0x90
    [<000000005022f6de>] el0t_64_sync_handler+0xf4/0x120
    [<0000000050bcc77c>] el0t_64_sync+0x18c/0x190
