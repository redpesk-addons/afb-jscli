/*
 * afb-jscli
 *
 * Inspired by QuickJS from Fabrice Bellard and Charlie Gordon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>
#include <limits.h>

#include "quickjs/cutils.h"
#include "quickjs/quickjs-libc.h"

static const char *extensions[] = {
	"",
	".js",
	".so",
	"/index.js"
};

/* variable names for path search */
static const char *varnames_for_path_search[] = { "JS_PATH", "AFB_JSCLI_PATH", NULL };

static const char *currentdir;

static int try_path_of_required(char *path, size_t size, const char *fmt, ...)
{
	va_list ap;
	size_t s;
	int idx, rc;
	struct stat st;

	/* get the base name */
	va_start(ap, fmt);
	s = vsnprintf(path, size, fmt, ap);
	va_end(ap);

	if (s < size) {
		for (idx = 0 ; idx < sizeof extensions / sizeof *extensions ; idx++) {
			if (s + strlen(extensions[idx]) < size) {
				strcpy(path + s, extensions[idx]);
				if (access(path, R_OK) == 0 && stat(path, &st) == 0 && S_ISREG(st.st_mode))
					return 1;
			}
		}
	}
	return 0;
}

static int path_search_enabled(const char *required)
{
	if (required[0] == '/')
		return 0;
	if (required[0] != '.')
		return required[0] != '/';
	if (required[1] != '.')
		return required[1] != '/';
	return required[2] != '/';
}

static int search_path_of_required(char *path, size_t size, const char *required)
{
	int ivar, iend, found;
	const char *spat;

	found = try_path_of_required(path, size, "%s", required);
	if (!found && path_search_enabled(required)) {
		if (currentdir)
			found = try_path_of_required(path, size, "%s/%s", currentdir, required);
		for (ivar = 0 ; !found && varnames_for_path_search[ivar] ; ivar++) {
			spat = getenv(varnames_for_path_search[ivar]);
			while (spat && *spat && !found) {
				for ( ; *spat && *spat == ':' ; spat++);
				for (iend = 0 ; spat[iend] && spat[iend] != ':' ; iend++);
				if (iend) {
					found = try_path_of_required(path, size, "%.*s/%s", iend, spat, required);
					spat += iend;
				}
			}
		}
	}
	return found;
}

static int eval_buf(JSContext *ctx, const void *buf, int buf_len,
                    const char *filename, int eval_flags)
{
	JSValue val;
	int ret;

	if ((eval_flags & JS_EVAL_TYPE_MASK) == JS_EVAL_TYPE_MODULE) {
		/* for the modules, we compile then run to be able to set
		import.meta */
		val = JS_Eval(ctx, buf, buf_len, filename,
				eval_flags | JS_EVAL_FLAG_COMPILE_ONLY);
		if (!JS_IsException(val)) {
			js_module_set_import_meta(ctx, val, FALSE, TRUE);
			val = JS_EvalFunction(ctx, val);
		}
	} else {
		val = JS_Eval(ctx, buf, buf_len, filename, eval_flags);
	}
	if (JS_IsException(val)) {
		js_std_dump_error(ctx);
		ret = -1;
	} else {
		ret = 0;
	}
	JS_FreeValue(ctx, val);
	return ret;
}

static int eval_file(JSContext *ctx, const char *filename)
{
	uint8_t *buf;
	int ret;
	size_t buf_len;

	buf = js_load_file(ctx, &buf_len, filename);
	if (!buf) {
		perror(filename);
		exit(1);
	}

	ret = eval_buf(ctx, buf, buf_len, filename, JS_EVAL_TYPE_MODULE);
	js_free(ctx, buf);
	return ret;
}

JSModuleDef *module_load(JSContext *ctx, const char *module_name, void *opaque)
{
	JSModuleDef *m;
	char path[PATH_MAX + 1];
	char dirname[PATH_MAX + 1];
	const char *name, *prvdir, *tmp;
	size_t length;

	name = search_path_of_required(path, sizeof path, module_name) ? path : module_name;
	prvdir = currentdir;
	tmp = strrchr(name, '/');
	if (!tmp)
		currentdir = 0;
	else {
		length = tmp - name;
		if (length > PATH_MAX)
			length = PATH_MAX;
		memcpy(dirname, name, length);
		dirname[length] = 0;
		currentdir = dirname;
	}

	m = js_module_loader(ctx, name, opaque);

	currentdir = prvdir;
	return m;
}


/* also used to initialize the worker context */
static JSContext *JS_NewCustomContext(JSRuntime *rt)
{
	JSContext *ctx = JS_NewContext(rt);
	if (ctx) {
		/* system modules */
		js_init_module_std(ctx, "std");
		js_init_module_os(ctx, "os");
	}
	return ctx;
}

#define PROG      "afb-jscli"
#define VERSION   "0.0.0"

void help(void)
{
	printf(PROG " version " VERSION "\n"
		"usage: " PROG " [options] [file [args]]\n"
		"-h  --help         list options\n"
	);
	exit(1);
}

int main(int argc, char **argv)
{
	JSRuntime *rt;
	JSContext *ctx;
	int optind, status = 1;

	/* cannot use getopt because we want to pass the command line to
	the script */
	optind = 1;
	while (optind < argc && *argv[optind] == '-') {
		char *arg = argv[optind] + 1;
		const char *longopt = "";
		/* a single - is not an option, it also stops argument scanning */
		if (!*arg)
			break;
		optind++;
		if (*arg == '-') {
			longopt = arg + 1;
			arg += strlen(arg);
			/* -- stops argument scanning */
			if (!*longopt)
				break;
		}
		for (; *arg || *longopt; longopt = "") {
			char opt = *arg;
			if (opt)
				arg++;
			if (opt == 'h' || opt == '?' || !strcmp(longopt, "help")) {
				help();
				continue;
			}
			if (opt) {
				fprintf(stderr, PROG": unknown option '-%c'\n", opt);
			} else {
				fprintf(stderr, PROG": unknown option '--%s'\n", longopt);
			}
			help();
		}
	}

	rt = JS_NewRuntime();
	if (!rt) {
		fprintf(stderr, PROG": cannot allocate JS runtime\n");
		exit(2);
	}
	js_std_set_worker_new_context_func(JS_NewCustomContext);
	js_std_init_handlers(rt);
	ctx = JS_NewCustomContext(rt);
	if (!ctx) {
		fprintf(stderr, PROG": cannot allocate JS context\n");
		exit(2);
	}

	/* loader for ES6 modules */
	JS_SetModuleLoaderFunc(rt, NULL, module_load, NULL);
	JS_SetHostPromiseRejectionTracker(rt, js_std_promise_rejection_tracker, NULL);
	js_std_add_helpers(ctx, argc - optind, argv + optind);

	/* make 'std' and 'os' visible to non module code */
	const char *str =
		"import * as std from 'std';\n"
		"import * as os from 'os';\n"
		"globalThis.std = std;\n"
		"globalThis.os = os;\n";
	eval_buf(ctx, str, strlen(str), "<input>", JS_EVAL_TYPE_MODULE);
	while (optind < argc) {
		const char *filename;
		filename = argv[optind++];
		if (eval_file(ctx, filename))
			goto fail;
	}
	js_std_loop(ctx);

	status = 0;
 fail:
	js_std_free_handlers(rt);
	JS_FreeContext(ctx);
	JS_FreeRuntime(rt);
	return status;
}
