/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2026 bmax121.
 *
 * Remote process hooking via ptrace — Linux ARM64 implementation.
 */

#ifdef __USERSPACE__
#ifdef __linux__

#include <remote_hook.h>

#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <linux/elf.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* ARM64 Linux syscall numbers */
#define SYS_MMAP 222
#define SYS_MPROTECT 226

/* ARM64 instruction: SVC #0 */
#define INST_SVC0 0xD4000001u
/* ARM64 instruction: BRK #0 (for breakpoint) */
#define INST_BRK0 0xD4200000u

struct kh_remote_hook_handle {
    int pid;
    struct user_pt_regs saved_regs;
};

/* Read all general-purpose registers from the tracee. */
static int read_regs(int pid, struct user_pt_regs *regs)
{
    struct iovec iov = { .iov_base = regs, .iov_len = sizeof(*regs) };
    if (ptrace(PTRACE_GETREGSET, pid, (void *)NT_PRSTATUS, &iov) < 0)
        return -errno;
    return 0;
}

/* Write all general-purpose registers to the tracee. */
static int write_regs(int pid, const struct user_pt_regs *regs)
{
    struct iovec iov = { .iov_base = (void *)regs, .iov_len = sizeof(*regs) };
    if (ptrace(PTRACE_SETREGSET, pid, (void *)NT_PRSTATUS, &iov) < 0)
        return -errno;
    return 0;
}

/*
 * Inject a syscall into the stopped tracee and return its result.
 *
 * Strategy:
 *   1. Save current registers and the instruction at PC.
 *   2. Set x8 = syscall number, x0..x5 = args, write SVC #0 at PC.
 *   3. Single-step the tracee so it executes the SVC.
 *   4. Read x0 (syscall return value).
 *   5. Restore the original instruction and registers.
 */
static int64_t inject_syscall(struct kh_remote_hook_handle *h, uint64_t nr,
                              uint64_t a0, uint64_t a1, uint64_t a2,
                              uint64_t a3, uint64_t a4, uint64_t a5)
{
    struct user_pt_regs cur_regs, mod_regs;
    int ret;

    /* 1. Read current state */
    ret = read_regs(h->pid, &cur_regs);
    if (ret < 0)
        return ret;

    /* Save the original instruction word at PC */
    errno = 0;
    long orig_inst = ptrace(PTRACE_PEEKTEXT, h->pid, (void *)cur_regs.pc, NULL);
    if (orig_inst == -1 && errno != 0)
        return -errno;

    /* 2. Set up syscall registers */
    mod_regs = cur_regs;
    mod_regs.regs[8] = nr;
    mod_regs.regs[0] = a0;
    mod_regs.regs[1] = a1;
    mod_regs.regs[2] = a2;
    mod_regs.regs[3] = a3;
    mod_regs.regs[4] = a4;
    mod_regs.regs[5] = a5;

    ret = write_regs(h->pid, &mod_regs);
    if (ret < 0)
        return ret;

    /* Write SVC #0 at the current PC */
    uint32_t svc_inst = INST_SVC0;
    if (ptrace(PTRACE_POKETEXT, h->pid, (void *)cur_regs.pc, (void *)(long)svc_inst) < 0)
        return -errno;

    /* 3. Single-step to execute the syscall */
    if (ptrace(PTRACE_SINGLESTEP, h->pid, NULL, NULL) < 0)
        return -errno;

    int status;
    if (waitpid(h->pid, &status, 0) < 0)
        return -errno;

    if (!WIFSTOPPED(status))
        return -ESRCH;

    /* 4. Read the result from x0 */
    struct user_pt_regs result_regs;
    ret = read_regs(h->pid, &result_regs);
    if (ret < 0)
        return ret;

    int64_t syscall_ret = (int64_t)result_regs.regs[0];

    /* 5. Restore original instruction and registers */
    if (ptrace(PTRACE_POKETEXT, h->pid, (void *)cur_regs.pc, (void *)orig_inst) < 0)
        return -errno;

    ret = write_regs(h->pid, &cur_regs);
    if (ret < 0)
        return ret;

    return syscall_ret;
}

