// Use x and y via normal C; expect compiler to generate GOTPCREL in PIE
extern long x;
extern long y;
void _start() {
  long u = x + y; // expect 13
  __asm__("movq $60, %%rax\n\n"
          "movq %0, %%rdi\n\n"
          "syscall\n" ::"r"(u)
          :"%rax", "%rdi");
}
