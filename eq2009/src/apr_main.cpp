#include "advanced_pr.h"
#include "zcash_blake.h"
#include <limits.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// Forward declarations from apr_alg.cpp
std::vector<Solution> plain_cip(int seed, uint8_t *base = nullptr);
std::vector<Solution> plain_cip_pr(int seed, uint8_t *base = nullptr);
std::vector<Solution> advanced_cip_pr(int seed, uint8_t *base = nullptr);
std::vector<Solution> cip_em(int seed, const std::string &em_path, uint8_t *base = nullptr);
std::vector<Solution> cip_em_extra_ip_cache(int seed, const std::string &em_path, uint8_t *base = nullptr);

// Globals
extern SortAlgo g_sort_algo;
extern bool g_verbose;
extern uint64_t MAX_ITEM_MEM_BYTES;
extern uint64_t MAX_IP_MEM_BYTES;
extern uint64_t MAX_MEM_BYTES_APR5;

const inline uint64_t MAX_CIP_BYTES = MAX_ITEM_MEM_BYTES + MAX_IP_MEM_BYTES * 8;
const inline uint64_t MAX_CIP_PR_BYTES = MAX_ITEM_MEM_BYTES;
const inline uint64_t MAX_CIP_APR_BYTES = MAX_MEM_BYTES_APR5;
const inline uint64_t MAX_CIP_EM_BYTES = MAX_ITEM_MEM_BYTES;
const inline uint64_t MAX_CIP_EMC_BYTES = MAX_ITEM_MEM_BYTES + MAX_IP_MEM_BYTES;

// ------------ tiny utils ------------
static int atoi_or(const char *s, int d)
{
    if (!s)
        return d;
    try
    {
        return std::stoi(std::string(s));
    }
    catch (...)
    {
        return d;
    }
}

static std::string self_exe_path()
{
    char buf[PATH_MAX];
    ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0)
        return std::string("apr");
    buf[n] = 0;
    return std::string(buf);
}

// Spawn isolated child: argv vector must end with nullptr for execve
static int run_isolated_child(const std::vector<std::string> &args)
{
    std::vector<char *> cargs;
    cargs.reserve(args.size() + 1);
    for (auto &s : args)
        cargs.push_back(const_cast<char *>(s.c_str()));
    cargs.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return 1;
    }
    if (pid == 0)
    {
        // child
        std::vector<char *> envp;
        envp.push_back(nullptr);
        execve(args[0].c_str(), cargs.data(), envp.data());
        perror("execve");
        _exit(127);
    }
    // parent
    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
    {
        perror("waitpid");
        return 1;
    }
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    return 1;
}

static inline double now_s()
{
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch())
        .count();
}

// ------------ per-mode drivers ------------

static int run_mode_cip(int seed, int iters, bool do_check, bool verbose,
                        const std::string &sort_name)
{
    g_verbose = verbose;
    g_sort_algo = (sort_name == "std") ? SortAlgo::STD : SortAlgo::KXSORT;
    uint8_t *base = static_cast<uint8_t *>(std::malloc(MAX_CIP_BYTES));
    if (base == nullptr)
    {
        std::cerr << "Failed to allocate " << (MAX_CIP_BYTES / (1024 * 1024)) << " MB of memory" << std::endl;
        return 1;
    }
    // Touch the allocation to ensure pages are committed (avoids lazy-fault surprises).
    std::memset(base, 0, MAX_CIP_BYTES);
    double t_fwd_exp_sum = 0.0, t_verify_sum = 0.0;
    size_t total_sols = 0;

    for (int it = 0; it < iters; ++it)
    {
        int current_seed = seed + it;

        auto t0 = now_s();
        std::vector<Solution> solutions = plain_cip(current_seed, base);
        auto t1 = now_s();
        t_fwd_exp_sum += (t1 - t0);

        if (do_check)
        {
            auto t2 = now_s();
            check_zero_xor(current_seed, solutions);
            auto t3 = now_s();
            t_verify_sum += (t3 - t2);
        }

        total_sols += solutions.size();
    }

    long peak_kb = peak_rss_kb();
    double avg_fwd = t_fwd_exp_sum / std::max(1, iters);
    double avg_ver = t_verify_sum / std::max(1, iters);

    double sols_per_s = (t_fwd_exp_sum > 0.0)
                            ? (static_cast<double>(total_sols) / t_fwd_exp_sum)
                            : 0.0;

    std::cout << std::fixed << std::setprecision(2) << "mode=cip sort="
              << (g_sort_algo == SortAlgo::STD ? "std" : "kx")
              << " iters=" << iters << " seed_range=" << seed << "-"
              << (seed + iters - 1) << " single_run_time=" << avg_fwd
              << " peakRSS_kB=" << peak_kb
              << " total_sols=" << total_sols << " Sol/s=" << sols_per_s
              << std::endl;
    std::free(base);
    return 0;
}

