/*
 * Copyright (C) 2019-2020 "IoT.bzh"
 * Author: José Bollo <jose.bollo@iot.bzh>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <quickjs/quickjs.h>
#include <libafbcli/afb-wsapi.h>

#define countof(x) (sizeof(x) / sizeof(*(x)))

static JSClassID afb_wsapi_class_id;
static JSClassID afb_wsapi_msg_class_id;

extern struct afb_wsapi *client_wsapi(const char *uri, struct afb_wsapi_itf *itf, void *closure);
extern int client_serve(const char *uri, int (*onclient)(void*,int), void *closure);

/**************************************************************/

struct holder
{
	JSContext *ctx;
	JSValue    value;
	void      *item;
};

/**************************************************************/

static void call_prop(JSContext *ctx, JSValue thisobj, const char *prop, int argc, JSValueConst *argv)
{
	JSValue func = JS_GetPropertyStr(ctx, thisobj, prop);
	if (JS_IsFunction(ctx, func))
		JS_Call(ctx, func, thisobj, argc, argv);
	JS_FreeValue(ctx, func);
}

/**************************************************************/

struct holdcb
{
	JSContext *ctx;
	JSValue    thisobj;
	JSValue    func;
};

static struct holdcb *mkholdcb(JSContext *ctx, JSValueConst thisobj, JSValueConst func)
{
	struct holdcb *r = malloc(sizeof *r);
	if (r) {
		r->ctx = JS_DupContext(ctx);
		r->thisobj = JS_DupValue(ctx, thisobj);
		r->func = JS_DupValue(ctx, func);
	}
	return r;
}

static void killholdcb(struct holdcb *h)
{
	JS_FreeValue(h->ctx, h->thisobj);
	JS_FreeValue(h->ctx, h->func);
	JS_FreeContext(h->ctx);
	free(h);
}

static void holdcbcall(struct holdcb *h, int argc, JSValueConst *argv)
{
	JS_Call(h->ctx, h->func, h->thisobj, argc, argv);
	killholdcb(h);
}

/**************************************************************/

static void msg_finalize(JSValue val)
{
	struct afb_wsapi_msg *msg = JS_GetOpaque(val, afb_wsapi_msg_class_id);
	JS_SetOpaque(val, 0);
	if (msg)
		afb_wsapi_msg_unref(msg);
}

static JSValue wsapi_msg_reply(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const struct afb_wsapi_msg *msg = JS_GetOpaque(this_val, afb_wsapi_msg_class_id);
	const char *obj = NULL, *err = NULL, *info = NULL;
	JSValue json, ret = JS_EXCEPTION;
	int s;

	if (!msg) {
		ret = JS_ThrowInternalError(ctx, "disconnected");
		goto error;
	}

	json = JS_JSONStringify(ctx, argv[0], JS_UNDEFINED, JS_UNDEFINED);
	if (!JS_IsString(json)) {
		JS_FreeValue(ctx, json);
		goto error;
	}
	obj = JS_ToCString(ctx, json);
	JS_FreeValue(ctx, json);
	if (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsNull(argv[1])) {
		err = JS_ToCString(ctx, argv[1]);
		if (!err)
			goto error;
	}
	if (argc >= 3 && !JS_IsUndefined(argv[2]) && !JS_IsNull(argv[2])) {
		info = JS_ToCString(ctx, argv[2]);
		if (!info)
			goto error;
	}
	s = afb_wsapi_msg_reply_s(msg, obj, err, info);
	if (s >= 0) {
		JS_SetOpaque(this_val, 0);
		msg_finalize(this_val);
		ret = JS_UNDEFINED;
	}

error:
	if (obj)
		JS_FreeCString(ctx, obj);
	if (err)
		JS_FreeCString(ctx, err);
	if (info)
		JS_FreeCString(ctx, info);
	return ret;
}

static JSValue wsapi_msg_subunsub(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int magic)
{
	const struct afb_wsapi_msg *msg = JS_GetOpaque(this_val, afb_wsapi_msg_class_id);
	int32_t i32;
	int s;

	if (!msg)
		return JS_ThrowInternalError(ctx, "disconnected");

	if (JS_ToInt32(ctx, &i32, argv[0]))
		return JS_ThrowTypeError(ctx, "number expected");
	if (i32 < 0 || i32 > UINT16_MAX)
		return JS_ThrowRangeError(ctx, "out of range");

	s = (magic ? afb_wsapi_msg_subscribe : afb_wsapi_msg_unsubscribe)(msg, (uint16_t)i32);
	if (s < 0)
		return JS_ThrowInternalError(ctx, "failed with code %d", s);
	return JS_UNDEFINED;
}

