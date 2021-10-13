
AFB = require('afb');

function onIncoming(ws) {
	ws.onCall = function(hndl, verb, obj, sessionid, tokenid, creds) {
		print("got a call!! "+verb+"("+obj+")\n");
		hndl.reply(obj, undefined, verb);
	};
};

AFB.AFBWSAPI.serve_("unix:/tmp/toto", onIncoming);
AFB.wait_forever();
