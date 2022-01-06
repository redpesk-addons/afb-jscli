/*
 * Copyright (C) 2019-2022 IoT.bzh Company
 * Author: José Bollo <jose.bollo@iot.bzh>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <quickjs/quickjs.h>
#include <libafbcli/afb-wsj1.h>

#define countof(x) (sizeof(x) / sizeof(*(x)))

static JSClassID afb_wsj1_class_id;

extern struct afb_wsj1 *client_wsj1(const char *uri, struct afb_wsj1_itf *itf, void *closure);

/**************************************************************/

struct holder
{
	JSContext *ctx;
	JSValue    value;
	void      *item;
};

static struct holder *mkholder(JSContext *ctx, JSValueConst value)
{
	struct holder *r = malloc(sizeof *r);
	if (r) {
		r->ctx = ctx;
		r->value = value;
		r->item = 0;
	}
	return r;
}

static void killholder(struct holder *h)
{
	free(h);
}

/**************************************************************/

void on_wsj1_hangup(void *closure, struct afb_wsj1 *_wsj1_)
{
	struct holder *holder = closure;
	struct afb_wsj1 *wsj1 = holder ? holder->item : 0;
	if (wsj1) {
		holder->item = 0;
		afb_wsj1_unref(wsj1);
		JSValue func = JS_GetPropertyStr(holder->ctx, holder->value, "onEvent");
		if (JS_IsFunction(holder->ctx, func)) {
			JS_Call(holder->ctx, func, holder->value, 0, 0);
		}
	}
}

void on_wsj1_call(void *closure, const char *api, const char *verb, struct afb_wsj1_msg *msg)
{
	afb_wsj1_reply_s(msg, "{\"response\":null,\"request\":{\"status\":\"unhandled\"}}", NULL, 1);
	afb_wsj1_msg_unref(msg);
}

void on_wsj1_event(void *closure, const char *event, struct afb_wsj1_msg *msg)
{
	const char *json;
	size_t jlen;
	JSValue argv[2];
	struct holder *holder = closure;
	JSValue func = JS_GetPropertyStr(holder->ctx, holder->value, "onEvent");
	if (JS_IsFunction(holder->ctx, func)) {
		argv[0] = JS_NewString(holder->ctx, event);
		json = afb_wsj1_msg_object_s(msg, &jlen);
		argv[1] = JS_ParseJSON(holder->ctx, json, jlen, "<wsj1.event>");
		JS_Call(holder->ctx, func, holder->value, 2, argv);
		JS_FreeValue(holder->ctx, argv[0]);
		JS_FreeValue(holder->ctx, argv[1]);
	}
}

struct afb_wsj1_itf itf_wsj1 = {
	.on_hangup = on_wsj1_hangup,
	.on_call = on_wsj1_call,
	.on_event = on_wsj1_event,
};

void wsj1_onreply(void *closure, struct afb_wsj1_msg *msg)
{
	const char *json;
	size_t jlen;
	JSValue argv[1];
	struct holder *holder = closure;
	struct holder *this_holder = holder->item;

	json = afb_wsj1_msg_object_s(msg, &jlen);
	argv[0] = JS_ParseJSON(holder->ctx, json, jlen, "<wsj1.event>");
	JS_Call(holder->ctx, holder->value, this_holder->value, 1, argv);
	JS_FreeValue(holder->ctx, argv[0]);
	JS_FreeValue(holder->ctx, holder->value);
	killholder(holder);
	JS_FreeValue(this_holder->ctx, this_holder->value);
}

static JSValue wsj1_call(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	int s;
	const char *api, *verb, *json;
	JSValue obj;
	struct holder *funholder;
	struct holder *holder = JS_GetOpaque(this_val, afb_wsj1_class_id);
	struct afb_wsj1 *wsj1 = holder ? holder->item : 0;

	if (!wsj1 || argc < 4 || !JS_IsFunction(ctx, argv[3]))
		goto error;

	api = JS_ToCString(ctx, argv[0]);
	if (!api)
		goto error;
	verb = JS_ToCString(ctx, argv[1]);
	if (!verb)
		goto error2;
	obj = JS_JSONStringify(ctx, argv[2], JS_UNDEFINED, JS_UNDEFINED);
	json = JS_ToCString(ctx, obj);
	JS_FreeValue(ctx, obj);
	if (!json)
		goto error3;

	obj = JS_DupValue(ctx, argv[3]);
	funholder = mkholder(ctx, obj);
	if (!funholder)
		goto error4;

	funholder->item = holder;
	s = afb_wsj1_call_s(wsj1, api, verb, json, wsj1_onreply, funholder);
	JS_FreeCString(ctx, json);
	if (s < 0)
		goto error5;
	JS_DupValue(ctx, this_val);
	return JS_UNDEFINED;

error5:
	killholder(holder);
error4:
	JS_FreeValue(ctx, obj);
	JS_FreeCString(ctx, json);
error3:
	JS_FreeCString(ctx, verb);
error2:
	JS_FreeCString(ctx, api);
error:
	return JS_EXCEPTION;
}

