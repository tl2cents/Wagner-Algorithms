import numpy as np
import matplotlib.pyplot as plt

# ---------------- Matplotlib Config ----------------
plt.rcParams['font.family'] = 'Times New Roman'
plt.rcParams['font.size'] = 14
plt.rcParams['mathtext.fontset'] = 'cm'
plt.rcParams['axes.unicode_minus'] = False


PARAMS = ["200_9", "144_5"]


def get_equihash_params(PARAM):
    if PARAM == "200_9":
        n = 200
        k = 9
        runtime_exp = np.array([1.11, 1.32, 1.64, 2.03, 2.48, 3.07, 3.70, 4.46, 5.24])
        memory_exp  = np.array([127.9, 115.1, 102.5, 90.1, 77.6, 64.9, 62.9, 62.6, 62.6])
        tromp = 144.24  # MB
    elif PARAM == "144_5":
        n = 144
        k = 5
        runtime_exp = np.array([9.96, 11.90, 16.09, 21.30, 27.36])
        memory_exp  = np.array([1464.2, 1204.8, 946.1, 718.9, 718.0])
        tromp = 2.6 * 1024  # MB
    else:
        raise ValueError("Unknown PARAM")

    ell = n // (k + 1)
    N   = 2 ** (ell + 1)
    return n, k, ell, N, runtime_exp, memory_exp, tromp


# ---------------- Create Subplots ----------------
fig, ax = plt.subplots(2, 2, figsize=(10.5, 7.2))

# Pre-define sota handles for legend (will be overwritten each loop)
sota_theory_line = None
sota_impl_line = None

for row, PARAM in enumerate(PARAMS):
    n, k, ell, N, runtime_exp, memory_exp, tromp = get_equihash_params(PARAM)

    h_list = np.arange(k)
    runtime_theory = N * (k + h_list * (h_list + 1) / 2)

    # ---------------- Runtime Penalty ----------------
    T0_exp = runtime_exp[0]
    T0_theory = runtime_theory[0]
    runtime_exp_penalty = runtime_exp / T0_exp
    runtime_theory_penalty = runtime_theory / T0_theory

    ax_time = ax[row, 0]
    ax_time.plot(h_list, runtime_exp_penalty, "o-", label="Our impl",
                 color="#EE2A25", linewidth=2.0)
    ax_time.plot(h_list, runtime_theory_penalty, "s--", label="Theory",
                 color="#0076AA", linewidth=2.0)

    ax_time.set_title(rf"$\mathrm{{Equihash}}({n},{k})$ Time Penalty")
    ax_time.set_xlabel("h")
    ax_time.set_ylabel(r"Penalty Factor $\gamma$ of $\gamma T_{0}$")
    ax_time.grid(False)
    ax_time.margins(x=0.05)

    ymax0 = max(runtime_exp_penalty.max(), runtime_theory_penalty.max())
    ax_time.set_ylim(0.1 * ymax0, ymax0 * 1.05)

    # ---------------- Memory Theory ----------------
    base_bits = n * N
    memory_theory_MB = []
    for h in h_list:
        M_h_bits = (k - 1 - h) * 2 * (ell + 1) * N + 2 * ell * N
        peak_bits = max(base_bits, M_h_bits)
        memory_theory_MB.append(peak_bits / (8 * 1024 * 1024))
    memory_theory_MB = np.array(memory_theory_MB)

    best_bits = 2 * n * N
    best_MB = best_bits / (8 * 1024 * 1024)
    tromp_MB = tromp

    # ---------------- Peak Memory Plot ----------------
    ax_mem = ax[row, 1]
    ax_mem.plot(h_list, memory_exp, "o-", label="Our impl",
                color="#EE2A25", linewidth=2.0)
    ax_mem.plot(h_list, memory_theory_MB, "s--", label="Theory",
                color="#0076AA", linewidth=2.0)

    # -------- SoTA (theory) 2nN line (segment aligned to points) --------
    x_min = h_list.min() if h_list.size > 0 else 0
    x_max = h_list.max() if h_list.size > 0 else 0
    sota_theory_line = ax_mem.plot(
        [x_min, x_max],
        [best_MB, best_MB],
        color="#AE8064",
        linestyle='--',
        linewidth=1.5,
        label="SoTA (theory)"
    )[0]
    x_center = (x_min + x_max) / 2.0

    # -------- SoTA (impl) Tromp (segment aligned to points) --------
    sota_impl_line = ax_mem.plot(
        [x_min, x_max],
        [tromp_MB, tromp_MB],
        color="#2E8B57",
        linestyle='--',
        linewidth=1.5,
        label="SoTA (impl)"
    )[0]

    # -------- Label for SoTA (impl) in MB (always MB) --------
    label_text = f"y = {tromp_MB:.1f} MB"

    ax_mem.set_title(rf"$\mathrm{{Equihash}}({n},{k})$ Peak Memory")
    ax_mem.set_xlabel("h")
    ax_mem.set_ylabel("PeakUSS (MB)")
    ax_mem.grid(False)
    ax_mem.margins(x=0.05)

    ymax1 = max(memory_exp.max(), memory_theory_MB.max(), best_MB, tromp_MB)
    ax_mem.set_ylim(-0.05 * ymax1, ymax1 * 1.05)

    # -------- Place labels after y-limits computed to avoid going outside axes --------
    # show theory with explicit value in MB
    label_text_theory = f"y = 2nN ({best_MB:.1f} MB)"
    y_theory_text = min(best_MB * 1.05, ymax1 * 0.98)
    # use same font size/style as impl label
    common_fs = 12
    common_fontstyle = "normal"
    common_fontweight = "normal"
    ax_mem.text(
        x_center,
        y_theory_text,
        label_text_theory,
        color="#AE8064",
        fontsize=common_fs,
        fontstyle=common_fontstyle,
        fontweight=common_fontweight,
        ha="center",
        va="bottom",
        clip_on=True
    )

    # place impl label below the line but stay within axis lower bound
    y_min_axis = -0.05 * ymax1
    margin = 0.02 * ymax1
    y_impl_text = max(tromp_MB * 0.97, y_min_axis + margin)
    ax_mem.text(
        x_center,
        y_impl_text,
        label_text,
        color="#2E8B57",
        fontsize=common_fs,
        fontstyle=common_fontstyle,
        fontweight=common_fontweight,
        ha="center",
        va="top",
        clip_on=True
    )


# ---------------- Global Legend ----------------
plt.tight_layout()
plt.subplots_adjust(bottom=0.18)

lines = [
    ax[0, 0].lines[0],     # Our impl
    ax[0, 0].lines[1],     # Theory
    sota_impl_line,        # SoTA (impl)
    sota_theory_line,      # SoTA (theory)
]

labels = ["Our impl", "Theory", "State-of-the-Art (impl)", "State-of-the-Art (theory)"]

fig.legend(
    handles=lines,
    labels=labels,
    loc="lower center",
    ncol=2,
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

# ---------------- Save PDF ----------------
# plt.savefig("apr-time-mem.pdf", format="pdf", bbox_inches="tight")
plt.savefig("apr-time-mem.svg", format="svg", bbox_inches="tight")

