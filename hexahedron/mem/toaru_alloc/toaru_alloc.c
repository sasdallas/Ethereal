/**
 * @file  kernel/misc/malloc.c
 * @brief klange's Slab Allocator
 *
 * This is one of the oldest parts of ToaruOS: the infamous heap allocator.
 * Used in userspace and the kernel alike, this is a straightforward "slab"-
 * style allocator. It has a handful of fixed sizes to stick small objects
 * in and keeps several together in a single page. It's surprisingly fast,
 * needs only an 'sbrk', makes only page-multiple calls to that sbrk, and
 * throwing a big lock around the whole thing seems to have worked just fine
 * for making it thread-safe in userspace applications (not necessarily
 * tested in the kernel).
 *
 * FIXME The heap allocator has long been lacking an ability to merge large
 *       freed blocks. 
 * 
 * @copyright
 * This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (c) 2010-2021 K. Lange.  All rights reserved.
 *
 * Developed by: K. Lange <klange@toaruos.org>
 *               Dave Majnemer <dmajnem2@acm.uiuc.edu>
 *               Assocation for Computing Machinery
 *               University of Illinois, Urbana-Champaign
 *               http://acm.uiuc.edu
 */

/* If another allocator was already included, bail out */
#ifdef HEXAHEDRON_ALLOC_SPECIFIED
#error "More than one allocator was specified." 
#else
#define HEXAHEDRON_ALLOC_SPECIFIED
#endif

#define ALLOC_VERSION_MAJOR 1
#define ALLOC_VERSION_MINOR 0
#define ALLOC_NAME "toaru_alloc"


/* Includes */
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>

#include <kernel/panic.h>
#include <kernel/debug.h>
#include <kernel/mem/alloc.h>
#include <kernel/mem/mem.h>


/* Addendums to Hexahedron */

static allocator_info_t *allocator_information = NULL; 

/**
 * @brief Get information on the allocator.
 */
allocator_info_t *alloc_getInfo() {
    // Allocate if not present.
    // !!!: This probably isn't a good idea. What happens if there was a fault in the allocator?
    // !!!: (contd) Should panic handle that or should we?
    if (allocator_information == NULL) {
        allocator_information = kmalloc(sizeof(allocator_info_t));
        strncpy((char*)allocator_information->name, ALLOC_NAME, 127);
        allocator_information->name[strlen(allocator_information->name)] = 0;

        allocator_information->version_major = ALLOC_VERSION_MAJOR;
        allocator_information->version_minor = ALLOC_VERSION_MINOR;

        allocator_information->support_valloc = 1;
    }
    
    return allocator_information;
}


/* Definitions {{{ */

/*
 * Defines for often-used integral values
 * related to our binning and paging strategy.
 */
#if defined(__ARCH_I386__)
#define NUM_BINS 11U								/* Number of bins, total, under 32-bit. */
#define SMALLEST_BIN_LOG 2U							/* Logarithm base two of the smallest bin: log_2(sizeof(int32)). */
#elif defined(__ARCH_X86_64__)
#define NUM_BINS 10U
#define SMALLEST_BIN_LOG 3U
#endif 

#define BIG_BIN (NUM_BINS - 1)						/* Index for the big bin, (NUM_BINS - 1) */
#define SMALLEST_BIN (1UL << SMALLEST_BIN_LOG)		/* Size of the smallest bin. */

#define PAGE_MASK (PAGE_SIZE - 1)					/* Block mask, size of a page * number of pages - 1. */
#define SKIP_P INT32_MAX							/* INT32_MAX is half of UINT32_MAX; this gives us a 50% marker for skip lists. */
#define SKIP_MAX_LEVEL 6							/* We have a maximum of 6 levels in our skip lists. */

#define BIN_MAGIC 0xDEFAD00D

#if 1
#define assert(statement) ((statement) ? (void)0 : __assert_fail(__FILE__, __LINE__, #statement))
#define assert_nonfatal(statement) ((statement) ? (void)0 : __assert_warn(__FILE__, __LINE__, #statement))
#else
#define assert(statement) (void)0
#endif

#if 0
#define LOG(status, ...) dprintf_module(status, "toaru_alloc", __VA_ARGS__)
#else
#define LOG(status, ...) (void)0
#endif

