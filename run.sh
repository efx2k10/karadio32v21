#!/usr/bin/env bash
cd ..
cd esp-idf

./install.sh
. ./export.sh


cd ..
cd Karadio32v21

cd webpage
./generate.sh

cd ..
make app
