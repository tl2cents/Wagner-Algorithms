#include "eq_core.h"

// forward decls from eq_alg.cpp
void run_cip_and_expand_seed(int seed, std::vector<std::vector<size_t>>& chains,
                             Layer0_IDX& L0, Layer1_IDX& L1, Layer2_IDX& L2,
                             Layer3_IDX& L3, Layer4_IDX& L4, Layer5_IDX& L5,
                             Layer6_IDX& L6, Layer7_IDX& L7, Layer8_IDX& L8,
                             Layer9_IDX& L9, Layer_IP& IP1, Layer_IP& IP2,
                             Layer_IP& IP3, Layer_IP& IP4, Layer_IP& IP5,
                             Layer_IP& IP6, Layer_IP& IP7, Layer_IP& IP8,
                             Layer_IP& IP9);

void run_pr_and_expand_seed(int seed, std::vector<std::vector<size_t>>& chains,
                            Layer0_IDX& L0, Layer1_IDX& L1, Layer2_IDX& L2,
                            Layer3_IDX& L3, Layer4_IDX& L4, Layer5_IDX& L5,
                            Layer6_IDX& L6, Layer7_IDX& L7, Layer8_IDX& L8,
                            Layer9_IDX& L9, Layer_IP& scratch_ip,
                            const Layer_IP& IP9);

struct IPDiskManifest;
void run_em_and_expand_seed(int seed, const std::string& em_path,
                            std::vector<std::vector<size_t>>& chains,
                            IPDiskManifest& man, Layer0_IDX& L0, Layer1_IDX& L1,
                            Layer2_IDX& L2, Layer3_IDX& L3, Layer4_IDX& L4,
                            Layer5_IDX& L5, Layer6_IDX& L6, Layer7_IDX& L7,
                            Layer8_IDX& L8, Layer9_IDX& L9, Layer_IP& IP1,
                            Layer_IP& IP9, Layer_IP& scratch_ip);

size_t check_with_refilled_L0_from_seed(
    int seed, const std::vector<std::vector<size_t>>& chains, Layer0_IDX& L0);

// globals from eq_alg.cpp
extern SortAlgo g_sort_algo;
extern bool g_verbose;

#include <limits.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// ------------ tiny utils ------------
static int atoi_or(const char* s, int d) {
    if (!s) return d;
    try {
        return std::stoi(std::string(s));
    } catch (...) {
        return d;
    }
}

static std::string self_exe_path() {
    char buf[PATH_MAX];
    ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return std::string("eq");
    buf[n] = 0;
    return std::string(buf);
}

// Spawn isolated child: argv vector must end with nullptr for execve
static int run_isolated_child(const std::vector<std::string>& args) {
    std::vector<char*> cargs;
    cargs.reserve(args.size() + 1);
    for (auto& s : args) cargs.push_back(const_cast<char*>(s.c_str()));
    cargs.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        // child
        std::vector<char*> envp;
        envp.push_back(nullptr);
        execve(args[0].c_str(), cargs.data(), envp.data());
        perror("execve");
        _exit(127);
    }
    // parent
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return 1;
}

// Measure peak RSS (kB)
static long peak_rss_kb() {
    // Use Linux /proc/self/status
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    char line[512];
    long kb = -1;
    while (fgets(line, sizeof(line), f)) {
        if (std::strncmp(line, "VmHWM:", 6) == 0) {  // peak resident set size
            // "VmHWM:\t  163740 kB"
            char* p = std::strrchr(line, '\t');
            if (!p) p = line;
            long v = 0;
            if (sscanf(line + 6, "%ld", &v) == 1) {
                kb = v;
                break;
            }
        }
    }
    fclose(f);
    return kb;
}

static inline double now_s() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch())
        .count();
}

// ------------ per-mode drivers with allocate-on-demand ------------

