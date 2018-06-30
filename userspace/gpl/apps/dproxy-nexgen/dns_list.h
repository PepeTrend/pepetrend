#include "dproxy.h"

/*
 * Add a request to dns_request_list.
 * RETURNS: 1 for success, 0 for failure 
 */
int dns_list_add(dns_request_t *r);

/*
 * Scans dns_request_list and compare the id field
 * of each node with that of the id fields of 'r'.
 * RETURNS: pointer to the first node that matches else NULL.
 */
dns_request_t *dns_list_find_by_id(dns_request_t *r);


/*
 * Removes and frees the node pointed to by r
 * RETURNS: 1 for success, 0 for failure
 */
int dns_list_remove(dns_request_t *r);

//<< [BCMBG-NTWK-086] jojopo : Fix static DNS cannot work when DNS proxy is enabled in profile, 2012/08/30

/*** remove dns_list_unarm_requests_after_this and dns_list_unarm_all_requests function via CSP#559223 patch ***/

/*
 * Removes and frees the nodes shared the same dns ip and then
 * swap dns1 and dns1 in the linked list
 * RETURNS: 1 for success, 0 for failure
 */
int dns_list_remove_related_requests_and_swap(dns_request_t *r);
//>> [BCMBG-NTWK-086] End

/*
 * Print out dns_request_list for debuging purposes
 */
void dns_list_print(void);

/*
 * Return the how many seconds left to expire the oldest request
 * RETURNS: seconds
 */
int dns_list_next_time(void);

extern dns_request_t *dns_request_list;
/* Last request in dns_request_list */
extern dns_request_t *dns_request_last;
