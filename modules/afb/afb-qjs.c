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
	uint64_t delay = (uint64_t)-1, adelay;

	if (argc > 0 && JS_ToInt64(ctx, &adelay, argv[0]) >= 0)
		delay = 1000LL * adelay;

	sts = sd_event_run(sdev,  delay);
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



























