# run 

mkdir -p duck_build
rm -r duck_build

./bin/duckc  experimental_compile_package_dump_graph $@ -n test --graph-output go --no-std --simplify-pass --remove-dead-nodes --no-other-input


python3 ../scripts/query_graph/query_graph_to_dot.py go/query_graph_post_opt.json --render svg --no-hash


python3 ../scripts/query_graph/query_graph_to_dot.py go/query_graph_pre_opt.json --render svg   --no-hash


du -hs go/query_graph_pre_opt.bin
du -bhs go/query_graph_post_opt.bin
