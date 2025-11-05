extern long var;
extern long var2;
void _start() {
  long u = var + var2;
  asm("movq $60, %%rax\n"
      "movq %0, %%rdi\n"
      "syscall\n"
      :
      : "r"(u)
      : "%rax", "%rdi");
}
