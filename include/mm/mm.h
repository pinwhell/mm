#ifndef MM_H_
#define MM_H_

#ifdef MM_MT
#include <spnlck/spnlck.h>
#endif 

#include <stddef.h>

enum {
	MM_SUCCESS = 0,
	MM_ARENA_NO_SPACE,
	MM_ARENA_ALREADY_EXISTS
};

struct mm_arena;
typedef struct mm_block_head {
	struct mm_arena* parent;
	size_t size;
	int free;
	struct mm_block_head* next;
} mm_block_head;

typedef struct {
#ifdef MM_MT
	spnlck lck;
#endif
	void* base;
	size_t size;
	mm_block_head* first;
} mm_arena;

void mm_arena_init(mm_arena* a, void* base, size_t size);
void* mm_arena_alloc(mm_arena* a, size_t size);
void* mm_arena_realloc(mm_arena* a, void* old_ptr, size_t new_sz);
void mm_arena_free(mm_arena* a, void* ptr);

int mm_add_arena(void* base, size_t size);
void* mm_alloc(size_t size);
void mm_free(void* ptr);
void* mm_realloc(void* old_ptr, size_t new_sz);

#ifdef MM_IMPL
void* mm_memcpy(void* dest, const void* src, size_t n) {
	unsigned char* d = dest;
	const unsigned char* s = src;
	while (n--) {
		*d++ = *s++;
	}
	return dest;
}

#ifndef MM_FREESTANDING
#include <string.h>
#define mm_memcpy memcpy
#endif

#ifndef ARENA_ARR_SZ 
#define ARENA_ARR_SZ 16u
#endif

mm_arena arenas[ARENA_ARR_SZ];
int arena_idx = 0u;
#ifdef MM_MT
spnlck arenas_lck;
#endif

void mm_arena_init(mm_arena* a, void* base, size_t size)
{
	a->base = base;
	a->size = size;
	a->first = (mm_block_head*)base;
	a->first->parent = a;
	a->first->size = size - sizeof(mm_block_head);
	a->first->free = 1u;
	a->first->next = NULL;
#ifdef MM_MT
	spnlck_init(&a->lck);
#endif
}

void mm_init() {
	spnlck_init(&arenas_lck);
}
// expects arenas_lck lock held
int __mm_add_arena(void* base, size_t size)
{
	if (!(arena_idx < ARENA_ARR_SZ)) return MM_ARENA_NO_SPACE;
	for (size_t i = 0; i < arena_idx; ++i)
		if (arenas[i].base == base) return MM_ARENA_ALREADY_EXISTS;
	mm_arena_init(&arenas[arena_idx], base, size);
	arena_idx++;
	return MM_SUCCESS;
}

// O(N)
// Expects mm_arena lock hold
void mm_arena_defrag(mm_arena* arena) {
	if (!arena || !arena->first) return;

	mm_block_head* cur = arena->first;
	while (cur && cur->next) {
		if (cur->free && cur->next->free) {
			// merge cur and next
			cur->size += sizeof(mm_block_head) + cur->next->size;
			cur->next = cur->next->next;
			// stay on cur to catch more free neighbors
		}
		else {
			cur = cur->next;
		}
	}
}

// Expects mm_arena lock hold
mm_block_head* mm_arena_find_fit(mm_arena* a, size_t size) {
	mm_block_head* cur = a->first;
	while (cur) {
		if (cur->free && cur->size >= size) return cur;
		cur = cur->next;
	}
	return NULL;
}

// Expect mm_arenas lock hold
mm_block_head* __mm_arenas_find_fit(size_t size, int keep_arena_lock_held, int mark_taken)
{
	for (size_t i = 0; i < arena_idx; i++)
	{
		mm_arena* a = &arenas[i];
#ifdef MM_MT
		spnlck_acquire(&a->lck);
#endif
		mm_block_head* head = mm_arena_find_fit(&arenas[i], size);
		if (!head) {
#ifdef MM_MT
			spnlck_release(&a->lck);
#endif
			continue;
		}

		if (mark_taken) head->free = 0u;
#ifdef MM_MT
		if(!keep_arena_lock_held) spnlck_release(&a->lck);
#endif
		return head;
	}
	return NULL;
}

