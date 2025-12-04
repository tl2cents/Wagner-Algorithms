import numpy as np
import matplotlib.pyplot as plt
import argparse

# ---------------- Matplotlib Config ----------------
plt.rcParams['font.family'] = 'Times New Roman'
plt.rcParams['font.size'] = 14
plt.rcParams['mathtext.fontset'] = 'cm'
plt.rcParams['axes.unicode_minus'] = False


PARAMS = ["144_5", "200_9"]


def get_GBP_params(PARAM):
    if PARAM == "200_9":
        n = 200
        k = 9
        # see eq1445/README.md for results
        runtime_exp = np.array([0.93, 1.15, 1.45, 1.79, 2.22, 2.70, 3.27, 3.89, 4.55])
        memory_exp  = np.array([123.9, 110.6, 99.8, 86.5, 74.2, 61.8, 60.8, 59.6, 56.9])
        tromp = 145.54  # MB
    elif PARAM == "144_5":
        n = 144
        k = 5
        # see eq1445/README.md for results
        runtime_exp = np.array([8.66, 11.52, 15.48, 20.35, 25.87])
        memory_exp  = np.array([1453.5, 1197.2, 939.5, 713.2, 706.8])
        tromp = 2569.71  # MB
    else:
        raise ValueError("Unknown PARAM")

    ell = n // (k + 1)
    N   = 2 ** (ell + 1)
    return n, k, ell, N, runtime_exp, memory_exp, tromp


# ---------------- Parse Arguments ----------------
parser = argparse.ArgumentParser(description='Plot trade-off curves.')
parser.add_argument('--show-lsr-star', action='store_true', help='Show LSR* curve with log scale')
args = parser.parse_args()

# ---------------- Create Subplots ----------------
# Trade-off of LSR and LSR*
# LSR^\star: T'(n, k, q) = 2^k q^{k/2} k^{k/2 - 1} T(n, k) when memory M' = M/q, q >= 1
# LSR      : T'(n, k, q) = (3 * q^{(k - 1)/2} + k) / (k + 1) T(n, k) when memory M' = M/q, q >= 1


fig, ax = plt.subplots(1, 2, figsize=(12, 5))

