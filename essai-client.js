/**************************************************************************************
 * basic test set
 */
import { AFBWSAPI, wait_completion, terminate } from 'libafbws';

var ws = new AFBWSAPI("unix:@toto");

ws.call_success("ping", true);
ws.call_success("pingfail", true);
ws.call_success("pingnull", true);

wait_completion();
terminate();