static JSValue wsapi_msg_description(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const struct afb_wsapi_msg *msg = JS_GetOpaque(this_val, afb_wsapi_msg_class_id);
	const char *obj = NULL, *err = NULL, *info = NULL;
	JSValue json, ret = JS_EXCEPTION;
	int s;

	if (!msg)
		return JS_ThrowInternalError(ctx, "disconnected");

	json = JS_JSONStringify(ctx, argv[0], JS_UNDEFINED, JS_UNDEFINED);
	if (!JS_IsString(json))
		return JS_ThrowInternalError(ctx, "unexpected value");
	obj = JS_ToCString(ctx, json);
	JS_FreeValue(ctx, json);
	s = afb_wsapi_msg_description_s(msg, obj);
	JS_FreeCString(ctx, obj);
	if (s < 0)
		return JS_ThrowInternalError(ctx, "failed with code %d", s);
	msg_finalize(this_val);
	return JS_UNDEFINED;
}

static void AFBWSAPIMSG_finalizer(JSRuntime *rt, JSValue val)
{
	msg_finalize(val);
}

static JSClassDef afb_wsapi_msg_class = {
	.class_name = "AFBWSAPIMSG",
	.finalizer = AFBWSAPIMSG_finalizer,
}; 

static const JSCFunctionListEntry afb_wsapi_msg_call_proto_funcs[] = {
	JS_CFUNC_DEF("reply", 3, wsapi_msg_reply),
	JS_CFUNC_MAGIC_DEF("subscribe", 1, wsapi_msg_subunsub, 1),
	JS_CFUNC_MAGIC_DEF("unsubscribe", 1, wsapi_msg_subunsub, 0),
};

static const JSCFunctionListEntry afb_wsapi_msg_desc_proto_funcs[] = {
	JS_CFUNC_DEF("description", 1, wsapi_msg_description),
};


static JSValue wsapi_msg_make(JSContext *ctx, const struct afb_wsapi_msg *msg)
{
	JSValue obj = JS_NewObjectClass(ctx, afb_wsapi_msg_class_id);
	JS_SetOpaque(obj, (void*)msg);

	switch (msg->type) {
	case afb_wsapi_msg_type_call:
		JS_SetPropertyFunctionList(ctx, obj, afb_wsapi_msg_call_proto_funcs, 3);
		break;
	case afb_wsapi_msg_type_describe:
		JS_SetPropertyFunctionList(ctx, obj, afb_wsapi_msg_desc_proto_funcs, 1);
		break;
	default:
		break;
	}
	return obj;
}

/**************************************************************/

static void wsapi_on_hangup(void *closure)
{
	struct holder *holder = closure;
	JSContext *ctx = holder->ctx;

	if (ctx) {
		JS_SetOpaque(holder->value, 0);
		holder->ctx = 0;
		holder->item = 0;
		call_prop(ctx, holder->value, "onHangup", 0, 0);
		free(holder);
	}
}

static void wsapi_on_call(void *closure, const struct afb_wsapi_msg *msg)
{
	struct holder *holder = closure;
	JSContext *ctx = holder->ctx;
	JSValue argv[6];

	if (!ctx)
		afb_wsapi_msg_unref(msg);
	else {
		argv[0] = wsapi_msg_make(ctx, msg);
		argv[1] = JS_NewString(ctx, msg->call.verb);
		argv[2] = JS_ParseJSON(ctx, msg->call.data, strlen(msg->call.data), "<wsapi.on-call>");
		argv[3] = JS_NewInt32(ctx, msg->call.sessionid);
		argv[4] = JS_NewInt32(ctx, msg->call.tokenid);
		argv[5] = msg->call.user_creds ? JS_NewString(ctx, msg->call.user_creds) : JS_NULL;
		call_prop(ctx, holder->value, "onCall", 6, argv);
		JS_FreeValue(ctx, argv[0]);
		JS_FreeValue(ctx, argv[1]);
		JS_FreeValue(ctx, argv[2]);
		JS_FreeValue(ctx, argv[3]);
		JS_FreeValue(ctx, argv[4]);
		JS_FreeValue(ctx, argv[5]);
	}
}

