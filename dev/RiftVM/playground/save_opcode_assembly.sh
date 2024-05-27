#!/bin/bash

binary_path="$1"
result_file="results.asm"
functions=( "vm::OpFuns::op_nop"            "vm::OpFuns::op_jmpRel_label" \
            "vm::OpFuns::op_mov_l64_imm"    "vm::OpFuns::op_add_l64_l64" \
            "vm::OpFuns::op_sub_l64_l64"    "vm::OpFuns::op_mul_l64_imm" \
            "vm::OpFuns::op_cmpG_l64_l64"   "vm::OpFuns::op_call_func" \
            "vm::OpFuns::op_ret_imm"        "vm::OpFuns::op_free_lptr" \
            "vm::OpFuns::op_setFstArg_l64")

echo "Disassembling functions and saving results to results.txt..."
echo "" > $result_file

for function in "${functions[@]}"; do
    echo $'\n \nFunction: ' >> $result_file
    echo $function >> $result_file
    echo "" >> $result_file
    gdb -batch -ex "file $binary_path" -ex 'set disassembly-flavor intel' -ex "disassemble $function" >> $result_file
done

echo "Done."

cat $result_file