kh_remote_hook_handle_t kh_remote_hook_attach(int pid)
{
    if (pid <= 0)
        return NULL;

    if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) < 0)
        return NULL;

    int status;
    if (waitpid(pid, &status, 0) < 0) {
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        return NULL;
    }

    if (!WIFSTOPPED(status)) {
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        return NULL;
    }

    struct kh_remote_hook_handle *h = malloc(sizeof(*h));
    if (!h) {
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        return NULL;
    }

    h->pid = pid;

    /* Save register state for restore on detach */
    if (read_regs(pid, &h->saved_regs) < 0) {
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        free(h);
        return NULL;
    }

    return h;
}

int kh_remote_hook_detach(kh_remote_hook_handle_t handle)
{
    if (!handle)
        return -EINVAL;

    /* Restore saved register state */
    int ret = write_regs(handle->pid, &handle->saved_regs);
    if (ret < 0) {
        /* Best-effort detach even if restore fails */
        ptrace(PTRACE_DETACH, handle->pid, NULL, NULL);
        free(handle);
        return ret;
    }

    if (ptrace(PTRACE_DETACH, handle->pid, NULL, NULL) < 0) {
        int err = -errno;
        free(handle);
        return err;
    }

    free(handle);
    return 0;
}

uint64_t kh_remote_hook_alloc(kh_remote_hook_handle_t handle, uint64_t size, int prot)
{
    if (!handle)
        return 0;

    /*
     * mmap(NULL, size, prot, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
     * ARM64: x8=222, x0=addr, x1=length, x2=prot, x3=flags, x4=fd, x5=offset
     */
    int64_t ret = inject_syscall(handle, SYS_MMAP,
                                 0,       /* addr = NULL (kernel picks) */
                                 size,
                                 (uint64_t)prot,
                                 0x22,    /* MAP_PRIVATE | MAP_ANONYMOUS */
                                 (uint64_t)-1, /* fd = -1 */
                                 0);      /* offset = 0 */

    /* mmap returns MAP_FAILED (-1 as unsigned) on error */
    if (ret < 0 || ret == (int64_t)(uint64_t)-1)
        return 0;

    return (uint64_t)ret;
}

int kh_remote_hook_install(kh_remote_hook_handle_t handle, uint64_t func_addr,
                        const void *transit_code, uint64_t transit_size)
{
    if (!handle || !transit_code || transit_size == 0)
        return -EINVAL;

    /*
     * Write the transit code to the remote process.
     * process_vm_writev transfers data without stopping the tracee
     * (the tracee is already stopped via ptrace).
     */
    struct iovec local_iov = {
        .iov_base = (void *)transit_code,
        .iov_len = transit_size,
    };
    struct iovec remote_iov = {
        .iov_base = (void *)func_addr,
        .iov_len = transit_size,
    };

    /* Make the target region writable first */
    uint64_t ps = sysconf(_SC_PAGE_SIZE);
    uint64_t page_start = func_addr & ~(ps - 1);
    uint64_t page_end = (func_addr + transit_size + ps - 1) & ~(ps - 1);
    uint64_t page_span = page_end - page_start;

    /* mprotect(page_start, page_span, PROT_READ|PROT_WRITE) */
    int64_t ret = inject_syscall(handle, SYS_MPROTECT,
                                 page_start, page_span,
                                 0x3, /* PROT_READ | PROT_WRITE */
                                 0, 0, 0);
    if (ret < 0)
        return (int)ret;

    /* Write the kh_hook code */
    ssize_t written = process_vm_writev(handle->pid, &local_iov, 1,
                                        &remote_iov, 1, 0);
    if (written < 0 || (uint64_t)written != transit_size) {
        /* Try to restore permissions before returning error */
        inject_syscall(handle, SYS_MPROTECT,
                       page_start, page_span,
                       0x5, /* PROT_READ | PROT_EXEC */
                       0, 0, 0);
        return -errno;
    }

    /* Restore execute permissions: mprotect(page_start, page_span, PROT_READ|PROT_EXEC) */
    ret = inject_syscall(handle, SYS_MPROTECT,
                         page_start, page_span,
                         0x5, /* PROT_READ | PROT_EXEC */
                         0, 0, 0);
    if (ret < 0)
        return (int)ret;

    /* Flush icache in the remote process — the mprotect RW→RX transition
     * combined with process_vm_writev is sufficient on ARM64 Linux,
     * as the kernel handles cache maintenance on permission changes. */

    return 0;
}

#endif /* __linux__ */
#endif /* __USERSPACE__ */