static void __assert_fail(const char * f, int l, const char * stmt) {
	// Assertion failed, crash.
    kernel_panic_prepare(MEMORY_MANAGEMENT_ERROR);
    dprintf(NOHEADER, "*** Assertion failed in allocator (%s:%i): %s\n\n", f, l, stmt);
    dprintf(NOHEADER, "Current allocator in use: toaru_alloc version %i.%i\n", ALLOC_VERSION_MAJOR, ALLOC_VERSION_MINOR);
    kernel_panic_finalize();
	__builtin_unreachable();
}

static void __assert_warn(const char *f, int l, const char *stmt) {
	dprintf(WARN, "Nonfatal assertion failed (%s:%i): %s\n", f, l, stmt);
}

/* }}} */

/*
 * Internal functions.
 */
static void * __attribute__ ((malloc)) klmalloc(uintptr_t size);
static void * __attribute__ ((malloc)) klrealloc(void * ptr, uintptr_t size);
static void * __attribute__ ((malloc)) klcalloc(uintptr_t nmemb, uintptr_t size);
static void * __attribute__ ((malloc)) klvalloc(uintptr_t size);
static void klfree(void * ptr);
 

void * __attribute__ ((malloc)) alloc_malloc(uintptr_t size) {
	void * out = klmalloc(size);
	return out;
}

void * __attribute__ ((malloc)) alloc_realloc(void * ptr, uintptr_t size) {
	void * out = klrealloc(ptr, size);
	return out;
}

void * __attribute__ ((malloc)) alloc_calloc(uintptr_t nmemb, uintptr_t size) {
	void * out = klcalloc(nmemb, size);
	return out;
}

void * __attribute__ ((malloc)) alloc_valloc(uintptr_t size) {
	void * out = klvalloc(size);
	return out;
}

void alloc_free(void * ptr) {
	klfree(ptr);
}

/* Bin management {{{ */

/*
 * Adjust bin size in bin_size call to proper bounds.
 */
static inline uintptr_t __attribute__ ((always_inline, pure)) klmalloc_adjust_bin(uintptr_t bin)
{
	if (bin <= (uintptr_t)SMALLEST_BIN_LOG)
	{
		return 0;
	}
	bin -= SMALLEST_BIN_LOG + 1;
	if (bin > (uintptr_t)BIG_BIN) {
		return BIG_BIN;
	}
	return bin;
}

/*
 * Given a size value, find the correct bin
 * to place the requested allocation in.
 */
static inline uintptr_t __attribute__ ((always_inline, pure)) klmalloc_bin_size(uintptr_t size) {
	uintptr_t bin = sizeof(size) * CHAR_BIT - __builtin_clzl(size);
	bin += !!(size & (size - 1));
	return klmalloc_adjust_bin(bin);
}

/*
 * Bin header - One page of memory.
 * Appears at the front of a bin to point to the
 * previous bin (or NULL if the first), the next bin
 * (or NULL if the last) and the head of the bin, which
 * is a stack of cells of data.
 */
typedef struct _klmalloc_bin_header {
	struct _klmalloc_bin_header *  next;	/* Pointer to the next node. */
	void * head;							/* Head of this bin. */
	uintptr_t size;							/* Size of this bin, if big; otherwise bin index. */
	uintptr_t bin_magic;
} klmalloc_bin_header;

/*
 * A big bin header is basically the same as a regular bin header
 * only with a pointer to the previous (physically) instead of
 * a "next" and with a list of forward headers.
 */
typedef struct _klmalloc_big_bin_header {
	struct _klmalloc_big_bin_header * next;
	void * head;
	uintptr_t size;
	uintptr_t bin_magic;
	struct _klmalloc_big_bin_header * prev;
	struct _klmalloc_big_bin_header * forward[SKIP_MAX_LEVEL+1];
} klmalloc_big_bin_header;


/*
 * List of pages in a bin.
 */
typedef struct _klmalloc_bin_header_head {
	klmalloc_bin_header * first;
} klmalloc_bin_header_head;

/*
 * Array of available bins.
 */
static klmalloc_bin_header_head klmalloc_bin_head[NUM_BINS - 1];	/* Small bins */
static struct _klmalloc_big_bins {
	klmalloc_big_bin_header head;
	int level;
} klmalloc_big_bins;
static klmalloc_big_bin_header * klmalloc_newest_big = NULL;		/* Newest big bin */

