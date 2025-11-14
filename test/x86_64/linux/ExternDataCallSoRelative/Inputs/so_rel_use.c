// Access hidden data via normal C symbol references
__attribute__((visibility("hidden"))) extern long a;
__attribute__((visibility("hidden"))) extern long b;

long get_sum_hidden(void) {
  return a + b; // 19
}
