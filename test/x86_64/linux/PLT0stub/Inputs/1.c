int foo();
void _start() {
  long u = foo();
  asm("movq $60, %%rax\n"
      "movq %0, %%rdi\n"
      "syscall\n"
      :
      : "r"(u)
      : "%rax", "%rdi");
}