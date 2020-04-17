#!/bin/sh

if [ $1 == "enable" ]
then
        cp /lib/systemd/system/obmc-chassis-poweroff@.target /lib/systemd/system/obmc-chassis-poweroff@_target
        rm /lib/systemd/system/obmc-chassis-poweroff@.target
        systemctl daemon-reload

        echo "disable chassis power off target"

elif [ $1 == "disable" ]
then
        cp /lib/systemd/system/obmc-chassis-poweroff@_target /lib/systemd/system/obmc-chassis-poweroff@.target
        rm /lib/systemd/system/obmc-chassis-poweroff@_target
        systemctl daemon-reload

        echo "enable chassis power off target"
else
        echo "argument error"
fi

