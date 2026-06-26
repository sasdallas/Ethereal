/**
 * @file hexahedron/drivers/net/route.c
 * @brief Routing table system
 * 
 * This routing table is modeled after the IPv4 Routing Trie seen
 * in the Linux kernel
 * 
 * @copyright
 * This file is part of the Hexahedron kernel, which is part of the Ethereal Operating System.
 * It is released under the terms of the BSD 3-clause license.
 * Please see the LICENSE file in the main repository for more details.
 * 
 * Copyright (C) 2026 Samuel Stuart
 */

#include <kernel/drivers/net/route.h>
#include <kernel/mm/alloc.h>
#include <kernel/mm/slab.h>
#include <kernel/lock/rwsem.h>
#include <kernel/init.h>
#include <kernel/debug.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <structs/hashmap.h>
#include <assert.h>

/* Routing table map */
route_ipv4_trie_entry_t *route_ipv4_trie = NULL;
rwsem_t route_table_lock = RWSEM_INITIALIZER;

/* Slabs */
slab_cache_t *route_cache = NULL;
slab_cache_t *trie_cache = NULL;

/* Log method */
#define LOG(status, ...) dprintf_module(status, "NET:ROUTE", __VA_ARGS__)

/**
 * @brief Calculate CIDR length
 */
static int route_getCIDRLength(in_addr_t a) {
    return ((~a) != 0) ? __builtin_clz(~a) : 0; 
}

/**
 * @brief Locate a route in the IPv4 routing table
 * @param ip The target destination IP (host byte order)
 * @returns Routing entry, or NULL if it could not be found
 */
route_ipv4_t *route_lookup(in_addr_t ip) {
    rwsem_startRead(&route_table_lock);

    // Key it
    route_ipv4_trie_entry_t *c = route_ipv4_trie;
    route_ipv4_t *best = NULL;
    int cidr_best = -1;
    while (c) {
        if ((ip & c->mask) == (c->dest & c->mask)) {
            if (c->route && (!best || (cidr_best <= c->cidr_length))) {
                route_ipv4_t *r = c->route;
                while (r && (r->flags & RT_FLAG_UP) == 0) r = r->next;

                if (r) {
                    best = r;
                    cidr_best = c->cidr_length;
                } else {
                    assert(0 && "no routes are up, case unhandled");
                }
            }
        } else {
            // diverged
            break;
        }

        if (c->cidr_length >= 32) break;

        int bit = (ip >> (31 - c->cidr_length)) & 1;
        c = c->child[bit];
    }
    

    rwsem_finishRead(&route_table_lock);
    return best;
}

/**
 * @brief Add a route to the IPv4 routing table
 * @param route The route to add
 * @returns 0 on success
 */
int route_add(route_ipv4_t *route_in) {
    // First re-allocate the route structure
    route_ipv4_t *route = slab_allocate(route_cache);
    memcpy(route, route_in, sizeof(route_ipv4_t));
    route->next = NULL;

    LOG(DEBUG, "Creating new IPv4 route for address %8X gateway %8X subnet %8X metric %d flags %x\n", route_in->dest, route_in->gw, route_in->netmask, route_in->metric, route_in->flags);

    int cidr = route_getCIDRLength(route->netmask);

    // start a walk
    rwsem_startWrite(&route_table_lock);

    route_ipv4_trie_entry_t **ent = &route_ipv4_trie;
    while (*ent) {
        route_ipv4_trie_entry_t *this = *ent;

        if (this->cidr_length == cidr && this->dest == route->dest) {
            // The route for this entry already exists, sort it by metric
            route_ipv4_t **tgt = &this->route;
            while (*tgt) {
                if ((*tgt)->metric <= route->metric) break;
                tgt = &(*tgt)->next;
            }

            // Add it here
            route->next = *tgt;
            *tgt = route;
            
            goto _leave;
        }

        // find divergence
        uint32_t div = this->dest ^ route->dest;
        int common = div ? (__builtin_clz(div)) : 32;
        // Determine the maximum possible common bits before one prefix terminates
        int max_possible_common = (this->cidr_length < cidr) ? this->cidr_length : cidr;

        if (common < max_possible_common) {
            route_ipv4_trie_entry_t *split = slab_allocate(trie_cache);
            split->cidr_length = common;
            split->mask = common ? (~0U << (32 - common)) : 0;
            split->dest = route->dest & split->mask;
            split->route = NULL;

            int this_bit = (this->dest >> (31 - common)) & 1;
            split->child[this_bit] = this;

            // Create the new leaf node for our route
            route_ipv4_trie_entry_t *new_node = slab_allocate(trie_cache);
            new_node->cidr_length = cidr;
            new_node->mask = route->netmask;
            new_node->dest = route->dest;
            new_node->route = route;
            new_node->child[0] = new_node->child[1] = NULL;

            split->child[!this_bit] = new_node;
            *ent = split;
            goto _leave;
        }

        if (cidr < this->cidr_length) {
            route_ipv4_trie_entry_t *new_node = slab_allocate(trie_cache);
            new_node->cidr_length = cidr;
            new_node->mask = route->netmask;
            new_node->dest = route->dest;
            new_node->route = route;

            int this_bit = (this->dest >> (31 - cidr)) & 1;
            new_node->child[this_bit] = this;
            new_node->child[!this_bit] = NULL;

            *ent = new_node;
            goto _leave;
        }
        

        int bit = (route->dest >> (31 - this->cidr_length)) & 1;
        ent = &this->child[bit];
    }

    // Create a new trie entry
    route_ipv4_trie_entry_t *split = slab_allocate(trie_cache);
    split->cidr_length = cidr;
    split->mask = route->netmask;
    split->dest = route->dest;
    split->route = route;
    split->child[0] = split->child[1] = NULL;
    route->next = NULL;
    *ent = split;

_leave:
    rwsem_finishWrite(&route_table_lock);
    return 0;
}

/**
 * @brief Init
 */
static int route_init() {
    route_cache = slab_createCache("ipv4 route cache", SLAB_CACHE_DEFAULT, sizeof(route_ipv4_t), 0, NULL, NULL);
    trie_cache = slab_createCache("ipv4 trie node cache", SLAB_CACHE_DEFAULT, sizeof(route_ipv4_trie_entry_t), 0, NULL, NULL);
    return 0;
}

NET_INIT_ROUTINE(route, INIT_FLAG_DEFAULT, route_init);
