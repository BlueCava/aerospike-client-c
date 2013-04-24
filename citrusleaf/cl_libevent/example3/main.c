/*
 *  Citrusleaf Tools
 *  src/ascli.c - command-line interface
 *
 *  Copyright 2008 by Citrusleaf.  All rights reserved.
 *  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE.  THE COPYRIGHT NOTICE
 *  ABOVE DOES NOT EVIDENCE ANY ACTUAL OR INTENDED PUBLICATION.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <getopt.h>

#include <event.h>
#include <evdns.h>

#include "citrusleaf_event/evcitrusleaf.h"

typedef struct config_s {
	
	char *host;
	int   port;
	char *ns;
	char *set;
	
	bool verbose;
	bool follow;
	
	int  timeout_ms;
	
	evcitrusleaf_object  o_key;
		
	evcitrusleaf_cluster	*asc;
	
	int	return_value;   // return value from the test
	
	uint32_t	blob_size;
	uint8_t	*blob;
	
} config;

config g_config;


void
test_terminate(int r)
{
	g_config.return_value = r;
	struct timeval x = { 0, 0 };
	event_loopexit(&x);
}

#define TEST_BLOB_SZ 1200


void
example_phase_three(int return_value, evcitrusleaf_bin *bins, int n_bins, uint32_t generation, void *udata)
{
	config *c = (config *) udata;
	
	fprintf(stderr, "example3 phase 3 get - received\n");
	
	if (return_value != 0) {
		fprintf(stderr, "example has FAILED? stage 3 return value %d\n",return_value);
		test_terminate(return_value);
		return;
	}

	// validate the request from phase II
	if (n_bins == 1) {
		fprintf(stderr, "It appears you have single-bin set in this namespace. If so, your test succeeds.\n");
	}
	else if (n_bins != 4) {
		fprintf(stderr, "Get all returned wrong number of bins: is %d should be 4\n",n_bins);
		test_terminate(return_value);
		return;
	}
	
	fprintf(stderr, "get all returned %d bins\n",n_bins);
	bool ok1, ok2, ok3, ok4;
	ok1 = ok2 = ok3 = ok4 = false;
	
	
	for (int i=0;i<n_bins;i++) {
		if (n_bins == 1) { // single bin namespace does not store bin names
			if (strcmp(bins[i].bin_name, "") != 0) {
				fprintf(stderr, "bin from single bin namespace has non-null bin name, FAIL\n");
				c->return_value = -1;
			}
			// storing 4 bins on the record will result in the last one being saved and the others being thrown away
			// check that what was stored is correct
			if (bins[i].object.type != CL_BLOB) {
				fprintf(stderr, "bin in single bin namespace has wrong type, should be blob, FAIL\n");
				c->return_value = -1;
				break;
			}
			if (bins[i].object.size != TEST_BLOB_SZ) {
				fprintf(stderr, "bin in single bin namespace has wrong size, should be 11000, FAIL\n");
				c->return_value = -1;
				break;
			}
			uint8_t *blob = bins[i].object.u.blob;
			for (uint i=0;i<TEST_BLOB_SZ;i++) {
				if (blob[i] != (i & 0xFF)) {
					fprintf(stderr, "bin in single bin namespace has wrong value at offset %d, FAIL\n",i);
				}
				c->return_value = -1;
				break;
			}
			fprintf(stderr, "The BLOB was received correctly\n");
			ok1 = true;
			ok2 = true;
			ok3 = true;
			ok4 = true;
			break;
		}
	}
	
	if (bins)		evcitrusleaf_bins_free(bins, n_bins);
	
	if (ok1 && ok2 && ok3 && ok4) {
		fprintf(stderr,"citrusleaf getall succeeded\n");
		test_terminate(0);
	}
	else {
		fprintf(stderr, "citrusleaf getall FAILED: an expected bin was not received\n");
		test_terminate(-1);
	}
	
	

}

void
example_phase_two(int return_value, evcitrusleaf_bin *bins, int n_bins, uint32_t generation, void *udata)
{
	config *c = (config *) udata;

	if (return_value != EVCITRUSLEAF_OK) {
		fprintf(stderr, "put failed: return code %d\n",return_value);
		test_terminate(return_value);
		return;
	}
	
	if (bins)		evcitrusleaf_bins_free(bins, n_bins);

	// Get all the values in this key (enjoy the fine c99 standard)
	if (0 != evcitrusleaf_get_all(c->asc, c->ns, c->set, &c->o_key, c->timeout_ms,
				example_phase_three, c)) {
		fprintf(stderr, "get after put could not dispatch\n");
		test_terminate(-1);
		return;
	}
	fprintf(stderr, "get all dispatched\n");

}

// this is kind of a wacky test, especially for single-bin
#if 0

void
example_phase_one(config *c)
{
	// Set up a key and bin etc the key, used in all phases ---
	// be a little careful here, because the o_key will contain a pointer
	// to strings
	fprintf(stderr, "using key: 0x451bf9231\n");
	evcitrusleaf_object_init_int(&c->o_key, 0x451bf9231);
	
	// Create 4 bins - single bin namespace? what will  the client do?
	evcitrusleaf_bin values[4];
	strcpy(values[0].bin_name, "test_bin_one");
	evcitrusleaf_object_init_str(&values[0].object, "example_value_one");
	strcpy(values[1].bin_name, "test_bin_two");
	evcitrusleaf_object_init_int(&values[1].object, 0xDEADBEEF);
	// warning! the integer here is signed, so the C code will work properly
	// abusing the sign bit this way, but another language will see the value
	// as negative.
	strcpy(values[2].bin_name, "test_bin_three");
	evcitrusleaf_object_init_int(&values[2].object, 0xDEADBEEF12341234);
	strcpy(values[3].bin_name, "test_bin_four");
	uint8_t blob[11000];
	for (int i=0;i<sizeof(blob);i++)
		blob[i] = i;
	evcitrusleaf_object_init_blob(&values[3].object, blob, sizeof(blob) );
	
	evcitrusleaf_write_parameters wparam;
	evcitrusleaf_write_parameters_init(&wparam);
	
	int rv = evcitrusleaf_put(c->asc, c->ns, c->set, &c->o_key, values, 4, &wparam, 0, example_phase_two, c);
	if (rv != 0) {
		fprintf(stderr, "citrusleaf put failed: error code %d\n",rv);
		test_terminate(rv);
		return;
	}
	fprintf(stderr, "citrusleaf put dispatched\n");
	
}

#endif



void
example_phase_one(config *c)
{
	// Set up a key and bin etc the key, used in all phases ---
	// be a little careful here, because the o_key will contain a pointer
	// to strings
	fprintf(stderr, "using key: 0x451bf9231\n");
	evcitrusleaf_object_init_int(&c->o_key, 0x451bf9231);
	
	// Create 4 bins - single bin namespace? what will  the client do?
	evcitrusleaf_bin values[1];
	strcpy(values[0].bin_name, "");
	uint8_t blob[TEST_BLOB_SZ];
	for (int i=0;i<sizeof(blob);i++)
		blob[i] = i;
	evcitrusleaf_object_init_blob(&values[0].object, blob, sizeof(blob) );
	
	evcitrusleaf_write_parameters wparam;
	evcitrusleaf_write_parameters_init(&wparam);
	
	int rv = evcitrusleaf_put(c->asc, c->ns, c->set, &c->o_key, values, 1, &wparam, c->timeout_ms, example_phase_two, c);
	if (rv != 0) {
		fprintf(stderr, "citrusleaf put failed: error code %d\n",rv);
		test_terminate(rv);
		return;
	}
	fprintf(stderr, "citrusleaf put dispatched\n");
	
}



void usage(void) {
	fprintf(stderr, "Usage key_c:\n");
	fprintf(stderr, "-h host [default 127.0.0.1] \n");
	fprintf(stderr, "-p port [default 3000]\n");
	fprintf(stderr, "-n namespace [default test]\n");
	fprintf(stderr, "-s set [default ""]\n");
	fprintf(stderr, "-b bin [default value]\n");
	fprintf(stderr, "-m milliseconds timeout [default 200]\n");
	fprintf(stderr, "-v is verbose\n");
}





int
main(int argc, char **argv)
{
	memset(&g_config, 0, sizeof(g_config));
	
	g_config.host = "127.0.0.1";
	g_config.port = 3000;
	g_config.ns = "test";
	g_config.set = "";
	g_config.verbose = false;
	g_config.follow = true;
	g_config.return_value = -1;
	
	int		c;
	
	printf("example of the C libevent citrusleaf library\n");
	
	while ((c = getopt(argc, argv, "h:p:n:s:m:v")) != -1) 
	{
		switch (c)
		{
		case 'h':
			g_config.host = strdup(optarg);
			break;
		
		case 'p':
			g_config.port = atoi(optarg);
			break;
		
		case 'n':
			g_config.ns = strdup(optarg);
			break;
			
		case 's':
			g_config.set = strdup(optarg);
			break;

		case 'm':
			g_config.timeout_ms = atoi(optarg);
			break;
			
		case 'v':
			g_config.verbose = true;
			break;

		case 'f':
			g_config.follow = false;
			break;
			
		default:
			usage();
			return(-1);
			
		}
	}

	fprintf(stderr, "example: host %s port %d ns %s set %s\n",
		g_config.host,g_config.port,g_config.ns,g_config.set);

	fprintf(stderr, "EXAMPLE3 -- tests functional behavior using bins,\n");
	fprintf(stderr, "         blobs, etc in a single-bin configuration\n");
	
	event_init();			// initialize the libevent system
	evdns_init();			// used by the cluster system for async name resolution
	// evdns_resolv_conf_parse(DNS_OPTIONS_ALL, "/etc/resolv.conf"); ???
	
	evcitrusleaf_init();    // initialize citrusleaf
	
	// Create a citrusleaf cluster object for subsequent requests
	g_config.asc = evcitrusleaf_cluster_create();
	if (!g_config.asc) {
		fprintf(stderr, "could not create cluster, internal error\n");
		return(-1);
	}
	
	evcitrusleaf_cluster_add_host(g_config.asc, g_config.host, g_config.port);
	
	// Start the train of example events
	example_phase_one(&g_config);
	
	// Burrow into the libevent event loop
	event_dispatch();

	evcitrusleaf_shutdown(false);
	
	if (g_config.return_value != 0)
		fprintf(stderr, "test complete: FAILED return value %d\n",g_config.return_value);
	else
		fprintf(stderr, "test complete: SUCCESS\n");
		
	
	return(0);
}