/******************************************************************************
 * Copyright 2008-2014 by Aerospike.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy 
 * of this software and associated documentation files (the "Software"), to 
 * deal in the Software without restriction, including without limitation the 
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or 
 * sell copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *****************************************************************************/
#include <aerospike/as_lookup.h>
#include <citrusleaf/cl_info.h>
#include <citrusleaf/cf_log_internal.h>
#include <citrusleaf/cf_byte_order.h>
#include <netdb.h>

bool
as_lookup(as_cluster* cluster, char* hostname, uint16_t port, bool enable_warning, as_vector* /*<struct sockaddr_in>*/ addresses)
{
	// Check if there is an alternate address that should be used for this hostname.
	if (cluster && cluster->ip_map) {
		as_addr_map* entry = cluster->ip_map;
		
		for (uint32_t i = 0; i < cluster->ip_map_size; i++) {
			if (strcmp(entry->orig, hostname) == 0) {
				// Found mapping for this address.  Use alternate.
				cf_debug("Using %s instead of %s", entry->alt, hostname);
				hostname = entry->alt;
				break;
			}
			entry++;
		}
	}
	
	// Lookup TCP addresses.
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	
	struct addrinfo* results = 0;
	int ret = getaddrinfo(hostname, 0, &hints, &results);
	
	if (ret) {
		if (enable_warning) {
			cf_warn("Invalid hostname %s: %s", hostname, gai_strerror(ret));
		}
		return false;
	}
	
	// Add addresses to vector if it exists.
	if (addresses) {
		uint16_t port_be = cf_swap_to_be16(port);
		
		for (struct addrinfo* r = results; r; r = r->ai_next) {
			struct sockaddr_in* addr = (struct sockaddr_in*)r->ai_addr;
			addr->sin_port = port_be;
			as_vector_append_unique(addresses, addr);
		}
	}
	
	freeaddrinfo(results);
	return true;
}
