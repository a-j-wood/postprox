#!/bin/sh

/usr/bin/clamdscan --disable-summary --stdout --mbox - < "$EMAIL"
STATUS=$?

[ -z "$STATUS" ] && STATUS=2

[ $STATUS -eq 0 ] && exit 0
if [ $STATUS -eq 1 ]; then
	echo 550 Message contains a virus 1>&2
	exit 1
fi

exit 2

# EOF