// expect mm_arena lock hold
void mm_block_head_truncate(mm_block_head* b, size_t size)
{
	size_t left_over = b->size - size;
	if (left_over < sizeof(mm_block_head) + sizeof(void*)) return; // Split
	mm_block_head* newb = (mm_block_head*)((char*)b + sizeof(mm_block_head) + size);
	newb->parent = b->parent;
	newb->size = b->size - size - sizeof(mm_block_head);
	newb->next = b->next;
	newb->free = 1u;
	b->next = newb;
	b->size = size;
}

// expects arenas_lck lock held
static inline void* __mm_alloc(size_t size)
{
	size = (size + 7) & ~7; // 8-byte align
	mm_block_head* b = __mm_arenas_find_fit(size, 1u, 1u);
	if (b == NULL) return NULL;
	mm_arena* a = b->parent;
	mm_block_head_truncate(b, size);
	b->free = 0u;
#ifdef MM_MT
	spnlck_release(&a->lck);
#endif
	return (char*)b + sizeof(mm_block_head);
}

// expects mm_arena lock held
static inline void* __mm_arena_alloc(mm_arena* a, size_t size)
{
	size = (size + 7) & ~7; // 8-byte align
	mm_block_head* b = mm_arena_find_fit(a, size);
	if (b == NULL) return NULL;
	mm_block_head_truncate(b, size);
	b->free = 0u;
	return (char*)b + sizeof(mm_block_head);
}

// expects mm_arena lock held
static inline void __mm_arena_free(mm_arena* a, void* ptr)
{
	if (!ptr) return;
	mm_block_head* b = (mm_block_head*)((char*)ptr - sizeof(mm_block_head));
	if (b->parent != a) return;
	b->free = 1;
	mm_arena_defrag(a);
}

// expects mm_arena lock held
static inline void __mm_free(void* ptr)
{
	if (!ptr) return;
	mm_block_head* b = (mm_block_head*)((char*)ptr - sizeof(mm_block_head));
	b->free = 1;
	mm_arena_defrag(b->parent);
}

// Expects mm_arena lock held
int mm_block_head_try_grow(mm_block_head* h, size_t to_new_size)
{
	to_new_size = (to_new_size + 7) & ~7; // 8-byte align

	if (to_new_size <= h->size) {
		// Already big enough
		return 1;
	}

	size_t needed = to_new_size - h->size;

	mm_block_head* old_nextp = h->next;

	if (!old_nextp) return 0u;

	mm_block_head old_next = *h->next;

	// Cannot expand if next is missing, used, or too small
	if (!old_nextp->free || old_nextp->size < needed)
		return 0;

	// Merge part or all of the next block
	if (old_next.size > needed + sizeof(mm_block_head)) {
		// Split the next block
		mm_block_head* new_next = (mm_block_head*)((char*)h->next + needed);
		new_next->size = old_next.size - needed - sizeof(mm_block_head);
		new_next->free = 1;
		new_next->next = old_next.next;
		new_next->parent = old_next.parent;

		h->next = new_next;
		h->size += needed;
	}
	else {
		// Take whole next block
		h->size += sizeof(mm_block_head) + old_next.size;
		h->next = old_next.next;
	}

	return 1; // Expanded successfully
}

// expect mm_arena lock held
static inline void* __mm_arena_realloc(mm_arena* a, void* old_ptr, size_t new_sz)
{
	new_sz = (new_sz + 7) & ~7; // 8-byte align
	if (!old_ptr) return __mm_arena_alloc(a, new_sz);
	mm_block_head* b = (mm_block_head*)((char*)old_ptr - sizeof(mm_block_head));
	if (b->parent != a) return NULL;
	if (b->size >= new_sz) return old_ptr; // Fits
	if (mm_block_head_try_grow(b, new_sz)) return old_ptr;
	void* new_ptr = __mm_arena_alloc(a, new_sz);
	if (new_ptr)
	{
		mm_memcpy(new_ptr, old_ptr, b->size);
		__mm_free(old_ptr);
	}
	return new_ptr;
}

