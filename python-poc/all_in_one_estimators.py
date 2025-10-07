from wagner_algorithmic_estimator import Wagner_Algorithmic_Framework
from prettytable import PrettyTable
from rich.console import Console
from rich.table import Table
from rich.text import Text
from math import ceil, log, floor, sqrt

def to_log2_complexity(complexity):
    """
    Convert the complexity to log2 scale.
    """
    if type(complexity) == list or type(complexity) == tuple:
        return [log(c, 2) for c in complexity]
    return log(complexity, 2)

def to_MB(mem_complexity):
    """
    Convert the memory complexity (bits) to MB.
    """
    return mem_complexity / (8 * 1024 * 1024)

def to_GB(mem_complexity):
    """
    Convert the memory complexity (bits) to GB.
    """
    return mem_complexity / (8 * 1024 * 1024 * 1024)

def to_TB(mem_complexity):
    """
    Convert the memory complexity (bits) to TB.
    """
    return mem_complexity / (8 * 1024 * 1024 * 1024 * 1024)

def single_list_k_upper_bound(n):
    return sqrt(n/2 + 1)

def revisiting_equihash_parameters():
    # the commented parameters do not satisfy k<= sqrt{n/2 + 1}, for which single-chain algorithm succeeds with small prob.
    Equihash_Parameter_Set = [
        (96, 5),
        (128, 7),
        (160, 9),
        # (176, 10),
        # (192, 11),
        (96, 3),
        (144, 5),
        (150, 5),
        (192, 7),
        (240, 9),
        (96, 2),
        (288, 8),
        (200, 9),
    ]
    headers = ["(n,k)", "CIV", "CIV-IT", "CIP", "CIP-PR", "TIV", "TIV-IT", "TIP"]
    pt = PrettyTable(headers)
    pt.title = "Improved Memory Complexity for GBP(n,2^k)"
    print("The paper uses the limited xor-removal case")
    for n, k in Equihash_Parameter_Set:
        waf = Wagner_Algorithmic_Framework(n, k)
        civ_mem, civ_time = to_log2_complexity(waf.single_list_iv_estimator())
        civ_star_mem, civ_star_time = to_log2_complexity(waf.single_list_iv_it_estimator())
        cip_mem, cip_time = to_log2_complexity(waf.single_list_ip_estimator(trade_type="plain"))
        cip_star_mem, cip_star_time = to_log2_complexity(waf.single_list_ip_estimator(trade_type="post_retrieval"))
        tiv_mem, tiv_time = to_log2_complexity(waf.k_tree_iv_estimator())
        tiv_star_mem, tiv_star_time = to_log2_complexity(waf.k_tree_iv_it_estimator())
        tip_mem, tip_time = to_log2_complexity(waf.k_tree_ip_estimator())
        pt.add_row([str((n,k)), f"2^%.1f"%civ_mem, f"2^%.1f"%civ_star_mem, f"2^%.1f"%cip_mem, f"2^%.1f"%cip_star_mem,
                    f"2^%.1f"%tiv_mem, f"2^%.1f"%tiv_star_mem, f"2^%.1f"%tip_mem])
    print(pt)
    # multi row table
    table = Table(title="Improved Trade-off for GBP(n,2^k)", 
                  padding=(0, 0), 
                  header_style="bold red",
                  title_style="bold white underline on blue"
                  )
    for header in headers:
        table.add_column(header, justify="center")
    
    st0 = Table(padding=(0, 0),
                show_header=False,
                show_edge=False,
                show_lines=True) 
    st0.add_column()
    st0.add_row()
    for n, k in Equihash_Parameter_Set:
        st0.add_row(str((n, k)))
    sub_tables = []
    for i in range(len(headers) - 1):
        st =  Table(padding=(0, 0),
                    show_edge=False,
                    show_lines=True,
                    header_style="bold blue")
        st.add_column("Mem", justify="center")
        st.add_column("Time", justify="center")
        sub_tables.append(st)
        
    for n, k in Equihash_Parameter_Set:
        waf = Wagner_Algorithmic_Framework(n, k)
        civ_mem, T0 = waf.single_list_iv_estimator()
        civ_mem = to_log2_complexity(civ_mem)
        sub_tables[0].add_row(f"2^%.2f"%civ_mem, f"T0(2^%.2f)"%to_log2_complexity(T0))
        civ_star_mem, civ_star_time = waf.single_list_iv_it_estimator()
        civ_star_mem = to_log2_complexity(civ_star_mem)
        sub_tables[1].add_row(f"2^%.2f"%civ_star_mem, f"%.2f * T0"%(civ_star_time/T0))
        cip_mem, cip_time = waf.single_list_ip_estimator(trade_type="plain")
        cip_mem = to_log2_complexity(cip_mem)
        assert cip_time == T0
        sub_tables[2].add_row(f"2^%.2f"%cip_mem, "T0")
        cip_star_mem, cip_star_time = waf.single_list_ip_estimator(trade_type="post_retrieval")
        cip_star_mem = to_log2_complexity(cip_star_mem)
        sub_tables[3].add_row(f"2^%.2f"%cip_star_mem, f"%.2f * T0" % (cip_star_time/T0))
        tiv_mem, T1 = waf.k_tree_iv_estimator()
        tiv_mem = to_log2_complexity(tiv_mem)
        sub_tables[4].add_row(f"2^%.2f"%tiv_mem, f"T1(2^%.2f)"%to_log2_complexity(T1))
        tiv_star_mem, tiv_star_time = waf.k_tree_iv_it_estimator()
        tiv_star_mem = to_log2_complexity(tiv_star_mem)
        sub_tables[5].add_row(f"2^%.2f"%tiv_star_mem, f"%.2f * T0" % (tiv_star_time/T1))
        tip_mem, tip_time = waf.k_tree_ip_estimator()
        tip_mem = to_log2_complexity(tip_mem)
        assert tip_time == T1
        sub_tables[6].add_row(f"2^%.2f"%tip_mem, "T1")
        # print latex tables for paper.
        # print(f"({n}, $2^{{{k}}}$) &  $2^{{{cip_mem:.2f}}}$ & $T_0=2^{{{to_log2_complexity(T0):.2f}}}$ & $2^{{{civ_star_mem:.2f}}}$ & ${civ_star_time/T0:.2f} \cdot T_0$ & $2^{{{cip_star_mem:.2f}}}$ & ${cip_star_time/T0:.2f} \cdot T_0$ & $2^{{{tiv_mem:.2f}}}$ & $T_1=2^{{{to_log2_complexity(T1):.2f}}}$ & $2^{{{tiv_star_mem:.2f}}}$ & ${tiv_star_time/T1:.2f} \cdot T_1$ \\\\")
        print(f"({n}, $2^{{{k}}}$) &  $2^{{{cip_mem:.2f}}}$ & $T_0$ & $2^{{{civ_star_mem:.2f}}}$ & ${civ_star_time/T0:.1f} \cdot T_0$ & $2^{{{cip_star_mem:.2f}}}$ & ${cip_star_time/T0:.1f} \cdot T_0$ & $2^{{{tiv_mem:.2f}}}$ & $T_1$ & $2^{{{tiv_star_mem:.2f}}}$ & ${tiv_star_time/T1:.1f} \cdot T_1$ \\\\")
        
    sub_tables = [st0] + sub_tables
    table.add_row(*sub_tables)
    console = Console()
    console.print(table)
    console.print(Text("Remark 1: If CIP-PR uses external memory, all time complexity will be upper bounded by c*T0 where c in [1, 2).", style="italic bold"))
    console.print(Text("Remark 2: Algorithms with * are new results in this work.", style="italic bold"))

