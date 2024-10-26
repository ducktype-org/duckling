#!/bin/bash

. config.sh

echo "First check that:"
echo

echo "python3 --version gives 3.11.3"
echo "result:"
$python_bin --version
echo

echo "java --version gives 20.0.1"
echo "javac --version gives 20.0.1"
echo "result:"
$java_bin --version
$javac_bin --version
echo

echo "node --version gives v20.2.0"
echo "result:"
$node_bin --version
echo

echo "g++ --version gives v11.3"
echo "result:"
$cpp_bin --version
echo

echo "RiftVm gives correct config"
echo "result:"
$rift_vm_bin --version
echo

echo "RiftVm with debug gives correct config"
echo "result:"
$rift_vm_bin_debug --version
echo

echo
