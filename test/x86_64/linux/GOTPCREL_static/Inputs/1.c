// Test for GOTPCREL relocation in static linking
// This test verifies that global variable access through GOT works correctly

int global_var = 42;

void _start() {
  // Access global variable through GOTPCREL relocation
  // This will generate R_X86_64_GOTPCREL relocation
  int *ptr;
  asm (
    "movq global_var@GOTPCREL(%%rip), %0\n"
    : "=r" (ptr)
    :
    : 
  );
  
  // Load the value from the GOT entry
  int value = *ptr;
  
  // Exit with the value from global_var (should be 42)
  asm (
    "movq $60, %%rax\n"
    "movq %0, %%rdi\n"
    "syscall\n"
    :
    : "r" ((long)value)
    : "%rax", "%rdi"
  );
}