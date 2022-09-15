/**************************************************************************************
 * This section defines a basic success/failure calls
 *
 * TAP: Test Anything Protocol (see https://testanything.org/)
 */

var count_of_tests = 0;
var count_of_success = 0;
var count_of_failure = 0;

var stop_on_failure = false;
var MODE_TAP = 0;
var MODE_OLD = 1;
var MODE_OLD_SUCCESS = 2;
var mode = MODE_TAP;
var mode_names = {};
mode_names[MODE_TAP] = "tap";
mode_names[MODE_OLD] = "old";
mode_names[MODE_OLD_SUCCESS] = "success";

function str(item) {
	return typeof(item) == 'object' ? JSON.stringify(item) : String(item);
}

export function success(obj) {
	count_of_success++;
	count_of_tests++;
	switch(mode) {
	case MODE_TAP:
		print("ok " + count_of_tests + " " + str(obj) + "\n");
		break;
	case MODE_OLD:
		break;
	case MODE_OLD_SUCCESS:
		print("SUCCESS: " + str(obj) + "\n");
		break;
	}
}

export function failure(obj) {
	count_of_failure++;
	count_of_tests++;

	switch(mode) {
	case MODE_TAP:
		print("not ok " + count_of_tests + " " + str(obj) + "\n");
		break;
	case MODE_OLD:
	case MODE_OLD_SUCCESS:
		print("FAILURE: " + str(obj) + "\n");
		break;
	}
	
	if (stop_on_failure)
		exit(1);
}

export function assert(isok, obj) {
	if (isok)
		success(obj);
	else
		failure(obj);
}

export function terminate() {
	switch(mode) {
	case MODE_TAP:
		print("1.." + count_of_tests + "\n");
		break;
	case MODE_OLD:
	case MODE_OLD_SUCCESS:
		print("success: "+count_of_success+" / "+(count_of_success+count_of_failure)+"\n")
		print("failure: "+count_of_failure+" / "+(count_of_success+count_of_failure)+"\n")
		break;
	}
	std.exit(count_of_failure == 0 ? 0 : 1);
}

export function options(obj) {
	if (obj && 'stop-on-failure' in obj)
		stop_on_failure = obj['stop-on-failure'];
	if (obj && 'mode' in obj) {
		switch (obj['mode']) {
		default:
		case mode_names[MODE_TAP]: mode = MODE_TAP; break;
		case mode_names[MODE_OLD]: mode = MODE_OLD; break;
		case mode_names[MODE_OLD_SUCCESS]: mode = MODE_OLD_SUCCESS; break;
		}
	}
	return { 'stop-on-failure': stop_on_failure, 'mode': mode_names[mode] };
}