static void wsapi_on_reply(void *closure, const struct afb_wsapi_msg *msg)
{
	struct holder *holder = closure;
	struct holdcb *holdcb = msg->reply.closure;
	JSContext *ctx = holdcb->ctx;
	JSValue argv[3];

	argv[0] = JS_ParseJSON(ctx, msg->reply.data, strlen(msg->reply.data), "<wsapi.on-reply>");
	argv[1] = msg->reply.error ? JS_NewString(ctx, msg->reply.error) : JS_NULL;
	argv[2] = msg->reply.info ? JS_NewString(ctx, msg->reply.info) : JS_NULL;
	holdcbcall(holdcb, 3, argv);
	JS_FreeValue(ctx, argv[0]);
	JS_FreeValue(ctx, argv[1]);
	JS_FreeValue(ctx, argv[2]);
	afb_wsapi_msg_unref(msg);
}

static void wsapi_on_event_create(void *closure, const struct afb_wsapi_msg *msg)
{
	struct holder *holder = closure;
	JSContext *ctx = holder->ctx;
	JSValue argv[2];

	if (ctx) {
		argv[0] = JS_NewInt32(ctx, msg->event_create.eventid);
		argv[1] = JS_NewString(ctx, msg->event_create.eventname);
		call_prop(ctx, holder->value, "onEventCreate", 2, argv);
		JS_FreeValue(ctx, argv[0]);
		JS_FreeValue(ctx, argv[1]);
	}
	afb_wsapi_msg_unref(msg);
}

static void wsapi_on_event_remove(void *closure, const struct afb_wsapi_msg *msg)
{
	struct holder *holder = closure;
	JSContext *ctx = holder->ctx;
	JSValue argv[1];

	if (ctx) {
		argv[0] = JS_NewInt32(ctx, msg->event_remove.eventid);
		call_prop(ctx, holder->value, "onEventRemove", 1, argv);
		JS_FreeValue(ctx, argv[0]);
	}
	afb_wsapi_msg_unref(msg);
}

static void wsapi_on_event_subscribe(void *closure, const struct afb_wsapi_msg *msg)
{
	struct holder *holder = closure;
	JSContext *ctx = holder->ctx;
	JSValue argv[1];

	if (ctx) {
		argv[0] = JS_NewInt32(ctx, msg->event_subscribe.eventid);
		call_prop(ctx, holder->value, "onEventSubscribe", 1, argv);
		JS_FreeValue(ctx, argv[0]);
	}
	afb_wsapi_msg_unref(msg);
}

static void wsapi_on_event_unsubscribe(void *closure, const struct afb_wsapi_msg *msg)
{
	struct holder *holder = closure;
	JSContext *ctx = holder->ctx;
	JSValue argv[1];

	if (ctx) {
		argv[0] = JS_NewInt32(ctx, msg->event_unsubscribe.eventid);
		call_prop(ctx, holder->value, "onEventUnsubscribe", 1, argv);
		JS_FreeValue(ctx, argv[0]);
	}
	afb_wsapi_msg_unref(msg);
}

static void wsapi_on_event_push(void *closure, const struct afb_wsapi_msg *msg)
{
	struct holder *holder = closure;
	JSContext *ctx = holder->ctx;
	JSValue argv[2];

	if (ctx) {
		argv[0] = JS_NewInt32(ctx, msg->event_push.eventid);
		argv[1] = JS_ParseJSON(ctx, msg->event_push.data, strlen(msg->event_push.data), "<wsapi.on-event-push>");
		call_prop(ctx, holder->value, "onEventPush", 2, argv);
		JS_FreeValue(ctx, argv[0]);
		JS_FreeValue(ctx, argv[1]);
	}
	afb_wsapi_msg_unref(msg);
}

static void wsapi_on_event_broadcast(void *closure, const struct afb_wsapi_msg *msg)
{
	struct holder *holder = closure;
	JSContext *ctx = holder->ctx;
	JSValue argv[3];

	if (ctx) {
		argv[0] = JS_NewString(ctx, msg->event_broadcast.name);
		argv[1] = JS_ParseJSON(ctx, msg->event_broadcast.data, strlen(msg->event_broadcast.data), "<wsapi.on-event-push>");
		argv[2] = JS_NewInt32(ctx, msg->event_broadcast.hop);
/* TODO but not very urgent
		duk_push_buffer_object(ctx, -1, 0, (int)sizeof(afb_wsapi_uuid_t), DUK_BUFOBJ_UINT8ARRAY);
		memcpy(duk_get_buffer_data(ctx, -1, NULL), msg->event_broadcast.uuid, sizeof(afb_wsapi_uuid_t));
		call_prop(ctx, holder->value, "onEventBroadcast", 4);
*/
		call_prop(ctx, holder->value, "onEventBroadcast", 3, argv);
		JS_FreeValue(ctx, argv[0]);
		JS_FreeValue(ctx, argv[1]);
		JS_FreeValue(ctx, argv[2]);
	}
	afb_wsapi_msg_unref(msg);
}

