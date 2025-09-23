
# MM – C Memory Manager / Arena Allocator

A lightweight, arena-based thread-safe memory manager for C.

----------

## Features
-   Thread Safe

-   Multiple memory **arenas** support
    
-   8-byte **aligned allocations**
    
-   `malloc` / `free` / `realloc` equivalents
    
-   **Block splitting** and **defragmentation**
    
-   Optional **freestanding mode** (no stdlib)
    
-   Simple **return codes**: success, no space, arena exists
    

----------

## Quick Start – Add Arena & Alloc
```c
#include <mm/mm.h>
#include <stdio.h>

int main() {
    mm_init();
    _
    char mem[1024];
    mm_add_arena(mem, sizeof(mem));

    int* x = mm_alloc(sizeof(int));
    *x = 42;
    printf("%d\n", *x);

    mm_free(x);
    return 0;
}
```
----------

## Realloc Example
```c
mm_init();_
int* arr = mm_alloc(4 * sizeof(int));
for (int i = 0; i < 4; i++) arr[i] = i;

// Grow array
arr = mm_realloc(arr, 8 * sizeof(int));
for (int i = 4; i < 8; i++) arr[i] = i;

mm_free(arr);
```
## Private Arena
```c
char b[0x1000];
mm_arena a = { 0u }; 
mm_arena_init(&a, b, sizeof(b));
void* p = mm_arena_alloc(&a, 0x100u);
p = mm_arena_realloc(&a, p, 0x200u);
mm_arena_free(&a, p);
```
## License

MIT
