## 2025 Computer Organization Final Project 
### Scoring Criteria
- [x] (Q1) GEM5 + NVMAIN BUILD-UP (40%)

- [x] (Q2) Enable L3 last level cache in GEM5 + NVMAIN (15%)
（看到 log 裡面有 L3 cache 的資訊）

- [x] (Q3) Config last level cache to  2-way and full-way associative cache and test performance (15%)
必須跑 benchmark quicksort 在 2-way 跟 full way（直接在 L3 cache implement，可以用 miss rate 判斷是否成功）
 
- [x] (Q4) Modify last level cache policy based on frequency based replacement policy (15%)
 
- [x] (Q5) Test the performance of write back and write through policy based on 4-way associative cache with isscc_pcm(15%) 
必須跑 benchmark multiply 在 write through 跟 write back ( gem5 default 使用 write back，可以用 write request 的數量判斷 write through 是否成功 )
 
- [x] Bonus (10%) Design last level cache policy to reduce the energy consumption of pcm_based main memory
 Baseline:LRU