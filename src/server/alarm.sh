#!/bin/bash

APPID=$1
APIKEY=$2
HOST=https://api.bluehouselab.com:443

SENDER=$3
RECEIVER=$4

TEMP=$5
HUM=$6

curl -i -u $APPID:$APIKEY \
     -X POST $HOST/smscenter/v1.0/sendsms \
     -H "Content-Type: application/json;charset=utf-8" \
     -d '{"sender": "'$SENDER'", "receivers": ['$RECEIVER'], "content": "[PCB]경고!! 식물배양실 온도('$TEMP')/습도('$HUM')가 임계치를 넘었습니다!"}'
