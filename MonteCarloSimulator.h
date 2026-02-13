#ifndef MONTE_CARLO_SIMULATOR_H
#define MONTE_CARLO_SIMULATOR_H

#include <vector>
#include <random>
#include <string>
#include <atomic> // For thread-safe stats
#include "GameModule.h" // Automatically includes the correct game module

enum class MemoryMode {
    EFFICIENT, 
    ACCURATE 
};

// Helper struct for thread-local online statistics
struct OnlineStats {
    long long count = 0;
    double M1 = 0.0, M2 = 0.0, M3 = 0.0, M4 = 0.0;
    void update(double value);
    void combine(const OnlineStats& other);
};

// Struct to hold confidence interval results
struct ConfidenceInterval {
    double level;
    double lower_bound;
    double upper_bound;
};

class MonteCarloSimulator {
public:
    MonteCarloSimulator();

    // --- Histogram Configuration Interface ---
    void setCustomHistogramBins(std::vector<double>& dividers);
    void setProgressiveHistogramBins();
    void setFixedWidthHistogramBins(double max_val, int num_bins);

    // --- Main Execution ---
    // Overload run for batch simulation with confidence interval calculation 
    void run(long long k_batches_or_bootstraps, long long m_batch_or_sample_size, Game::SimulationMode sim_mode, MemoryMode mem_mode, bool useParallel, double second_chance_prob = 0.0);
    // The fallback run
    void run(long long numSimulations, Game::SimulationMode sim_mode, MemoryMode mem_mode, bool useParallel, double second_chance_prob = 0.0);
    void printResults(int base_bet = 20) const;

private:
    std::mt19937 m_rng; // Master RNG for seeding threads

    // --- Data for ACCURATE mode ---
    std::vector<double> m_results;
    
    // --- Data for EFFICIENT mode ---
    OnlineStats m_final_online_stats;
    OnlineStats m_final_bg_online_stats;  // BG-only stats for stdDev calculation

    // --- Common Data --- 
    struct Histogram {
        std::vector<double> dividers; 
        std::vector<long long> bins;
        long long underflow = 0;
        long long overflow = 0;
    } m_histogram;
    bool m_histogram_configured = false;
    std::vector<double> m_top_values_tracker;
    double m_avg_bg_value = 0.0;

    // --- New members for CI calculations ---
    std::vector<double> m_batch_means;     // For Plan A (Efficient Mode)
    std::vector<double> m_bootstrap_means; // For Plan B (Accurate Mode)

    // --- Thread-safe statistics for FG run length ---
    std::atomic<long long> m_fg_triggered_count{0};
    std::atomic<long long> m_total_fg_runs{0};
    std::atomic<long long> m_total_fg_picks{0};      // Total FG items/picks processed across all FG sessions
    std::atomic<long long> m_max_fg_length{0};
    // Trackers for score components
    std::atomic<double> m_total_bg_score{0.0};
    std::atomic<double> m_total_fg_score{0.0};
    // Trackers for nonzero value frequencies
    std::atomic<long long> m_nonzero_bg_count{0};         // Count of rounds with nonzero BG score
    std::atomic<long long> m_nonzero_fg_sessions_count{0}; // Count of FG sessions with nonzero total payout
    std::atomic<long long> m_nonzero_fg_picks_count{0};   // Count of individual FG picks with nonzero value
    std::atomic<long long> m_nonzero_total_count{0};      // Count of rounds with nonzero total score
    // Trackers for maximum multipliers
    std::atomic<long long> m_max_bg_multiplier{1};        // Max BG multiplier observed (always 1 for this game)
    std::atomic<long long> m_max_fg_multiplier{1};        // Max FG multiplier observed across all rounds

    // --- Trackers for Levels Statistics ---
    // Category 1: BG Items (denominator: m_stats.count)
    std::atomic<long long> m_total_bg_levels{0};           // Sum of all BG levels
    std::atomic<long long> m_bg_nonzero_levels_sum{0};     // Sum where level != 1
    std::atomic<long long> m_bg_nonzero_levels_count{0};   // Count where level != 1
    std::atomic<int> m_max_bg_level{0};                    // Max BG level

    // Category 2: FG Picks (denominator: m_total_fg_picks - already exists!)
    std::atomic<long long> m_total_fg_levels{0};           // Sum of all FG levels
    std::atomic<long long> m_fg_nonzero_levels_sum{0};     // Sum where level != 1
    std::atomic<long long> m_fg_nonzero_levels_count{0};   // Count where level != 1
    std::atomic<int> m_max_fg_level{0};                    // Max FG level

    // Category 3: Per Run (denominator: m_stats.count + m_total_fg_picks)
    std::atomic<long long> m_total_run_levels{0};          // Sum of all levels (BG + FG)
    std::atomic<long long> m_run_nonzero_levels_sum{0};    // Sum where level != 1
    std::atomic<long long> m_run_nonzero_levels_count{0};  // Count where level != 1
    std::atomic<int> m_max_run_level{0};                   // Max level in any run

    // --- Final Calculated Statistics ---
    struct Stats {
        long long count = 0;
        double mean = 0.0, variance = 0.0, stdDev = 0.0, skewness = 0.0, kurtosis = 0.0;
        double bg_stdDev = 0.0;  // BG-only standard deviation
        // Changed to 99th percentile
        double p95 = 0.0, p99 = 0.0; 
        // Added storage for top 5 values
        std::vector<double> top_values; 
        // --- Store multiple CIs in the final stats ---
        std::vector<ConfidenceInterval> confidence_intervals;
    } m_stats;


    MemoryMode m_mode;

    // --- Private Runner Methods ---
    void runEfficientMode_Parallel(long long numSimulations, Game::SimulationMode sim_mode, double second_chance_prob);
    void runAccurateMode_Parallel(long long numSimulations, Game::SimulationMode sim_mode, double second_chance_prob);
    void runEfficientMode_SingleThread(long long numSimulations, Game::SimulationMode sim_mode, double second_chance_prob);
    void runAccurateMode_SingleThread(long long numSimulations, Game::SimulationMode sim_mode, double second_chance_prob);
    
    // --- New Private Runner Methods for batched operations 
    void runEfficientMode_Parallel(long long k, long long m, Game::SimulationMode sim_mode, double second_chance_prob);
    void runAccurateMode_Parallel(long long k, long long m, Game::SimulationMode sim_mode, double second_chance_prob);
    void runEfficientMode_SingleThread(long long k, long long m, Game::SimulationMode sim_mode, double second_chance_prob);
    void runAccurateMode_SingleThread(long long k, long long m, Game::SimulationMode sim_mode, double second_chance_prob);
    
    // --- Private Helper Methods ---
    void resetState();
    void analyzeEfficientResults();
    void analyzeAccurateResults();
    double getPercentileFromHistogram(double percentile) const;
    // --- New Helper Methods for batch operations 
    void analyzeEfficientResults(long long k);
    void analyzeAccurateResults(long long k, long long m);

};

#endif // MONTE_CARLO_SIMULATOR_H
