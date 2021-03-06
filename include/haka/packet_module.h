/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _HAKA_PACKET_MODULE_H
#define _HAKA_PACKET_MODULE_H

#include <haka/module.h>
#include <haka/packet.h>
#include <haka/types.h>


typedef enum {
	FILTER_ACCEPT,
	FILTER_DROP
} filter_result;

/* Opaque state structure. */
struct packet_module_state;

struct packet_module {
	struct module    module;

	bool           (*multi_threaded)();
	struct packet_module_state *(*init_state)(int thread_id);
	void           (*cleanup_state)(struct packet_module_state *state);
	int            (*receive)(struct packet_module_state *state, struct packet **pkt);
	void           (*verdict)(struct packet *pkt, filter_result result);
	size_t         (*get_length)(struct packet *pkt);
	uint8         *(*make_modifiable)(struct packet *pkt);
	int            (*resize)(struct packet *pkt, size_t size);
	uint64         (*get_id)(struct packet *pkt);
	const uint8   *(*get_data)(struct packet *pkt);
	const char    *(*get_dissector)(struct packet *pkt);
	void           (*release_packet)(struct packet *pkt);
	enum packet_status (*packet_getstate)(struct packet *pkt);
	struct packet *(*new_packet)(struct packet_module_state *state, size_t size);
	bool           (*send_packet)(struct packet *pkt);
	size_t         (*get_mtu)(struct packet *pkt);
	time_us        (*get_timestamp)(struct packet *pkt);
};

#endif /* _HAKA_PACKET_MODULE_H */