static int run_mode_pr(int seed, int iters, bool do_check, bool verbose,
                       const std::string &sort_name)
{
    g_verbose = verbose;
    g_sort_algo = (sort_name == "std") ? SortAlgo::STD : SortAlgo::KXSORT;

    uint8_t *base = static_cast<uint8_t *>(std::malloc(MAX_CIP_PR_BYTES));
    if (base == nullptr)
    {
        std::cerr << "Failed to allocate " << (MAX_CIP_PR_BYTES / (1024 * 1024)) << " MB of memory" << std::endl;
        return 1;
    }
    // Touch the allocation to ensure pages are committed (avoids lazy-fault surprises).
    std::memset(base, 0, MAX_CIP_PR_BYTES);

    double t_fwd_exp_sum = 0.0, t_verify_sum = 0.0;
    size_t total_sols = 0;

    for (int it = 0; it < iters; ++it)
    {
        int current_seed = seed + it;

        auto t0 = now_s();
        std::vector<Solution> solutions = plain_cip_pr(current_seed, base);
        auto t1 = now_s();
        t_fwd_exp_sum += (t1 - t0);

        if (do_check)
        {
            auto t2 = now_s();
            check_zero_xor(current_seed, solutions);
            auto t3 = now_s();
            t_verify_sum += (t3 - t2);
        }

        total_sols += solutions.size();
    }

    long peak_kb = peak_rss_kb();
    double avg_fwd = t_fwd_exp_sum / std::max(1, iters);
    double avg_ver = t_verify_sum / std::max(1, iters);
    double sols_per_s = (do_check && t_fwd_exp_sum > 0.0)
                            ? (total_sols / t_fwd_exp_sum)
                            : 0.0;

    std::cout << std::fixed << std::setprecision(2) << "mode=cip-pr sort="
              << (g_sort_algo == SortAlgo::STD ? "std" : "kx")
              << " iters=" << iters << " seed_range=" << seed << "-"
              << (seed + iters - 1) << " single_run_time=" << avg_fwd
              << " peakRSS_kB=" << peak_kb
              << " total_sols=" << total_sols << " Sol/s=" << sols_per_s
              << std::endl;
    std::free(base);
    return 0;
}

static int run_mode_apr(int seed, int iters, bool do_check, bool verbose,
                        const std::string &sort_name)
{
    g_verbose = verbose;
    g_sort_algo = (sort_name == "std") ? SortAlgo::STD : SortAlgo::KXSORT;
    uint8_t *base = static_cast<uint8_t *>(std::malloc(MAX_CIP_APR_BYTES));
    if (base == nullptr)
    {
        std::cerr << "Failed to allocate " << (MAX_CIP_APR_BYTES / (1024 * 1024)) << " MB of memory" << std::endl;
        return 1;
    }
    // Touch the allocation to ensure pages are committed (avoids lazy-fault surprises).
    std::memset(base, 0, MAX_CIP_APR_BYTES);

    double t_fwd_exp_sum = 0.0, t_verify_sum = 0.0;
    size_t total_sols = 0;

    for (int it = 0; it < iters; ++it)
    {
        int current_seed = seed + it;

        auto t0 = now_s();
        std::vector<Solution> solutions = advanced_cip_pr(current_seed, base);
        auto t1 = now_s();
        t_fwd_exp_sum += (t1 - t0);

        if (do_check)
        {
            auto t2 = now_s();
            check_zero_xor(current_seed, solutions);
            auto t3 = now_s();
            t_verify_sum += (t3 - t2);
        }

        total_sols += solutions.size();
    }

    long peak_kb = peak_rss_kb();
    double avg_fwd = t_fwd_exp_sum / std::max(1, iters);
    double avg_ver = t_verify_sum / std::max(1, iters);
    double sols_per_s = (do_check && t_fwd_exp_sum > 0.0)
                            ? (total_sols / t_fwd_exp_sum)
                            : 0.0;

    std::cout << std::fixed << std::setprecision(2) << "mode=cip-apr sort="
              << (g_sort_algo == SortAlgo::STD ? "std" : "kx")
              << " iters=" << iters << " seed_range=" << seed << "-"
              << (seed + iters - 1) << " single_run_time=" << avg_fwd
              << " peakRSS_kB=" << peak_kb
              << " total_sols=" << total_sols << " Sol/s=" << sols_per_s
              << std::endl;
    std::free(base);
    return 0;
}

