/**************************************************************************************
* example with AFBWSAPI connection
**************************************************************************************/

AFB = require('afb');

var events = {};
function getevent(name) {
	if (!events[name])
		events[name] = [];
	return events[name];
};

function onIncoming(ws) {
	var idg = 0;
	var idn = {};
	var getev = function(name) {
		if (!idn[name]) {
			idn[name] = ++idg;
			print("mkev "+String(idg)+" "+name+"\n");
			ws.eventCreate_(idg, name);
			print("DONE\n");
			getevent(name).push({ ws: ws, id: idg });
		}
		return idn[name];
	};
	ws.onCall = function(hndl, verb, obj, sessionid, tokenid, creds) {
		print("got a call!! "+verb+"("+JSON.stringify(obj)+")\n");
		if (obj.action == "SUBSCRIBE") {
			hndl.subscribe(getev(verb));
			hndl.reply(true);
		}
		else if (obj.action == "PUSH") {
			getevent(verb).forEach(function(item){
					print("pushing id "+String(item.id)+"\n");
					item.ws.eventPush_(item.id,obj.data);
				});
			hndl.reply(true);
		}
	};
};

AFB.AFBWSAPI.serve_("unix:@modbus", onIncoming);
AFB.wait_forever();
