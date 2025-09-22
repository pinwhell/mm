#define MM_IMPL
#include <mm/mm.h>

#define TEST_ARENA_HIDDEN_SIZE sizeof(mm_block_head) * 3u
#define TEST_ARENA_ACTUAL_SIZE 4u * 8u
char test_arena[TEST_ARENA_HIDDEN_SIZE + TEST_ARENA_ACTUAL_SIZE];

int main()
{
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

	char test_arena_buff_2[0x1000];
	mm_arena test_arena_2 = { 0u }; 
	mm_arena_init(&test_arena_2, test_arena_buff_2, sizeof(test_arena_buff_2));
	void* a = mm_arena_alloc(&test_arena_2, 0x100u);
	a = mm_arena_realloc(a, 0x200u);
	mm_arena_free(a);
}