import * as AFB from 'afb';
//import { AFBWSJ1 } from './modules/afb';
//import * as AFB from 'modules/afb/afb-qjs.so';

//console.log(AFB.afb_loop);

/*
console.log("hello", AFB.AFBWSJ1);
var x = new AFB.AFBWSJ1("localhost:1234/api");
console.log("buon dia");
x.call("hello","ping",true, function(x){console.log("received ", x);});
console.log("salud");
AFB.wait_completion();
console.log("ciao");
*/

var y = new AFB.AFBWSAPI("unix:@hello");
y.call("ping",true, function(x){console.log("received ", x);});
AFB.wait_completion();
