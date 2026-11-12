/* native 宿主：读 ELF 到 risc6_in，调 risc6_run，刷 out/diag。
 * 只「读符号 + 调入口」，不碰 c6 内部状态（docs/design.md §5.2）。 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "risc6.h"

static int usage(void)
{
    fprintf(stderr,
            "usage: risc6 [options] <file.elf>\n"
            "  --limit N     instruction budget (default %llu)\n"
            "  --trace       trace every instruction to stderr\n"
            "  --quiet       do not print instruction count\n",
            (unsigned long long)RISC6_MAX_INSNS);
    return 2;
}

int main(int argc, char **argv)
{
    const char *path = NULL;
    u64 limit = RISC6_MAX_INSNS;
    int trace = 0;
    int quiet = 0;
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
    rc = risc6_run((u32)n);

    if (risc6_out_len > 0) {
        fwrite(risc6_out, 1, (size_t)risc6_out_len, stdout);
    }
    fflush(stdout);
    if (risc6_diag_len > 0) {
        fwrite(risc6_diag, 1, (size_t)risc6_diag_len, stderr);
    }
    if (!quiet) {
        fprintf(stderr, "risc6: insns=%llu rc=%d\n",
                (unsigned long long)risc6_stat_insns, rc);
    }
    return rc;
}
