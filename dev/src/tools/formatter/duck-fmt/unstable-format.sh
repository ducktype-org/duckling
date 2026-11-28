#!/bin/sh

cargo fmt -- --config imports_granularity=Module,group_imports=StdExternalCrate
