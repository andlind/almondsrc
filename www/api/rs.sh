#!/usr/bin/bash

if [ -f /.dockerenv ]; then
	sleep 15
	/usr/bin/supervisorctl stop howru-api
	sleep 5
	/usr/bin/supervisorctl start howru-api
else
	sleep 15
	/usr/bin/systemctl stop howru-api.service
	sleep 5
	/usr/bin/systemctl start howru-api.service
fi
