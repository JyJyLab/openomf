// Switch: stdout and stderr go to sdmc:/switch/openomf/stderr.log from before main (the previous
// run's is kept as stderr.log.old), so a start that ends before OpenOMF's own log file opens can be
// read back over FTP. A crash (CPU exception) writes the registers, the frame-pointer chain and the
// code addresses on the stack as offsets from the module base (addr2line on openomf.elf), then ends
// the process.
#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <switch.h>

#define OMF_DEBUG_LOG "sdmc:/switch/openomf/stderr.log"

extern char nx_module_start[] __asm__("_start");

alignas(16) u8 __nx_exception_stack[0x4000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);

void userAppInit(void) {
    mkdir("sdmc:/switch", 0777);
    mkdir("sdmc:/switch/openomf", 0777);
    remove(OMF_DEBUG_LOG ".old");
    rename(OMF_DEBUG_LOG, OMF_DEBUG_LOG ".old");
    if(freopen(OMF_DEBUG_LOG, "w", stdout)) {
        setvbuf(stdout, NULL, _IOLBF, 0);
        dup2(fileno(stdout), fileno(stderr));
        setvbuf(stderr, NULL, _IONBF, 0);
    }
    printf("OpenOMF debug log (Switch), built " __DATE__ " " __TIME__ ", module base %p\n", (void *)nx_module_start);
    fflush(stdout);
}

void userAppExit(void) {
    printf("exit\n");
    fflush(stdout);
}

static uint64_t rel(uint64_t address) {
    const uint64_t base = (uint64_t)nx_module_start;
    return address >= base && address - base < 0x4000000ull ? address - base : 0;
}

void __libnx_exception_handler(ThreadExceptionDump *ctx) {
    printf("\n*** CRASH: exception 0x%x, pc %#llx (+%#llx) lr %#llx (+%#llx) sp %#llx fp %#llx far %#llx esr %#x\n",
           ctx->error_desc, (unsigned long long)ctx->pc.x, (unsigned long long)rel(ctx->pc.x),
           (unsigned long long)ctx->lr.x, (unsigned long long)rel(ctx->lr.x), (unsigned long long)ctx->sp.x,
           (unsigned long long)ctx->fp.x, (unsigned long long)ctx->far.x, ctx->esr);
    for(int i = 0; i < 29; i++)
        printf("  x%-2d %#018llx%s", i, (unsigned long long)ctx->cpu_gprs[i].x, i % 4 == 3 ? "\n" : "");
    printf("\n");
    fflush(stdout);

    // only read inside the mapping the stack pointer is in (a bad frame pointer must not fault here)
    MemoryInfo info = {0};
    u32 page = 0;
    uint64_t lo = ctx->sp.x & ~7ull, hi = lo;
    if(R_SUCCEEDED(svcQueryMemory(&info, &page, ctx->sp.x)))
        hi = info.addr + info.size;
    printf("  frame chain (module offsets):");
    uint64_t fp = ctx->fp.x;
    for(int depth = 0; depth < 32 && fp >= lo && fp + 16 <= hi && (fp & 7) == 0; depth++) {
        const uint64_t *frame = (const uint64_t *)fp;
        printf(" +%#llx", (unsigned long long)rel(frame[1]));
        if(frame[0] <= fp)
            break;
        fp = frame[0];
    }
    printf("\n  code addresses on the stack:");
    int found = 0;
    for(uint64_t at = lo; at + 8 <= hi && at < lo + 0x2000 && found < 48; at += 8) {
        const uint64_t value = *(const uint64_t *)at;
        if(rel(value)) {
            printf(" +%#llx", (unsigned long long)rel(value));
            found++;
        }
    }
    printf("\n");
    fflush(stdout);
    fclose(stdout);
    svcExitProcess();
}
