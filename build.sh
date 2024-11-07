#!/bin/bash
set -xe

cd ./web
npm config set loglevel verbose
npm i
npm run build
npx svelteesp32 -e async -s ./dist -o svelteesp32.h --etag=true
cp -rf svelteesp32.h  ../src/ssvc2/
cd ../
pio run