static JSValue wsj1_disconnect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	struct holder *holder = JS_GetOpaque(this_val, afb_wsj1_class_id);
	struct afb_wsj1 *wsj1 = holder ? holder->item : 0;
	if (!wsj1)
		return JS_FALSE;
	holder->item = 0;
	afb_wsj1_unref(wsj1);
	return JS_TRUE;
}

static JSValue wsj1_is_connected(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	struct holder *holder = JS_GetOpaque(this_val, afb_wsj1_class_id);
	struct afb_wsj1 *wsj1 = holder ? holder->item : 0;
	return JS_NewBool(ctx, !!wsj1);
}

static JSValue AFBWSJ1_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	const char *uri;
	struct afb_wsj1 *wsj1;
	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	struct holder *holder;

	uri = JS_ToCString(ctx, argv[0]);
	if (!uri)
		goto error;

	/* using new_target to get the prototype is necessary when the
	class is extended. */
	proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	if (JS_IsException(proto))
		goto error2;
	obj = JS_NewObjectProtoClass(ctx, proto, afb_wsj1_class_id);
	JS_FreeValue(ctx, proto);
	if (JS_IsException(obj))
		goto error2;
	if (JS_VALUE_GET_TAG(obj) != JS_TAG_OBJECT)
		goto error3;
	holder = mkholder(ctx, obj);
	if (!holder)
		goto error3;

	JS_SetPropertyStr(ctx, obj, "uri", JS_DupValue(ctx, argv[0]));
	holder->item = client_wsj1(uri, &itf_wsj1, holder);
	if (!holder->item)
		goto error4;

	JS_SetOpaque(obj, holder);
	JS_FreeCString(ctx, uri);
	return obj;

error4:
	killholder(holder);
error3:
	JS_FreeValue(ctx, obj);
error2:
	JS_FreeCString(ctx, uri);
error:
	return JS_EXCEPTION;
}

static void AFBWSJ1_finalizer(JSRuntime *rt, JSValue val)
{
	struct holder *holder = JS_GetOpaque(val, afb_wsj1_class_id);
	if (holder) {
		struct afb_wsj1 *wsj1 = holder ? holder->item : 0;
		holder->item = 0;
		if (wsj1)
			afb_wsj1_unref(wsj1);
		killholder(holder);
	}
}

static JSClassDef afb_wsj1_class = {
	.class_name = "AFBWSJ1",
	.finalizer = AFBWSJ1_finalizer,
}; 

static const JSCFunctionListEntry afb_wsj1_proto_funcs[] = {
	JS_CFUNC_DEF("isConnected_", 0, wsj1_is_connected),
	JS_CFUNC_DEF("disconnect_", 0, wsj1_disconnect),
	JS_CFUNC_DEF("call_", 4, wsj1_call),
};

int AFBWSJ1_init(JSContext *ctx, JSModuleDef *m)
{
	JSValue proto, afbwsj1;

	/* create the class */
	JS_NewClassID(&afb_wsj1_class_id);
	JS_NewClass(JS_GetRuntime(ctx), afb_wsj1_class_id, &afb_wsj1_class);

	proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, proto, afb_wsj1_proto_funcs, countof(afb_wsj1_proto_funcs));

	afbwsj1 = JS_NewCFunction2(ctx, AFBWSJ1_constructor, "AFBWSJ1", 1, JS_CFUNC_constructor, 0);
	/* set proto.constructor and ctor.prototype */
	JS_SetConstructor(ctx, afbwsj1, proto);
	JS_SetClassProto(ctx, afb_wsj1_class_id, proto);
			
	JS_SetModuleExport(ctx, m, "AFBWSJ1", afbwsj1);
	return 0;
}

int AFBWSJ1_preinit(JSContext *ctx, JSModuleDef *m)
{
	JS_AddModuleExport(ctx, m, "AFBWSJ1");
	return 0;
}