static void wsapi_on_event_unexpected(void *closure, const struct afb_wsapi_msg *msg)
{
	struct holder *holder = closure;
	JSContext *ctx = holder->ctx;
	JSValue argv[1];

	if (ctx) {
		argv[0] = JS_NewInt32(ctx, msg->event_unexpected.eventid);
		call_prop(ctx, holder->value, "onEventUnexpected", 1, argv);
		JS_FreeValue(ctx, argv[0]);
	}
	afb_wsapi_msg_unref(msg);
}

static void wsapi_on_session_create(void *closure, const struct afb_wsapi_msg *msg)
{
	struct holder *holder = closure;
	JSContext *ctx = holder->ctx;
	JSValue argv[2];

	if (ctx) {
		argv[0] = JS_NewInt32(ctx, msg->session_create.sessionid);
		argv[1] = JS_NewString(ctx, msg->session_create.sessionname);
		call_prop(ctx, holder->value, "onSessionCreate", 2, argv);
		JS_FreeValue(ctx, argv[0]);
		JS_FreeValue(ctx, argv[1]);
	}
	afb_wsapi_msg_unref(msg);
}

static void wsapi_on_session_remove(void *closure, const struct afb_wsapi_msg *msg)
{
	struct holder *holder = closure;
	JSContext *ctx = holder->ctx;
	JSValue argv[1];

	if (ctx) {
		argv[0] = JS_NewInt32(ctx, msg->session_remove.sessionid);
		call_prop(ctx, holder->value, "onSessionRemove", 1, argv);
		JS_FreeValue(ctx, argv[0]);
	}
	afb_wsapi_msg_unref(msg);
}

static void wsapi_on_token_create(void *closure, const struct afb_wsapi_msg *msg)
{
	struct holder *holder = closure;
	JSContext *ctx = holder->ctx;
	JSValue argv[2];

	if (ctx) {
		argv[0] = JS_NewInt32(ctx, msg->token_create.tokenid);
		argv[1] = JS_NewString(ctx, msg->token_create.tokenname);
		call_prop(ctx, holder->value, "onTokenCreate", 2, argv);
		JS_FreeValue(ctx, argv[0]);
		JS_FreeValue(ctx, argv[1]);
	}
	afb_wsapi_msg_unref(msg);
}

static void wsapi_on_token_remove(void *closure, const struct afb_wsapi_msg *msg)
{
	struct holder *holder = closure;
	JSContext *ctx = holder->ctx;
	JSValue argv[1];

	if (ctx) {
		argv[0] = JS_NewInt32(ctx, msg->token_remove.tokenid);
		call_prop(ctx, holder->value, "onTokenRemove", 1, argv);
		JS_FreeValue(ctx, argv[0]);
	}
	afb_wsapi_msg_unref(msg);
}

static void wsapi_on_describe(void *closure, const struct afb_wsapi_msg *msg)
{
	struct holder *holder = closure;
	JSContext *ctx = holder->ctx;
	JSValue argv[1];

	if (!ctx)
		afb_wsapi_msg_unref(msg);
	else {
		argv[0] = wsapi_msg_make(ctx, msg);
		call_prop(ctx, holder->value, "onCall", 1, argv);
		JS_FreeValue(ctx, argv[0]);
	}
}

static void wsapi_on_description(void *closure, const struct afb_wsapi_msg *msg)
{
	struct holder *holder = closure;
	struct holdcb *holdcb = msg->description.closure;
	JSContext *ctx = holdcb->ctx;
	JSValue obj = JS_ParseJSON(ctx, msg->description.data, strlen(msg->description.data), "<wsapi.on-description>");
	holdcbcall(holdcb, 1, &obj);
	JS_FreeValue(ctx, obj);
	afb_wsapi_msg_unref(msg);
}

/**************************************************************/