static int run_mode_cip(int seed, int iters, bool do_check, bool verbose,
                        const std::string& sort_name) {
    g_verbose = verbose;
    g_sort_algo = (sort_name == "std") ? SortAlgo::STD : SortAlgo::KXSORT;

    double t_fwd_exp_sum = 0.0, t_verify_sum = 0.0;
    size_t total_chains = 0, total_trivial = 0, total_valid180 = 0,
           total_valid200 = 0;

    for (int it = 0; it < iters; ++it) {
        int current_seed = seed + it;

        // arenas — only CIP needs IP1..IP8 resident + IP9
        void* raw_main = std::malloc(MAX_MEM_BYTES);
        if (!raw_main) {
            std::cerr << "malloc failed\n";
            return 1;
        }
        uint8_t* base_main = (uint8_t*)raw_main;

        Layer0_IDX L0 = init_Layer<Item0_IDX>(base_main, MAX_MEM_BYTES);
        Layer1_IDX L1 = init_Layer<Item1_IDX>(base_main, MAX_MEM_BYTES);
        Layer2_IDX L2 = init_Layer<Item2_IDX>(base_main, MAX_MEM_BYTES);
        Layer3_IDX L3 = init_Layer<Item3_IDX>(base_main, MAX_MEM_BYTES);
        Layer4_IDX L4 = init_Layer<Item4_IDX>(base_main, MAX_MEM_BYTES);
        Layer5_IDX L5 = init_Layer<Item5_IDX>(base_main, MAX_MEM_BYTES);
        Layer6_IDX L6 = init_Layer<Item6_IDX>(base_main, MAX_MEM_BYTES);
        Layer7_IDX L7 = init_Layer<Item7_IDX>(base_main, MAX_MEM_BYTES);
        Layer8_IDX L8 = init_Layer<Item8_IDX>(base_main, MAX_MEM_BYTES);
        Layer9_IDX L9 = init_Layer<Item9_IDX>(base_main, MAX_MEM_BYTES);

        const size_t IP_ALL_BYTES = 8 * MAX_IP_MEM_BYTES;
        uint8_t* raw_ip_all = (uint8_t*)std::malloc(IP_ALL_BYTES);
        if (!raw_ip_all) {
            std::cerr << "malloc ip arena failed\n";
            std::free(raw_main);
            return 1;
        }
        auto slice = [&](int i) {
            return raw_ip_all + size_t(i) * MAX_IP_MEM_BYTES;
        };
        Layer_IP IP1 = init_slice<Item_IP>(slice(0), MAX_IP_MEM_BYTES);
        Layer_IP IP2 = init_slice<Item_IP>(slice(1), MAX_IP_MEM_BYTES);
        Layer_IP IP3 = init_slice<Item_IP>(slice(2), MAX_IP_MEM_BYTES);
        Layer_IP IP4 = init_slice<Item_IP>(slice(3), MAX_IP_MEM_BYTES);
        Layer_IP IP5 = init_slice<Item_IP>(slice(4), MAX_IP_MEM_BYTES);
        Layer_IP IP6 = init_slice<Item_IP>(slice(5), MAX_IP_MEM_BYTES);
        Layer_IP IP7 = init_slice<Item_IP>(slice(6), MAX_IP_MEM_BYTES);
        Layer_IP IP8 = init_slice<Item_IP>(slice(7), MAX_IP_MEM_BYTES);

        const size_t IP9_CAP = 1024;
        uint8_t* raw_ip9 = (uint8_t*)std::malloc(IP9_CAP * sizeof(Item_IP));
        Layer_IP IP9 = init_slice<Item_IP>(raw_ip9, IP9_CAP * sizeof(Item_IP));

        std::vector<std::vector<size_t>> chains;

        auto t0 = now_s();
        run_cip_and_expand_seed(current_seed, chains, L0, L1, L2, L3, L4, L5,
                                L6, L7, L8, L9, IP1, IP2, IP3, IP4, IP5, IP6,
                                IP7, IP8, IP9);
        auto t1 = now_s();
        t_fwd_exp_sum += (t1 - t0);

        if (do_check) {
            auto t2 = now_s();
            size_t valid200 =
                check_with_refilled_L0_from_seed(current_seed, chains, L0);
            auto t3 = now_s();
            t_verify_sum += (t3 - t2);
            total_valid200 += valid200;
        }

        // optional aggregate stats (if your check prints them already, you can
        // skip)
        total_chains += chains.size();

        std::free(raw_ip9);
        std::free(raw_ip_all);
        std::free(raw_main);
    }

    long peak_kb = peak_rss_kb();
    double avg_fwd = t_fwd_exp_sum / std::max(1, iters);
    double avg_ver = t_verify_sum / std::max(1, iters);

    double sols_per_s =
        (t_fwd_exp_sum > 0.0)
            ? (static_cast<double>(total_valid200) / t_fwd_exp_sum)
            : 0.0;

    std::cout << std::fixed << std::setprecision(2) << "mode=cip sort="
              << (g_sort_algo == SortAlgo::STD ? "std" : "kx")
              << " iters=" << iters << " seed_range=" << seed << "-"
              << (seed + iters - 1) << " forward_expand_avg_s=" << avg_fwd
              << " verify_avg_s=" << avg_ver << " peakRSS_kB=" << peak_kb
              << " total_sols=" << total_valid200 << " Sol/s=" << sols_per_s
              << std::endl;
    return 0;
}

