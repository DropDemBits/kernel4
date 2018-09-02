#!/bin/bash
nm -n $1 | egrep " [Tt] " | awk '{ print $1" "$3 }' > $2.sym
