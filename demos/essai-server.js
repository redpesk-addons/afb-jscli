
import * as AFB from 'afb';

function onIncoming(ws) {
	ws.onCall = function(hndl, verb, obj, sessionid, tokenid, creds) {
		print("got a call!! "+verb+"("+obj+")\n");
		print(hndl.reply);
		hndl.reply(obj, undefined, verb);
	};
};

AFB.AFBWSAPI.serve_("unix:@toto", onIncoming);
AFB.wait_forever();
