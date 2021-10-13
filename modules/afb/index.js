import * as afbqjs from 'afb-qjs.so';
import * as os from 'os';

export var AFBWSJ1 = afbqjs.AFBWSJ1;
export var AFBWSAPI = afbqjs.AFBWSAPI;
export var afb_loop = afbqjs.afb_loop; /* TODO remove ? */
export var afb_break = afbqjs.afb_break; /* TODO remove ? */

/**************************************************************************************
 * This section events to wait
 */
var pending_calls = 0;
export var expected_events = 0;

export function wait(timeoutms, pred) {
	timeoutms = timeoutms === undefined ? -1 : timeoutms;
	if (pred === undefined)
		pred = function(){return true;}
	else if (typeof(pred) != 'function')
		pred = function(){return pred;}
	while(true) {
		var z = pred();
		if (!z)
			break;
		afb_loop(timeoutms);
	}
}

export function wait_while(pred) {
	wait(-1, pred);
}
 
export function wait_forever() {
	wait(-1);
}
 
export function wait_calls() {
	wait_while(function(){ return pending_calls != 0; });
}
 
export function wait_events() {
	wait_while(function(){ return expected_events != 0; });
}
 
export function wait_count(n) {
	var i = 0;
	wait_while(function(){ return ++i <= n; });
}
 
export function wait_completion() {
	wait_while(function(){
		return expected_events + pending_calls;
	});
}

function enter_call() {
	pending_calls++;
}

function leave_call() {
	pending_calls--;
	afb_break();
}

export function expect_event() {
	expected_events++;
}

function got_event() {
	if (expected_events)
		expected_events--;
	afb_break();
}


/**************************************************************************************
 * This section defines AFBWSJ1 calls
 */
 
AFBWSJ1.prototype.call = function(api, verb, obj, fun) {
	enter_call();
	this.call_(api, verb, obj, function(r) {
		try {
			if (fun) fun(r);
		}
		finally {
			leave_call();
		}
	});
};

AFBWSJ1.prototype.isConnected = AFBWSJ1.prototype.isConnected_;
AFBWSJ1.prototype.disconnect = AFBWSJ1.prototype.disconnect_;

AFBWSJ1.prototype.onEvent = function (e, o) {
	print("received event " + e + ": " + JSON.stringify(o) + "\n");
	got_event();
};

AFBWSJ1.prototype.onCall = function (msg, api, verb, data) {
	print("received call " + api + "/" + verb + "(" + JSON.stringify(data) + ")\n");
	msg.reply({request:{status:"unhandled"}}, true);
};

/**************************************************************************************
 * This section defines AFBWSAPI calls
 */

AFBWSAPI.prototype.call = function(verb, obj, fun) {
	enter_call();
	this.call_(verb, obj, function(res,err,info) {
		try {
			if (fun) fun(res,err,info);
		}
		finally {
			leave_call();
		}
	});
};

AFBWSAPI.prototype.describe = function(fun) {
	enter_call();
	this.describe_(function(desc){
		fun(desc);
		leave_call();
	});
};

AFBWSAPI.prototype.isConnected = AFBWSAPI.prototype.isConnected_;
AFBWSAPI.prototype.disconnect = AFBWSAPI.prototype.disconnect_;
AFBWSAPI.prototype.serve = AFBWSAPI.prototype.serve_;
AFBWSAPI.prototype.sessionCreate = AFBWSAPI.prototype.sessionCreate_;
AFBWSAPI.prototype.sessionRemove = AFBWSAPI.prototype.sessionRemove_;
AFBWSAPI.prototype.tokenCreate = AFBWSAPI.prototype.tokenCreate_;
AFBWSAPI.prototype.tokenRemove = AFBWSAPI.prototype.tokenRemove_;
AFBWSAPI.prototype.eventCreate = AFBWSAPI.prototype.eventCreate_;
AFBWSAPI.prototype.eventRemove = AFBWSAPI.prototype.eventRemove_;
AFBWSAPI.prototype.eventPush = AFBWSAPI.prototype.eventPush_;
AFBWSAPI.prototype.eventUnexpected = AFBWSAPI.prototype.eventUnexpected_;
AFBWSAPI.prototype.eventBroadcast = AFBWSAPI.prototype.eventBroadcast_;

AFBWSAPI.prototype.onHangup = function () {
	print("onHangup " + "\n");
};

AFBWSAPI.prototype.onCall = function (hndl, verb, obj, sessionid, tokenid, creds) {
	print("onCall " + verb + "(" + JSON.stringify(obj) +
		") session("+sessionid+")  token("+tokenid+")"+
		" ["+creds+"]\n");
};

AFBWSAPI.prototype.onEventCreate = function (id, name) {
	print("onEventCreate(" + id + ") " + name + "\n");
};

AFBWSAPI.prototype.onEventRemove = function (id) {
	print("onEventRemove(" + id + ")\n");
};

AFBWSAPI.prototype.onEventSubscribe = function (id) {
	print("onEventSubscribe(" + id + ")\n");
};

AFBWSAPI.prototype.onEventUnsubscribe = function () {
	print("onEventUnsubscribe(" + id + ")\n");
};

AFBWSAPI.prototype.onEventPush = function (id, obj) {
	print("onEventPush(" + id + ") " + JSON.stringify(obj) + "\n");
	got_event();
	if (this.unexpected) {
		print("unexpected event " + id + "\n");
		this.eventUnexpected_(id);
	}
};

AFBWSAPI.prototype.onEventBroadcast = function (name,obj,hops,uuid) {
	print("onEventBroadcast(" + name + ") " + JSON.stringify(obj) + "\n");
	got_event();
};

AFBWSAPI.prototype.onEventUnexpected = function (id) {
	print("onEventUnexpected(" + id + ")\n");
};

AFBWSAPI.prototype.onSessionCreate = function (id, name) {
	print("onSessionCreate(" + id + ") " + name + "\n");
};

AFBWSAPI.prototype.onSessionRemove = function (id) {
	print("onSessionRemove(" + id + ")\n");
};

AFBWSAPI.prototype.onTokenCreate = function (id, name) {
	print("onTokenCreate(" + id + ") " + name + "\n");
};

AFBWSAPI.prototype.onTokenRemove = function (id) {
	print("onTokenRemove(" + id + ")\n");
};

AFBWSAPI.prototype.onDescribe = function (hndl) {
	print("onDescribe\n");
};
