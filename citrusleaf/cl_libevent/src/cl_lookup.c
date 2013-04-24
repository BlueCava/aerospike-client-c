/*
 * A good, basic C client for the Aerospike protocol
 * Creates a library which is linkable into a variety of systems
 *
 * This module does async DNS lookups using the libevent async DNS system
 *
 * Brian Bulkowski, 2009
 * All rights reserved
 */

#include <sys/types.h>
#include <sys/socket.h> // socket calls
#include <stdio.h>
#include <errno.h> //errno
#include <stdlib.h> //fprintf
#include <unistd.h> // close
#include <string.h>
#include <fcntl.h>
#include <arpa/inet.h>

#include <event.h>
#include <evdns.h>

#include "citrusleaf_event/evcitrusleaf.h"
#include "citrusleaf_event/cl_cluster.h"
#include "citrusleaf/cf_clock.h"
#include "citrusleaf/proto.h"


// #define DEBUG 1


//
// Tries to do an immediate, local conversion, which works if it's
// a simple dotted-decimal address instead of an actual hostname
//
// fills out the passed-in sockaddr and returns 0 on succes, -1 otherwise


int
cl_lookup_immediate(char *hostname, short port, struct sockaddr_in *sin)
{

	uint32_t addr;
	if (1 == inet_pton(AF_INET, hostname, &addr)) {
		memset(sin, 0, sizeof(*sin));
//		sin->sin_addr.s_addr = htonl(addr);
		sin->sin_addr.s_addr = addr;
		sin->sin_family = AF_INET;
		sin->sin_port = htons(port);
		return(0);
	}
	
	return(-1);
}


//
// Do a lookup on the given name and port.
// Async function using the libevent dns system
// 
// Function will be called back with a stack-allocated
// vector. You can run the vector, look at its size,
// copy bits out.
//
// The lookup function returns an array of the kind of addresses you were looking
// for - so, in this case, uint32
//



typedef struct cl_lookup_state_s {
	cl_lookup_async_fn cb;
	void *udata;
	short port;
} cl_lookup_state;


void
cl_lookup_result_fn(int result, char type, int count, int ttl, void *addresses, void *udata)
{
	cl_lookup_state *cls = (cl_lookup_state *) udata;
	
	uint64_t _s = cf_getms();

	CL_LOG( CL_VERBOSE, "libevent dns result %d type %d count %d ttl %d\n",result,type,count,ttl);
	
	if ((result == 0) && (count > 0) && (type == DNS_IPv4_A)) 
	{
		cf_vector_define(result_v, sizeof(struct sockaddr_in), 0);
		
		uint32_t *s_addr = (uint32_t *)addresses;
		for (int i=0;i<count;i++) {
			struct sockaddr_in sin;
			memset(&sin, 0, sizeof(sin));
			sin.sin_family = AF_INET;
			sin.sin_addr.s_addr = s_addr[i];
			
			CL_LOG(CL_VERBOSE, "libevent dns: %d: %x\n",i,sin.sin_addr.s_addr);
			
			sin.sin_port = htons(cls->port);
			cf_vector_append(&result_v, &sin );
		}
		
		// callback
		(*cls->cb) (0, &result_v, cls->udata);
		
		cf_vector_destroy(&result_v);                        
	}
	else {
		(*cls->cb) (-1, 0, cls->udata);
	}
	
	// cleanup
	free(cls);
	
	uint64_t delta = cf_getms() - _s;
	if (delta > CL_LOG_DELAY_WARN) CL_LOG(CL_WARNING, " CL DELAY: cl_lookup result fn: %"PRIu64"\n",delta);

}

int
cl_lookup(char *hostname, short port, cl_lookup_async_fn cb, void *udata)
{
	
	CL_LOG( CL_VERBOSE, "libevent dns start: hostname %s\n",hostname);

	uint64_t _s = cf_getms();

	cl_lookup_state *cls = malloc(sizeof(cl_lookup_state));
	if (!cls)	return(-1);
	cls->cb = cb;
	cls->udata = udata;
	cls->port = port;
	
	int rv = evdns_resolve_ipv4( hostname, 0 /*search flag*/, cl_lookup_result_fn, cls);
	if (0 != rv) {

		CL_LOG(CL_INFO, "libevent dns fail: hostname %s rv %d\n",hostname, rv);
		free(cls);
		uint64_t delta = cf_getms() - _s;
		if (delta > CL_LOG_DELAY_WARN) CL_LOG(CL_INFO," CL_DELAY: cl_lookup: error: %"PRIu64"\n",delta);
		return(-1);
	}
	uint64_t delta = cf_getms() - _s;
	if (delta > CL_LOG_DELAY_WARN) CL_LOG(CL_WARNING," CL_DELAY: cl_lookup: %"PRIu64"\n",delta);
	return(0);
}	