static int run_mode_pr(int seed, int iters, bool do_check, bool verbose,
                       const std::string& sort_name) {
    g_verbose = verbose;
    g_sort_algo = (sort_name == "std") ? SortAlgo::STD : SortAlgo::KXSORT;

    double t_fwd_exp_sum = 0.0, t_verify_sum = 0.0;
    size_t total_valid200 = 0;

    for (int it = 0; it < iters; ++it) {
        int current_seed = seed + it;

        void* raw_main = std::malloc(MAX_MEM_BYTES);
        if (!raw_main) {
            std::cerr << "malloc failed\n";
            return 1;
        }
        uint8_t* base_main = (uint8_t*)raw_main;

        Layer0_IDX L0 = init_Layer<Item0_IDX>(base_main, MAX_MEM_BYTES);
        Layer1_IDX L1 = init_Layer<Item1_IDX>(base_main, MAX_MEM_BYTES);
        Layer2_IDX L2 = init_Layer<Item2_IDX>(base_main, MAX_MEM_BYTES);
        Layer3_IDX L3 = init_Layer<Item3_IDX>(base_main, MAX_MEM_BYTES);
        Layer4_IDX L4 = init_Layer<Item4_IDX>(base_main, MAX_MEM_BYTES);
        Layer5_IDX L5 = init_Layer<Item5_IDX>(base_main, MAX_MEM_BYTES);
        Layer6_IDX L6 = init_Layer<Item6_IDX>(base_main, MAX_MEM_BYTES);
        Layer7_IDX L7 = init_Layer<Item7_IDX>(base_main, MAX_MEM_BYTES);
        Layer8_IDX L8 = init_Layer<Item8_IDX>(base_main, MAX_MEM_BYTES);
        Layer9_IDX L9 = init_Layer<Item9_IDX>(base_main, MAX_MEM_BYTES);

        uint8_t* raw_ip_tmp = (uint8_t*)std::malloc(MAX_IP_MEM_BYTES);
        if (!raw_ip_tmp) {
            std::cerr << "malloc tmp ip failed\n";
            std::free(raw_main);
            return 1;
        }
        Layer_IP scratch_ip = init_slice<Item_IP>(raw_ip_tmp, MAX_IP_MEM_BYTES);

        const size_t IP9_CAP = 1024;
        uint8_t* raw_ip9 = (uint8_t*)std::malloc(IP9_CAP * sizeof(Item_IP));
        Layer_IP IP9 = init_slice<Item_IP>(raw_ip9, IP9_CAP * sizeof(Item_IP));

        std::vector<std::vector<size_t>> chains;

        auto t0 = now_s();
        run_pr_and_expand_seed(current_seed, chains, L0, L1, L2, L3, L4, L5, L6,
                               L7, L8, L9, scratch_ip, IP9);
        auto t1 = now_s();
        t_fwd_exp_sum += (t1 - t0);

        if (do_check) {
            auto t2 = now_s();
            size_t valid200 =
                check_with_refilled_L0_from_seed(current_seed, chains, L0);
            auto t3 = now_s();
            total_valid200 += valid200;
            t_verify_sum += (t3 - t2);
        }

        std::free(raw_ip9);
        std::free(raw_ip_tmp);
        std::free(raw_main);
    }

    long peak_kb = peak_rss_kb();
    double avg_fwd = t_fwd_exp_sum / std::max(1, iters);
    double avg_ver = t_verify_sum / std::max(1, iters);
    double sols_per_s = (do_check && t_fwd_exp_sum > 0.0)
                            ? (total_valid200 / t_fwd_exp_sum)
                            : 0.0;

    std::cout << std::fixed << std::setprecision(2) << "mode=cip-pr sort="
              << (g_sort_algo == SortAlgo::STD ? "std" : "kx")
              << " iters=" << iters << " seed_range=" << seed << "-"
              << (seed + iters - 1) << " forward_expand_avg_s=" << avg_fwd
              << " verify_avg_s=" << avg_ver << " peakRSS_kB=" << peak_kb
              << " total_sols=" << total_valid200 << " Sol/s=" << sols_per_s
              << std::endl;
    return 0;
}