for i, PARAM in enumerate(PARAMS):
    n, k, ell, N, runtime_exp, memory_exp, tromp = get_GBP_params(PARAM)
    curr_ax = ax[i]

    # ---------------- LSR and LSR* Curves ----------------
    # q range: [1, 2] corresponding to memory fraction [1, 0.5]
    q_vals = np.linspace(1, 2, 100)
    mem_frac_vals = 1.0 / q_vals

    # LSR* Penalty
    # T'/T = 2^k * q^(k/2) * k^(k/2 - 1)
    lsr_star_penalty = (2**k) * (q_vals**(k/2.0)) * (k**(k/2.0 - 1))

    # LSR Penalty
    # T'/T = (3 * q^((k-1)/2) + k) / (k + 1)
    lsr_penalty = (3 * (q_vals**((k - 1)/2.0)) + k) / (k + 1)

    curr_ax.plot(mem_frac_vals, lsr_penalty, label="LSR", linestyle='-', color='#AE8064', linewidth=2.0)
    
    if args.show_lsr_star:
        curr_ax.plot(mem_frac_vals, lsr_star_penalty, label=r"LSR$^\star$", linestyle='--', color='#2E8B57', linewidth=2.0)

    # ---------------- Our Theory Points ----------------
    h_list = np.arange(k)
    
    # Memory Theory
    base_bits = n * N
    memory_theory_MB = []
    for h in h_list:
        M_h_bits = (k - 1 - h) * 2 * (ell + 1) * N + 2 * ell * N
        peak_bits = max(base_bits, M_h_bits)
        memory_theory_MB.append(peak_bits / (8 * 1024 * 1024))
    memory_theory_MB = np.array(memory_theory_MB)
    
    # Runtime Theory
    runtime_theory = N * (k + h_list * (h_list + 1) / 2)
    
    # Normalize Theory (Baseline h=0)
    mem_theory_frac = memory_theory_MB / memory_theory_MB[0]
    time_theory_penalty = runtime_theory / runtime_theory[0]

    curr_ax.plot(mem_theory_frac, time_theory_penalty, "s--", label="Our Theory", color="#0076AA", linewidth=2.0)

    # ---------------- Our Impl Points ----------------
    # Normalize Impl (Baseline h=0)
    mem_impl_frac = memory_exp / memory_exp[0]
    time_impl_penalty = runtime_exp / runtime_exp[0]

    curr_ax.plot(mem_impl_frac, time_impl_penalty, "o-", label="Our Impl.", color="#EE2A25", linewidth=2.0)

    # ---------------- Annotate h values ----------------
    import matplotlib.patheffects as pe
    
    # Common style for annotations
    anno_opts = dict(fontsize=11, 
                     path_effects=[pe.withStroke(linewidth=2.5, foreground="white", alpha=0.8)])

    # Annotate Our Theory (Lower curve) -> Label Below
    for h_val, x, y in zip(h_list, mem_theory_frac, time_theory_penalty):
        if h_val >= k - 3:
            # For last few points, place label to the right
            curr_ax.annotate(rf"${h_val}$", (x, y), xytext=(6, 0), textcoords='offset points',
                             color="#0076AA", ha='left', va='center', **anno_opts)
        else:
            curr_ax.annotate(rf"${h_val}$", (x, y), xytext=(0, -10), textcoords='offset points',
                             color="#0076AA", ha='center', va='top', **anno_opts)

    # Annotate Our Impl (Upper curve) -> Label Above
    for h_val, x, y in zip(h_list, mem_impl_frac, time_impl_penalty):
        if h_val >= k - 3:
            # For last few points, place label to the right
            curr_ax.annotate(rf"${h_val}$", (x, y), xytext=(6, 0), textcoords='offset points',
                             color="#EE2A25", ha='left', va='center', **anno_opts)
        else:
            curr_ax.annotate(rf"${h_val}$", (x, y), xytext=(0, 8), textcoords='offset points',
                             color="#EE2A25", ha='center', va='bottom', **anno_opts)

    # Explanation for h
    curr_ax.text(0.05, 0.95, "Numbers indicate\nswitching height $h$ \n of our algorithm", 
                 transform=curr_ax.transAxes, 
                 fontsize=12, ha='left', va='top',
                 bbox=dict(boxstyle="round,pad=0.4", fc="white", ec="#AAAAAA", alpha=0.8))

    # ---------------- Formatting ----------------
    curr_ax.set_title(rf"$\mathrm{{GBP}}({n},2^{{{k}}})$ Trade-off")
    curr_ax.set_xlabel(r"Memory Fraction ($q = M_1/M_0$)")
    curr_ax.set_ylabel(r"Time Penalty Factor ($\gamma = T_1/T_0$)")
    
    # Invert x axis to show 1.0 -> 0.5
    curr_ax.set_xlim(1.05, 0.45)
    
    if args.show_lsr_star:
        # Mode 2: Log scale to show LSR*
        curr_ax.set_yscale('log')
        curr_ax.grid(True, which="both", linestyle=':', alpha=0.6)
    else:
        # Mode 1: Linear scale, focus on interesting region
        max_y_interesting = max(
            np.max(time_impl_penalty),
            np.max(time_theory_penalty),
            np.max(lsr_penalty)
        )
        curr_ax.set_ylim(0, max_y_interesting * 1.15)
        curr_ax.grid(True, linestyle=':', alpha=0.6)


# ---------------- Global Legend ----------------
plt.tight_layout()
plt.subplots_adjust(bottom=0.24)

# Get handles/labels from the last axis (they are the same)
handles, labels = ax[0].get_legend_handles_labels()

fig.legend(
    handles=handles,
    labels=labels,
    loc="lower center",
    ncol=4,
    fontsize=14,
    frameon=True,
    framealpha=0.90,
    facecolor="white",
    edgecolor="#444444",
    borderpad=0.5,
    handlelength=2.8,
    handletextpad=0.7,
    labelspacing=0.5,
    bbox_to_anchor=(0.5, 0.025)
)

# ---------------- Save SVG ----------------
plt.savefig("trade-off.svg", format="svg", bbox_inches="tight")

