// Executable calls into liba to fetch preemptible data from libb
extern long fetch_dat(void);
void _start() {
  long u = fetch_dat();
  asm("movq $60, %%rax\n"
      "movq %0, %%rdi\n"
      "syscall\n"
      :
      : "r"(u)
      : "%rax", "%rdi");
}
