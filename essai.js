/**************************************************************************************
 * basic test set
 */
import { AFBWSAPI, AFBWSJ1, expect_event, wait_completion, terminate } from 'libafbws';

var ws = new AFBWSAPI("unix:/tmp/hello");

ws.call_success("ping", true);
ws.call_error("pingfail", true);
ws.call_success("pingnull", true);
ws.call_error("pingbug", true);

ws.call_success("broadcast", {name: "event", data: true} );
expect_event();

ws.sessionCreate(7, "session");
ws.sessionId = 7;
ws.tokenCreate(5, "token");
ws.tokenId = 5;
ws.call_success("ping", true);

ws.call_success("eventadd", {tag: "e", name: "name-of-the-event"});
ws.call_success("eventsub", {tag: "e"});
ws.call_success("eventpush", {tag: "e", data: "\"text-of-the-event-1\""});
ws.call_success("eventunsub", {tag: "e"});
expect_event();
wait_completion();

ws.unexpected = true;
ws.call_success("eventpush", {tag: "e", data: "\"text-of-the-event-2\""});
ws.call_success("eventpush", {tag: "e", data: "\"text-of-the-event-2bis\""});
ws.call_success("eventpush", {tag: "e", data: "\"text-of-the-event-2bis\""});
expect_event();
wait_completion();
delete ws.unexpected;

ws.call_success("eventsub", {tag: "e"});
ws.call_success("eventpush", {tag: "e", data: "\"text-of-the-event-3\""});
ws.call_success("eventdel", {tag: "e"});
ws.call_error("eventpush", {tag: "e", data: "\"text-of-the-event-4\""});

ws.sessionRemove(7);
ws.tokenRemove(5);
delete ws.sessionId;
delete ws.tokenId;
ws.call_success("ping", true);

ws.describe(function(d){
	print("got description "+JSON.stringify(d)+"\n");
});

wait_completion();
ws.disconnect();

ws = new AFBWSJ1("localhost:1234/api");

ws.call_success("hello", "ping", true);
ws.call_error("hello", "pingfail", true);
ws.call_success("hello", "broadcast", {name: "event", data: true} );
expect_event();
ws.call_success("hello", "pingnull", true);
ws.call_error("hello", "pingbug", true);

wait_completion();
terminate();
