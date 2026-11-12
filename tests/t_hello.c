/* tests/t_hello.c —— 最小用例：write + exit，验证装载/控制台/退出码。 */

#include "testlib.h"

void os_main(void)
{
    puts_("c6os rv32 boot\n");
    puts_("PASS: rv32 hello\n");
    flush_out();
}
