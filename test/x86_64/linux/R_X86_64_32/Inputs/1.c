long global = 42;

void _start(){
    unsigned int* ptr32;
    asm volatile(
        "movl $global, %0\n"
        : "=r"(ptr32)
    );
    asm volatile(
        "movq $60, %%rax\n"
        "movq $0, %%rdi\n"
        "syscall\n"
        :
        :
        : "rax", "rdi"
    );
}