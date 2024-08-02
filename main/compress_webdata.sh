#!/bin/bash

HTTPD_DATA_DIR="srv"

[ ! -z $1 ] && HTTPD_DATA_DIR=$1

for file in $(find $HTTPD_DATA_DIR -regextype posix-extended -regex '.*\.(html|css|js|svg|png|jpg|jpeg|ico)'); do
    gzip -kfq --best $file
    echo -n "${file}.gz "
done
echo ""
