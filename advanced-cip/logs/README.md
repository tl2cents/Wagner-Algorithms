
## How to reproduce these results locally

> Merge Benchmark:
```
nohup ./run_merge.sh 0 1000 kx > logs/merge_$(date +%Y%m%d_%H%M%S).log 2>&1 &
```

> CIP-APR Benchmark:
```
nohup ./run_apr.sh --nk 144_5 --seed 0 --iters 100 > logs/apr_eq1445_$(date +%Y%m%d_%H%M%S).log 2>&1 &
nohup ./run_apr.sh --nk 200_9 --seed 0 --iters 1000 > logs/apr_eq2009_$(date +%Y%m%d_%H%M%S).log 2>&1 &
```


> Compare our CIP-PR and Tromp's:
```
nohup ./run_bench.sh --variant 144_5 --seed 0 --iters 100 > logs/bench_eq1445_$(date +%Y%m%d_%H%M%S).log 2>&1 &
nohup ./run_bench.sh --variant 144_5 --seed 0 --iters 2 > logs/bench_eq2009_$(date +%Y%m%d_%H%M%S).log 2>&1 &
```