#include "buddy.h"

#define NULL ((void *)0)
#define MAX_RANK 16
#define PAGE_SIZE 4096
#define MAX_PAGES 65536

typedef struct {
    int next;
} freenode_t;

static void *base_addr = NULL;
static int total_pages = 0;

static unsigned char g_page_rank[MAX_PAGES];
static unsigned char g_page_free[MAX_PAGES];
static unsigned char g_page_alloc[MAX_PAGES];
static freenode_t g_nodes[MAX_PAGES];

static unsigned char *page_rank;
static unsigned char *page_free;
static unsigned char *page_alloc;
static freenode_t *nodes;

/* Free lists heads for rank 1..MAX_RANK; -1 if empty */
static int free_list[MAX_RANK + 1];

static int floor_log2(int x) {
    int r = 0;
    while ((1 << (r + 1)) <= x)
        r++;
    return r;
}

/* number of pages in a block of given rank: 2^(rank-1) */
static int rank_pages(int rank) {
    return 1 << (rank - 1);
}

static int addr_to_idx(void *p) {
    unsigned long off;

    if (p < base_addr)
        return -1;
    off = (unsigned long)p - (unsigned long)base_addr;
    if (off % PAGE_SIZE != 0)
        return -1;
    {
        int idx = (int)(off / PAGE_SIZE);
        if (idx < 0 || idx >= total_pages)
            return -1;
        return idx;
    }
}

static void *idx_to_addr(int idx) {
    return (void *)((char *)base_addr + (unsigned long)idx * PAGE_SIZE);
}

static void list_push(int rank, int idx) {
    nodes[idx].next = free_list[rank];
    free_list[rank] = idx;
    page_free[idx] = 1;
    page_rank[idx] = (unsigned char)rank;
    page_alloc[idx] = 0;
}

static int list_pop(int rank) {
    int idx = free_list[rank];
    if (idx < 0)
        return -1;
    free_list[rank] = nodes[idx].next;
    page_free[idx] = 0;
    return idx;
}

static void list_remove(int rank, int idx) {
    int *pp = &free_list[rank];
    while (*pp >= 0) {
        if (*pp == idx) {
            *pp = nodes[idx].next;
            page_free[idx] = 0;
            return;
        }
        pp = &nodes[*pp].next;
    }
}

int init_page(void *p, int pgcount) {
    int i, rank, idx, sz, remaining;

    if (p == NULL || pgcount <= 0 || pgcount > MAX_PAGES)
        return -EINVAL;

    base_addr = p;
    total_pages = pgcount;
    page_rank = g_page_rank;
    page_free = g_page_free;
    page_alloc = g_page_alloc;
    nodes = g_nodes;

    for (i = 0; i <= MAX_RANK; i++)
        free_list[i] = -1;

    for (i = 0; i < pgcount; i++) {
        page_rank[i] = 0;
        page_free[i] = 0;
        page_alloc[i] = 0;
        nodes[i].next = -1;
    }

    /* Cover [0, pgcount) with largest buddy-aligned free blocks */
    remaining = pgcount;
    idx = 0;
    while (remaining > 0) {
        rank = 1 + floor_log2(remaining);
        if (rank > MAX_RANK)
            rank = MAX_RANK;
        while (rank > 1) {
            sz = rank_pages(rank);
            if ((idx & (sz - 1)) == 0 && sz <= remaining)
                break;
            rank--;
        }
        sz = rank_pages(rank);
        list_push(rank, idx);
        idx += sz;
        remaining -= sz;
    }

    return OK;
}

void *alloc_pages(int rank) {
    int r, idx, half, buddy;

    if (rank < 1 || rank > MAX_RANK)
        return ERR_PTR(-EINVAL);

    for (r = rank; r <= MAX_RANK; r++) {
        if (free_list[r] >= 0)
            break;
    }
    if (r > MAX_RANK)
        return ERR_PTR(-ENOSPC);

    idx = list_pop(r);

    while (r > rank) {
        r--;
        half = rank_pages(r);
        buddy = idx + half;
        list_push(r, buddy);
    }

    page_alloc[idx] = 1;
    page_rank[idx] = (unsigned char)rank;
    page_free[idx] = 0;

    return idx_to_addr(idx);
}

int return_pages(void *p) {
    int idx, rank, sz, buddy;

    if (p == NULL || base_addr == NULL)
        return -EINVAL;

    idx = addr_to_idx(p);
    if (idx < 0)
        return -EINVAL;

    if (!page_alloc[idx])
        return -EINVAL;

    rank = (int)page_rank[idx];
    page_alloc[idx] = 0;

    while (rank < MAX_RANK) {
        sz = rank_pages(rank);
        buddy = idx ^ sz;
        if (buddy < 0 || buddy + sz > total_pages)
            break;
        if (!page_free[buddy] || (int)page_rank[buddy] != rank)
            break;
        list_remove(rank, buddy);
        if (buddy < idx)
            idx = buddy;
        rank++;
    }

    list_push(rank, idx);
    return OK;
}

int query_ranks(void *p) {
    int idx, i;

    if (p == NULL || base_addr == NULL)
        return -EINVAL;

    idx = addr_to_idx(p);
    if (idx < 0)
        return -EINVAL;

    if (page_alloc[idx] || page_free[idx])
        return (int)page_rank[idx];

    /* Find covering block by walking from the start of the region */
    i = 0;
    while (i <= idx && i < total_pages) {
        if (page_alloc[i] || page_free[i]) {
            int sz = rank_pages((int)page_rank[i]);
            if (i + sz > idx)
                return (int)page_rank[i];
            i += sz;
        } else {
            i++;
        }
    }

    return -EINVAL;
}

int query_page_counts(int rank) {
    int idx, cnt;

    if (rank < 1 || rank > MAX_RANK)
        return -EINVAL;

    cnt = 0;
    idx = free_list[rank];
    while (idx >= 0) {
        cnt++;
        idx = nodes[idx].next;
    }
    return cnt;
}