/* }}} Bin management */
/* Doubly-Linked List {{{ */

/*
 * Remove an entry from a page list.
 * Decouples the element from its
 * position in the list by linking
 * its neighbors to eachother.
 */
static inline void __attribute__ ((always_inline)) klmalloc_list_decouple(klmalloc_bin_header_head *head, klmalloc_bin_header *node) {
	LOG(DEBUG, "klmalloc_list_decouple head 0x%llX node 0x%llX\n", head, node);
	LOG(WARN, "node->next = 0x%llX head->first = 0x%llX\n", node->next, head->first);
	klmalloc_bin_header *next	= node->next;
	head->first = next;
	node->next = NULL;
}

/*
 * Insert an entry into a page list.
 * The new entry is placed at the front
 * of the list and the existing border
 * elements are updated to point back
 * to it (our list is doubly linked).
 */
static inline void __attribute__ ((always_inline)) klmalloc_list_insert(klmalloc_bin_header_head *head, klmalloc_bin_header *node) {
	
	LOG(DEBUG, "klmalloc_list_insert head 0x%llX node 0x%llX head->first = 0x%llX\n", head, node, head->first);
	node->next = head->first;
	head->first = node;
}

/*
 * Get the head of a page list.
 * Because redundant function calls
 * are really great, and just in case
 * we change the list implementation.
 */
static inline klmalloc_bin_header * __attribute__ ((always_inline)) klmalloc_list_head(klmalloc_bin_header_head *head) {
	return head->first;
}

/* }}} Lists */
/* Skip List {{{ */

/*
 * Skip lists are efficient
 * data structures for storing
 * and searching ordered data.
 *
 * Here, the skip lists are used
 * to keep track of big bins.
 */

/*
 * Generate a random value in an appropriate range.
 * This is a xor-shift RNG.
 */
static uint32_t __attribute__ ((pure)) klmalloc_skip_rand(void) {
	static uint32_t x = 123456789;
	static uint32_t y = 362436069;
	static uint32_t z = 521288629;
	static uint32_t w = 88675123;

	uint32_t t;

	t = x ^ (x << 11);
	x = y; y = z; z = w;
	return w = w ^ (w >> 19) ^ t ^ (t >> 8);
}

/*
 * Generate a random level for a skip node
 */
static inline int __attribute__ ((pure, always_inline)) klmalloc_random_level(void) {
	int level = 0;
	/*
	 * Keep trying to check rand() against 50% of its maximum.
	 * This provides 50%, 25%, 12.5%, etc. chance for each level.
	 */
	while (klmalloc_skip_rand() < SKIP_P && level < SKIP_MAX_LEVEL) {
		++level;
	}
	return level;
}

/*
 * Find best fit for a given value.
 */
static klmalloc_big_bin_header * klmalloc_skip_list_findbest(uintptr_t search_size) {
	klmalloc_big_bin_header * node = &klmalloc_big_bins.head;
	/*
	 * Loop through the skip list until we hit something > our search value.
	 */
	int i;
	for (i = klmalloc_big_bins.level; i >= 0; --i) {
		while (node->forward[i] && (node->forward[i]->size < search_size)) {
			node = node->forward[i];
			if (node)
				assert((node->size + sizeof(klmalloc_big_bin_header)) % PAGE_SIZE == 0);
		}
	}
	/*
	 * This value will either be NULL (we found nothing)
	 * or a node (we found a minimum fit).
	 */
	node = node->forward[0];
	if (node) {
		assert((uintptr_t)node % PAGE_SIZE == 0);
		assert((node->size + sizeof(klmalloc_big_bin_header)) % PAGE_SIZE == 0);
	}
	return node;
}

/*
 * Insert a header into the skip list.
 */