static int run_mode_em(int seed, int iters, bool do_check, bool verbose,
                       const std::string &sort_name, const std::string &em_path)
{
    g_verbose = verbose;
    g_sort_algo = (sort_name == "std") ? SortAlgo::STD : SortAlgo::KXSORT;

    uint8_t *base = static_cast<uint8_t *>(std::malloc(MAX_CIP_EM_BYTES));
    if (base == nullptr)
    {
        std::cerr << "Failed to allocate " << (MAX_CIP_EM_BYTES / (1024 * 1024)) << " MB of memory" << std::endl;
        return 1;
    }
    // Touch the allocation to ensure pages are committed (avoids lazy-fault surprises).
    std::memset(base, 0, MAX_CIP_EM_BYTES);
    

    double t_fwd_exp_sum = 0.0, t_verify_sum = 0.0;
    size_t total_sols = 0;

    for (int it = 0; it < iters; ++it)
    {
        int current_seed = seed + it;

        auto t0 = now_s();
        std::vector<Solution> solutions = cip_em(current_seed, em_path, base);
        auto t1 = now_s();
        t_fwd_exp_sum += (t1 - t0);

        if (do_check)
        {
            auto t2 = now_s();
            check_zero_xor(current_seed, solutions);
            auto t3 = now_s();
            t_verify_sum += (t3 - t2);
        }

        total_sols += solutions.size();
    }

    long peak_kb = peak_rss_kb();
    double avg_fwd = t_fwd_exp_sum / std::max(1, iters);
    double avg_ver = t_verify_sum / std::max(1, iters);
    double sols_per_s = (do_check && t_fwd_exp_sum > 0.0)
                            ? (total_sols / t_fwd_exp_sum)
                            : 0.0;

    std::cout << std::fixed << std::setprecision(2) << "mode=cip-em sort="
              << (g_sort_algo == SortAlgo::STD ? "std" : "kx")
              << " iters=" << iters << " seed_range=" << seed << "-"
              << (seed + iters - 1) << " single_run_time=" << avg_fwd
              << " peakRSS_kB=" << peak_kb
              << " total_sols=" << total_sols << " Sol/s=" << sols_per_s
              << std::endl;
    std::free(base);
    return 0;
}

// `cip-emc` mode removed: no longer supported.

// ------------ test harness (isolation via exec) ------------
static int run_test_harness(int seed, int iters, bool do_check,
                            const std::string &sortopt, const std::string &em_path)
{
    const std::string exe = self_exe_path();
    const char *modes[4] = {"cip", "cip-pr", "cip-apr", "cip-em"};

    for (int i = 0; i < 4; ++i)
    {
        std::vector<std::string> args;
        args.push_back(exe);
        args.push_back(std::string("--mode=") + modes[i]);
        args.push_back(std::string("--seed=") + std::to_string(seed));
        args.push_back(std::string("--iters=") + std::to_string(iters));
        args.push_back(std::string("--sort=") + sortopt);
        if (do_check)
            args.push_back("--check");
        if (std::string(modes[i]) == "cip-em")
        {
            args.push_back(std::string("--em=") + em_path);
        }
        // no --verbose to avoid IO noise
        int rc = run_isolated_child(args);
        if (rc != 0)
            return rc;
    }
    return 0;
}

// -------- CLI --------
int main(int argc, char **argv)
{
    int seed = 0, iters = 1;
    bool do_check = true, verbose = false, run_test = false;
    std::string mode = "cip", sortopt = "kx", em_path = "ip_cache.bin";

    for (int i = 1; i < argc; i++)
    {
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
        // Note: --no-check option removed; verification behavior is fixed.
        else if (a == "-h" || a == "--help")
        {
                std::cout << "Usage: " << argv[0]
                             << " [--mode=cip|cip-pr|cip-apr|cip-em] [--seed=N] [--iters=M] "
                                 "[--sort=std|kx]"
                             << " [--verbose] [--test] [--em=path]\n"
                             << "  --em=path: External memory file path (default: ip_cache.bin)\n";
            return 0;
        }
    }


    if (run_test)
    {
        return run_test_harness(seed, iters, do_check, sortopt, em_path);
    }

    if (mode == "cip")
    {
        return run_mode_cip(seed, iters, do_check, verbose, sortopt);
    }
    else if (mode == "cip-pr")
    {
        return run_mode_pr(seed, iters, do_check, verbose, sortopt);
    }
    else if (mode == "cip-apr")
    {
        return run_mode_apr(seed, iters, do_check, verbose, sortopt);
    }
    else if (mode == "cip-em")
    {
        return run_mode_em(seed, iters, do_check, verbose, sortopt, em_path);
    }
    else
    {
        std::cerr << "Unknown mode: " << mode << "\n";
        return 1;
    }
}