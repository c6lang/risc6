/* native 宿主：读 ELF 到 risc6_in，调 risc6_run，刷 out/diag。
 * 只「读符号 + 调入口」，不碰 c6 内部状态（docs/design.md §5.2）。 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "risc6.h"

static int usage(void)
{
    fprintf(stderr,
            "risc6 " RISC6_VERSION " -- RV32IMAC+Zicsr machine emulator\n"
            "usage: risc6 [options] <file.elf>\n"
            "  --limit N     instruction budget (default %llu)\n"
            "  --trace       trace every instruction to stderr\n"
            "  --quiet       do not print instruction count\n"
            "  --time        print wall time and instructions/second\n"
            "  --stats       print coverage counters\n"
            "  --bare        bare mode (no syscalls, mtvec/mret traps, MTIP)\n"
            "  --version     print version and exit\n",
            (unsigned long long)RISC6_MAX_INSNS);
    return 2;
}

int main(int argc, char **argv)
{
    const char *path = NULL;
    u64 limit = RISC6_MAX_INSNS;
    int trace = 0;
    int quiet = 0;
    int want_time = 0;
    int want_stats = 0;
    int bare = 0;
    double t0 = 0.0, t1 = 0.0;
    FILE *f = NULL;
    size_t n = 0;
    int rc = 0;
    int i = 0;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--limit") == 0 && i + 1 < argc) {
            limit = strtoull(argv[++i], NULL, 0);
        } else if (strcmp(a, "--trace") == 0) {
            trace = 1;
        } else if (strcmp(a, "--quiet") == 0) {
            quiet = 1;
        } else if (strcmp(a, "--time") == 0) {
            want_time = 1;
        } else if (strcmp(a, "--stats") == 0) {
            want_stats = 1;
        } else if (strcmp(a, "--bare") == 0) {
            bare = 1;
        } else if (strcmp(a, "--version") == 0) {
            printf("risc6 " RISC6_VERSION "\n");
            return 0;
        } else if (a[0] == '-') {
            return usage();
        } else if (path == NULL) {
            path = a;
        } else {
            return usage();
        }
    }
    if (path == NULL) {
        return usage();
    }

    f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "risc6: cannot open %s\n", path);
        return RISC6_RC_LOAD;
    }
    n = fread(risc6_in, 1, RISC6_IN_MAX, f);
    if (ferror(f)) {
        fprintf(stderr, "risc6: read error on %s\n", path);
        fclose(f);
        return RISC6_RC_LOAD;
    }
    fclose(f);
    if (n == 0) {
        fprintf(stderr, "risc6: %s is empty\n", path);
        return RISC6_RC_LOAD;
    }

    risc6_set_limit(limit);
    risc6_set_trace((u32)trace);
    if (bare) {
        risc6_set_mode(1u);
    }
    if (want_time) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        t0 = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
    }
    rc = risc6_run((u32)n);
    if (want_time) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        t1 = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
    }

    if (risc6_out_len > 0) {
        fwrite(risc6_out, 1, (size_t)risc6_out_len, stdout);
    }
    fflush(stdout);
    if (risc6_diag_len > 0) {
        fwrite(risc6_diag, 1, (size_t)risc6_diag_len, stderr);
    }
    if (want_stats) {
        static const char *names[RISC6_STAT_N] = {
            "lui", "auipc", "jal", "jalr", "branch", "load", "store",
            "op-imm", "op", "fence", "amo", "system", "bad-insn",
            "compressed", "ecall", "trap", "mtip", "csr-unimpl"
        };
        unsigned k = 0;
        unsigned long long sum = 0;
        for (k = 0; k < RISC6_STAT_N; k++) {
            if (names[k] != NULL && risc6_stat[k] != 0) {
                fprintf(stderr, "risc6: stat %-11s %u\n", names[k],
                        (unsigned)risc6_stat[k]);
                if (k <= ST_SYSTEM) {
                    sum += risc6_stat[k];
                }
            }
        }
        fprintf(stderr, "risc6: stat decoded     %llu\n", sum);
    }
    if (want_time && !quiet) {
        double dt = (t1 > t0) ? (t1 - t0) : 0.0;
        double rate = (dt > 0.0) ? (double)risc6_stat_insns / dt / 1e6 : 0.0;
        fprintf(stderr, "risc6: time=%.3fms rate=%.1f Minsn/s\n",
                dt * 1000.0, rate);
    }
    if (!quiet) {
        fprintf(stderr, "risc6: insns=%llu rc=%d\n",
                (unsigned long long)risc6_stat_insns, rc);
    }
    return rc;
}
