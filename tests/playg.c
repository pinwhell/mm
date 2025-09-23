#define MM_IMPL
#include <mm/mm.h>

#define TEST_ARENA_HIDDEN_SIZE sizeof(mm_block_head) * 3u
#define TEST_ARENA_ACTUAL_SIZE 4u * 8u
char test_arena[TEST_ARENA_HIDDEN_SIZE + TEST_ARENA_ACTUAL_SIZE];

int main()
{
	mm_init();
	if (mm_add_arena(test_arena, sizeof(test_arena))) return 1u;
	char* v1 = mm_alloc(8u);
	char* v2 = mm_alloc(8u);
	char* v3 = mm_alloc(8u);
	mm_free(v3);
	mm_free(v2);
	mm_free(v1);
	v1 = mm_alloc(8u);
	v2 = mm_alloc(8u);
	v3 = mm_alloc(8u);
	mm_free(v3);
	mm_free(v2);
	v1 = mm_realloc(v1, TEST_ARENA_ACTUAL_SIZE); // Success!
	void* fail = mm_realloc(v1, sizeof(test_arena)); // Fail!

	// Test Private Arena

	char b[0x1000];
	mm_arena a = { 0u };
	mm_arena_init(&a, b, sizeof(b));
	void* p = mm_arena_alloc(&a, 0x100u);
	p = mm_arena_realloc(&a, p, 0x200u);
	mm_arena_free(&a, p);
}