static JSValue wsapi_call(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	struct holder *holder = JS_GetOpaque(this_val, afb_wsapi_class_id);
	struct afb_wsapi *wsapi = holder ? holder->item : 0;

	int s;
	int32_t sessionid = 0, tokenid = 0;
	const char *verb = 0, *obj = 0, *user_creds = 0;
	JSValue json = JS_UNDEFINED, ret = JS_EXCEPTION;
	struct holdcb *holdcb = 0;

	if (!wsapi) {
		ret = JS_ThrowInternalError(ctx, "disconnected");
		goto end;
	}

	if (!JS_IsFunction(ctx, argv[2])) {
		ret = JS_ThrowTypeError(ctx, "function expected");
		goto end;
	}
	holdcb = mkholdcb(ctx, this_val, argv[2]);
	if (!holdcb) {
		ret = JS_ThrowOutOfMemory(ctx);
		goto end;
	}

	verb = JS_ToCString(ctx, argv[0]);
	if (!verb)  {
		ret = JS_ThrowTypeError(ctx, "verb string expected");
		goto end;
	}

	json = JS_JSONStringify(ctx, argv[1], JS_UNDEFINED, JS_UNDEFINED);
	obj = JS_ToCString(ctx, json);
	JS_FreeValue(ctx, json);
	if (!obj) {
		ret = JS_ThrowTypeError(ctx, "object expected");
		goto end;
	}

	if (!JS_IsUndefined(argv[3])) {
		if (JS_ToInt32(ctx, &sessionid, argv[3])
		 || sessionid < 0 || sessionid > UINT16_MAX) {
			ret = JS_ThrowTypeError(ctx, "invalid sessionid");
			goto end;
		}
	}

	if (!JS_IsUndefined(argv[4])) {
		if (JS_ToInt32(ctx, &tokenid, argv[4])
		 || tokenid < 0 || tokenid > UINT16_MAX) {
			ret = JS_ThrowTypeError(ctx, "invalid tokenid");
			goto end;
		}
	}

	if (!JS_IsUndefined(argv[5])) {
		user_creds = JS_ToCString(ctx, argv[5]);
		if (!user_creds) {
			ret = JS_ThrowTypeError(ctx, "invalid user creds");
			goto end;
		}
	}

	s = afb_wsapi_call_s(wsapi, verb, obj, sessionid, tokenid, holdcb, user_creds);

	if (s < 0)
		ret = JS_ThrowInternalError(ctx, "failed with code %d", s);
	else {
		ret = JS_UNDEFINED;
		holdcb = 0;
	}
end:
	if (user_creds)
		JS_FreeCString(ctx, user_creds);
	if (obj)
		JS_FreeCString(ctx, obj);
	if (verb)
		JS_FreeCString(ctx, verb);
	if (holdcb)
		killholdcb(holdcb);
	return ret;
}

static JSValue wsapi_any_u16_str_vals(JSContext *ctx, JSValueConst this_val, JSValueConst au16, JSValueConst astr, int (*fun)(struct afb_wsapi*,uint16_t,const char*))
{
	int s;
	int32_t i32;
	const char *str;
	struct holder *holder = JS_GetOpaque(this_val, afb_wsapi_class_id);
	struct afb_wsapi *wsapi = holder ? holder->item : 0;

	if (!wsapi)
		return JS_ThrowInternalError(ctx, "disconnected");
	if (JS_ToInt32(ctx, &i32, au16))
		return JS_ThrowTypeError(ctx, "number expected");
	if (i32 < 0 || i32 > UINT16_MAX)
		return JS_ThrowRangeError(ctx, "out of range");
	str = JS_ToCString(ctx, astr);
	if (!str)
		return JS_ThrowTypeError(ctx, "string expected");
	s = fun(wsapi, (uint16_t)i32, str);
	JS_FreeCString(ctx, str);
	if (s < 0)
		return JS_ThrowInternalError(ctx, "failed with code %d", s);
	return JS_UNDEFINED;
}

static JSValue wsapi_any_u16_str(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int (*fun)(struct afb_wsapi*,uint16_t,const char*))
{
	return wsapi_any_u16_str_vals(ctx, this_val, argv[0], argv[1], fun);
}

static JSValue wsapi_any_u16(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv, int (*fun)(struct afb_wsapi*,uint16_t))
{
	int s;
	int32_t i32;
	struct holder *holder = JS_GetOpaque(this_val, afb_wsapi_class_id);
	struct afb_wsapi *wsapi = holder ? holder->item : 0;

	if (!wsapi)
		return JS_ThrowInternalError(ctx, "disconnected");
	if (JS_ToInt32(ctx, &i32, argv[0]))
		return JS_ThrowTypeError(ctx, "number expected");
	if (i32 < 0 || i32 > UINT16_MAX)
		return JS_ThrowRangeError(ctx, "out of range");
	s = fun(wsapi, (uint16_t)i32);
	if (s < 0)
		return JS_ThrowInternalError(ctx, "failed with code %d", s);
	return JS_UNDEFINED;
}

static JSValue wsapi_session_create(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return wsapi_any_u16_str(ctx, this_val, argc, argv, afb_wsapi_session_create);
}

