# New delete allocator, 17 shards

```bash
$  ./bin/concurrent_example 1 ThreadUnsafeDuckMap 1  
0.672247 0.725426 0.694899 0.707515 0.700763 ^C

$  ./bin/concurrent_example 1 ThreadUnsafeMap 1 
1.00017 0.883737 0.924795 0.97552 0.888017 0.875604 ^C

$  ./bin/concurrent_example 1 ThreadUnsafeDuckMap 1 
0.709141 0.663317 0.65509 0.688001 0.696484 0.662352 0.65272 0.659985 0.660854 0.679377 ^C





$  ./bin/concurrent_example 1 DuckStdMap 1  
1.08044 1.05674 1.05626 1.05672 ^C

$  ./bin/concurrent_example 1 DuckMap 1 
0.70681 0.689231 0.688789 0.688596 0.687812 0.686693 0.690931 ^C

$  ./bin/concurrent_example 1 KVIntelTBB 1 
1.03116 1.05192 1.0704 1.05403 1.05695 ^C

$  ./bin/concurrent_example 1 KVBigLock 1 
1.06305 1.03354 1.28426 1.19978 ^C

$  ./bin/concurrent_example 1 KVSharedLock 1 
1.13587 1.13816 1.1194 1.11811 1.1138 ^C





$  ./bin/concurrent_example 8 DuckMap 1  
1.21571 1.20151 1.19676 1.21581 1.18627 1.19401 1.20803 1.25079 1.22667 1.27266 1.24835 1.24976 1.25535 1.23497 1.21802 1.42314 
$  # cpu utilizatoin ~40%  2 


$  ./bin/concurrent_example 8 KVIntelTBB 1 
1.49348 1.52194 1.51098 1.51593 1.50204 1.50045 1.50175 1.50223 1.50224 1.50489 1.50712 1.54083 1.49371 1.49217 1.49097 1.48853 
$  # cpu utilizatoin ~50%  2 

$  ./bin/concurrent_example 8 DuckStdMap 1 
1.89177 1.95111 1.94428 1.89652 1.89316 1.88515 1.91581 1.95348 ^C
$  # cpu utilizatoin ~40-50% 1 

$  ./bin/concurrent_example 8 KVSharded 17  
terminate called after throwing an instance of 'std::runtime_error'
  what():  num_shard must be a power of two
[1] 14922 IOT instruction (core dumped)  ./bin/concurrent_example 8 KVSharded 17
$  ./bin/concurrent_example 8 KVSharded 16 IOT х  
2.74016 2.71304 2.74352 ^C



$  ./bin/concurrent_example 16 DuckMap 1 
2.09975 2.10223 2.09863 2.10023 2.05843 2.05612 ^C

$  ./bin/concurrent_example 16 KVIntelTBB 1  1 
2.21428 2.14755 2.14821 2.15521 2.2116 2.22445 2.16251 2.18333 2.24082 ^C

```