def concrete_civ_trade_offs():
    # the commented parameters do not satisfy k<= sqrt{n/2 + 1} and the single-list algorithm succeeds with small prob.
    Equihash_Parameter_Set = [
        (96, 5),
        (128, 7),
        (160, 9),
        # (176, 10),
        # (192, 11),
        (96, 3),
        (144, 5),
        (150, 5),
        (192, 7),
        (240, 9),
        (96, 2),
        (288, 8),
        (200, 9),
    ]
    headers = ["(n,k)", "trimmed_length", "peak_mem", "runtime", 
               "switching_height1", "peak_mem1", "runtime1", "peak_layer1",
               "switching_height2", "peak_mem2", "runtime2", "peak_layer2", "activating_height",
               ]
    results = []
    for n, k in Equihash_Parameter_Set:
        waf = Wagner_Algorithmic_Framework(n, k)
        results.append(waf.single_list_iv_it_estimator(True))
    # multi row table
    table = Table(title="Concrete Index Vector Trade-off for GBP(n,2^k)", 
                  padding=(0, 0), 
                  header_style="bold red",
                  title_style="bold white underline on blue"
                  )
    for header in headers:
        table.add_column(header, justify="center")
        
    
    for (n, k), result in zip(Equihash_Parameter_Set, results):
        waf = Wagner_Algorithmic_Framework(n, k)
        civ_mem, T0 = waf.single_list_iv_estimator()
        row = []
        row.append(f"{result['(n,k)']}")
        row.append(f"{result['trimmed_length']}")
        row.append(f"2^{to_log2_complexity(result['peak_mem']):.2f}")
        row.append(f"{result['runtime']/T0:.2f} * T0")
        
        row.append(f"{result['switching_height1']}")
        row.append(f"2^{to_log2_complexity(result['peak_mem1']):.2f}")
        row.append(f"{result['runtime1']/T0:.2f} * T0")
        row.append(f"{result['peak_layer1']}")
        
        
        row.append(f"{result['switching_height2']}")
        row.append(f"2^{to_log2_complexity(result['peak_mem2']):.2f}")
        row.append(f"{result['runtime2']/T0:.2f} * T0")
        row.append(f"{result['peak_layer2']}")
        row.append(f"{result['activating_height']}")
        table.add_row(*row)
        print(f"({n}, $2^{{{k}}}$) & {result['trimmed_length']} & $2^{{{to_log2_complexity(result['peak_mem']):.2f}}}$ & ${result['runtime']/T0:.2f} \cdot T_0$ & {result['switching_height1']} & $2^{{{to_log2_complexity(result['peak_mem1']):.2f}}}$ & ${result['runtime1']/T0:.2f} \cdot T_0$ & {result['peak_layer1']} & {result['switching_height2']} & $2^{{{to_log2_complexity(result['peak_mem2']):.2f}}}$ & ${result['runtime2']/T0:.2f} \cdot T_0$ & {result['peak_layer2']} & {result['activating_height']}  \\\\")
        
    console = Console()
    console.print(table)
    