static JSValue wsapi_session_remove(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return wsapi_any_u16(ctx, this_val, argc, argv, afb_wsapi_session_remove);
}

static JSValue wsapi_token_create(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return wsapi_any_u16_str(ctx, this_val, argc, argv, afb_wsapi_token_create);
}

static JSValue wsapi_token_remove(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return wsapi_any_u16(ctx, this_val, argc, argv, afb_wsapi_token_remove);
}

static JSValue wsapi_event_create(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return wsapi_any_u16_str(ctx, this_val, argc, argv, afb_wsapi_event_create);
}

static JSValue wsapi_event_remove(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return wsapi_any_u16(ctx, this_val, argc, argv, afb_wsapi_event_remove);
}

static JSValue wsapi_event_push(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue json = JS_JSONStringify(ctx, argv[1], JS_UNDEFINED, JS_UNDEFINED);
	JSValue ret = wsapi_any_u16_str_vals(ctx, this_val, argv[0], json, afb_wsapi_event_push_s);
	JS_FreeValue(ctx, json);
	return ret;
}

static JSValue wsapi_event_unexpected(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return wsapi_any_u16(ctx, this_val, argc, argv, afb_wsapi_event_unexpected);
}

static JSValue wsapi_event_broadcast(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	struct holder *holder = JS_GetOpaque(this_val, afb_wsapi_class_id);
	struct afb_wsapi *wsapi = holder ? holder->item : 0;

	int s;
	int32_t hop = 0;
	const char *event = 0, *obj = 0;
	JSValue json = JS_UNDEFINED, ret = JS_EXCEPTION;
	afb_wsapi_uuid_t uuid;

	if (!wsapi) {
		ret = JS_ThrowInternalError(ctx, "disconnected");
		goto end;
	}

	event = JS_ToCString(ctx, argv[0]);
	if (!event)  {
		ret = JS_ThrowTypeError(ctx, "event string expected");
		goto end;
	}

	json = JS_JSONStringify(ctx, argv[1], JS_UNDEFINED, JS_UNDEFINED);
	obj = JS_ToCString(ctx, json);
	JS_FreeValue(ctx, json);
	if (!obj) {
		ret = JS_ThrowTypeError(ctx, "object expected");
		goto end;
	}

	if (!JS_IsUndefined(argv[2])) {
		if (JS_ToInt32(ctx, &hop, argv[2])
		 || hop < 0 || hop > UINT8_MAX) {
			ret = JS_ThrowTypeError(ctx, "invalid hop");
			goto end;
		}
	}

	memset(uuid, 0, sizeof uuid);
	s = afb_wsapi_event_broadcast_s(wsapi, event, obj, uuid, (uint8_t)hop);
	if (s < 0)
		ret = JS_ThrowInternalError(ctx, "failed with code %d", s);
end:
	if (obj)
		JS_FreeCString(ctx, obj);
	if (event)
		JS_FreeCString(ctx, event);
	return ret;
}


static JSValue wsapi_describe(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	struct holder *holder = JS_GetOpaque(this_val, afb_wsapi_class_id);
	struct afb_wsapi *wsapi = holder ? holder->item : 0;
	struct holdcb *holdcb;
	int s;

	if (!wsapi)
		return JS_ThrowInternalError(ctx, "disconnected");

	if (!JS_IsFunction(ctx, argv[0]))
		return JS_ThrowTypeError(ctx, "function expected");

	holdcb = mkholdcb(ctx, this_val, argv[0]);
	if (!holdcb)
		return JS_ThrowOutOfMemory(ctx);

	s = afb_wsapi_describe(wsapi, holdcb);
	if (s < 0) {
		killholdcb(holdcb);
		return JS_ThrowInternalError(ctx, "afb_wsapi_describe failed with code %d", s);
	}
	return JS_UNDEFINED;
}

static JSValue wsapi_disconnect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	struct holder *holder = JS_GetOpaque(this_val, afb_wsapi_class_id);
	struct afb_wsapi *wsapi = holder ? holder->item : 0;
	if (!wsapi)
		return JS_FALSE;
	holder->item = 0;
	afb_wsapi_unref(wsapi);
	return JS_TRUE;
}

static JSValue wsapi_is_connected(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	struct holder *holder = JS_GetOpaque(this_val, afb_wsapi_class_id);
	struct afb_wsapi *wsapi = holder ? holder->item : 0;
	return JS_NewBool(ctx, !!wsapi);
}

