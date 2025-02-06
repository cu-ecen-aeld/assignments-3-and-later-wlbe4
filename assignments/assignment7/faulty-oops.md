## Kernel oops
A kernel oops is an error that occurs when the Linux kernel encounters an unexpected condition that it cannot handle. In this case, the oops is caused by a write operation to the /dev/faulty device. 
Writing into this faulty device generates the below dump:
```
# echo "wlbe4" > /dev/faulty 
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x0000000096000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=0000000041c11000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 0000000096000045 [#1] SMP
Modules linked in: faulty(O) hello(O) scull(O)
CPU: 0 PID: 160 Comm: sh Tainted: G           O       6.1.44 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x10/0x20 [faulty]
lr : vfs_write+0xc8/0x390
sp : ffffffc008e13d20
x29: ffffffc008e13d80 x28: ffffff8001b30d40 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 0000000000000006 x22: 0000000000000006 x21: ffffffc008e13dc0
x20: 000000558a0d5180 x19: ffffff8001b5a600 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc00078c000 x3 : ffffffc008e13dc0
x2 : 0000000000000006 x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
 faulty_write+0x10/0x20 [faulty]
 ksys_write+0x74/0x110
 __arm64_sys_write+0x1c/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x2c/0xc0
 el0_svc+0x2c/0x90
 el0t_64_sync_handler+0xf4/0x120
 el0t_64_sync+0x18c/0x190
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace 0000000000000000 ]---
```

# Error Description:
The kernel encountered a NULL pointer dereference at virtual address 0000000000000000.
Memory Abort Information:

ESR (Exception Syndrome Register): 0x0000000096000045
EC (Exception Class): 0x25 (Data Abort, current EL)
FSC (Fault Status Code): 0x05 (level 1 translation fault)
Data Abort Information:

ISS (Instruction Specific Syndrome): 0x00000045
WnR (Write not Read): 1 (indicates a write operation)

# Call Trace:
The error occurred in the faulty_write function within the faulty module.
The call trace shows the sequence of function calls leading to the error:
faulty_write+0x10/0x20 [faulty]
ksys_write+0x74/0x110
__arm64_sys_write+0x1c/0x30
invoke_syscall+0x54/0x130
el0_svc_common.constprop.0+0x44/0xf0
do_el0_svc+0x2c/0xc0
el0_svc+0x2c/0x90
el0t_64_sync_handler+0xf4/0x120
el0t_64_sync+0x18c/0x190

# Registers:
The register values at the time of the error are provided, showing that several registers (x0, x1, etc.) contain 0x0000000000000000, indicating NULL values.
Code:

The code at the faulting instruction is shown, with the instruction causing the fault being b900003f.

# Summary
The kernel oops was triggered by a NULL pointer dereference during a write operation to the /dev/faulty device. This indicates a bug in the faulty module, where it attempted to access a NULL pointer, leading to a crash. The call trace and register values provide additional context for debugging the issue.