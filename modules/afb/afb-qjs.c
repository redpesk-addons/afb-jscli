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

#include <stdbool.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <systemd/sd-event.h>

#include <quickjs/quickjs.h>

#include <libafbcli/afb-wsapi.h>
#include <libafbcli/afb-wsj1.h>
#include <libafbcli/afb-ws-client.h>

extern int AFBWSAPI_preinit(JSContext *ctx, JSModuleDef *m);
extern int AFBWSAPI_init(JSContext *ctx, JSModuleDef *m);

extern int AFBWSJ1_preinit(JSContext *ctx, JSModuleDef *m);
extern int AFBWSJ1_init(JSContext *ctx, JSModuleDef *m);

#define countof(x) (sizeof(x) / sizeof(*(x)))

/**************************************************************/

static sd_event *sdev = NULL;
static sd_event_source *break_src = NULL;
static int break_fd;

/**************************************************************/

struct afb_wsj1 *client_wsj1(const char *uri, struct afb_wsj1_itf *itf, void *closure)
{
	return afb_ws_client_connect_wsj1(sdev, uri, itf, closure);
}

struct afb_proto_ws *client_proto_ws(const char *uri, struct afb_proto_ws_client_itf *itf, void *closure)
{
	return afb_ws_client_connect_api(sdev, uri, itf, closure);
}

struct afb_wsapi *client_wsapi(const char *uri, struct afb_wsapi_itf *itf, void *closure)
{
	return afb_ws_client_connect_wsapi(sdev, uri, itf, closure);
}

int client_serve(const char *uri, int (*onclient)(void*,int), void *closure)
{
	return afb_ws_client_serve(sdev, uri, onclient, closure);
}

/**************************************************************/

static JSValue qjs_loop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	int sts;
	int64_t delay = -1, adelay;

	if (argc > 0 && JS_ToInt64(ctx, &adelay, argv[0]) >= 0)
		delay = 1000LL * adelay;

	sts = sd_event_run(sdev,  (uint64_t)delay);
	return JS_NewBool(ctx, sts > 0);
}

static JSValue qjs_break(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	uint64_t x = 1;
	write(break_fd, &x, sizeof x);
	return JS_UNDEFINED;
}

static JSValue qjs_print(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	int i = 0, p = 0;
	while(i < argc) {
		const char *str = JS_ToCString(ctx, argv[i++]);
		if (str) {
			p++;
			printf("%s", str);
			JS_FreeCString(ctx, str);
		}
	}
	if (p)
		printf("\n");
	return JS_UNDEFINED;
}

static int break_cb(sd_event_source *s, int fd, uint32_t revents, void *userdata)
{
	uint64_t x;
	read(break_fd, &x, sizeof x);
	return 0;
}

static int init_loop()
{
	int s;

	/* get the event loop */
	s = sd_event_default(&sdev);
	if (s >= 0) {
		s = eventfd(0, EFD_CLOEXEC|EFD_SEMAPHORE);
		if (s >= 0) {
			break_fd = s;
			s = sd_event_add_io(sdev, &break_src, break_fd, EPOLLIN, break_cb, NULL);
		}
	}

	return s;
}

static const JSCFunctionListEntry afb_qjs_funcs[] = {
    JS_CFUNC_DEF("afb_loop", 0, qjs_loop ),
    JS_CFUNC_DEF("afb_break", 0, qjs_break ),
};

static int js_afb_init(JSContext *ctx, JSModuleDef *m)
{
	int rc;

	rc = init_loop();
	if (rc < 0)
		return rc;

	AFBWSAPI_init(ctx, m);
	AFBWSJ1_init(ctx, m);
	return JS_SetModuleExportList(ctx, m, afb_qjs_funcs, countof(afb_qjs_funcs));
	return 0;
}

#define JS_INIT_MODULE js_init_module
//#define JS_INIT_MODULE js_init_module_afb

JSModuleDef *JS_INIT_MODULE(JSContext *ctx, const char *module_name)
{
	JSModuleDef *m;
	m = JS_NewCModule(ctx, module_name, js_afb_init);
	if (!m)
		return NULL;
	JS_AddModuleExportList(ctx, m, afb_qjs_funcs, countof(afb_qjs_funcs));

	AFBWSAPI_preinit(ctx, m);
	AFBWSJ1_preinit(ctx, m);
	return m;
}



























