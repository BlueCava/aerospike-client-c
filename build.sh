#!/bin/bash

apt-get install -y libc6-dev libssl-dev liblua5.1-dev autoconf automake libtool g++
apt-get install -y make
export CPATH=$CPATH:/usr/include/lua5.1

ln -s /usr/lib/x86_64-linux-gnu/liblua5.1.so /usr/lib/liblua.so
ln -s /usr/lib/x86_64-linux-gnu/liblua5.1.a /usr/lib/liblua.a

git submodule update --init

make clean
make

ln -s /usr/local/source/aerospike-client-c/target/Linux-x86_64/include/aerospike /usr/include/aerospike
ln -s /usr/local/source/aerospike-client-c/target/Linux-x86_64/include/citrusleaf /usr/include/citrusleaf