static void klmalloc_skip_list_insert(klmalloc_big_bin_header * value) {
	/*
	 * You better be giving me something valid to insert,
	 * or I will slit your ****ing throat.
	 */
	assert(value != NULL);
	assert(value->head != NULL);
	assert((uintptr_t)value->head > (uintptr_t)value);
	if (value->size > NUM_BINS) {
		assert((uintptr_t)value->head < (uintptr_t)value + value->size);
	} else {
		assert((uintptr_t)value->head < (uintptr_t)value + PAGE_SIZE);
	}
	assert((uintptr_t)value % PAGE_SIZE == 0);
	assert((value->size + sizeof(klmalloc_big_bin_header)) % PAGE_SIZE == 0);
	assert(value->size != 0);

	/*
	 * Starting from the head node of the bin locator...
	 */
	klmalloc_big_bin_header * node = &klmalloc_big_bins.head;
	klmalloc_big_bin_header * update[SKIP_MAX_LEVEL + 1];

	/*
	 * Loop through the skiplist to find the right place
	 * to insert the node (where ->forward[] > value)
	 */
	int i;
	for (i = klmalloc_big_bins.level; i >= 0; --i) {
		while (node->forward[i] && node->forward[i]->size < value->size) {
			node = node->forward[i];
			if (node)
				assert((node->size + sizeof(klmalloc_big_bin_header)) % PAGE_SIZE == 0);
		}
		update[i] = node;
	}
	node = node->forward[0];

	/*
	 * Make the new skip node and update
	 * the forward values.
	 */
	if (node != value) {
		int level = klmalloc_random_level();
		/*
		 * Get all of the nodes before this.
		 */
		if (level > klmalloc_big_bins.level) {
			for (i = klmalloc_big_bins.level + 1; i <= level; ++i) {
				update[i] = &klmalloc_big_bins.head;
			}
			klmalloc_big_bins.level = level;
		}

		/*
		 * Make the new node.
		 */
		node = value;

		/*
		 * Run through and point the preceeding nodes
		 * for each level to the new node.
		 */
		for (i = 0; i <= level; ++i) {
			node->forward[i] = update[i]->forward[i];
			if (node->forward[i])
				assert((node->forward[i]->size + sizeof(klmalloc_big_bin_header)) % PAGE_SIZE == 0);
			update[i]->forward[i] = node;
		}
	}
}

/*
 * Delete a header from the skip list.
 * Be sure you didn't change the size, or we won't be able to find it.
 */
static void klmalloc_skip_list_delete(klmalloc_big_bin_header * value) {
	/*
	 * Debug assertions
	 */
	assert(value != NULL);
	assert(value->head);
	assert((uintptr_t)value->head > (uintptr_t)value);
	if (value->size > NUM_BINS) {
		assert((uintptr_t)value->head < (uintptr_t)value + value->size);
	} else {
		assert((uintptr_t)value->head < (uintptr_t)value + PAGE_SIZE);
	}

	/*
	 * Starting from the bin header, again...
	 */
	klmalloc_big_bin_header * node = &klmalloc_big_bins.head;
	klmalloc_big_bin_header * update[SKIP_MAX_LEVEL + 1];

	/*
	 * Find the node.
	 */
	int i;
	for (i = klmalloc_big_bins.level; i >= 0; --i) {
		while (node->forward[i] && node->forward[i]->size < value->size) {
			node = node->forward[i];
			if (node)
				assert((node->size + sizeof(klmalloc_big_bin_header)) % PAGE_SIZE == 0);
		}
		update[i] = node;
	}
	node = node->forward[0];
	while (node != value) {
		node = node->forward[0];
	}

	if (node != value) {
		node = klmalloc_big_bins.head.forward[0];
		while (node->forward[0] && node->forward[0] != value) {
			node = node->forward[0];
		}
		node = node->forward[0];
	}
	/*
	 * If we found the node, delete it;
	 * otherwise, we do nothing.
	 */
	if (node == value) {
		for (i = 0; i <= klmalloc_big_bins.level; ++i) {
			if (update[i]->forward[i] != node) {
				break;
			}
			update[i]->forward[i] = node->forward[i];
			if (update[i]->forward[i]) {
				assert((uintptr_t)(update[i]->forward[i]) % PAGE_SIZE == 0);
				assert((update[i]->forward[i]->size + sizeof(klmalloc_big_bin_header)) % PAGE_SIZE == 0);
			}
		}

		while (klmalloc_big_bins.level > 0 && klmalloc_big_bins.head.forward[klmalloc_big_bins.level] == NULL) {
			--klmalloc_big_bins.level;
		}
	}
}

/* }}} */
/* Stack {{{ */
/*
 * Pop an item from a block.
 * Free space is stored as a stack,
 * so we get a free space for a bin
 * by popping a free node from the
 * top of the stack.
 */
