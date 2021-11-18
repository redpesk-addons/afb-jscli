/**************************************************************************************
* example with AFBWSAPI connection
**************************************************************************************/
import * as AFB from 'libafbws';

modbus = new AFB.AFBWSAPI("unix:@modbus");
redis = new AFB.AFBWSAPI("unix:@redis");

modbus._eventids_ = {};

modbus.onEventCreate = function (id, name) { /* TODO add in LIB */
	print("add event " + String(id) + " => " + name + "\n");
	this._eventids_[id] = name;
}

modbus.onEventRemove = function (id) { /* TODO add in LIB */
	print("del event " + String(id) + "\n");
	delete this._eventids_[id];
}

modbus.onEventPush = function (id, obj) {
	print("received event " + String(id) 
		 + " " + this._eventids_[id]
		 + " " + JSON.stringify(obj)
		 + "\n");
	redis.call("ts_jinsert", {
		class: this._eventids_[id],
		data: obj,
		timestamp: '*'
	});
}

function subscribe_modbus() {

	modbus.call("1510SP/dig", {action: "SUBSCRIBE"});
	modbus.call("1510SP/ana", {action: "SUBSCRIBE"});
}

subscribe_modbus();

AFB.wait_forever();

