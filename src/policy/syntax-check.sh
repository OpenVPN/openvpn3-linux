#!/bin/bash

set -eu

for f in *.conf;
do
	xmllint --format $f > /dev/null
done
