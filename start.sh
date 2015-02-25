#!/bin/bash

# dump core file
ulimit -c unlimited
./hdmilter -p inet:5800@127.0.0.1

