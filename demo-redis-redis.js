/**************************************************************************************
* example with AFBWSAPI connection
**************************************************************************************/

import * as AFB from 'libafbws';

function onIncoming(ws) {
	ws.onCall = function(hndl, verb, obj, sessionid, tokenid, creds) {
		print("got a call!! "+verb+"("+JSON.stringify(obj)+")\n");
		hndl.reply(obj, undefined, verb);
	};
};

AFB.AFBWSAPI.serve_("unix:@redis", onIncoming);
AFB.wait_forever();
