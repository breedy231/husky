#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>

#include "hmalloc.h"

// Credit to Nat Tuck for providing starter code.
// Sahaj Kumar - CS3500 HW07


/*
  typedef struct hm_stats {
  long pages_mapped;
  long pages_unmapped;
  long chunks_allocated;
  long chunks_freed;
  long free_length;
  } hm_stats;
*/


// struct used for free list
typedef struct mem_node {
  size_t size;
  struct mem_node* next;  
} mem_node;


// Here is the free list, initialized to NULL
static mem_node* free_list = NULL;
const size_t PAGE_SIZE = 4096;
static hm_stats stats; // This initializes the stats to 0.

// map a new page
mem_node*
new_page() {

  mem_node* new_node = (mem_node*) mmap(0, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

  stats.pages_mapped += 1;

  new_node->size = PAGE_SIZE;
  new_node->next = NULL;

  return new_node;
}

// return length of free list
long
free_list_length()
{
  if (free_list == NULL) {
    return 0;
  }

  mem_node* pointer = free_list;
  long count = 1;

  while (pointer->next != NULL) {
    count += 1;
    pointer = pointer->next;
  }

  return count;
}


hm_stats*
hgetstats()
{
    stats.free_length = free_list_length();
    return &stats;
}

void
hprintstats()
{
    stats.free_length = free_list_length();
    fprintf(stderr, "\n== husky malloc stats ==\n");
    fprintf(stderr, "Mapped:   %ld\n", stats.pages_mapped);
    fprintf(stderr, "Unmapped: %ld\n", stats.pages_unmapped);
    fprintf(stderr, "Allocs:   %ld\n", stats.chunks_allocated);
    fprintf(stderr, "Frees:    %ld\n", stats.chunks_freed);
    fprintf(stderr, "Freelen:  %ld\n", free_list_length());
}

static
size_t
div_up(size_t xx, size_t yy)
{
    // This is useful to calculate # of pages
    // for large allocations.
    size_t zz = xx / yy;

    if (zz * yy == xx) {
        return zz;
    }
    else {
        return zz + 1;
    }
}


mem_node*
get_mem_node(size_t bytes) {

  if (free_list == NULL) {
    return NULL;
  }
  else {
    mem_node* node = free_list;
    if (bytes <= node->size) {
      free_list = node->next;
      return node;
    }
    else {
      while (node->next != NULL) {
	if (bytes <= node->next->size) {
	  mem_node* the_one = node->next;
	  the_one->size = node->next->size;
	  node->next = node->next->next;
	  return the_one;
	}
	node = node->next;
      }
      return NULL;
    }
  }
}

void
rejoin_mem_chunks() {
  
  mem_node* pointer = free_list;
  
  while(pointer != NULL && pointer->next != NULL) {   
    if (((void*)pointer) + pointer->size == pointer->next) {
      pointer->size +=  pointer->next->size;
      pointer->next = pointer->next->next;
    }
    else {
      pointer = pointer->next;
    }
  }
}


void
add_to_free_list(mem_node* node) {
  
  if (free_list == NULL || node < free_list) {
    node->next = free_list;
    free_list = node;
    return;
  }

  mem_node* pointer = free_list;
  
  while (pointer->next != NULL && pointer->next < node) {
    pointer = pointer->next;
  }

  // Add to free list
  node->next = pointer->next;
  pointer->next = node;

  rejoin_mem_chunks();
}


void*
hmalloc(size_t size)
{

    stats.chunks_allocated += 1;

    size_t number_of_bytes = size + sizeof(size_t);

    if (number_of_bytes > PAGE_SIZE) {
      int num_of_pages = div_up(number_of_bytes, PAGE_SIZE);
      void* chunk = mmap(0, PAGE_SIZE * num_of_pages, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
      stats.pages_mapped += num_of_pages;
      *((size_t*)chunk) = PAGE_SIZE * num_of_pages;
      return  chunk + sizeof(size_t);
    }

    // try to get node to use for allocation from free list
    mem_node* node = get_mem_node(number_of_bytes);

    if (node == NULL) {
      node = new_page();
    }
    else {
      // remove one from list
      stats.free_length -= 1;
    }

    // before returning allocated memory, put leftovers back on free list
    size_t leftover_size = node->size - number_of_bytes;

    // NOTE: if leftover size is not bigger than size of memory node, there is
    // no point in adding it back to freelist, so just allocate here anyways.
    if (leftover_size > sizeof(mem_node)) {
      // add back to free_list
      void* pointer = (void*) node;
      mem_node* leftover_mem = (mem_node*) (pointer + number_of_bytes);
      leftover_mem->size = leftover_size;
      stats.free_length += 1;
      add_to_free_list(leftover_mem);
    }
    else {
      // allocate the whole node (leftover is too small)
      number_of_bytes = number_of_bytes + leftover_size;
    }

    *((size_t*)node) = number_of_bytes;
    return ((void*) node) + sizeof(size_t);
}


void
hfree(void* item)
{
    stats.chunks_freed += 1;

    mem_node* node = (mem_node*) (item - sizeof(size_t));


    size_t size = *((size_t*) node);    

    if (size > PAGE_SIZE) {
      int num_of_pages = size / PAGE_SIZE;
      munmap((void*) node, size);
      stats.pages_unmapped += num_of_pages;
    }
    else {
      node->size = size;
      add_to_free_list(node);
      stats.free_length += 1;
    }
    
}

