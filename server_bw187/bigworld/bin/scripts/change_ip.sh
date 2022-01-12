#!/bin/sh

# When using this file, it should be changed to be writable (and probably
# readable) only by root.

echo Changing $1 to $2
/sbin/ifconfig $1 $2
# echo New details of $1:
# /sbin/ifconfig $1

# Fix up the routing table
/sbin/route add -net default gw 10.40.1.2 dev eth0
# echo New routing table is:
# /sbin/route