static int run_mode_em(int seed, int iters, bool do_check, bool verbose,
                       const std::string& sort_name,
                       const std::string& em_path) {
    g_verbose = verbose;
    g_sort_algo = (sort_name == "std") ? SortAlgo::STD : SortAlgo::KXSORT;

    double t_fwd_exp_sum = 0.0, t_verify_sum = 0.0;
    size_t total_valid200 = 0;

    for (int it = 0; it < iters; ++it) {
        int current_seed = seed + it;

        void* raw_main = std::malloc(MAX_MEM_BYTES);
        if (!raw_main) {
            std::cerr << "malloc failed\n";
            return 1;
        }
        uint8_t* base_main = (uint8_t*)raw_main;

        Layer0_IDX L0 = init_Layer<Item0_IDX>(base_main, MAX_MEM_BYTES);
        Layer1_IDX L1 = init_Layer<Item1_IDX>(base_main, MAX_MEM_BYTES);
        Layer2_IDX L2 = init_Layer<Item2_IDX>(base_main, MAX_MEM_BYTES);
        Layer3_IDX L3 = init_Layer<Item3_IDX>(base_main, MAX_MEM_BYTES);
        Layer4_IDX L4 = init_Layer<Item4_IDX>(base_main, MAX_MEM_BYTES);
        Layer5_IDX L5 = init_Layer<Item5_IDX>(base_main, MAX_MEM_BYTES);
        Layer6_IDX L6 = init_Layer<Item6_IDX>(base_main, MAX_MEM_BYTES);
        Layer7_IDX L7 = init_Layer<Item7_IDX>(base_main, MAX_MEM_BYTES);
        Layer8_IDX L8 = init_Layer<Item8_IDX>(base_main, MAX_MEM_BYTES);
        Layer9_IDX L9 = init_Layer<Item9_IDX>(base_main, MAX_MEM_BYTES);

        const size_t IP9_CAP = 1024;
        uint8_t* raw_ip9 = (uint8_t*)std::malloc(IP9_CAP * sizeof(Item_IP));
        Layer_IP IP9 = init_slice<Item_IP>(raw_ip9, IP9_CAP * sizeof(Item_IP));

        uint8_t* raw_ip1 = (uint8_t*)std::malloc(MAX_IP_MEM_BYTES);
        uint8_t* raw_ip_s = (uint8_t*)std::malloc(MAX_IP_MEM_BYTES);
        if (!raw_ip1 || !raw_ip_s) {
            std::cerr << "malloc ip failed\n";
            if (raw_ip1) std::free(raw_ip1);
            if (raw_ip_s) std::free(raw_ip_s);
            std::free(raw_ip9);
            std::free(raw_main);
            return 1;
        }
        Layer_IP IP1 = init_slice<Item_IP>(raw_ip1, MAX_IP_MEM_BYTES);
        Layer_IP scratch_ip = init_slice<Item_IP>(raw_ip_s, MAX_IP_MEM_BYTES);

        // forward & expand
        std::vector<std::vector<size_t>> chains;
        IPDiskManifest man{};
        auto t0 = now_s();
        run_em_and_expand_seed(current_seed, em_path, chains, man, L0, L1, L2,
                               L3, L4, L5, L6, L7, L8, L9, IP1, IP9,
                               scratch_ip);
        auto t1 = now_s();
        t_fwd_exp_sum += (t1 - t0);

        if (do_check) {
            auto t2 = now_s();
            size_t valid200 =
                check_with_refilled_L0_from_seed(current_seed, chains, L0);
            auto t3 = now_s();
            total_valid200 += valid200;
            t_verify_sum += (t3 - t2);
        }

        std::free(raw_ip_s);
        std::free(raw_ip1);
        std::free(raw_ip9);
        std::free(raw_main);
    }

    long peak_kb = peak_rss_kb();
    double avg_fwd = t_fwd_exp_sum / std::max(1, iters);
    double avg_ver = t_verify_sum / std::max(1, iters);
    double sols_per_s = (do_check && t_fwd_exp_sum > 0.0)
                            ? (total_valid200 / t_fwd_exp_sum)
                            : 0.0;

    std::cout << std::fixed << std::setprecision(2) << "mode=cip-em sort="
              << (g_sort_algo == SortAlgo::STD ? "std" : "kx")
              << " iters=" << iters << " seed_range=" << seed << "-"
              << (seed + iters - 1) << " forward_expand_avg_s=" << avg_fwd
              << " verify_avg_s=" << avg_ver << " peakRSS_kB=" << peak_kb
              << " total_sols=" << total_valid200 << " Sol/s=" << sols_per_s
              << std::endl;
    return 0;
}