static void * klmalloc_stack_pop(klmalloc_bin_header *header) {

	LOG(DEBUG, "klmalloc_stack_pop header 0x%llX\n", header);
	LOG(DEBUG, "header->head = 0x%llX next = 0x%llX\n", header->head, *((uintptr_t**)header->head));

	assert(header);
	assert(header->head != NULL);
	
	// !!!: This is a problem area. Soem bins actually do have their head point to themselves.
	// !!!: It doesn't appear to be an issue (from what I can tell unfortunately), but I will warn if it happens
	assert_nonfatal((uintptr_t)header->head > (uintptr_t)header);
	if (header->size > NUM_BINS) {
		assert((uintptr_t)header->head < (uintptr_t)header + header->size);
	} else {
		assert((uintptr_t)header->head < (uintptr_t)header + PAGE_SIZE);
		assert_nonfatal((uintptr_t)header->head > (uintptr_t)header + sizeof(klmalloc_bin_header) - 1);
	}
	
	/*
	 * Remove the current head and point
	 * the head to where the old head pointed.
	 */
	void *item = header->head;
	uintptr_t **head = header->head;
	uintptr_t *next = *head;
	header->head = next;
	return item;
}

/*
 * Push an item into a block.
 * When we free memory, we need
 * to add the freed cell back
 * into the stack of free spaces
 * for the block.
 */
static void klmalloc_stack_push(klmalloc_bin_header *header, void *ptr) {
	assert(ptr != NULL);
	assert((uintptr_t)ptr > (uintptr_t)header);
	if (header->size > NUM_BINS) {
		assert((uintptr_t)ptr < (uintptr_t)header + header->size);
	} else {
		assert((((uintptr_t)ptr - sizeof(klmalloc_bin_header)) & ((1UL << (header->size + SMALLEST_BIN_LOG)) - 1)) == 0);
		assert((uintptr_t)ptr < (uintptr_t)header + PAGE_SIZE);
	}
	uintptr_t **item = (uintptr_t **)ptr;
	*item = (uintptr_t *)header->head;
	header->head = item;
}

/*
 * Is this cell stack empty?
 * If the head of the stack points
 * to NULL, we have exhausted the
 * stack, so there is no more free
 * space available in the block.
 */
static inline int __attribute__ ((always_inline)) klmalloc_stack_empty(klmalloc_bin_header *header) {
	return header->head == NULL;
}

/* }}} Stack */

