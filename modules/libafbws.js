/**************************************************************************************
 * INCLUDE AFB
 */
import * as AFB from 'afb';
import * as diag from 'diag';

export * from 'afb';
export * from 'diag';

var afb_loop = AFB.afb_loop;
var afb_break = AFB.afb_break;
var AFBWSJ1 = AFB.AFBWSJ1;
var AFBWSAPI = AFB.AFBWSAPI;
var wait_until = AFB.wait_until;
var wait_until = AFB.wait_until;
var wait_while = AFB.wait_while;
var wait_forever = AFB.wait_forever;
var wait_calls = AFB.wait_calls;
var wait_count = AFB.wait_count;
var wait_completion = AFB.wait_completion;
var expect_event = AFB.expect_event;
var expected_events = AFB.expected_events;

/**************************************************************************************
 * INCLUDE DIAG
 */
var failure = diag.failure;
var diagnose = diag.assert;
var terminate = diag.terminate;
var success = diag.success;

/**************************************************************************************
 * This section defines a basic calls
 */

function contains(obj, ref) {
	var i;

	if (typeof ref === "undefined")
		return true;

	if (ref instanceof Object) {
		for(i in ref)
			if (!(i in obj) || !contains(obj[i],ref[i]))
				return false;
		return true;
	}

	if (ref instanceof Array) {
		for(i in ref)
			if (!(i in obj) || !contains(obj[i],ref[i]))
				return false;
		return true;
	}

	return obj === ref;
}

/**************************************************************************************
 * This section defines wsj1 calls
 */

AFBWSJ1.prototype.call_match = function(api, verb, obj, match, notmatch) {
	var m, nm;
	if (typeof match === "undefined")
		m = function(x) { return true; };
	else if (!(match instanceof Function))
		m = function(x) { return contains(x,match); };
	else
		m = match;
	if (typeof notmatch === "undefined")
		nm = function(x) { return false; };
	else if (!(notmatch instanceof Function))
		nm = function(x) { return contains(x,notmatch); };
	else
		nm = notmatch;
	this.call(api, verb, obj,
		function(x) {
			var status = m(x) && !nm(x);
			diagnose(status, {
				api: api, verb: verb, request: obj, reply: x, match: match, notmatch: notmatch
			});
		});
};

AFBWSJ1.prototype.call_success = function(api, verb, obj) {
	this.call_match(api, verb, obj,
		{jtype:"afb-reply",request:{status:"success"}},
		undefined);
};

AFBWSJ1.prototype.call_error = function(api, verb, obj) {
	this.call_match(api, verb, obj,
		{jtype:"afb-reply",request:{status:undefined}},
		{request:{status:"success"}});
};

/**************************************************************************************
 * This section defines wsapi calls
 */

AFBWSAPI.prototype.call_match = function(verb, obj, match, notmatch) {
	var m, nm;
	if (typeof match === "undefined")
		m = function(res,err,info) { return true; };
	else if (!(match instanceof Function))
		m = function(res,err,info) { return contains(res,match); };
	else
		m = match;
	if (typeof notmatch === "undefined")
		nm = function(res,err,info) { return false; };
	else if (!(notmatch instanceof Function))
		nm = function(res,err,info) { return contains(res,notmatch); };
	else
		nm = notmatch;
	this.call(verb, obj,
		function(res,err,info) {
			var status = m(res,err,info) && !nm(res,err,info);
			diagnose(status, {
				verb: verb,
				request: obj,
				reply: res,
				error: err,
				info: info,
				match: match,
				notmatch: notmatch
			});
		});
};

AFBWSAPI.prototype.call_success = function(verb, obj) {
	this.call_match(verb, obj,
		function(res,err,info) { return err==null || err=="success"; },
		undefined);
};

AFBWSAPI.prototype.call_error = function(verb, obj) {
	this.call_match(verb, obj,
		function(res,err,info) { return err!=null && err!="success"; },
		undefined);
};