// ------------ test harness (isolation via exec) ------------
static int run_test_harness(int seed, int iters, bool do_check,
                            const std::string& sortopt,
                            const std::string& em_path) {
    const std::string exe = self_exe_path();
    const char* modes[3] = {"cip", "cip-pr", "cip-em"};

    for (int i = 0; i < 3; ++i) {
        std::vector<std::string> args;
        args.push_back(exe);
        args.push_back(std::string("--mode=") + modes[i]);
        args.push_back(std::string("--seed=") + std::to_string(seed));
        args.push_back(std::string("--iters=") + std::to_string(iters));
        args.push_back(std::string("--sort=") + sortopt);
        if (do_check) args.push_back("--check");
        // no --verbose to avoid IO noise
        if (std::string(modes[i]) == "cip-em") {
            args.push_back(std::string("--em=") + em_path);
        }
        int rc = run_isolated_child(args);
        if (rc != 0) return rc;
    }
    return 0;
}

// -------- CLI --------
int main(int argc, char** argv) {
    int seed = 0, iters = 1;
    bool do_check = true, verbose = false, run_test = false;
    std::string mode = "cip", sortopt = "kx", em_path = "ip_cache.bin";

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a.rfind("--seed=", 0) == 0)
            seed = atoi_or(argv[i] + 7, seed);
        else if (a.rfind("--iters=", 0) == 0)
            iters = atoi_or(argv[i] + 8, iters);
        else if (a.rfind("--mode=", 0) == 0)
            mode = a.substr(7);
        else if (a.rfind("--sort=", 0) == 0)
            sortopt = a.substr(7);
        else if (a.rfind("--em=", 0) == 0)
            em_path = a.substr(5);
        else if (a == "--verbose")
            verbose = true;
        else if (a == "--test")
            run_test = true;
        else if (a == "-h" || a == "--help") {
            std::cout << "Usage: " << argv[0]
                      << " [--mode=cip|cip-pr|cip-em] [--seed=N] [--iters=M] "
                         "[--sort=std|kx]"
                      << " [--verbose] [--em=path] [--test]\n";
            return 0;
        }
    }

    if (run_test) {
        return run_test_harness(seed, iters, do_check, sortopt, em_path);
    }

    if (mode == "cip") {
        return run_mode_cip(seed, iters, do_check, verbose, sortopt);
    } else if (mode == "cip-pr") {
        return run_mode_pr(seed, iters, do_check, verbose, sortopt);
    } else if (mode == "cip-em") {
        return run_mode_em(seed, iters, do_check, verbose, sortopt, em_path);
    } else {
        std::cerr << "Unknown mode: " << mode << "\n";
        return 1;
    }
}