/* malloc() {{{ */
static void * __attribute__ ((malloc)) klmalloc(uintptr_t size) {
	/*
	 * C standard implementation:
	 * If size is zero, we can choose do a number of things.
	 * This implementation will return a NULL pointer.
	 */
	if (__builtin_expect(size == 0, 0))
		return NULL;

	/*
	 * Find the appropriate bin for the requested
	 * allocation and start looking through that list.
	 */
	unsigned int bucket_id = klmalloc_bin_size(size);

	LOG(DEBUG, "bucket_id = %i\n", bucket_id);

	if (bucket_id < BIG_BIN) {
		/*
		 * Small bins.
		 */
		klmalloc_bin_header * bin_header = klmalloc_list_head(&klmalloc_bin_head[bucket_id]);
		LOG(DEBUG, "Using bin header at 0x%llX\n", bin_header);
		if (!bin_header) {
			/*
			 * Grow the heap for the new bin.
			 */
			bin_header = (klmalloc_bin_header*)mem_sbrk(PAGE_SIZE);
			bin_header->bin_magic = BIN_MAGIC;
			assert((uintptr_t)bin_header % PAGE_SIZE == 0);

			LOG(DEBUG, "New bin_header created at 0x%llX\n", bin_header);

			/*
			 * Set the head of the stack.
			 */
			bin_header->head = (void*)((uintptr_t)bin_header + sizeof(klmalloc_bin_header));
			/*
			 * Insert the new bin at the front of
			 * the list of bins for this size.
			 */
			klmalloc_list_insert(&klmalloc_bin_head[bucket_id], bin_header);
			/*
			 * Initialize the stack inside the bin.
			 * The stack is initially full, with each
			 * entry pointing to the next until the end
			 * which points to NULL.
			 */
			uintptr_t adj = SMALLEST_BIN_LOG + bucket_id;
			uintptr_t i, available = ((PAGE_SIZE - sizeof(klmalloc_bin_header)) >> adj) - 1;

			uintptr_t **base = bin_header->head;
			for (i = 0; i < available; ++i) {
				/*
				 * Our available memory is made into a stack, with each
				 * piece of memory turned into a pointer to the next
				 * available piece. When we want to get a new piece
				 * of memory from this block, we just pop off a free
				 * spot and give its address.
				 */
				base[i << bucket_id] = (uintptr_t *)&base[(i + 1) << bucket_id];
			}
			base[available << bucket_id] = NULL;
			bin_header->size = bucket_id;
		} else {
			assert(bin_header->bin_magic == BIN_MAGIC);
		}
		uintptr_t ** item = klmalloc_stack_pop(bin_header);
		if (klmalloc_stack_empty(bin_header)) {
			klmalloc_list_decouple(&(klmalloc_bin_head[bucket_id]),bin_header);
		}

		return item;
	} else {
		/*
		 * Big bins.
		 */
		klmalloc_big_bin_header * bin_header = klmalloc_skip_list_findbest(size);
		LOG(DEBUG, "Using big bin header 0x%llX\n", bin_header);
		if (bin_header) {
			assert(bin_header->size >= size);
			/*
			 * If we found one, delete it from the skip list
			 */
			klmalloc_skip_list_delete(bin_header);
			/*
			 * Retreive the head of the block.
			 */
			uintptr_t ** item = klmalloc_stack_pop((klmalloc_bin_header *)bin_header);

			return item;
		} else {
			/*
			 * Round requested size to a set of pages, plus the header size.
			 */
			uintptr_t pages = (size + sizeof(klmalloc_big_bin_header)) / PAGE_SIZE + 1;
			bin_header = (klmalloc_big_bin_header*)mem_sbrk(PAGE_SIZE * pages);
			bin_header->bin_magic = BIN_MAGIC;
			
			LOG(DEBUG, "(big_bins) New bin_header allocated to 0x%llX\n", bin_header);

			assert((uintptr_t)bin_header % PAGE_SIZE == 0);
			/*
			 * Give the header the remaining space.
			 */
			bin_header->size = pages * PAGE_SIZE - sizeof(klmalloc_big_bin_header);
			assert((bin_header->size + sizeof(klmalloc_big_bin_header)) % PAGE_SIZE == 0);
			/*
			 * Link the block in physical memory.
			 */
			bin_header->prev = klmalloc_newest_big;
			if (bin_header->prev) {
				bin_header->prev->next = bin_header;
			}
			klmalloc_newest_big = bin_header;
			bin_header->next = NULL;
			/*
			 * Return the head of the block.
			 */
			bin_header->head = NULL;
			return (void*)((uintptr_t)bin_header + sizeof(klmalloc_big_bin_header));
		}
	}
}
/* }}} */
/* free() {{{ */
static void klfree(void *ptr) {
	/*
	 * C standard implementation: Do nothing when NULL is passed to free.
	 */
	if (__builtin_expect(ptr == NULL, 0)) {
		return;
	}

	/*
	 * Woah, woah, hold on, was this a page-aligned block?
	 */
	if ((uintptr_t)ptr % PAGE_SIZE == 0) {
		/*
		 * Well howdy-do, it was.
		 */
		ptr = (void *)((uintptr_t)ptr - 1);
	}

	/*
	 * Get our pointer to the head of this block by
	 * page aligning it.
	 */
	klmalloc_bin_header * header = (klmalloc_bin_header *)((uintptr_t)ptr & (uintptr_t)~PAGE_MASK);
	assert((uintptr_t)header % PAGE_SIZE == 0);

	if (header->bin_magic != BIN_MAGIC)
		return;

	/*
	 * For small bins, the bin number is stored in the size
	 * field of the header. For large bins, the actual size
	 * available in the bin is stored in this field. It's
	 * easy to tell which is which, though.
	 */
	uintptr_t bucket_id = header->size;
	if (bucket_id > (uintptr_t)NUM_BINS) {
		bucket_id = BIG_BIN;
		klmalloc_big_bin_header *bheader = (klmalloc_big_bin_header*)header;
		
		assert(bheader);
		assert(bheader->head == NULL);
		assert((bheader->size + sizeof(klmalloc_big_bin_header)) % PAGE_SIZE == 0);
		/*
		 * Push new space back into the stack.
		 */
		klmalloc_stack_push((klmalloc_bin_header *)bheader, (void *)((uintptr_t)bheader + sizeof(klmalloc_big_bin_header)));
		assert(bheader->head != NULL);
		/*
		 * Insert the block into list of available slabs.
		 */
		klmalloc_skip_list_insert(bheader);
	} else {

		/*
		 * If the stack is empty, we are freeing
		 * a block from a previously full bin.
		 * Return it to the busy bins list.
		 */
		if (klmalloc_stack_empty(header)) {
			klmalloc_list_insert(&klmalloc_bin_head[bucket_id], header);
		}
		/*
		 * Push new space back into the stack.
		 */
		klmalloc_stack_push(header, ptr);
	}
}
/* }}} */
/* valloc() {{{ */
static void * __attribute__ ((malloc)) klvalloc(uintptr_t size) {
	/*
	 * Allocate a page-aligned block.
	 * XXX: THIS IS HORRIBLY, HORRIBLY WASTEFUL!! ONLY USE THIS
	 *      IF YOU KNOW WHAT YOU ARE DOING!
	 */
	uintptr_t true_size = size + PAGE_SIZE - sizeof(klmalloc_big_bin_header); /* Here we go... */
	void * result = klmalloc(true_size);
	void * out = (void *)((uintptr_t)result + (PAGE_SIZE - sizeof(klmalloc_big_bin_header)));
	assert((uintptr_t)out % PAGE_SIZE == 0);
	return out;
}
/* }}} */
/* realloc() {{{ */
static void * __attribute__ ((malloc)) klrealloc(void *ptr, uintptr_t size) {
	/*
	 * C standard implementation: When NULL is passed to realloc,
	 * simply malloc the requested size and return a pointer to that.
	 */
	if (__builtin_expect(ptr == NULL, 0))
		return klmalloc(size);

	/*
	 * C standard implementation: For a size of zero, free the
	 * pointer and return NULL, allocating no new memory.
	 */
	if (__builtin_expect(size == 0, 0))
	{
		alloc_free(ptr);
		return NULL;
	}

	/*
	 * Find the bin for the given pointer
	 * by aligning it to a page.
	 */
	klmalloc_bin_header * header_old = (void *)((uintptr_t)ptr & (uintptr_t)~PAGE_MASK);
	if (header_old->bin_magic != BIN_MAGIC) {
		assert(0 && "Bad magic on realloc.");
		return NULL;
	}

	uintptr_t old_size = header_old->size;
	if (old_size < (uintptr_t)BIG_BIN) {
		/*
		 * If we are copying from a small bin,
		 * we need to get the size of the bin
		 * from its id.
		 */
		old_size = (1UL << (SMALLEST_BIN_LOG + old_size));
	}

	/*
	 * (This will only happen for a big bin, mathematically speaking)
	 * If we still have room in our bin for the additonal space,
	 * we don't need to do anything.
	 */
	if (old_size >= size) {

		/*
		 * TODO: Break apart blocks here, which is far more important
		 *       than breaking them up on allocations.
		 */
		return ptr;
	}

	/*
	 * Reallocate more memory.
	 */
	void * newptr = klmalloc(size);
	if (__builtin_expect(newptr != NULL, 1)) {

		/*
		 * Copy the old value into the new value.
		 * Be sure to only copy as much as was in
		 * the old block.
		 */
		memcpy(newptr, ptr, old_size);
		klfree(ptr);
		return newptr;
	}

	/*
	 * We failed to allocate more memory,
	 * which means we're probably out.
	 *
	 * Bail and return NULL.
	 */
	return NULL;
}
/* }}} */
/* calloc() {{{ */
static void * __attribute__ ((malloc)) klcalloc(uintptr_t nmemb, uintptr_t size) {
	/*
	 * Allocate memory and zero it before returning
	 * a pointer to the newly allocated memory.
	 * 
	 * Implemented by way of a simple malloc followed
	 * by a memset to 0x00 across the length of the
	 * requested memory chunk.
	 */

	void *ptr = klmalloc(nmemb * size);
	if (__builtin_expect(ptr != NULL, 1))
		memset(ptr,0x00,nmemb * size);
	return ptr;
}
/* }}} */

