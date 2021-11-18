#!/bin/bash

set -x

#BINDINGDIR=$HOME/.local/share/afb-binding/tutorials
BINDINGDIR=/home/jobol/dev/afb/binding/tutorials/v4
export JS_PATH=$(dirname $0)/modules

if [[ "$1" = -v ]]
then
        EXTRARGS="-v --tracereq=common --traceevt=common"
        shift
fi

PORT=1234
ARGS="-p $PORT -b $BINDINGDIR/hello4.so --ws-server unix:/tmp/hello"
afb-binder $ARGS $EXTRARGS &
ap=$!

sleep 2

valgrind --leak-check=full --show-leak-kinds=definite ./afb-jscli essai.js
RET=$?

afb-client localhost:$PORT/api hello exit > /dev/null
kill $ap
exit $RET