// expects arenas_lck lock held
static inline void* __mm_realloc(void* old_ptr, size_t new_sz)
{
	new_sz = (new_sz + 7) & ~7; // 8-byte align
	if (!old_ptr) return __mm_alloc(new_sz);
	mm_block_head* old_b = (mm_block_head*)((char*)old_ptr - sizeof(mm_block_head));
	mm_arena* old_a = old_b->parent;
#ifdef MM_MT
	spnlck_acquire(&old_a->lck);
#endif
	if (old_b->size >= new_sz || mm_block_head_try_grow(old_b, new_sz))
	{
#ifdef MM_MT
		spnlck_release(&old_a->lck);
#endif
		return old_ptr; // Fits
	}
#ifdef MM_MT
	spnlck_release(&old_a->lck);
#endif
	void* new_ptr = __mm_alloc(new_sz);
	if (new_ptr)
	{
		mm_memcpy(new_ptr, old_ptr, old_b->size);
#ifdef MM_MT
		spnlck_acquire(&old_a->lck);
#endif
		__mm_free(old_ptr);
#ifdef MM_MT
		spnlck_release(&old_a->lck);
#endif
	}
	return new_ptr;
}
// Public: add a new arena
int mm_add_arena(void* base, size_t size) {
	int ret;
#ifdef MM_MT
	spnlck_acquire(&arenas_lck);
#endif
	ret = __mm_add_arena(base, size);
#ifdef MM_MT
	spnlck_release(&arenas_lck);
#endif
	return ret;
}

// Public: allocate memory
void* mm_alloc(size_t size) {
	void* ret;
#ifdef MM_MT
	spnlck_acquire(&arenas_lck);
#endif
	ret = __mm_alloc(size);
#ifdef MM_MT
	spnlck_release(&arenas_lck);
#endif
	return ret;
}

// Public: free memory
void mm_free(void* ptr) {
#ifdef MM_MT
	if (!ptr) return;
	mm_block_head* b = (mm_block_head*)((char*)ptr - sizeof(mm_block_head));
	mm_arena* a = b->parent;
	spnlck_acquire(&a->lck);
	__mm_free(ptr);
	spnlck_release(&a->lck);
#else
	__mm_free(ptr);
#endif
}

// Public: realloc memory
void* mm_realloc(void* ptr, size_t size) {
	void* ret;
#ifdef MM_MT
	spnlck_acquire(&arenas_lck);
#endif
	ret = __mm_realloc(ptr, size);
#ifdef MM_MT
	spnlck_release(&arenas_lck);
#endif
	return ret;
}

// Public: arena-level alloc
void* mm_arena_alloc(mm_arena* a, size_t size) {
#ifdef MM_MT
	spnlck_acquire(&a->lck);
#endif
	void* ret = __mm_arena_alloc(a, size);
#ifdef MM_MT
	spnlck_release(&a->lck);
#endif
	return ret;
}

// Public: arena-level free
void mm_arena_free(mm_arena* a, void* ptr) {
#ifdef MM_MT
	spnlck_acquire(&a->lck);
#endif
	__mm_arena_free(a, ptr);
#ifdef MM_MT
	spnlck_release(&a->lck);
#endif
}

// Public: arena-level realloc
void* mm_arena_realloc(mm_arena* a, void* ptr, size_t size) {
#ifdef MM_MT
	spnlck_acquire(&a->lck);
#endif
	void* ret = __mm_arena_realloc(a, ptr, size);
#ifdef MM_MT
	spnlck_release(&a->lck);
#endif
	return ret;
}

#endif

#endif