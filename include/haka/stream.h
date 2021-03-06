/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _HAKA_STREAM_H
#define _HAKA_STREAM_H

#include <stddef.h>

#include <haka/types.h>
#include <haka/compiler.h>
#include <haka/error.h>
#include <haka/packet.h>
#include <haka/lua/object.h>


struct stream;
struct stream_mark;

struct stream_ftable {
	bool      (*destroy)(struct stream *s);
	size_t    (*read)(struct stream *s, uint8 *data, size_t length);
	size_t    (*available)(struct stream *s);
	size_t    (*insert)(struct stream *s, const uint8 *data, size_t length);
	size_t    (*replace)(struct stream *s, const uint8 *data, size_t length);
	size_t    (*erase)(struct stream *s, size_t length);
	struct stream_mark *(*mark)(struct stream *s, bool readonly);
	bool      (*unmark)(struct stream *s, struct stream_mark *mark);
	bool      (*seek)(struct stream *s, struct stream_mark *mark, bool unmark);
};


/* Opaque stream structure. */
struct stream {
	struct stream_ftable *ftable;
	struct lua_object     lua_object;
};



INLINE size_t stream_read(struct stream *stream, uint8 *data, size_t length)
{
	return stream->ftable->read(stream, data, length);
}

INLINE size_t stream_available(struct stream *stream)
{
	return stream->ftable->available(stream);
}

INLINE bool stream_destroy(struct stream *stream)
{
	return stream->ftable->destroy(stream);
}

INLINE size_t stream_insert(struct stream *stream, const uint8 *data, size_t length)
{
	if (!stream->ftable->insert) {
		error(L"usupported operation");
		return 0;
	}

	if (packet_mode() == MODE_PASSTHROUGH) {
		error(L"operation not supported (pass-through mode)");
		return -1;
	}

	return stream->ftable->insert(stream, data, length);
}

INLINE size_t stream_replace(struct stream *stream, const uint8 *data, size_t length)
{
	if (!stream->ftable->replace) {
		error(L"usupported operation");
		return 0;
	}

	if (packet_mode() == MODE_PASSTHROUGH) {
		error(L"operation not supported (pass-through mode)");
		return -1;
	}

	return stream->ftable->replace(stream, data, length);
}

INLINE size_t stream_erase(struct stream *stream, size_t length)
{
	if (!stream->ftable->erase) {
		error(L"usupported operation");
		return 0;
	}

	if (packet_mode() == MODE_PASSTHROUGH) {
		error(L"operation not supported (pass-through mode)");
		return -1;
	}

	return stream->ftable->erase(stream, length);
}

INLINE struct stream_mark *stream_mark(struct stream *stream, bool readonly)
{
	if (!stream->ftable->mark) {
		error(L"usupported operation");
		return NULL;
	}

	if (packet_mode() == MODE_PASSTHROUGH && !readonly) {
		error(L"operation not supported (pass-through mode)");
		return NULL;
	}

	return stream->ftable->mark(stream, readonly);
}

INLINE bool stream_unmark(struct stream *stream, struct stream_mark *mark)
{
	if (!stream->ftable->unmark) {
		error(L"usupported operation");
		return false;
	}

	if (!mark) {
		error(L"invalid argument");
		return false;
	}

	return stream->ftable->unmark(stream, mark);
}

INLINE bool stream_seek(struct stream *stream, struct stream_mark *mark, bool unmark)
{
	if (!stream->ftable->seek) {
		error(L"usupported operation");
		return false;
	}

	if (!mark) {
		error(L"invalid argument");
		return false;
	}

	return stream->ftable->seek(stream, mark, unmark);
}

#endif /* _HAKA_STREAM_H */
