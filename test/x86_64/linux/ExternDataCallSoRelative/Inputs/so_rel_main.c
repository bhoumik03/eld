// Executable calls into shared object to retrieve hidden sum.
extern long get_sum_hidden(void);
void _start() {
  long u = get_sum_hidden();
  asm("movq $60, %%rax\n"
      "movq %0, %%rdi\n"
      "syscall\n"
      :
      : "r"(u)
      : "%rax", "%rdi");
}
