/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

%module alert

%{
#include <haka/alert.h>
#include <haka/error.h>

struct alert_id {
	uint64   id;
};

static void free_array(void **array)
{
	if (array) {
		void **iter;
		for (iter = array; *iter; ++iter) {
			free(*iter);
		}
		free(array);
	}
}

static void free_nodes(struct alert_node **nodes)
{
	if (nodes) {
		struct alert_node **iter;
		for (iter = nodes; *iter; ++iter) {
			free_array((void **)(*iter)->list);
			free(*iter);
		}
		free(nodes);
	}
}

%}

%include "haka/lua/wchar.si"
%include "haka/lua/swig.si"
%include "time.si"
%include "array.si"

%nodefaultctor;
%nodefaultdtor;

enum alert_level { HAKA_ALERT_LOW, HAKA_ALERT_MEDIUM, HAKA_ALERT_HIGH, HAKA_ALERT_NUMERIC };
enum alert_completion { HAKA_ALERT_FAILED, HAKA_ALERT_SUCCESSFUL };
enum alert_node_type { HAKA_ALERT_NODE_ADDRESS, HAKA_ALERT_NODE_SERVICE };

%typemap(in) wchar_t ** {
	if (lua_istable(L, $input)) {
		int i, size = lua_objlen(L, $input);
		$1 = malloc((size+1)*sizeof(wchar_t *));
		for (i = 0; i < size; ++i) {
			lua_rawgeti(L, $input, i+1);
			$1[i] = str2wstr(lua_tostring(L, -1), lua_objlen(L, -1));
			lua_pop(L, 1);
		}
		$1[i] = NULL;
	} else {
		$1 = 0;
		lua_pushstring(L,"Expecting string array");
		lua_error(L);
	}
}

%typemap(typecheck, precedence=SWIG_TYPECHECK_STRING_ARRAY) wchar_t ** {
	$1 = lua_istable(L, $input);
}

%typemap(memberin) wchar_t ** {
	free_array((void **)$1);
	$1 = $input;
	$input = NULL;
}

%typemap(freearg) wchar_t ** {
	free_array((void**)$1);
}

struct alert_node {
	alert_node_type   type;
	wchar_t         **list;

	%extend {
		alert_node()
		{
			struct alert_node *node = malloc(sizeof(struct alert_node));
			if (!node) {
				error(L"memory error");
				return NULL;
			}
			memset(node, 0, sizeof(struct alert_node));
			return node;
		}

		~alert_node()
		{
			free_array((void**)$self->list);
			free($self);
		}
	}
};

APPLY_NULLTERM_ARRAY(alert_node);
STRUCT_UNKNOWN_KEY_ERROR(alert_node);

struct alert_id {
	%extend {
		~alert_id() {
			free($self);
		}

		void _update(struct alert *alert) {
			alert_update($self->id, alert);
		}
	}
};

APPLY_NULLTERM_ARRAY(alert_id);
STRUCT_UNKNOWN_KEY_ERROR(alert_id);

struct alert {
	wchar_t            *description;
	wchar_t            *method_description;
	alert_level         severity;
	alert_level         confidence;
	double              confidence_num;
	alert_completion    completion;
	wchar_t           **method_ref;

	%extend {
		alert()
		{
			struct alert *alert = malloc(sizeof(struct alert));
			if (!alert) {
				error(L"memory error");
				return NULL;
			}
			memset(alert, 0, sizeof(struct alert));
			return alert;
		}

		~alert()
		{
			free($self->description);
			free($self->method_description);
			free_nodes($self->sources);
			free_nodes($self->targets);
			free_array((void**)$self->method_ref);
			free($self->alert_ref);
			free($self);
		}

		void start_time(struct time_lua *t) {
			$self->start_time = t->seconds*1000000LL + t->micro_seconds;
		}

		void end_time(struct time_lua *t) {
			$self->end_time = t->seconds*1000000LL + t->micro_seconds;
		}

		void alert_ref(struct alert_id **ids) {
			int i, size = 0;
			struct alert_id **iter;
			for (iter = ids; *iter; ++iter) {
				++size;
			}

			free($self->alert_ref);
			$self->alert_ref = malloc(size*sizeof(uint64));
			if (!$self->alert_ref) {
				error(L"memory error");
				return;
			}

			$self->alert_ref_count = size;
			for (iter = ids, i = 0; *iter; ++iter, ++i) {
				$self->alert_ref[i] = (*iter)->id;
			}
		}

		void sources(struct alert_node **DISOWN)
		{
			$self->sources = DISOWN;
		}

		void targets(struct alert_node **DISOWN)
		{
			$self->targets = DISOWN;
		}
	}
};

STRUCT_UNKNOWN_KEY_ERROR(alert);

%newobject _post;
struct alert_id *_post(struct alert *alert);

