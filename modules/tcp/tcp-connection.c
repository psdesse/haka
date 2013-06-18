#include "haka/tcp.h"
#include "haka/tcp-connection.h"
#include <haka/tcp-stream.h>
#include <haka/thread.h>

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>

#include <haka/log.h>
#include <haka/error.h>


struct ctable {
	struct ctable         *prev;
	struct ctable         *next;
	struct tcp_connection tcp_conn;
};

static struct ctable *ct_head = NULL;
static struct ctable *ct_drop_head = NULL;

mutex_t ct_mutex = PTHREAD_MUTEX_INITIALIZER;


void tcp_connection_insert(struct ctable **head, struct ctable *elem)
{
	mutex_lock(&ct_mutex);

	elem->next = *head;
	if (*head) (*head)->prev = elem;
	*head = elem;

	mutex_unlock(&ct_mutex);
}

static struct ctable *tcp_connection_find(struct ctable *head, const struct tcp *tcp, bool *direction_in)
{
	struct ctable *ptr;
	uint16 srcport, dstport;
	ipv4addr srcip, dstip;

	srcip = ipv4_get_src(tcp->packet);
	dstip = ipv4_get_dst(tcp->packet);
	srcport = tcp_get_srcport(tcp);
	dstport = tcp_get_dstport(tcp);

	mutex_lock(&ct_mutex);

	ptr = head;
	while (ptr) {
		if ((ptr->tcp_conn.srcip == srcip) && (ptr->tcp_conn.srcport == srcport) &&
		    (ptr->tcp_conn.dstip == dstip) && (ptr->tcp_conn.dstport == dstport)) {
			mutex_unlock(&ct_mutex);
			if (direction_in) *direction_in = true;
			return ptr;
		}
		if ((ptr->tcp_conn.srcip == dstip) && (ptr->tcp_conn.srcport == dstport) &&
		    (ptr->tcp_conn.dstip == srcip) && (ptr->tcp_conn.dstport == srcport)) {
			mutex_unlock(&ct_mutex);
			if (direction_in) *direction_in = false;
			return ptr;
		}
		ptr = ptr->next;
	}

	mutex_unlock(&ct_mutex);

	return NULL;
}

static void tcp_connection_remove(struct ctable **head, struct ctable *elem)
{
	mutex_lock(&ct_mutex);

	if (elem->prev) {
		elem->prev->next = elem->prev;
	}
	else {
		assert(*head == elem);
		*head = elem->next;
	}

	if (elem->next) elem->next->prev = elem->prev;

	mutex_unlock(&ct_mutex);

	elem->prev = NULL;
	elem->next = NULL;
}

static void tcp_connection_release(struct ctable *elem, bool freemem)
{
	lua_ref_clear(&elem->tcp_conn.lua_table);

	if (elem->tcp_conn.stream_input) {
		stream_destroy(elem->tcp_conn.stream_input);
		elem->tcp_conn.stream_input = NULL;
	}

	if (elem->tcp_conn.stream_output) {
		stream_destroy(elem->tcp_conn.stream_output);
		elem->tcp_conn.stream_output = NULL;
	}

	if (freemem) {
		free(elem);
	}
}


struct tcp_connection *tcp_connection_new(const struct tcp *tcp)
{
	struct ctable *ptr = malloc(sizeof(struct ctable));
	if (!ptr) {
		error(L"memory error");
		return NULL;
	}

	ptr->tcp_conn.srcip = ipv4_get_src(tcp->packet);
	ptr->tcp_conn.dstip = ipv4_get_dst(tcp->packet);
	ptr->tcp_conn.srcport = tcp_get_srcport(tcp);
	ptr->tcp_conn.dstport = tcp_get_dstport(tcp);
	lua_ref_init(&ptr->tcp_conn.lua_table);
	ptr->tcp_conn.stream_input = tcp_stream_create();
	ptr->tcp_conn.stream_output = tcp_stream_create();

	ptr->prev = NULL;
	ptr->next = NULL;

	tcp_connection_insert(&ct_head, ptr);

	{
		char srcip[IPV4_ADDR_STRING_MAXLEN+1], dstip[IPV4_ADDR_STRING_MAXLEN+1];

		ipv4_addr_to_string(ptr->tcp_conn.srcip, srcip, IPV4_ADDR_STRING_MAXLEN);
		ipv4_addr_to_string(ptr->tcp_conn.dstip, dstip, IPV4_ADDR_STRING_MAXLEN);

		messagef(HAKA_LOG_DEBUG, L"tcp-connection", L"opening connection %s:%u -> %s:%u",
				srcip, ptr->tcp_conn.srcport, dstip, ptr->tcp_conn.dstport);
	}

	/* Clear any drop stored connection */
	{
		struct ctable *dropped = tcp_connection_find(ct_drop_head, tcp, NULL);
		if (dropped) {
			tcp_connection_remove(&ct_drop_head, dropped);
			tcp_connection_release(dropped, true);
		}
	}

	return &ptr->tcp_conn;
}

struct tcp_connection *tcp_connection_get(const struct tcp *tcp, bool *direction_in)
{
	struct ctable *elem = tcp_connection_find(ct_head, tcp, direction_in);
	if (elem) {
		return &elem->tcp_conn;
	}
	else {
		return NULL;
	}
}

void tcp_connection_close(struct tcp_connection* tcp_conn)
{
	struct ctable *current = (struct ctable *)((uint8 *)tcp_conn - offsetof(struct ctable, tcp_conn));

	{
		char srcip[IPV4_ADDR_STRING_MAXLEN+1], dstip[IPV4_ADDR_STRING_MAXLEN+1];

		ipv4_addr_to_string(current->tcp_conn.srcip, srcip, IPV4_ADDR_STRING_MAXLEN);
		ipv4_addr_to_string(current->tcp_conn.dstip, dstip, IPV4_ADDR_STRING_MAXLEN);

		messagef(HAKA_LOG_DEBUG, L"tcp-connection", L"closing connection %s:%u -> %s:%u",
				srcip, current->tcp_conn.srcport, dstip, current->tcp_conn.dstport);
	}

	tcp_connection_remove(&ct_head, current);
	tcp_connection_release(current, true);
}

void tcp_connection_drop(struct tcp_connection *tcp_conn)
{
	struct ctable *current = (struct ctable *)((uint8 *)tcp_conn - offsetof(struct ctable, tcp_conn));

	{
		char srcip[IPV4_ADDR_STRING_MAXLEN+1], dstip[IPV4_ADDR_STRING_MAXLEN+1];

		ipv4_addr_to_string(current->tcp_conn.srcip, srcip, IPV4_ADDR_STRING_MAXLEN);
		ipv4_addr_to_string(current->tcp_conn.dstip, dstip, IPV4_ADDR_STRING_MAXLEN);

		messagef(HAKA_LOG_DEBUG, L"tcp-connection", L"dropping connection %s:%u -> %s:%u",
				srcip, current->tcp_conn.srcport, dstip, current->tcp_conn.dstport);
	}

	tcp_connection_remove(&ct_head, current);
	tcp_connection_release(current, false);
	tcp_connection_insert(&ct_drop_head, current);
}

bool tcp_connection_isdropped(const struct tcp *tcp)
{
	return tcp_connection_find(ct_drop_head, tcp, NULL) != NULL;
}
