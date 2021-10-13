/**************************************************************************************
 * basic test set
 */

ws = new AFBWSAPI("unix:/tmp/toto");

ws.call_success("ping", true);
ws.call_success("pingfail", true);
ws.call_success("pingnull", true);

wait_completion();
terminate();