struct afb_wsapi_itf itf_wsapi =
{
	.on_hangup = wsapi_on_hangup,
	.on_call = wsapi_on_call,
	.on_reply = wsapi_on_reply,
	.on_event_create = wsapi_on_event_create,
	.on_event_remove = wsapi_on_event_remove,
	.on_event_subscribe = wsapi_on_event_subscribe,
	.on_event_unsubscribe = wsapi_on_event_unsubscribe,
	.on_event_push = wsapi_on_event_push,
	.on_event_broadcast = wsapi_on_event_broadcast,
	.on_event_unexpected = wsapi_on_event_unexpected,
	.on_session_create = wsapi_on_session_create,
	.on_session_remove = wsapi_on_session_remove,
	.on_token_create = wsapi_on_token_create,
	.on_token_remove = wsapi_on_token_remove,
	.on_describe = wsapi_on_describe,
	.on_description = wsapi_on_description
};

static int mkAFBWSAPI(JSContext *ctx, JSValue target, const char *uri, int fd)
{
	struct afb_wsapi *wsapi;
	/* get the callback function */

	struct holder *holder = malloc(sizeof *holder);
	if (holder) {
		holder->ctx = ctx;
		holder->value = target;
		if (fd < 0)
			holder->item = client_wsapi(uri, &itf_wsapi, holder);
		else if (afb_wsapi_create((struct afb_wsapi **)&holder->item, fd, &itf_wsapi, holder) < 0)
			holder->item = 0;
		if (holder->item) {
			JS_SetOpaque(target, holder);
			JS_SetPropertyStr(ctx, target, "uri", JS_NewString(ctx, uri));
			return 1;
		}
		free(holder);
	}
	return 0;
}

struct server
{
	struct server *next, *previous;
	int fd;
	int used;
	JSContext *ctx;
	JSValue thisobj;
	JSValue func;
	char uri[];
};

static struct server *servers;

int wsapi_onclient(void *closure, int fd)
{
	int s = -1;
	struct server *srv = closure;
	struct holder *holder;
	JSValue obj, obj2;

	/* create a new instance of AFBWSAPI object */
	if (srv->used) {
		obj = JS_NewObjectClass(srv->ctx, afb_wsapi_class_id);
		if (JS_IsObject(obj)) {
				if (mkAFBWSAPI(srv->ctx, obj, srv->uri, fd)) {
					obj2 = JS_DupValue(srv->ctx, obj);
					JS_Call(srv->ctx, srv->func, srv->thisobj, 1, &obj2);
				}
		}
		JS_FreeValue(srv->ctx, obj);
	}
	return s;
}

static JSValue wsapi_serve(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	int s;
	struct closure *holder;
	const char *uri;
	struct server *srv;

	uri = JS_ToCString(ctx, argv[0]);
	if (!uri)
		return JS_ThrowTypeError(ctx, "string expected");
	for (srv = servers ; srv && strcmp(srv->uri, uri) ; srv = srv->next);
	if (JS_IsUndefined(argv[1]) || JS_IsNull(argv[1])) {
		JS_FreeCString(ctx, uri);
		if (srv) {
			/* close the current server */
			srv->used = 0;
			if (srv->next)
				srv->next->previous = srv->previous;
			if (srv->previous)
				srv->previous->next = srv->next;
			else
				servers = srv->next;
			if (srv->fd > 0) /* for older libafbcli! */
				close(srv->fd);
			JS_FreeValue(srv->ctx, srv->thisobj);
			JS_FreeValue(srv->ctx, srv->func);
			JS_FreeContext(srv->ctx);
			free(srv);
		}
		return JS_UNDEFINED;
	}
	if (!JS_IsFunction(ctx, argv[1])) {
		JS_FreeCString(ctx, uri);
		return JS_ThrowTypeError(ctx, "function or null expected");
	}
	if (srv) {
		JS_FreeCString(ctx, uri);
		JS_FreeValue(srv->ctx, srv->thisobj);
		JS_FreeValue(srv->ctx, srv->func);
		JS_FreeContext(srv->ctx);
		srv->thisobj = JS_DupValue(ctx, this_val);
		srv->func = JS_DupValue(ctx, argv[1]);
		srv->ctx = JS_DupContext(ctx);
	}
	else {
		srv = malloc(sizeof*srv + 1 + strlen(uri));
		if (!srv) {
			JS_FreeCString(ctx, uri);
			return JS_ThrowOutOfMemory(ctx);
		}
		strcpy(srv->uri, uri);
		JS_FreeCString(ctx, uri);
		srv->used = 0;
		s = client_serve(srv->uri, wsapi_onclient, srv);
		if (s < 0) {
			free(srv);
			return JS_ThrowInternalError(ctx, "failed with code %d", s);
		}
		srv->ctx = JS_DupContext(ctx);
		srv->func = JS_DupValue(ctx, argv[1]);
		srv->thisobj = JS_DupValue(ctx, this_val);
		srv->fd = s;
		srv->used = 1;
		srv->previous = 0;
		srv->next = servers;
		servers = srv;
	}
	return JS_UNDEFINED;
}