def concrete_cip_trade_offs():
    # the commented parameters do not satisfy k<= sqrt{n/2 + 1} and the single-list algorithm succeeds with small prob.
    Equihash_Parameter_Set = [
        (96, 5),
        (128, 7),
        (160, 9),
        # (176, 10),
        # (192, 11),
        (96, 3),
        (144, 5),
        (150, 5),
        (192, 7),
        (240, 9),
        (96, 2),
        (288, 8),
        (200, 9),
    ]
    headers = ["(n,k)", "peak_mem", "plain_peak_mem","runtime", "switching_height"]
    results = []
    for n, k in Equihash_Parameter_Set:
        waf = Wagner_Algorithmic_Framework(n, k)
        results.append(waf.search_best_ip_with_post_retrieval(True))
    # multi row table
    headers 
    table = Table(title="Concrete Index Pointer Trade-off for GBP(n,2^k)", 
                  padding=(0, 0), 
                  header_style="bold red",
                  title_style="bold white underline on blue"
                  )
    for header in headers:
        table.add_column(header, justify="center")
    
    for (n, k), result in zip(Equihash_Parameter_Set, results):
        waf = Wagner_Algorithmic_Framework(n, k)
        cip_mem, T0 = waf.single_list_ip_estimator()
        row = []
        row.append(f"{result['(n,k)']}")
        row.append(f"2^{to_log2_complexity(result['peak_mem']):.2f}")
        row.append(f"2^{to_log2_complexity(cip_mem):.2f}")
        row.append(f"{result['runtime']/T0:.2f} * T0")
        row.append(f"{result['switching_height']}")
        table.add_row(*row)
        print(f"({n}, $2^{{{k}}}$) $2^{{{to_log2_complexity(result['peak_mem']):.2f}}}$ & $2^{{{to_log2_complexity(cip_mem):.2f}}}$ & ${result['runtime']/T0:.2f} \cdot T_0$ & {result['switching_height']}\\\\")
    console = Console()
    console.print(table)
    
def hybrid_single_chain_all_paras():
    # the commented parameters do not satisfy k<= sqrt{n/2 + 1} and the single-list algorithm succeeds with small prob.
    Equihash_Parameter_Set = [
        (96, 5),       # h1 = 1, h2 = 3, peak: 23.04, peak_layer = 4, extra runtime 2.80 * T0
        (128, 7),      # h1 = 1, h2 = 4, peak: 23.64, peak_layer = 6, extra runtime 2.86 * T0
        (160, 9),      # h1 = 1, h2 = 5, peak: 24.07, peak_layer = 8, extra runtime 3.00 * T0
        # (176, 10), 
        # (192, 11),
        (96, 3),       # h1 = 1, h2 = 2, peak: 30.64, peak_layer = 1, extra runtime 3.00 * T0
        (144, 5),      # h1 = 1, h2 = 3, peak: 31.61, peak_layer = 4, extra runtime 2.80 * T0
        (150, 5),      # h1 = 1, h2 = 3, peak: 32.67, peak_layer = 4, extra runtime 2.80 * T0
        (192, 7),      # h1 = 1, h2 = 4, peak: 32.21, peak_layer = 6, extra runtime 2.86 * T0
        (240, 9),      # h1 = 1, h2 = 5, peak: 32.63, peak_layer = 8, extra runtime 3.00 * T0
        (96, 2),       # h1 = 0, h2 = 1, peak: 39.00, peak_layer = 1, extra runtime 0.50 * T0
        (288, 8),      # h1 = 1, h2 = 4, peak: 41.03, peak_layer = 7, extra runtime 2.50 * T0
        (200, 9),      # h1 = 1, h2 = 5, peak: 28.38, peak_layer = 8, extra runtime 3.00 * T0
    ]
    for n, k in Equihash_Parameter_Set:
        waf = Wagner_Algorithmic_Framework(n, k)
        waf.search_best_hybrid_single_chian(verbose=True)
    
        
if __name__ == "__main__":
    # concrete_civ_trade_offs()
    # concrete_cip_trade_offs()
    hybrid_single_chain_all_paras()
    # revisiting_equihash_parameters()