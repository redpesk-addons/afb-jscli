/**************************************************************************************
 * basic test set
 */

ws = new AFBWSAPI("unix:@hello");

ws.call_success("eventadd", {tag: "ev1", name:"evenement"});
ws.call_success("eventsub", {tag: "ev1"});

ws.onEventPush = function(id, obj) {
	if (Array.isArray(obj)) {
		print("j'ai un tableau\n");
	}
}

wait_forever();