static JSValue AFBWSAPI_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	const char *uri;
	struct afb_wsapi *wsapi;
	JSValue obj = JS_UNDEFINED;
	JSValue proto;

	uri = JS_ToCString(ctx, argv[0]);
	if (!uri)
		goto error;

	/* using new_target to get the prototype is necessary when the
	class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	if (JS_IsException(proto))
		goto error2;
	obj = JS_NewObjectProtoClass(ctx, proto, afb_wsapi_class_id);
	JS_FreeValue(ctx, proto);
	if (JS_IsException(obj))
		goto error2;
	if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
		goto error3;

	if (mkAFBWSAPI(ctx, obj, uri, -1)) {
		JS_FreeCString(ctx, uri);
		return obj;
	}

error3:
	JS_FreeValue(ctx, obj);
error2:
	JS_FreeCString(ctx, uri);
error:
	return JS_EXCEPTION;
}

static void AFBWSAPI_finalizer(JSRuntime *rt, JSValue val)
{
	struct holder *holder = JS_GetOpaque(val, afb_wsapi_class_id);
	if (holder) {
		struct afb_wsapi *wsapi = holder ? holder->item : 0;
		holder->ctx = 0;
		holder->item = 0;
		if (wsapi) {
			afb_wsapi_hangup(wsapi);
			afb_wsapi_unref(wsapi);
		}
	}
}

static JSClassDef afb_wsapi_class = {
	.class_name = "AFBWSAPI",
	.finalizer = AFBWSAPI_finalizer,
}; 

static const JSCFunctionListEntry afb_wsapi_proto_funcs[] = {
	JS_CFUNC_DEF("isConnected_", 0, wsapi_is_connected),
	JS_CFUNC_DEF("disconnect_", 0, wsapi_disconnect),
	JS_CFUNC_DEF("call_", 6, wsapi_call),
	JS_CFUNC_DEF("sessionCreate_", 2, wsapi_session_create),
	JS_CFUNC_DEF("sessionRemove_", 1, wsapi_session_remove),
	JS_CFUNC_DEF("tokenCreate_", 2, wsapi_token_create),
	JS_CFUNC_DEF("tokenRemove_", 1, wsapi_token_remove),
	JS_CFUNC_DEF("eventCreate_", 2, wsapi_event_create),
	JS_CFUNC_DEF("eventRemove_", 1, wsapi_event_remove),
	JS_CFUNC_DEF("eventPush_", 2, wsapi_event_push),
	JS_CFUNC_DEF("eventUnexpected_", 1, wsapi_event_unexpected),
	JS_CFUNC_DEF("eventBroadcast_", 4, wsapi_event_broadcast),
	JS_CFUNC_DEF("describe_", 1, wsapi_describe),
};
static const JSCFunctionListEntry afb_wsapi_funcs[] = {
	JS_CFUNC_DEF("serve_", 2, wsapi_serve),
};

int AFBWSAPI_init(JSContext *ctx, JSModuleDef *m)
{
	JSValue proto, afbwsapi;

	/* create the class */
	JS_NewClassID(&afb_wsapi_class_id);
	JS_NewClass(JS_GetRuntime(ctx), afb_wsapi_class_id, &afb_wsapi_class);
	JS_NewClassID(&afb_wsapi_msg_class_id);
	JS_NewClass(JS_GetRuntime(ctx), afb_wsapi_msg_class_id, &afb_wsapi_msg_class);

	proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, proto, afb_wsapi_proto_funcs, countof(afb_wsapi_proto_funcs));

	afbwsapi = JS_NewCFunction2(ctx, AFBWSAPI_constructor, "AFBWSAPI", 1, JS_CFUNC_constructor, 0);
	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, afbwsapi, proto);
	JS_SetClassProto(ctx, afb_wsapi_class_id, proto);
	JS_SetPropertyFunctionList(ctx, afbwsapi, afb_wsapi_funcs, 1);
			
	JS_SetModuleExport(ctx, m, "AFBWSAPI", afbwsapi);
	return 0;
}

int AFBWSAPI_preinit(JSContext *ctx, JSModuleDef *m)
{
	JS_AddModuleExport(ctx, m, "AFBWSAPI");
	return 0;
}
