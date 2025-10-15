
## Proof-of-Concept for In-Place Merge

To reproduce the results in Table 5 from the paper, run:

```bash
./inplace-merge-benchmark.sh
```

Below is a sample output:

``` text
============== Formatted Summary ==============
Sort     | Metric       | Min        | Max        | Avg       
-------------------------------------------------------------
kx       | sort_time    | 0.055349   | 0.067512   | 0.058323  
kx       | merge_time   | 0.031130   | 0.036092   | 0.032903  
kx       | max_group    | 55.000000  | 4652.000000 | 1507.610000
kx       | mem_peak_kb  | 54272.000000 | 54784.000000 | 54544.640000
std      | sort_time    | 0.120916   | 0.132953   | 0.126595  
std      | merge_time   | 0.031353   | 0.037775   | 0.032908  
std      | max_group    | 55.000000  | 4652.000000 | 1507.610000
std      | mem_peak_kb  | 54272.000000 | 54784.000000 | 54529.280000
-------------------------------------------------------------
```



We also test more sorting algorithms for in-place merge in python. To test the in-place merge algorithms with multiple sorting algorithms, run:

```bash
python merge_in_place.py --n 200 --k 9 --seed deadbeef --algo rsort
```

with choices of `--algo` in `['qsort', 'hsort', 'csort', 'rsort', 'tsort']`. There are some insightful comments in [merge_in_place.py](./merge_in_place.py).

