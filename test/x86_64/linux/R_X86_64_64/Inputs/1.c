long global64 = 42;

void _start(){
    long loaded;

    asm volatile(
        "movabs $global64, %%rax\n"
        :
        :
        : "rax"
    );

    asm volatile(
        "movq (%%rax), %0\n"
        : "=r" (loaded)
        :
        : "rax"
    );

    asm volatile (
        "movq $60, %%rax\n"
        "movq %0, %%rdi\n"
        "syscall\n"
        :
        : "r" (loaded)
        : "rax", "rdi"
    );
}