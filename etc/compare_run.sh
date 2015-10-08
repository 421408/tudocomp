#!/bin/bash

pushd ..
make tudocomp_driver
make compare_tool
make compare_workspace
popd
export PATH=../src/tudocomp_driver/:$PATH
export DATASETS=../../datasets
../src/compare_tool/debug/compare_tool $@