%{

struct alert_id *_post(struct alert *_alert)
{
	struct alert_id *ret = malloc(sizeof(struct alert_id));
	if (!ret) {
		error(L"memory error");
		return NULL;
	}

	ret->id = alert(_alert);
	return ret;
}

%}

%luacode {
	local this = unpack({...})

	--
	-- Table checks
	--
	local function check_table(key, tbl, desc)
		if type(key) == 'number' then
			key = string.format("#%d", key)
		end

		if type(tbl) ~= 'table' then
			error(string.format("expected table for '%s'", key))
		end

		local field
		for k, value in pairs(tbl) do
			if key then field = string.format("%s.%s", key, k)
			else field = k end

			local check = desc[k]
			if not check then
				error(string.format("unexpected field '%s'", field))
			end

			tbl[k] = check(field, tbl[k])
		end

		return tbl
	end

	local function check_any(key, value) return value end
	local function check_string(key, value) return tostring(value) end

	local valid_level = {
		low = true,
		medium = true,
		high = true
	}

	local function check_level(key, value)
		if not valid_level[value] then
			error(string.format("invalid %s value '%s'", key, value))
		end
		return value
	end

	local function check_confidence(key, value)
		if type(value) == 'number' then return value end
		return check_level(key, value)
	end

	local valid_completion = {
		failed = true,
		successful = true
	}

	local function check_completion(key, value)
		if not valid_completion[value] then
			error(string.format("invalid completion value '%s'", value))
		end
		return value
	end

	local function check_array(key, value)
		if type(value) ~= 'table' then
			value = { value }
		end
		return value
	end

	local function check_string_array(key, values)
		values = check_array(key, values)
		for index, value in ipairs(values) do
			values[index] = tostring(value)
		end
		return values
	end

	local method_desc = {
		description = check_string,
		ref = check_string_array
	}

	local function check_method(key, value)
		if type(value) ~= 'table' then
			error("invalid method")
		end

		return check_table(key, value, method_desc)
	end

	local source_desc = {
		type = check_any,
		values = check_string_array
	}

	local function check_sources(key, values)
		if type(values) ~= 'table' or values.type then
			values = { values }
		end

		for i, value in ipairs(values) do
			check_table(string.format("%s#%d", key, i), value, source_desc)
		end

		return values
	end

	local alert_desc = {
		start_time = check_any,
		end_time = check_any,
		severity = check_level,
		completion = check_completion,
		confidence = check_confidence,
		description = check_string,
		method = check_method,
		sources = check_sources,
		targets = check_sources,
		ref = check_array
	}

	local convert_level = {
		low = this.HAKA_ALERT_LOW,
		medium = this.HAKA_ALERT_MEDIUM,
		high = this.HAKA_ALERT_HIGH,
	}

	local convert_completion = {
		failed = this.HAKA_ALERT_FAILED,
		successful = this.HAKA_ALERT_SUCCESSFUL,
	}

	local convert_node_type = {
		address = this.HAKA_ALERT_NODE_ADDRESS,
		service = this.HAKA_ALERT_NODE_SERVICE,
	}

	local function convert_nodes(nodes)
		local ret = {}
		for _, node in ipairs(nodes) do
			local cnode = this.alert_node()
			cnode.type = convert_node_type[node.type]
			cnode.list = node.values
			table.insert(ret, cnode)
		end
		return ret
	end

	local function convert(alert)
		check_table(nil, alert, alert_desc)

		local calert = this.alert()
		if alert.start_time then calert:start_time(alert.start_time) end
		if alert.end_time then calert:end_time(alert.end_time) end
		if alert.severity then calert.severity = convert_level[alert.severity] end
		if alert.confidence then
			if type(alert.confidence) == 'number' then
				calert.confidence_num = alert.confidence
				calert.confidence = this.HAKA_ALERT_NUMERIC
			else
				calert.confidence = convert_level[alert.confidence]
			end
		end
		if alert.completion then calert.completion = convert_completion[alert.completion] end
		if alert.description then calert.description = alert.description end
		if alert.method then
			if alert.method.description then
				calert.method_description = alert.method.description
			end
			if alert.method.ref then
				calert.method_ref = alert.method.ref
			end
		end
		if alert.sources then calert:sources(convert_nodes(alert.sources)) end
		if alert.targets then calert:targets(convert_nodes(alert.targets)) end
		if alert.ref then calert:alert_ref(alert.ref) end
		return calert
	end

	function this.update(alertid, alert)
		alertid:_update(convert(alert))
	end

	function this.address(...)
		return {
			type = 'address',
			values = {...}
		}
	end

	function this.service(...)
		return {
			type = 'service',
			values = {...}
		}
	end

	getmetatable(this).__call = function (_, alert)
		return this._post(convert(alert))
	end
}
