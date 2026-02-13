#include "MonteCarloSimulator.h"
#include "Statistics.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <omp.h>
#include <sstream>
#include <limits>
#include <stdexcept>
#include <atomic>

// --- OnlineStats Method Implementations ---

void OnlineStats::update(double value) {
    count++;
    double delta = value - M1;
    double delta_n = delta / count;
    double delta_n2 = delta_n * delta_n;
    double term1 = delta * delta_n * (count - 1);
    
    M1 += delta_n;
    M4 += term1 * delta_n2 * (count*count - 3*count + 3) + 6 * delta_n2 * M2 - 4 * delta_n * M3;
    M3 += term1 * delta_n * (count - 2) - 3 * delta_n * M2;
    M2 += term1;
}

void OnlineStats::combine(const OnlineStats& other) {
    if (other.count == 0) return;
    if (count == 0) { *this = other; return; }
    long long combined_count = count + other.count;
    double delta = other.M1 - M1;
    double delta2 = delta * delta;
    double delta3 = delta * delta2;
    double delta4 = delta2 * delta2;
    double combined_M1 = (count * M1 + other.count * other.M1) / combined_count;
    double combined_M2 = M2 + other.M2 + delta2 * count * other.count / combined_count;
    double combined_M3 = M3 + other.M3 + delta3 * count * other.count * (count - other.count) / (combined_count * combined_count);
    combined_M3 += 3.0 * delta * (count * other.M2 - other.count * M2) / combined_count;
    double combined_M4 = M4 + other.M4 + delta4 * count * other.count * (count * count - count * other.count + other.count * other.count) / (combined_count * combined_count * combined_count);
    combined_M4 += 6.0 * delta2 * (count * count * other.M2 + other.count * other.count * M2) / (combined_count * combined_count) + 4.0 * delta * (count * other.M3 - other.count * M3) / combined_count;
    M1 = combined_M1; M2 = combined_M2; M3 = combined_M3; M4 = combined_M4; count = combined_count;
}

// --- Helper function to keep a top-k list ---
void updateTopValues(std::vector<double>& top_values, double new_value, size_t k) {
    if (top_values.size() < k) {
        top_values.push_back(new_value);
        std::sort(top_values.begin(), top_values.end());
    } else if (new_value > top_values[0]) {
        top_values[0] = new_value;
        std::sort(top_values.begin(), top_values.end());
    }
}


// --- MonteCarloSimulator Method Implementations ---

MonteCarloSimulator::MonteCarloSimulator() {
    unsigned seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    m_rng.seed(seed);
}

void MonteCarloSimulator::setCustomHistogramBins(std::vector<double>& dividers) {
    if (dividers.empty() || dividers.front() < 1.0) {
        throw std::invalid_argument("Custom dividers must not be empty and must start with a value >= 1.");
    }
    m_histogram.dividers.clear();
    m_histogram.dividers.push_back(0.0);
    m_histogram.dividers.push_back(1.0);
    m_histogram.dividers.insert(m_histogram.dividers.end(), dividers.begin(), dividers.end());
    m_histogram.bins.assign(m_histogram.dividers.size() - 1, 0);
    m_histogram_configured = true;
    std::cout << "[Config] Custom histogram configured with " << m_histogram.bins.size() << " bins." << std::endl;
}

void MonteCarloSimulator::setProgressiveHistogramBins() {
    std::vector<double> dividers;
    for (double val = 5; val <= 100; val += 5) dividers.push_back(val);
    for (double val = 110; val <= 500; val += 10) dividers.push_back(val);
    for (double val = 600; val <= 2000; val += 100) dividers.push_back(val);
    for (double val = 2500; val <= 20000; val += 500) dividers.push_back(val);
    setCustomHistogramBins(dividers);
    std::cout << "[Config] Progressive histogram configured." << std::endl;
}

void MonteCarloSimulator::setFixedWidthHistogramBins(double max_val, int num_bins) {
    if (max_val <= 1 || num_bins < 1) {
        throw std::invalid_argument("Max value must be > 1 and num_bins must be > 0.");
    }
    std::vector<double> dividers;
    double bin_width = (max_val - 1.0) / num_bins;
    for (int i = 0; i < num_bins; ++i) {
        dividers.push_back(1.0 + (i + 1) * bin_width);
    }
    setCustomHistogramBins(dividers);
    std::cout << "[Config] Fixed-width histogram configured." << std::endl;
}

// --- State Management ---
void MonteCarloSimulator::resetState() {
    m_stats = Stats();
    m_results.clear();
    m_final_online_stats = OnlineStats();
    m_final_bg_online_stats = OnlineStats();  // Reset BG-only stats
    m_top_values_tracker.clear();
    m_batch_means.clear();
    m_bootstrap_means.clear();

    // Reset levels statistics
    m_total_bg_levels = 0;
    m_bg_nonzero_levels_sum = 0;
    m_bg_nonzero_levels_count = 0;
    m_max_bg_level = 0;
    m_total_fg_levels = 0;
    m_fg_nonzero_levels_sum = 0;
    m_fg_nonzero_levels_count = 0;
    m_max_fg_level = 0;
    m_total_run_levels = 0;
    m_run_nonzero_levels_sum = 0;
    m_run_nonzero_levels_count = 0;
    m_max_run_level = 0;

    if (m_histogram_configured) {
        m_histogram.bins.assign(m_histogram.dividers.size() - 1, 0);
        m_histogram.underflow = 0;
        m_histogram.overflow = 0;
    }
}

void MonteCarloSimulator::run(long long numSimulations, Game::SimulationMode sim_mode, MemoryMode mem_mode, bool useParallel, double second_chance_prob) {
    resetState();

    m_mode = mem_mode;
    m_stats = Stats();
    m_total_fg_runs = 0;
    m_total_fg_picks = 0;
    m_max_fg_length = 0;
    m_fg_triggered_count = 0; // Reset all stats at the beginning of a run.
    m_nonzero_bg_count = 0;
    m_nonzero_fg_sessions_count = 0;
    m_nonzero_fg_picks_count = 0;
    m_nonzero_total_count = 0;
    m_max_bg_multiplier = 1;
    m_max_fg_multiplier = 1;

    // Reset levels statistics
    m_total_bg_levels = 0;
    m_bg_nonzero_levels_sum = 0;
    m_bg_nonzero_levels_count = 0;
    m_max_bg_level = 0;
    m_total_fg_levels = 0;
    m_fg_nonzero_levels_sum = 0;
    m_fg_nonzero_levels_count = 0;
    m_max_fg_level = 0;
    m_total_run_levels = 0;
    m_run_nonzero_levels_sum = 0;
    m_run_nonzero_levels_count = 0;
    m_max_run_level = 0;

    // --- Calculate average BG value for later use ---
    m_total_bg_score = 0.0;
    m_total_fg_score = 0.0;


    if (!m_histogram_configured) {
        std::cout << "[Config] No histogram specified, using default Progressive Bins." << std::endl;
        setProgressiveHistogramBins();
    }
    // --- Adjust simulation count for FG_ONLY mode ---
    long long effective_simulations = numSimulations;
    if (sim_mode == Game::SimulationMode::FG_ONLY) {
        effective_simulations /= 10;
        if (effective_simulations == 0 && numSimulations > 0) effective_simulations = 1; // Ensure at least one run
        std::cout << "[Monitor] FG_ONLY mode selected. Adjusting total simulation rounds to " << effective_simulations 
                  << " for comparable FG event count." << std::endl;
    }

    if (!useParallel) {
        std::cout << "\n[Monitor] Running in SINGLE-THREADED mode." << std::endl;
        if (mem_mode == MemoryMode::EFFICIENT) { runEfficientMode_SingleThread(effective_simulations, sim_mode, second_chance_prob); } 
        else { runAccurateMode_SingleThread(effective_simulations, sim_mode, second_chance_prob); }
        return;
    }
    
    std::cout << "\n[Monitor] Running in PARALLEL mode." << std::endl;
    if (mem_mode == MemoryMode::EFFICIENT) { runEfficientMode_Parallel(effective_simulations, sim_mode, second_chance_prob); } 
    else { runAccurateMode_Parallel(effective_simulations, sim_mode, second_chance_prob); }

}

// --- HIGHLIGHT: Main run function updated to accept k and m ---
void MonteCarloSimulator::run(long long k, long long m, Game::SimulationMode sim_mode, MemoryMode mode, bool useParallel, double second_chance_prob) {
    resetState(); // Clear all previous state including batch_means and bootstrap_means

    m_mode = mode;
    m_stats = Stats();
    m_total_fg_runs = 0;
    m_total_fg_picks = 0;
    m_max_fg_length = 0;
    m_fg_triggered_count = 0; // Reset all stats at the beginning of a run.
    m_nonzero_bg_count = 0;
    m_nonzero_fg_sessions_count = 0;
    m_nonzero_fg_picks_count = 0;
    m_nonzero_total_count = 0;
    m_max_bg_multiplier = 1;
    m_max_fg_multiplier = 1;

    // Reset levels statistics
    m_total_bg_levels = 0;
    m_bg_nonzero_levels_sum = 0;
    m_bg_nonzero_levels_count = 0;
    m_max_bg_level = 0;
    m_total_fg_levels = 0;
    m_fg_nonzero_levels_sum = 0;
    m_fg_nonzero_levels_count = 0;
    m_max_fg_level = 0;
    m_total_run_levels = 0;
    m_run_nonzero_levels_sum = 0;
    m_run_nonzero_levels_count = 0;
    m_max_run_level = 0;

    // --- Calculate average BG value for later use ---
    m_total_bg_score = 0.0;
    m_total_fg_score = 0.0;

    if (!m_histogram_configured) {
        std::cout << "[Config] No histogram specified, using default Progressive Bins." << std::endl;
        setProgressiveHistogramBins();
    }
    long long numBatches = k;
    long long numRounds = m;
    long long numSimulations = k * m;

    if (numSimulations <= 0) {
        throw std::invalid_argument("Total number of simulations (k * m) must be positive.");
    }

    if (sim_mode == Game::SimulationMode::FG_ONLY) {
        // IMPORTANT: In FG_ONLY mode, we reduce the rounds per batch (m) by 10x
        // to maintain a comparable number of FG events per simulation.
        // This keeps k (number of batches) constant, which maintains the
        // statistical power of the confidence interval calculation.
        //
        // Rationale:
        //   - In FULL_GAME mode, only ~10% of rounds trigger FG
        //   - In FG_ONLY mode, 100% of rounds have FG
        //   - Reducing m by 10x gives similar FG event counts
        //   - Keeping k constant preserves CI accuracy (degrees of freedom)
        //
        // Example: k=100, m=10,000 in FULL_GAME mode
        //       → k=100, m=1,000 in FG_ONLY mode
        //       → Both have 100 batches for CI calculation
        //       → FG_ONLY has shorter batches but same number of batches
        numRounds /= 10;
        if (numRounds == 0 && numSimulations > 0) {
            numRounds = 1; // Ensure at least one round per batch
        }
        numSimulations = numRounds * numBatches;
        std::cout << "[Monitor] FG_ONLY mode selected." << std::endl;
        std::cout << "[Monitor] Adjusting rounds per batch from " << m << " to " << numRounds
                  << " for comparable FG event count." << std::endl;
        std::cout << "[Monitor] Total simulations: " << numSimulations
                  << " (" << numBatches << " batches × " << numRounds << " rounds/batch)" << std::endl;
    }

    if (!useParallel) {
        std::cout << "\n[Monitor] Running in SINGLE-THREADED mode." << std::endl;
        if (mode == MemoryMode::EFFICIENT) { runEfficientMode_SingleThread(numBatches, numRounds, sim_mode, second_chance_prob); } 
        else { runAccurateMode_SingleThread(numBatches, numRounds, sim_mode, second_chance_prob); }
        return;
    }
    std::cout << "\n[Monitor] Running in PARALLEL mode." << std::endl;
    if (mode == MemoryMode::EFFICIENT) { runEfficientMode_Parallel(numBatches, numRounds, sim_mode, second_chance_prob); } 
    else { runAccurateMode_Parallel(numBatches, numRounds, sim_mode, second_chance_prob); }
}




// --- MonteCarloSimulator Run Modes Method Implementations ---


// --- Single-Threaded Runners ---

// Fallback Implementation without CI
void MonteCarloSimulator::runEfficientMode_SingleThread(long long numSimulations, Game::SimulationMode sim_mode, double second_chance_prob) {
    std::cout << "[Monitor] Starting simulation in EFFICIENT memory mode." << std::endl;
    auto start_sim_time = std::chrono::high_resolution_clock::now();
    m_final_online_stats = OnlineStats();
    m_top_values_tracker.clear();
    m_histogram.bins.assign(m_histogram.dividers.size() - 1, 0);
    m_histogram.underflow = 0; m_histogram.overflow = 0;

    const long long progress_interval = numSimulations > 20 ? numSimulations / 20 : 1;

    for (long long i = 0; i < numSimulations; ++i) {
        Game::GameResult result = Game::simulateGameRound(m_rng, sim_mode, second_chance_prob);
        double total_score = result.bg_score + result.fg_score;

        m_final_online_stats.update(total_score);
        m_final_bg_online_stats.update(result.bg_score);
        updateTopValues(m_top_values_tracker, total_score, 5);

        m_total_bg_score = m_total_bg_score.load() + result.bg_score;
        m_total_fg_score = m_total_fg_score.load() + result.fg_score;

        // Track nonzero frequencies
        if (result.bg_score != 0) m_nonzero_bg_count++;
        if (result.fg_score != 0) m_nonzero_fg_sessions_count++;  // Session-level tracking
        if (total_score != 0) m_nonzero_total_count++;
        m_nonzero_fg_picks_count += result.fg_nonzero_picks;  // Pick-level tracking

        // Track FG statistics
        if (result.fg_was_triggered) {
            m_total_fg_picks += result.fg_run_length;
            m_fg_triggered_count++;
            if (result.fg_run_length > 0) {
                m_total_fg_runs++;
                if (result.fg_run_length > m_max_fg_length) {
                    m_max_fg_length = result.fg_run_length;
                }
            }
        }

        // Track max multipliers
        if (result.max_bg_multiplier > m_max_bg_multiplier) m_max_bg_multiplier = result.max_bg_multiplier;
        if (result.max_fg_multiplier > m_max_fg_multiplier) m_max_fg_multiplier = result.max_fg_multiplier;

        // Track levels statistics
        // Category 1: BG levels
        m_total_bg_levels += result.bg_levels;
        if (result.bg_levels != 1) {
            m_bg_nonzero_levels_sum += result.bg_levels;
            m_bg_nonzero_levels_count++;
        }
        if (result.bg_levels > m_max_bg_level) {
            m_max_bg_level = result.bg_levels;
        }

        // Category 2: FG picks
        int run_max_fg_level = 0;
        for (int fg_level : result.fg_levels) {
            m_total_fg_levels += fg_level;
            if (fg_level != 1) {
                m_fg_nonzero_levels_sum += fg_level;
                m_fg_nonzero_levels_count++;
            }
            if (fg_level > run_max_fg_level) run_max_fg_level = fg_level;
        }
        if (run_max_fg_level > m_max_fg_level) {
            m_max_fg_level = run_max_fg_level;
        }

        // Category 3: Per run
        long long run_total_levels = result.bg_levels;
        long long run_nonzero_sum = (result.bg_levels != 1) ? result.bg_levels : 0;
        long long run_nonzero_count = (result.bg_levels != 1) ? 1 : 0;
        int run_max_level = result.bg_levels;

        for (int fg_level : result.fg_levels) {
            run_total_levels += fg_level;
            if (fg_level != 1) {
                run_nonzero_sum += fg_level;
                run_nonzero_count++;
            }
            if (fg_level > run_max_level) run_max_level = fg_level;
        }

        m_total_run_levels += run_total_levels;
        m_run_nonzero_levels_sum += run_nonzero_sum;
        m_run_nonzero_levels_count += run_nonzero_count;
        if (run_max_level > m_max_run_level) {
            m_max_run_level = run_max_level;
        }

        if (total_score < 0) { m_histogram.underflow++; } 
        else if (total_score >= m_histogram.dividers.back()) { m_histogram.overflow++; } 
        else {
            auto it = std::upper_bound(m_histogram.dividers.begin(), m_histogram.dividers.end(), total_score);
            int bin_index = std::distance(m_histogram.dividers.begin(), it) - 1;
            m_histogram.bins[bin_index]++;
        }

        if ((i + 1) % progress_interval == 0) {
            std::cout << "          ... Progress: " << (100 * (i + 1) / numSimulations) << "% complete." << std::endl;
        }
    }
    auto end_sim_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> sim_elapsed = end_sim_time - start_sim_time;
    std::cout << "[Monitor] Simulation loop finished in " << sim_elapsed.count() << " seconds." << std::endl;
    analyzeEfficientResults();
}

// New EfficientMode with batch calculation of CI
void MonteCarloSimulator::runEfficientMode_SingleThread(long long k, long long m, Game::SimulationMode sim_mode, double second_chance_prob) {
    std::cout << "[Monitor] Starting simulation in EFFICIENT memory mode with batch-level structure." << std::endl;
    std::cout << "[Monitor] Configuration: " << k << " batches × " << m << " rounds/batch = " << (k * m) << " total rounds" << std::endl;
    auto start_sim_time = std::chrono::high_resolution_clock::now();

    m_final_online_stats = OnlineStats();
    m_top_values_tracker.clear();
    m_histogram.bins.assign(m_histogram.dividers.size() - 1, 0);
    m_histogram.underflow = 0;
    m_histogram.overflow = 0;

    const long long progress_interval_batches = k > 100 ? k / 100 : 1;

    // BATCH-LEVEL LOOP: Process batches sequentially
    for (long long batch = 0; batch < k; ++batch) {
        OnlineStats batch_stats; // Fresh statistics for this batch

        // INNER LOOP: Process all rounds in this batch
        for (long long round = 0; round < m; ++round) {
            Game::GameResult result = Game::simulateGameRound(m_rng, sim_mode, second_chance_prob);
            double total_score = result.bg_score + result.fg_score;

            // UPDATE 1: Overall statistics (for all rounds across all batches)
            m_final_online_stats.update(total_score);
            m_final_bg_online_stats.update(result.bg_score);

            // UPDATE 2: Batch statistics (for this specific batch)
            batch_stats.update(total_score);

            // UPDATE 3: Top values tracking
            updateTopValues(m_top_values_tracker, total_score, 5);

            // UPDATE 4: BG/FG score contributions
            m_total_bg_score = m_total_bg_score.load() + result.bg_score;
            m_total_fg_score = m_total_fg_score.load() + result.fg_score;

            // UPDATE 5: Track nonzero frequencies
            if (result.bg_score != 0) m_nonzero_bg_count++;
            if (result.fg_score != 0) m_nonzero_fg_sessions_count++;  // Session-level tracking
            if (total_score != 0) m_nonzero_total_count++;
            m_nonzero_fg_picks_count += result.fg_nonzero_picks;  // Pick-level tracking

            // UPDATE 6: FG statistics (trigger count, run lengths)
            if (result.fg_was_triggered) {
                m_total_fg_picks += result.fg_run_length;
                m_fg_triggered_count++;
                if (result.fg_run_length > 0) {
                    m_total_fg_runs++;
                    if (result.fg_run_length > m_max_fg_length) {
                        m_max_fg_length = result.fg_run_length;
                    }
                }
            }

            // UPDATE 7: Track max multipliers
            if (result.max_bg_multiplier > m_max_bg_multiplier) m_max_bg_multiplier = result.max_bg_multiplier;
            if (result.max_fg_multiplier > m_max_fg_multiplier) m_max_fg_multiplier = result.max_fg_multiplier;

            // UPDATE 8: Track levels statistics
            // Category 1: BG levels
            m_total_bg_levels += result.bg_levels;
            if (result.bg_levels != 1) {
                m_bg_nonzero_levels_sum += result.bg_levels;
                m_bg_nonzero_levels_count++;
            }
            if (result.bg_levels > m_max_bg_level) {
                m_max_bg_level = result.bg_levels;
            }

            // Category 2: FG picks
            int run_max_fg_level = 0;
            for (int fg_level : result.fg_levels) {
                m_total_fg_levels += fg_level;
                if (fg_level != 1) {
                    m_fg_nonzero_levels_sum += fg_level;
                    m_fg_nonzero_levels_count++;
                }
                if (fg_level > run_max_fg_level) run_max_fg_level = fg_level;
            }
            if (run_max_fg_level > m_max_fg_level) {
                m_max_fg_level = run_max_fg_level;
            }

            // Category 3: Per run
            long long run_total_levels = result.bg_levels;
            long long run_nonzero_sum = (result.bg_levels != 1) ? result.bg_levels : 0;
            long long run_nonzero_count = (result.bg_levels != 1) ? 1 : 0;
            int run_max_level = result.bg_levels;

            for (int fg_level : result.fg_levels) {
                run_total_levels += fg_level;
                if (fg_level != 1) {
                    run_nonzero_sum += fg_level;
                    run_nonzero_count++;
                }
                if (fg_level > run_max_level) run_max_level = fg_level;
            }

            m_total_run_levels += run_total_levels;
            m_run_nonzero_levels_sum += run_nonzero_sum;
            m_run_nonzero_levels_count += run_nonzero_count;
            if (run_max_level > m_max_run_level) {
                m_max_run_level = run_max_level;
            }

            // UPDATE 9: Histogram distribution tracking
            if (total_score < 0) {
                m_histogram.underflow++;
            } else if (total_score >= m_histogram.dividers.back()) {
                m_histogram.overflow++;
            } else {
                auto it = std::upper_bound(m_histogram.dividers.begin(), m_histogram.dividers.end(), total_score);
                int bin_index = std::distance(m_histogram.dividers.begin(), it) - 1;
                m_histogram.bins[bin_index]++;
            }
        }

        // After completing all m rounds in this batch, store the batch mean
        m_batch_means.push_back(batch_stats.M1);

        // Progress reporting by batch
        if ((batch + 1) % progress_interval_batches == 0) {
            std::cout << "          ... Progress: Batch " << (batch + 1) << "/" << k
                      << " (" << std::fixed << std::setprecision(1)
                      << (100.0 * (batch + 1) / k) << "% complete)" << std::endl;
        }
    }

    auto end_sim_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> sim_elapsed = end_sim_time - start_sim_time;
    std::cout << "[Monitor] Simulation loop finished in " << sim_elapsed.count() << " seconds." << std::endl;
    analyzeEfficientResults(k);
}



// Fallback Implementation without CI
void MonteCarloSimulator::runAccurateMode_SingleThread(long long numSimulations, Game::SimulationMode sim_mode, double second_chance_prob) {
    std::cout << "[Monitor] Starting simulation in ACCURATE memory mode." << std::endl;
    auto start_sim_time = std::chrono::high_resolution_clock::now();
    m_results.clear(); m_results.reserve(numSimulations);
    const long long progress_interval = numSimulations > 20 ? numSimulations / 20 : 1;

    for (long long i = 0; i < numSimulations; ++i) {
        Game::GameResult result = Game::simulateGameRound(m_rng, sim_mode, second_chance_prob);
        double total_score = result.bg_score + result.fg_score;
        m_results.push_back(total_score);

        m_total_bg_score = m_total_bg_score.load() + result.bg_score;
        m_total_fg_score = m_total_fg_score.load() + result.fg_score;

        // Track nonzero frequencies
        if (result.bg_score != 0) m_nonzero_bg_count++;
        if (result.fg_score != 0) m_nonzero_fg_sessions_count++;  // Session-level tracking
        if (total_score != 0) m_nonzero_total_count++;
        m_nonzero_fg_picks_count += result.fg_nonzero_picks;  // Pick-level tracking

        // Track FG statistics
        if (result.fg_was_triggered) {
            m_total_fg_picks += result.fg_run_length;
            m_fg_triggered_count++;
            if (result.fg_run_length > 0) {
                m_total_fg_runs++;
                if (result.fg_run_length > m_max_fg_length) {
                    m_max_fg_length = result.fg_run_length;
                }
            }
        }

        // Track max multipliers
        if (result.max_bg_multiplier > m_max_bg_multiplier) m_max_bg_multiplier = result.max_bg_multiplier;
        if (result.max_fg_multiplier > m_max_fg_multiplier) m_max_fg_multiplier = result.max_fg_multiplier;

        // Track levels statistics
        // Category 1: BG levels
        m_total_bg_levels += result.bg_levels;
        if (result.bg_levels != 1) {
            m_bg_nonzero_levels_sum += result.bg_levels;
            m_bg_nonzero_levels_count++;
        }
        if (result.bg_levels > m_max_bg_level) {
            m_max_bg_level = result.bg_levels;
        }

        // Category 2: FG picks
        int run_max_fg_level = 0;
        for (int fg_level : result.fg_levels) {
            m_total_fg_levels += fg_level;
            if (fg_level != 1) {
                m_fg_nonzero_levels_sum += fg_level;
                m_fg_nonzero_levels_count++;
            }
            if (fg_level > run_max_fg_level) run_max_fg_level = fg_level;
        }
        if (run_max_fg_level > m_max_fg_level) {
            m_max_fg_level = run_max_fg_level;
        }

        // Category 3: Per run
        long long run_total_levels = result.bg_levels;
        long long run_nonzero_sum = (result.bg_levels != 1) ? result.bg_levels : 0;
        long long run_nonzero_count = (result.bg_levels != 1) ? 1 : 0;
        int run_max_level = result.bg_levels;

        for (int fg_level : result.fg_levels) {
            run_total_levels += fg_level;
            if (fg_level != 1) {
                run_nonzero_sum += fg_level;
                run_nonzero_count++;
            }
            if (fg_level > run_max_level) run_max_level = fg_level;
        }

        m_total_run_levels += run_total_levels;
        m_run_nonzero_levels_sum += run_nonzero_sum;
        m_run_nonzero_levels_count += run_nonzero_count;
        if (run_max_level > m_max_run_level) {
            m_max_run_level = run_max_level;
        }

        if ((i + 1) % progress_interval == 0) {
            std::cout << "          ... Progress: " << (100 * (i + 1) / numSimulations) << "% complete." << std::endl;
        }
    }
    auto end_sim_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> sim_elapsed = end_sim_time - start_sim_time;
    std::cout << "[Monitor] Simulation loop finished in " << sim_elapsed.count() << " seconds." << std::endl;
    analyzeAccurateResults();
}


// New implementation with bootstrapping for CI
void MonteCarloSimulator::runAccurateMode_SingleThread(long long k, long long m, Game::SimulationMode sim_mode, double second_chance_prob) {
    std::cout << "[Monitor] Starting simulation in ACCURATE memory mode with batch-level structure." << std::endl;
    std::cout << "[Monitor] Configuration: " << k << " batches × " << m << " rounds/batch = " << (k * m) << " total rounds" << std::endl;
    auto start_sim_time = std::chrono::high_resolution_clock::now();

    long long numSimulations = k * m;
    m_results.clear();
    m_results.reserve(numSimulations);
    const long long progress_interval_batches = k > 100 ? k / 100 : 1;

    // BATCH-LEVEL LOOP: Process batches sequentially
    for (long long batch = 0; batch < k; ++batch) {
        // INNER LOOP: Process all rounds in this batch
        for (long long round = 0; round < m; ++round) {
            Game::GameResult result = Game::simulateGameRound(m_rng, sim_mode, second_chance_prob);
            double total_score = result.bg_score + result.fg_score;
            m_results.push_back(total_score);

            m_total_bg_score = m_total_bg_score.load() + result.bg_score;
            m_total_fg_score = m_total_fg_score.load() + result.fg_score;

            // Track nonzero frequencies
            if (result.bg_score != 0) m_nonzero_bg_count++;
            if (result.fg_score != 0) m_nonzero_fg_sessions_count++;  // Session-level tracking
            if (total_score != 0) m_nonzero_total_count++;
            m_nonzero_fg_picks_count += result.fg_nonzero_picks;  // Pick-level tracking

            // Track FG statistics
            if (result.fg_was_triggered) {
                m_total_fg_picks += result.fg_run_length;
                m_fg_triggered_count++;
                if (result.fg_run_length > 0) {
                    m_total_fg_runs++;
                    if (result.fg_run_length > m_max_fg_length) {
                        m_max_fg_length = result.fg_run_length;
                    }
                }
            }

            // Track max multipliers
            if (result.max_bg_multiplier > m_max_bg_multiplier) m_max_bg_multiplier = result.max_bg_multiplier;
            if (result.max_fg_multiplier > m_max_fg_multiplier) m_max_fg_multiplier = result.max_fg_multiplier;

            // Track levels statistics
            // Category 1: BG levels
            m_total_bg_levels += result.bg_levels;
            if (result.bg_levels != 1) {
                m_bg_nonzero_levels_sum += result.bg_levels;
                m_bg_nonzero_levels_count++;
            }
            if (result.bg_levels > m_max_bg_level) {
                m_max_bg_level = result.bg_levels;
            }

            // Category 2: FG picks
            int run_max_fg_level = 0;
            for (int fg_level : result.fg_levels) {
                m_total_fg_levels += fg_level;
                if (fg_level != 1) {
                    m_fg_nonzero_levels_sum += fg_level;
                    m_fg_nonzero_levels_count++;
                }
                if (fg_level > run_max_fg_level) run_max_fg_level = fg_level;
            }
            if (run_max_fg_level > m_max_fg_level) {
                m_max_fg_level = run_max_fg_level;
            }

            // Category 3: Per run
            long long run_total_levels = result.bg_levels;
            long long run_nonzero_sum = (result.bg_levels != 1) ? result.bg_levels : 0;
            long long run_nonzero_count = (result.bg_levels != 1) ? 1 : 0;
            int run_max_level = result.bg_levels;

            for (int fg_level : result.fg_levels) {
                run_total_levels += fg_level;
                if (fg_level != 1) {
                    run_nonzero_sum += fg_level;
                    run_nonzero_count++;
                }
                if (fg_level > run_max_level) run_max_level = fg_level;
            }

            m_total_run_levels += run_total_levels;
            m_run_nonzero_levels_sum += run_nonzero_sum;
            m_run_nonzero_levels_count += run_nonzero_count;
            if (run_max_level > m_max_run_level) {
                m_max_run_level = run_max_level;
            }
        }

        // Progress reporting by batch
        if ((batch + 1) % progress_interval_batches == 0) {
            std::cout << "          ... Progress: Batch " << (batch + 1) << "/" << k
                      << " (" << std::fixed << std::setprecision(1)
                      << (100.0 * (batch + 1) / k) << "% complete)" << std::endl;
        }
    }

    auto end_sim_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> sim_elapsed = end_sim_time - start_sim_time;
    std::cout << "[Monitor] Simulation loop finished in " << sim_elapsed.count() << " seconds." << std::endl;
    analyzeAccurateResults(k, m);
}


// --- Parallel Runners ---

// Fallback Implementation without CI
void MonteCarloSimulator::runEfficientMode_Parallel(long long numSimulations, Game::SimulationMode sim_mode, double second_chance_prob) {
    std::cout << "[Monitor] Starting parallel simulation in EFFICIENT memory mode." << std::endl;
    auto start_sim_time = std::chrono::high_resolution_clock::now();
    int num_threads = 0;
    std::vector<OnlineStats> thread_stats;
    std::vector<OnlineStats> thread_bg_stats;  // BG-only stats per thread
    std::vector<Histogram> thread_histograms;
    std::vector<std::vector<double>> thread_top_values;
    std::vector<long long> thread_max_fg_lengths;
    std::vector<long long> thread_max_bg_multipliers;
    std::vector<long long> thread_max_fg_multipliers;
    std::vector<int> thread_max_bg_levels;
    std::vector<int> thread_max_fg_levels;
    std::vector<int> thread_max_run_levels;
    long long total_fg_runs_p = 0;
    long long total_fg_picks_p = 0;
    long long fg_triggered_count_p = 0;
    double total_bg_score_p = 0.0;
    double total_fg_score_p = 0.0;
    long long nonzero_bg_p = 0, nonzero_fg_sessions_p = 0, nonzero_fg_picks_p = 0, nonzero_total_p = 0;
    long long total_bg_levels_p = 0, bg_nonzero_levels_sum_p = 0, bg_nonzero_levels_count_p = 0;
    long long total_fg_levels_p = 0, fg_nonzero_levels_sum_p = 0, fg_nonzero_levels_count_p = 0;
    long long total_run_levels_p = 0, run_nonzero_levels_sum_p = 0, run_nonzero_levels_count_p = 0;
    std::atomic<long long> completed_count = 0;
    const long long progress_interval = numSimulations > 20 ? numSimulations / 20 : 1;

    #pragma omp parallel
    {
        #pragma omp master
        {
            num_threads = omp_get_num_threads();
            std::cout << "[Monitor] Detected and using " << num_threads << " threads." << std::endl;
            thread_stats.resize(num_threads);
            thread_bg_stats.resize(num_threads);  // BG-only stats per thread
            thread_histograms.resize(num_threads);
            thread_top_values.resize(num_threads);
            thread_max_fg_lengths.resize(num_threads, 0);
            thread_max_bg_multipliers.resize(num_threads, 1);
            thread_max_fg_multipliers.resize(num_threads, 1);
            thread_max_bg_levels.resize(num_threads, 0);
            thread_max_fg_levels.resize(num_threads, 0);
            thread_max_run_levels.resize(num_threads, 0);
            for(int i = 0; i < num_threads; ++i) {
                thread_histograms[i].bins.assign(m_histogram.dividers.size() - 1, 0);
            }
        }
        #pragma omp barrier
        int thread_id = omp_get_thread_num();
        std::mt19937 local_rng(m_rng());

        #pragma omp for reduction(+:total_fg_runs_p, total_fg_picks_p, fg_triggered_count_p, total_bg_score_p, total_fg_score_p, nonzero_bg_p, nonzero_fg_sessions_p, nonzero_fg_picks_p, nonzero_total_p, total_bg_levels_p, bg_nonzero_levels_sum_p, bg_nonzero_levels_count_p, total_fg_levels_p, fg_nonzero_levels_sum_p, fg_nonzero_levels_count_p, total_run_levels_p, run_nonzero_levels_sum_p, run_nonzero_levels_count_p) schedule(dynamic)
        for (long long i = 0; i < numSimulations; ++i) {
            Game::GameResult result = Game::simulateGameRound(local_rng, sim_mode, second_chance_prob);
            double total_score = result.bg_score + result.fg_score;
            thread_stats[thread_id].update(total_score);
            thread_bg_stats[thread_id].update(result.bg_score);
            updateTopValues(thread_top_values[thread_id], total_score, 5);

            total_bg_score_p += result.bg_score;
            total_fg_score_p += result.fg_score;

            // Track nonzero frequencies
            if (result.bg_score != 0) nonzero_bg_p++;
            if (result.fg_score != 0) nonzero_fg_sessions_p++;  // Session-level tracking
            if (total_score != 0) nonzero_total_p++;
            nonzero_fg_picks_p += result.fg_nonzero_picks;  // Pick-level tracking

            // Track FG statistics
            if (result.fg_was_triggered) {
                total_fg_picks_p += result.fg_run_length;
                fg_triggered_count_p++;
                if (result.fg_run_length > 0) {
                    total_fg_runs_p++;
                    if(result.fg_run_length > thread_max_fg_lengths[thread_id]) {
                        thread_max_fg_lengths[thread_id] = result.fg_run_length;
                    }
                }
            }

            // Track max multipliers
            if(result.max_bg_multiplier > thread_max_bg_multipliers[thread_id]) {
                thread_max_bg_multipliers[thread_id] = result.max_bg_multiplier;
            }
            if(result.max_fg_multiplier > thread_max_fg_multipliers[thread_id]) {
                thread_max_fg_multipliers[thread_id] = result.max_fg_multiplier;
            }

            // Track levels statistics
            // Category 1: BG levels
            total_bg_levels_p += result.bg_levels;
            if (result.bg_levels != 1) {
                bg_nonzero_levels_sum_p += result.bg_levels;
                bg_nonzero_levels_count_p++;
            }
            if (result.bg_levels > thread_max_bg_levels[thread_id]) {
                thread_max_bg_levels[thread_id] = result.bg_levels;
            }

            // Category 2: FG picks
            int run_max_fg_level = 0;
            for (int fg_level : result.fg_levels) {
                total_fg_levels_p += fg_level;
                if (fg_level != 1) {
                    fg_nonzero_levels_sum_p += fg_level;
                    fg_nonzero_levels_count_p++;
                }
                if (fg_level > run_max_fg_level) run_max_fg_level = fg_level;
            }
            if (run_max_fg_level > thread_max_fg_levels[thread_id]) {
                thread_max_fg_levels[thread_id] = run_max_fg_level;
            }

            // Category 3: Per run
            long long run_total_levels = result.bg_levels;
            long long run_nonzero_sum = (result.bg_levels != 1) ? result.bg_levels : 0;
            long long run_nonzero_count = (result.bg_levels != 1) ? 1 : 0;
            int run_max_level = result.bg_levels;

            for (int fg_level : result.fg_levels) {
                run_total_levels += fg_level;
                if (fg_level != 1) {
                    run_nonzero_sum += fg_level;
                    run_nonzero_count++;
                }
                if (fg_level > run_max_level) run_max_level = fg_level;
            }

            total_run_levels_p += run_total_levels;
            run_nonzero_levels_sum_p += run_nonzero_sum;
            run_nonzero_levels_count_p += run_nonzero_count;
            if (run_max_level > thread_max_run_levels[thread_id]) {
                thread_max_run_levels[thread_id] = run_max_level;
            }

            if (total_score < 0) { thread_histograms[thread_id].underflow++; } 
            else if (total_score >= m_histogram.dividers.back()) { thread_histograms[thread_id].overflow++; } 
            else {
                auto it = std::upper_bound(m_histogram.dividers.begin(), m_histogram.dividers.end(), total_score);
                int bin_index = std::distance(m_histogram.dividers.begin(), it) - 1;
                thread_histograms[thread_id].bins[bin_index]++;
            }
            
            long long current_completed = completed_count.fetch_add(1, std::memory_order_relaxed) + 1;
            if ((current_completed % progress_interval == 0) && (thread_id == 0)) {
                #pragma omp critical
                std::cout << "          ... Progress: " << (100 * current_completed / numSimulations) << "% complete." << std::endl;
            }
        }
    }
    std::cout << "[Monitor] Combining results from all threads..." << std::endl;
    m_fg_triggered_count = fg_triggered_count_p;
    m_total_fg_runs = total_fg_runs_p;
    m_total_fg_picks = total_fg_picks_p;
    m_total_bg_score = total_bg_score_p;
    m_total_fg_score = total_fg_score_p;
    m_nonzero_bg_count = nonzero_bg_p;
    m_nonzero_fg_sessions_count = nonzero_fg_sessions_p;
    m_nonzero_fg_picks_count = nonzero_fg_picks_p;
    m_nonzero_total_count = nonzero_total_p;
    for(long long max_val : thread_max_fg_lengths) { if(max_val > m_max_fg_length) { m_max_fg_length = max_val; } }
    for(long long max_val : thread_max_bg_multipliers) { if(max_val > m_max_bg_multiplier) { m_max_bg_multiplier = max_val; } }
    for(long long max_val : thread_max_fg_multipliers) { if(max_val > m_max_fg_multiplier) { m_max_fg_multiplier = max_val; } }

    // Aggregate levels statistics
    m_total_bg_levels = total_bg_levels_p;
    m_bg_nonzero_levels_sum = bg_nonzero_levels_sum_p;
    m_bg_nonzero_levels_count = bg_nonzero_levels_count_p;
    m_total_fg_levels = total_fg_levels_p;
    m_fg_nonzero_levels_sum = fg_nonzero_levels_sum_p;
    m_fg_nonzero_levels_count = fg_nonzero_levels_count_p;
    m_total_run_levels = total_run_levels_p;
    m_run_nonzero_levels_sum = run_nonzero_levels_sum_p;
    m_run_nonzero_levels_count = run_nonzero_levels_count_p;
    for(int max_val : thread_max_bg_levels) { if(max_val > m_max_bg_level) { m_max_bg_level = max_val; } }
    for(int max_val : thread_max_fg_levels) { if(max_val > m_max_fg_level) { m_max_fg_level = max_val; } }
    for(int max_val : thread_max_run_levels) { if(max_val > m_max_run_level) { m_max_run_level = max_val; } }

    m_final_online_stats = thread_stats[0];
    m_final_bg_online_stats = thread_bg_stats[0];  // BG-only stats
    m_histogram.bins.assign(m_histogram.dividers.size() - 1, 0);
    for (int i = 0; i < num_threads; ++i) {
        if(i > 0) {
            m_final_online_stats.combine(thread_stats[i]);
            m_final_bg_online_stats.combine(thread_bg_stats[i]);  // Combine BG stats
        }
        for(size_t j = 0; j < m_histogram.bins.size(); ++j) { m_histogram.bins[j] += thread_histograms[i].bins[j]; }
        m_histogram.underflow += thread_histograms[i].underflow;
        m_histogram.overflow += thread_histograms[i].overflow;
    }
    m_top_values_tracker.clear();
    for (const auto& vec : thread_top_values) {
        m_top_values_tracker.insert(m_top_values_tracker.end(), vec.begin(), vec.end());
    }
    std::sort(m_top_values_tracker.rbegin(), m_top_values_tracker.rend());
    m_top_values_tracker.resize(std::min((size_t)5, m_top_values_tracker.size()));

    auto end_sim_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> sim_elapsed = end_sim_time - start_sim_time;
    std::cout << "[Monitor] Simulation loop finished in " << sim_elapsed.count() << " seconds." << std::endl;
    analyzeEfficientResults();
}


// New Efficient Mode with batch calculation of CI
void MonteCarloSimulator::runEfficientMode_Parallel(long long k, long long m, Game::SimulationMode sim_mode, double second_chance_prob) {
    std::cout << "[Monitor] Starting parallel simulation in EFFICIENT memory mode with batch-level parallelization." << std::endl;
    std::cout << "[Monitor] Configuration: " << k << " batches " << m << " rounds/batch = " << (k * m) << " total rounds" << std::endl;
    auto start_sim_time = std::chrono::high_resolution_clock::now();

    int num_threads = 0;
    std::vector<OnlineStats> thread_stats;
    std::vector<OnlineStats> thread_bg_stats;  // BG-only stats per thread
    std::vector<std::vector<double>> thread_batch_means;
    std::vector<Histogram> thread_histograms;
    std::vector<std::vector<double>> thread_top_values;
    std::vector<long long> thread_max_fg_lengths;
    std::vector<long long> thread_max_bg_multipliers;
    std::vector<long long> thread_max_fg_multipliers;
    std::vector<int> thread_max_bg_levels;
    std::vector<int> thread_max_fg_levels;
    std::vector<int> thread_max_run_levels;
    long long total_fg_runs_p = 0;
    long long total_fg_picks_p = 0;
    long long fg_triggered_count_p = 0;
    double total_bg_score_p = 0.0;
    double total_fg_score_p = 0.0;
    long long nonzero_bg_p = 0, nonzero_fg_sessions_p = 0, nonzero_fg_picks_p = 0, nonzero_total_p = 0;
    long long total_bg_levels_p = 0, bg_nonzero_levels_sum_p = 0, bg_nonzero_levels_count_p = 0;
    long long total_fg_levels_p = 0, fg_nonzero_levels_sum_p = 0, fg_nonzero_levels_count_p = 0;
    long long total_run_levels_p = 0, run_nonzero_levels_sum_p = 0, run_nonzero_levels_count_p = 0;
    std::atomic<long long> completed_batch_count{0};
    const long long progress_interval_batches = k > 100 ? k / 100 : 1;

    #pragma omp parallel
    {
        #pragma omp master
        {
            num_threads = omp_get_num_threads();
            std::cout << "[Monitor] Detected and using " << num_threads << " threads." << std::endl;
            std::cout << "[Monitor] Using dynamic batch scheduling for optimal load balancing." << std::endl;
            thread_stats.resize(num_threads);
            thread_bg_stats.resize(num_threads);  // BG-only stats per thread
            thread_batch_means.resize(num_threads);
            thread_histograms.resize(num_threads);
            thread_top_values.resize(num_threads);
            thread_max_fg_lengths.resize(num_threads, 0);
            thread_max_bg_multipliers.resize(num_threads, 1);
            thread_max_fg_multipliers.resize(num_threads, 1);
            thread_max_bg_levels.resize(num_threads, 0);
            thread_max_fg_levels.resize(num_threads, 0);
            thread_max_run_levels.resize(num_threads, 0);
            for(int i = 0; i < num_threads; ++i) {
                thread_histograms[i].bins.assign(m_histogram.dividers.size() - 1, 0);
            }
        }
        #pragma omp barrier
        int thread_id = omp_get_thread_num();
        std::mt19937 local_rng(m_rng());

        // BATCH-LEVEL PARALLELIZATION: Each thread processes complete batches
        #pragma omp for reduction(+:total_fg_runs_p, total_fg_picks_p, fg_triggered_count_p, total_bg_score_p, total_fg_score_p, nonzero_bg_p, nonzero_fg_sessions_p, nonzero_fg_picks_p, nonzero_total_p, total_bg_levels_p, bg_nonzero_levels_sum_p, bg_nonzero_levels_count_p, total_fg_levels_p, fg_nonzero_levels_sum_p, fg_nonzero_levels_count_p, total_run_levels_p, run_nonzero_levels_sum_p, run_nonzero_levels_count_p) schedule(dynamic)
        for (long long batch = 0; batch < k; ++batch) {
            OnlineStats batch_stats; // Fresh statistics for this batch

            // INNER LOOP: Process all rounds in this batch
            for (long long round = 0; round < m; ++round) {
                Game::GameResult result = Game::simulateGameRound(local_rng, sim_mode, second_chance_prob);
                double total_score = result.bg_score + result.fg_score;

                // UPDATE 1: Overall statistics (for all rounds across all batches)
                thread_stats[thread_id].update(total_score);
                thread_bg_stats[thread_id].update(result.bg_score);

                // UPDATE 2: Batch statistics (for this specific batch)
                batch_stats.update(total_score);

                // UPDATE 3: Top values tracking
                updateTopValues(thread_top_values[thread_id], total_score, 5);

                // UPDATE 4: BG/FG score contributions
                total_bg_score_p += result.bg_score;
                total_fg_score_p += result.fg_score;

                // UPDATE 5: Track nonzero frequencies
                if (result.bg_score != 0) nonzero_bg_p++;
                if (result.fg_score != 0) nonzero_fg_sessions_p++;  // Session-level tracking
                if (total_score != 0) nonzero_total_p++;
                nonzero_fg_picks_p += result.fg_nonzero_picks;  // Pick-level tracking

                // UPDATE 6: FG statistics (trigger count, run lengths)
                if (result.fg_was_triggered) {
                    total_fg_picks_p += result.fg_run_length;
                    fg_triggered_count_p++;
                    if (result.fg_run_length > 0) {
                        total_fg_runs_p++;
                        if(result.fg_run_length > thread_max_fg_lengths[thread_id]) {
                            thread_max_fg_lengths[thread_id] = result.fg_run_length;
                        }
                    }
                }

                // UPDATE 7: Track max multipliers
                if(result.max_bg_multiplier > thread_max_bg_multipliers[thread_id]) {
                    thread_max_bg_multipliers[thread_id] = result.max_bg_multiplier;
                }
                if(result.max_fg_multiplier > thread_max_fg_multipliers[thread_id]) {
                    thread_max_fg_multipliers[thread_id] = result.max_fg_multiplier;
                }

                // UPDATE 8: Track levels statistics
                // Category 1: BG levels
                total_bg_levels_p += result.bg_levels;
                if (result.bg_levels != 1) {
                    bg_nonzero_levels_sum_p += result.bg_levels;
                    bg_nonzero_levels_count_p++;
                }
                if (result.bg_levels > thread_max_bg_levels[thread_id]) {
                    thread_max_bg_levels[thread_id] = result.bg_levels;
                }

                // Category 2: FG picks
                int run_max_fg_level = 0;
                for (int fg_level : result.fg_levels) {
                    total_fg_levels_p += fg_level;
                    if (fg_level != 1) {
                        fg_nonzero_levels_sum_p += fg_level;
                        fg_nonzero_levels_count_p++;
                    }
                    if (fg_level > run_max_fg_level) run_max_fg_level = fg_level;
                }
                if (run_max_fg_level > thread_max_fg_levels[thread_id]) {
                    thread_max_fg_levels[thread_id] = run_max_fg_level;
                }

                // Category 3: Per run
                long long run_total_levels = result.bg_levels;
                long long run_nonzero_sum = (result.bg_levels != 1) ? result.bg_levels : 0;
                long long run_nonzero_count = (result.bg_levels != 1) ? 1 : 0;
                int run_max_level = result.bg_levels;

                for (int fg_level : result.fg_levels) {
                    run_total_levels += fg_level;
                    if (fg_level != 1) {
                        run_nonzero_sum += fg_level;
                        run_nonzero_count++;
                    }
                    if (fg_level > run_max_level) run_max_level = fg_level;
                }

                total_run_levels_p += run_total_levels;
                run_nonzero_levels_sum_p += run_nonzero_sum;
                run_nonzero_levels_count_p += run_nonzero_count;
                if (run_max_level > thread_max_run_levels[thread_id]) {
                    thread_max_run_levels[thread_id] = run_max_level;
                }

                // UPDATE 9: Histogram distribution tracking
                if (total_score < 0) {
                    thread_histograms[thread_id].underflow++;
                } else if (total_score >= m_histogram.dividers.back()) {
                    thread_histograms[thread_id].overflow++;
                } else {
                    auto it = std::upper_bound(m_histogram.dividers.begin(), m_histogram.dividers.end(), total_score);
                    int bin_index = std::distance(m_histogram.dividers.begin(), it) - 1;
                    thread_histograms[thread_id].bins[bin_index]++;
                }
            }

            // After completing all m rounds in this batch, store the batch mean
            thread_batch_means[thread_id].push_back(batch_stats.M1);

            // Progress reporting by batch
            long long batches_completed = completed_batch_count.fetch_add(1, std::memory_order_relaxed) + 1;
            if (batches_completed % progress_interval_batches == 0) {
                #pragma omp critical
                std::cout << "          ... Progress: Batch " << batches_completed << "/" << k
                          << " (" << std::fixed << std::setprecision(1)
                          << (100.0 * batches_completed / k) << "% complete)" << std::endl;
            }
        }
    }

    std::cout << "[Monitor] Combining results from all threads..." << std::endl;

    // Combine FG statistics
    m_fg_triggered_count = fg_triggered_count_p;
    m_total_fg_runs = total_fg_runs_p;
    m_total_fg_picks = total_fg_picks_p;
    m_total_bg_score = total_bg_score_p;
    m_total_fg_score = total_fg_score_p;
    m_nonzero_bg_count = nonzero_bg_p;
    m_nonzero_fg_sessions_count = nonzero_fg_sessions_p;
    m_nonzero_fg_picks_count = nonzero_fg_picks_p;
    m_nonzero_total_count = nonzero_total_p;
    for(long long max_val : thread_max_fg_lengths) {
        if(max_val > m_max_fg_length) {
            m_max_fg_length = max_val;
        }
    }
    for(long long max_val : thread_max_bg_multipliers) {
        if(max_val > m_max_bg_multiplier) {
            m_max_bg_multiplier = max_val;
        }
    }
    for(long long max_val : thread_max_fg_multipliers) {
        if(max_val > m_max_fg_multiplier) {
            m_max_fg_multiplier = max_val;
        }
    }

    // Aggregate levels statistics
    m_total_bg_levels = total_bg_levels_p;
    m_bg_nonzero_levels_sum = bg_nonzero_levels_sum_p;
    m_bg_nonzero_levels_count = bg_nonzero_levels_count_p;
    m_total_fg_levels = total_fg_levels_p;
    m_fg_nonzero_levels_sum = fg_nonzero_levels_sum_p;
    m_fg_nonzero_levels_count = fg_nonzero_levels_count_p;
    m_total_run_levels = total_run_levels_p;
    m_run_nonzero_levels_sum = run_nonzero_levels_sum_p;
    m_run_nonzero_levels_count = run_nonzero_levels_count_p;
    for(int max_val : thread_max_bg_levels) { if(max_val > m_max_bg_level) { m_max_bg_level = max_val; } }
    for(int max_val : thread_max_fg_levels) { if(max_val > m_max_fg_level) { m_max_fg_level = max_val; } }
    for(int max_val : thread_max_run_levels) { if(max_val > m_max_run_level) { m_max_run_level = max_val; } }

    // Combine overall statistics
    m_final_online_stats = thread_stats[0];
    m_final_bg_online_stats = thread_bg_stats[0];  // BG-only stats
    for (int i = 1; i < num_threads; ++i) {
        m_final_online_stats.combine(thread_stats[i]);
        m_final_bg_online_stats.combine(thread_bg_stats[i]);  // Combine BG stats
    }

    // Combine batch means
    m_batch_means.clear();
    for (int i = 0; i < num_threads; ++i) {
        m_batch_means.insert(m_batch_means.end(), thread_batch_means[i].begin(), thread_batch_means[i].end());
    }

    // Combine histograms
    m_histogram.bins.assign(m_histogram.dividers.size() - 1, 0);
    m_histogram.underflow = 0;
    m_histogram.overflow = 0;
    for (int i = 0; i < num_threads; ++i) {
        for(size_t j = 0; j < m_histogram.bins.size(); ++j) {
            m_histogram.bins[j] += thread_histograms[i].bins[j];
        }
        m_histogram.underflow += thread_histograms[i].underflow;
        m_histogram.overflow += thread_histograms[i].overflow;
    }

    // Combine and sort top values
    m_top_values_tracker.clear();
    for (const auto& vec : thread_top_values) {
        m_top_values_tracker.insert(m_top_values_tracker.end(), vec.begin(), vec.end());
    }
    std::sort(m_top_values_tracker.rbegin(), m_top_values_tracker.rend());
    m_top_values_tracker.resize(std::min((size_t)5, m_top_values_tracker.size()));

    auto end_sim_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> sim_elapsed = end_sim_time - start_sim_time;
    std::cout << "[Monitor] Simulation loop finished in " << sim_elapsed.count() << " seconds." << std::endl;
    analyzeEfficientResults(k);
}


// Fallback Implementation without CI
void MonteCarloSimulator::runAccurateMode_Parallel(long long numSimulations, Game::SimulationMode sim_mode, double second_chance_prob) {
    std::cout << "[Monitor] Starting parallel simulation in ACCURATE memory mode." << std::endl;
    auto start_sim_time = std::chrono::high_resolution_clock::now();
    m_results.assign(numSimulations, 0.0);
    std::vector<double> bg_scores(numSimulations, 0.0);
    std::vector<double> fg_scores(numSimulations, 0.0);
    std::vector<long long> fg_run_lengths(numSimulations, 0);
    std::vector<bool> fg_triggered(numSimulations, false);
    std::vector<long long> fg_nonzero_picks(numSimulations, 0);
    std::vector<long long> bg_multipliers(numSimulations, 1);
    std::vector<long long> fg_multipliers(numSimulations, 1);

    // Levels tracking - optimized storage (only essential per-round data)
    std::vector<int> bg_levels(numSimulations, 0);
    std::vector<long long> total_fg_levels_per_round(numSimulations, 0);
    std::vector<long long> fg_nonzero_levels_count_per_round(numSimulations, 0);

    // Thread-local max tracking (not per-round to save memory)
    int num_threads = 0;
    std::vector<int> thread_max_bg_levels;
    std::vector<int> thread_max_fg_levels;
    std::vector<int> thread_max_run_levels;

    std::atomic<long long> completed_count = 0;
    const long long progress_interval = numSimulations > 20 ? numSimulations / 20 : 1;

    #pragma omp parallel
    {
        std::mt19937 local_rng(m_rng());
        #pragma omp master
        {
            num_threads = omp_get_num_threads();
            std::cout << "[Monitor] Detected and using " << num_threads << " threads." << std::endl;
            thread_max_bg_levels.resize(num_threads, 0);
            thread_max_fg_levels.resize(num_threads, 0);
            thread_max_run_levels.resize(num_threads, 0);
        }
        #pragma omp barrier
        int thread_id = omp_get_thread_num();

        #pragma omp for schedule(dynamic)
        for (long long i = 0; i < numSimulations; ++i) {
            Game::GameResult result = Game::simulateGameRound(local_rng, sim_mode, second_chance_prob);
            m_results[i] = result.bg_score + result.fg_score;
            bg_scores[i] = result.bg_score;
            fg_scores[i] = result.fg_score;
            fg_run_lengths[i] = result.fg_run_length;
            fg_triggered[i] = result.fg_was_triggered;
            fg_nonzero_picks[i] = result.fg_nonzero_picks;
            bg_multipliers[i] = result.max_bg_multiplier;
            fg_multipliers[i] = result.max_fg_multiplier;

            // Track levels statistics
            bg_levels[i] = result.bg_levels;

            // Update thread-local max for BG
            if (result.bg_levels > thread_max_bg_levels[thread_id]) {
                thread_max_bg_levels[thread_id] = result.bg_levels;
            }

            // Category 2: FG picks - compute per-round aggregates
            int run_max_fg_level = 0;
            long long run_total_fg_levels = 0;
            long long run_fg_nonzero_count = 0;
            for (int fg_level : result.fg_levels) {
                run_total_fg_levels += fg_level;
                if (fg_level != 1) run_fg_nonzero_count++;
                if (fg_level > run_max_fg_level) run_max_fg_level = fg_level;
            }
            total_fg_levels_per_round[i] = run_total_fg_levels;
            fg_nonzero_levels_count_per_round[i] = run_fg_nonzero_count;

            // Update thread-local max for FG and Per Run
            if (run_max_fg_level > thread_max_fg_levels[thread_id]) {
                thread_max_fg_levels[thread_id] = run_max_fg_level;
            }
            int run_max_level = std::max(result.bg_levels, run_max_fg_level);
            if (run_max_level > thread_max_run_levels[thread_id]) {
                thread_max_run_levels[thread_id] = run_max_level;
            }

            long long current_completed = completed_count.fetch_add(1, std::memory_order_relaxed) + 1;
            if (current_completed % progress_interval == 0) {
                 #pragma omp critical
                 std::cout << "          ... Progress: " << (100 * current_completed / numSimulations) << "% complete." << std::endl;
            }
        }
    }

    for (double score : bg_scores) { m_total_bg_score = m_total_bg_score.load() + score; }
    for (double score : fg_scores) { m_total_fg_score = m_total_fg_score.load() + score; }

    // Count total FG picks
    for (long long length : fg_run_lengths) {
        m_total_fg_picks += length;
        if (length > 0) {
            m_total_fg_runs++;
            if (length > m_max_fg_length) {
                m_max_fg_length = length;
            }
        }
    }

    for (bool triggered : fg_triggered) {
        if (triggered) {
            m_fg_triggered_count++;
        }
    }

    // Count nonzero frequencies
    for (size_t i = 0; i < numSimulations; ++i) {
        if (bg_scores[i] != 0) m_nonzero_bg_count++;
        if (fg_scores[i] != 0) m_nonzero_fg_sessions_count++;  // Session-level tracking
        if (m_results[i] != 0) m_nonzero_total_count++;
        m_nonzero_fg_picks_count += fg_nonzero_picks[i];  // Pick-level tracking
    }

    // Aggregate max multipliers
    for (long long val : bg_multipliers) { if (val > m_max_bg_multiplier) m_max_bg_multiplier = val; }
    for (long long val : fg_multipliers) { if (val > m_max_fg_multiplier) m_max_fg_multiplier = val; }

    // Aggregate levels statistics
    // Category 1: BG levels
    for (int level : bg_levels) {
        m_total_bg_levels += level;
        if (level != 1) {
            m_bg_nonzero_levels_sum += level;
            m_bg_nonzero_levels_count++;
        }
    }
    // Aggregate thread-local max for BG
    for (int max_val : thread_max_bg_levels) {
        if (max_val > m_max_bg_level) m_max_bg_level = max_val;
    }

    // Category 2: FG picks
    for (size_t i = 0; i < numSimulations; ++i) {
        m_total_fg_levels += total_fg_levels_per_round[i];
        m_fg_nonzero_levels_count += fg_nonzero_levels_count_per_round[i];
    }
    // Aggregate thread-local max for FG
    for (int max_val : thread_max_fg_levels) {
        if (max_val > m_max_fg_level) m_max_fg_level = max_val;
    }
    // Derive fg_nonzero_levels_sum: total - (count_of_ones × 1)
    long long total_fg_picks_val = m_total_fg_picks.load();
    long long fg_ones_sum = total_fg_picks_val - m_fg_nonzero_levels_count.load();
    m_fg_nonzero_levels_sum = m_total_fg_levels.load() - fg_ones_sum;

    // Category 3: Per run - derive from Categories 1 & 2
    for (size_t i = 0; i < numSimulations; ++i) {
        // Derive total run levels: bg_levels + total_fg_levels
        long long run_total = bg_levels[i] + total_fg_levels_per_round[i];
        m_total_run_levels += run_total;

        // Derive run nonzero count: (bg != 1 ? 1 : 0) + fg_nonzero_count
        long long run_nonzero_count = (bg_levels[i] != 1 ? 1 : 0) + fg_nonzero_levels_count_per_round[i];
        m_run_nonzero_levels_count += run_nonzero_count;
    }
    // Aggregate thread-local max for Per Run
    for (int max_val : thread_max_run_levels) {
        if (max_val > m_max_run_level) m_max_run_level = max_val;
    }
    // Derive run_nonzero_levels_sum: total - (count_of_ones × 1)
    long long total_items = m_stats.count + total_fg_picks_val;
    long long run_ones_sum = total_items - m_run_nonzero_levels_count.load();
    m_run_nonzero_levels_sum = m_total_run_levels.load() - run_ones_sum;

    auto end_sim_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> sim_elapsed = end_sim_time - start_sim_time;
    std::cout << "[Monitor] Simulation loop finished in " << sim_elapsed.count() << " seconds." << std::endl;
    analyzeAccurateResults();
}

// New implementation with bootstrapping for CI
void MonteCarloSimulator::runAccurateMode_Parallel(long long k, long long m, Game::SimulationMode sim_mode, double second_chance_prob) {
    std::cout << "[Monitor] Starting parallel simulation in ACCURATE memory mode with batch-level parallelization." << std::endl;
    std::cout << "[Monitor] Configuration: " << k << " batches " << m << " rounds/batch = " << (k * m) << " total rounds" << std::endl;
    auto start_sim_time = std::chrono::high_resolution_clock::now();

    long long numSimulations = k * m;
    m_results.assign(numSimulations, 0.0);
    std::vector<double> bg_scores(numSimulations, 0.0);
    std::vector<double> fg_scores(numSimulations, 0.0);
    std::vector<long long> fg_run_lengths(numSimulations, 0);
    std::vector<bool> fg_triggered(numSimulations, false);
    std::vector<long long> fg_nonzero_picks(numSimulations, 0);
    std::vector<long long> bg_multipliers(numSimulations, 1);
    std::vector<long long> fg_multipliers(numSimulations, 1);

    // Levels tracking - optimized storage (only essential per-round data)
    std::vector<int> bg_levels(numSimulations, 0);
    std::vector<long long> total_fg_levels_per_round(numSimulations, 0);
    std::vector<long long> fg_nonzero_levels_count_per_round(numSimulations, 0);

    // Thread-local max tracking (not per-round to save memory)
    int num_threads = 0;
    std::vector<int> thread_max_bg_levels;
    std::vector<int> thread_max_fg_levels;
    std::vector<int> thread_max_run_levels;

    std::atomic<long long> completed_batch_count{0};
    const long long progress_interval_batches = k > 100 ? k / 100 : 1;

    #pragma omp parallel
    {
        std::mt19937 local_rng(m_rng());
        #pragma omp master
        {
            num_threads = omp_get_num_threads();
            std::cout << "[Monitor] Detected and using " << num_threads << " threads." << std::endl;
            std::cout << "[Monitor] Using dynamic batch scheduling for optimal load balancing." << std::endl;
            thread_max_bg_levels.resize(num_threads, 0);
            thread_max_fg_levels.resize(num_threads, 0);
            thread_max_run_levels.resize(num_threads, 0);
        }
        #pragma omp barrier
        int thread_id = omp_get_thread_num();

        // BATCH-LEVEL PARALLELIZATION: Each thread processes complete batches
        #pragma omp for schedule(dynamic)
        for (long long batch = 0; batch < k; ++batch) {
            // INNER LOOP: Process all rounds in this batch
            for (long long round = 0; round < m; ++round) {
                long long idx = batch * m + round; // Calculate global index

                Game::GameResult result = Game::simulateGameRound(local_rng, sim_mode, second_chance_prob);
                m_results[idx] = result.bg_score + result.fg_score;
                bg_scores[idx] = result.bg_score;
                fg_scores[idx] = result.fg_score;
                fg_run_lengths[idx] = result.fg_run_length;
                fg_triggered[idx] = result.fg_was_triggered;
                fg_nonzero_picks[idx] = result.fg_nonzero_picks;
                bg_multipliers[idx] = result.max_bg_multiplier;
                fg_multipliers[idx] = result.max_fg_multiplier;

                // Track levels statistics
                bg_levels[idx] = result.bg_levels;

                // Update thread-local max for BG
                if (result.bg_levels > thread_max_bg_levels[thread_id]) {
                    thread_max_bg_levels[thread_id] = result.bg_levels;
                }

                // Category 2: FG picks - compute per-round aggregates
                int run_max_fg_level = 0;
                long long run_total_fg_levels = 0;
                long long run_fg_nonzero_count = 0;
                for (int fg_level : result.fg_levels) {
                    run_total_fg_levels += fg_level;
                    if (fg_level != 1) run_fg_nonzero_count++;
                    if (fg_level > run_max_fg_level) run_max_fg_level = fg_level;
                }
                total_fg_levels_per_round[idx] = run_total_fg_levels;
                fg_nonzero_levels_count_per_round[idx] = run_fg_nonzero_count;

                // Update thread-local max for FG and Per Run
                if (run_max_fg_level > thread_max_fg_levels[thread_id]) {
                    thread_max_fg_levels[thread_id] = run_max_fg_level;
                }
                int run_max_level = std::max(result.bg_levels, run_max_fg_level);
                if (run_max_level > thread_max_run_levels[thread_id]) {
                    thread_max_run_levels[thread_id] = run_max_level;
                }
            }

            // Progress reporting by batch
            long long batches_completed = completed_batch_count.fetch_add(1, std::memory_order_relaxed) + 1;
            if (batches_completed % progress_interval_batches == 0) {
                #pragma omp critical
                std::cout << "          ... Progress: Batch " << batches_completed << "/" << k
                          << " (" << std::fixed << std::setprecision(1)
                          << (100.0 * batches_completed / k) << "% complete)" << std::endl;
            }
        }
    }

    // Aggregate statistics from individual results
    for (double score : bg_scores) { m_total_bg_score = m_total_bg_score.load() + score; }
    for (double score : fg_scores) { m_total_fg_score = m_total_fg_score.load() + score; }

    // Count total FG picks
    for (long long length : fg_run_lengths) {
        m_total_fg_picks += length;
        if (length > 0) {
            m_total_fg_runs++;
            if (length > m_max_fg_length) {
                m_max_fg_length = length;
            }
        }
    }

    for (bool triggered : fg_triggered) {
        if (triggered) {
            m_fg_triggered_count++;
        }
    }

    // Count nonzero frequencies
    for (size_t i = 0; i < numSimulations; ++i) {
        if (bg_scores[i] != 0) m_nonzero_bg_count++;
        if (fg_scores[i] != 0) m_nonzero_fg_sessions_count++;  // Session-level tracking
        if (m_results[i] != 0) m_nonzero_total_count++;
        m_nonzero_fg_picks_count += fg_nonzero_picks[i];  // Pick-level tracking
    }

    // Aggregate max multipliers
    for (long long val : bg_multipliers) { if (val > m_max_bg_multiplier) m_max_bg_multiplier = val; }
    for (long long val : fg_multipliers) { if (val > m_max_fg_multiplier) m_max_fg_multiplier = val; }

    // Aggregate levels statistics
    // Category 1: BG levels
    for (int level : bg_levels) {
        m_total_bg_levels += level;
        if (level != 1) {
            m_bg_nonzero_levels_sum += level;
            m_bg_nonzero_levels_count++;
        }
    }
    // Aggregate thread-local max for BG
    for (int max_val : thread_max_bg_levels) {
        if (max_val > m_max_bg_level) m_max_bg_level = max_val;
    }

    // Category 2: FG picks
    for (size_t i = 0; i < numSimulations; ++i) {
        m_total_fg_levels += total_fg_levels_per_round[i];
        m_fg_nonzero_levels_count += fg_nonzero_levels_count_per_round[i];
    }
    // Aggregate thread-local max for FG
    for (int max_val : thread_max_fg_levels) {
        if (max_val > m_max_fg_level) m_max_fg_level = max_val;
    }
    // Derive fg_nonzero_levels_sum: total - (count_of_ones × 1)
    long long total_fg_picks_val = m_total_fg_picks.load();
    long long fg_ones_sum = total_fg_picks_val - m_fg_nonzero_levels_count.load();
    m_fg_nonzero_levels_sum = m_total_fg_levels.load() - fg_ones_sum;

    // Category 3: Per run - derive from Categories 1 & 2
    for (size_t i = 0; i < numSimulations; ++i) {
        // Derive total run levels: bg_levels + total_fg_levels
        long long run_total = bg_levels[i] + total_fg_levels_per_round[i];
        m_total_run_levels += run_total;

        // Derive run nonzero count: (bg != 1 ? 1 : 0) + fg_nonzero_count
        long long run_nonzero_count = (bg_levels[i] != 1 ? 1 : 0) + fg_nonzero_levels_count_per_round[i];
        m_run_nonzero_levels_count += run_nonzero_count;
    }
    // Aggregate thread-local max for Per Run
    for (int max_val : thread_max_run_levels) {
        if (max_val > m_max_run_level) m_max_run_level = max_val;
    }
    // Derive run_nonzero_levels_sum: total - (count_of_ones × 1)
    long long total_items = m_stats.count + total_fg_picks_val;
    long long run_ones_sum = total_items - m_run_nonzero_levels_count.load();
    m_run_nonzero_levels_sum = m_total_run_levels.load() - run_ones_sum;

    auto end_sim_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> sim_elapsed = end_sim_time - start_sim_time;
    std::cout << "[Monitor] Simulation loop finished in " << sim_elapsed.count() << " seconds." << std::endl;
    analyzeAccurateResults(k, m);
}



void MonteCarloSimulator::analyzeEfficientResults() {
    std::cout << "\n[Monitor] Starting detailed analysis from online statistics..." << std::endl;
    auto start_analysis_time = std::chrono::high_resolution_clock::now();
    m_stats.count = m_final_online_stats.count;
    if (m_stats.count == 0) { std::cerr << "Analysis failed: No results to analyze." << std::endl; return; }
    m_stats.mean = m_final_online_stats.M1;
    m_stats.variance = m_stats.count > 1 ? m_final_online_stats.M2 / (m_stats.count -1) : 0.0;
    m_stats.stdDev = std::sqrt(m_stats.variance);
    // Calculate BG-only standard deviation
    double bg_variance = m_stats.count > 1 ? m_final_bg_online_stats.M2 / (m_stats.count - 1) : 0.0;
    m_stats.bg_stdDev = std::sqrt(bg_variance);
    m_stats.skewness = (m_stats.count > 2 && m_final_online_stats.M2 > 0) ? (std::sqrt(m_stats.count) * m_final_online_stats.M3) / std::pow(m_final_online_stats.M2, 1.5) : 0.0;
    m_stats.kurtosis = (m_stats.count > 3 && m_final_online_stats.M2 > 0) ? (m_stats.count * m_final_online_stats.M4) / (m_final_online_stats.M2 * m_final_online_stats.M2) - 3.0 : 0.0;
    std::cout << "[Analysis] Calculated Mean, StdDev, BG StdDev, Skewness, Kurtosis... Done." << std::endl;
    std::cout << "[Analysis] Calculating Percentiles from histogram... ";
    m_stats.p95 = getPercentileFromHistogram(95.0);
    m_stats.p99 = getPercentileFromHistogram(99.0);
    m_stats.top_values = m_top_values_tracker;
    std::cout << "Done." << std::endl;
    auto end_analysis_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> analysis_elapsed = end_analysis_time - start_analysis_time;
    std::cout << "[Monitor] Full analysis complete in " << analysis_elapsed.count() << " seconds." << std::endl;
}

// --- HIGHLIGHT: New analysis function for Efficient Mode CI ---
void MonteCarloSimulator::analyzeEfficientResults(long long k) {
    // ... (unchanged analysis of overall mean, variance, etc. from m_final_online_stats) ...
    std::cout << "\n[Monitor] Starting detailed analysis from online statistics..." << std::endl;
    auto start_analysis_time = std::chrono::high_resolution_clock::now();
    m_stats.count = m_final_online_stats.count;
    if (m_stats.count == 0) { std::cerr << "Analysis failed: No results to analyze." << std::endl; return; }
    m_stats.mean = m_final_online_stats.M1;
    m_stats.variance = m_stats.count > 1 ? m_final_online_stats.M2 / (m_stats.count -1) : 0.0;
    m_stats.stdDev = std::sqrt(m_stats.variance);
    // Calculate BG-only standard deviation
    double bg_variance = m_stats.count > 1 ? m_final_bg_online_stats.M2 / (m_stats.count - 1) : 0.0;
    m_stats.bg_stdDev = std::sqrt(bg_variance);
    m_stats.skewness = (m_stats.count > 2 && m_final_online_stats.M2 > 0) ? (std::sqrt(m_stats.count) * m_final_online_stats.M3) / std::pow(m_final_online_stats.M2, 1.5) : 0.0;
    m_stats.kurtosis = (m_stats.count > 3 && m_final_online_stats.M2 > 0) ? (m_stats.count * m_final_online_stats.M4) / (m_final_online_stats.M2 * m_final_online_stats.M2) - 3.0 : 0.0;
    std::cout << "[Analysis] Calculated Mean, StdDev, BG StdDev, Skewness, Kurtosis... Done." << std::endl;
    std::cout << "[Analysis] Calculating Percentiles from histogram... ";
    m_stats.p95 = getPercentileFromHistogram(95.0);
    m_stats.p99 = getPercentileFromHistogram(99.0);
    m_stats.top_values = m_top_values_tracker;
    std::cout << "Done." << std::endl;

    // --- Validate batch count and calculate CI using Method of Batched Means ---
    std::cout << "[Analysis] Calculating confidence intervals from " << m_batch_means.size() << " batch means..." << std::endl;

    // Validation: Check if we have the expected number of batches
    if (m_batch_means.size() != static_cast<size_t>(k)) {
        std::cout << "[Warning] Expected " << k << " batches, but collected "
                  << m_batch_means.size() << " batch means." << std::endl;
        std::cout << "[Warning] This indicates incomplete batches. Confidence intervals may be inaccurate." << std::endl;
    }

    if (m_batch_means.size() < 2) {
        std::cout << "[Warning] Not enough batches to compute a confidence interval (need at least 2)." << std::endl;
        return;
    }

    double mean_of_means = Statistics::calculateMean(m_batch_means);
    double variance_of_means = Statistics::calculateVariance(m_batch_means, mean_of_means);
    double std_error = std::sqrt(variance_of_means / m_batch_means.size());
    int df = m_batch_means.size() - 1;

    m_stats.confidence_intervals.clear();
    double t90 = Statistics::findTValue(90.0, df);
    m_stats.confidence_intervals.push_back({90.0, mean_of_means - t90 * std_error, mean_of_means + t90 * std_error});
    double t95 = Statistics::findTValue(95.0, df);
    m_stats.confidence_intervals.push_back({95.0, mean_of_means - t95 * std_error, mean_of_means + t95 * std_error});
    double t99 = Statistics::findTValue(99.0, df);
    m_stats.confidence_intervals.push_back({99.0, mean_of_means - t99 * std_error, mean_of_means + t99 * std_error});


    auto end_analysis_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> analysis_elapsed = end_analysis_time - start_analysis_time;
    std::cout << "[Monitor] Full analysis complete in " << analysis_elapsed.count() << " seconds." << std::endl;
}

void MonteCarloSimulator::analyzeAccurateResults() {
    std::cout << "\n[Monitor] Starting detailed analysis from stored data..." << std::endl;
    if (m_results.empty()) { std::cerr << "Analysis failed: No results to analyze." << std::endl; return; }
    auto start_analysis_time = std::chrono::high_resolution_clock::now();
    m_stats.count = m_results.size();
    std::cout << "[Analysis] Calculating Mean... "; m_stats.mean = Statistics::calculateMean(m_results); std::cout << "Done." << std::endl;
    std::cout << "[Analysis] Calculating Variance... "; m_stats.variance = Statistics::calculateVariance(m_results, m_stats.mean); std::cout << "Done." << std::endl;
    m_stats.stdDev = Statistics::calculateStdDev(m_stats.variance);
    std::cout << "[Analysis] Calculating Skewness... "; m_stats.skewness = Statistics::calculateSkewness(m_results, m_stats.mean, m_stats.stdDev); std::cout << "Done." << std::endl;
    std::cout << "[Analysis] Calculating Kurtosis... "; m_stats.kurtosis = Statistics::calculateKurtosis(m_results, m_stats.mean, m_stats.stdDev); std::cout << "Done." << std::endl;
    std::cout << "[Analysis] Sorting " << m_stats.count << " results for percentile and binning calculations..." << std::endl;
    auto sort_start = std::chrono::high_resolution_clock::now();
    std::sort(m_results.begin(), m_results.end());
    std::chrono::duration<double> sort_elapsed = std::chrono::high_resolution_clock::now() - sort_start;
    std::cout << "[Analysis] Sorting complete in " << sort_elapsed.count() << " seconds." << std::endl;
    std::cout << "[Analysis] Calculating Percentiles... ";
    m_stats.p95 = Statistics::findValueAtPercentile(m_results, 95.0);
    m_stats.p99 = Statistics::findValueAtPercentile(m_results, 99.0);
    std::cout << "Done." << std::endl;
    std::cout << "[Analysis] Extracting top values... ";
    m_stats.top_values.clear();
    for(size_t i = 0; i < 5 && i < m_results.size(); ++i) { m_stats.top_values.push_back(m_results[m_results.size() - 1 - i]); }
    std::cout << "Done." << std::endl;
    std::cout << "[Analysis] Grouping results into histogram bins... ";
    m_histogram.bins.assign(m_histogram.dividers.size() - 1, 0);
    m_histogram.underflow = 0;
    m_histogram.overflow = 0;
    for(const double result : m_results) {
        if (result < 0) { m_histogram.underflow++; }
        else if (result >= m_histogram.dividers.back()) { m_histogram.overflow++; }
        else {
            auto it = std::upper_bound(m_histogram.dividers.begin(), m_histogram.dividers.end(), result);
            int bin_index = std::distance(m_histogram.dividers.begin(), it) - 1;
            m_histogram.bins[bin_index]++;
        }
    }
    std::cout << "Done." << std::endl;
    auto end_analysis_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> analysis_elapsed = end_analysis_time - start_analysis_time;
    std::cout << "[Monitor] Full analysis complete in " << analysis_elapsed.count() << " seconds." << std::endl;
}

// --- Accurate mode analysis now includes parallel bootstrapping ---
void MonteCarloSimulator::analyzeAccurateResults(long long k, long long m) {
    // ... (unchanged analysis of overall mean, variance, etc. for m_results) ...
    std::cout << "\n[Monitor] Starting detailed analysis from stored data..." << std::endl;
    if (m_results.empty()) { std::cerr << "Analysis failed: No results to analyze." << std::endl; return; }
    auto start_analysis_time = std::chrono::high_resolution_clock::now();
    m_stats.count = m_results.size();
    std::cout << "[Analysis] Calculating Mean... "; m_stats.mean = Statistics::calculateMean(m_results); std::cout << "Done." << std::endl;
    std::cout << "[Analysis] Calculating Variance... "; m_stats.variance = Statistics::calculateVariance(m_results, m_stats.mean); std::cout << "Done." << std::endl;
    m_stats.stdDev = Statistics::calculateStdDev(m_stats.variance);
    std::cout << "[Analysis] Calculating Skewness... "; m_stats.skewness = Statistics::calculateSkewness(m_results, m_stats.mean, m_stats.stdDev); std::cout << "Done." << std::endl;
    std::cout << "[Analysis] Calculating Kurtosis... "; m_stats.kurtosis = Statistics::calculateKurtosis(m_results, m_stats.mean, m_stats.stdDev); std::cout << "Done." << std::endl;
    std::cout << "[Analysis] Sorting " << m_stats.count << " results for percentile and binning calculations..." << std::endl;
    auto sort_start = std::chrono::high_resolution_clock::now();
    std::sort(m_results.begin(), m_results.end());
    std::chrono::duration<double> sort_elapsed = std::chrono::high_resolution_clock::now() - sort_start;
    std::cout << "[Analysis] Sorting complete in " << sort_elapsed.count() << " seconds." << std::endl;
    std::cout << "[Analysis] Calculating Percentiles... ";
    m_stats.p95 = Statistics::findValueAtPercentile(m_results, 95.0);
    m_stats.p99 = Statistics::findValueAtPercentile(m_results, 99.0);
    std::cout << "Done." << std::endl;
    std::cout << "[Analysis] Extracting top values... ";
    m_stats.top_values.clear();
    for(size_t i = 0; i < 5 && i < m_results.size(); ++i) { m_stats.top_values.push_back(m_results[m_results.size() - 1 - i]); }
    std::cout << "Done." << std::endl;
    std::cout << "[Analysis] Grouping results into histogram bins... ";
    m_histogram.bins.assign(m_histogram.dividers.size() - 1, 0);
    m_histogram.underflow = 0;
    m_histogram.overflow = 0;
    for(const double result : m_results) {
        if (result < 0) { m_histogram.underflow++; }
        else if (result >= m_histogram.dividers.back()) { m_histogram.overflow++; }
        else {
            auto it = std::upper_bound(m_histogram.dividers.begin(), m_histogram.dividers.end(), result);
            int bin_index = std::distance(m_histogram.dividers.begin(), it) - 1;
            m_histogram.bins[bin_index]++;
        }
    }
    std::cout << "Done." << std::endl;

    // --- HIGHLIGHT: New Bootstrap Resampling Section ---
    std::cout << "[Analysis] Starting bootstrap resampling (" << k << " samples of size " << m << ")..." << std::endl;
    auto bootstrap_start_time = std::chrono::high_resolution_clock::now();
    m_bootstrap_means.assign(k, 0.0);

    #pragma omp parallel
    {
        std::mt19937 local_rng(m_rng()); // Each thread gets its own RNG
        std::uniform_int_distribution<long long> dist(0, m_results.size() - 1);

        #pragma omp for
        for (long long i = 0; i < k; ++i) {
            double current_sum = 0.0;
            for (long long j = 0; j < m; ++j) {
                // Draw a random index with replacement
                long long random_index = dist(local_rng);
                current_sum += m_results[random_index];
            }
            m_bootstrap_means[i] = current_sum / m;
        }
    }
    auto bootstrap_end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> bootstrap_elapsed = bootstrap_end_time - bootstrap_start_time;
    std::cout << "[Analysis] Bootstrap resampling complete in " << bootstrap_elapsed.count() << " seconds." << std::endl;

    // --- HIGHLIGHT: Calculate CI from bootstrap percentiles ---
    std::cout << "[Analysis] Calculating confidence intervals from bootstrap results..." << std::endl;
    std::sort(m_bootstrap_means.begin(), m_bootstrap_means.end());
    m_stats.confidence_intervals.clear();
    m_stats.confidence_intervals.push_back({90.0, Statistics::findValueAtPercentile(m_bootstrap_means, 5.0), Statistics::findValueAtPercentile(m_bootstrap_means, 95.0)});
    m_stats.confidence_intervals.push_back({95.0, Statistics::findValueAtPercentile(m_bootstrap_means, 2.5), Statistics::findValueAtPercentile(m_bootstrap_means, 97.5)});
    m_stats.confidence_intervals.push_back({99.0, Statistics::findValueAtPercentile(m_bootstrap_means, 0.5), Statistics::findValueAtPercentile(m_bootstrap_means, 99.5)});
    
    auto end_analysis_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> analysis_elapsed = end_analysis_time - start_analysis_time;
    std::cout << "[Monitor] Full analysis complete in " << analysis_elapsed.count() << " seconds." << std::endl;
}

double MonteCarloSimulator::getPercentileFromHistogram(double percentile) const {
    if (m_stats.count == 0) return 0.0;
    long long target_count = m_stats.count * (percentile / 100.0);
    long long current_count = m_histogram.underflow;
    if (target_count <= current_count) { return m_histogram.dividers.front(); }
    for (size_t i = 0; i < m_histogram.bins.size(); ++i) {
        current_count += m_histogram.bins[i];
        if (current_count >= target_count) {
            long long prev_count = current_count - m_histogram.bins[i];
            double fraction = (m_histogram.bins[i] > 0) ? (double)(target_count - prev_count) / m_histogram.bins[i] : 0.0;
            return m_histogram.dividers[i] + fraction * (m_histogram.dividers[i+1] - m_histogram.dividers[i]);
        }
    }
    return m_histogram.dividers.back();
}

void MonteCarloSimulator::printResults(int base_bet) const {
    std::cout << "\n------ Monte Carlo Simulation Results ------" << std::endl;
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Simulations Run:   " << m_stats.count << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "Mean:              " << m_stats.mean << std::endl;
    std::cout << "Standard Deviation:" << m_stats.stdDev << std::endl;
    std::cout << "Skewness:          " << m_stats.skewness << std::endl;
    std::cout << "Kurtosis:          " << m_stats.kurtosis << std::endl;
    std::cout << "RTP:               " << std::fixed << std::setprecision(4) << m_stats.mean/base_bet*100 << "% "<< std::endl;
    std::cout << "RTP Std:           " << m_stats.stdDev/base_bet << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "95th Percentile:   " << m_stats.p95 << " (approx. if efficient mode)" << std::endl;
    std::cout << "99th Percentile:   " << m_stats.p99 << " (approx. if efficient mode)" << std::endl;
    if(!m_stats.top_values.empty()){
        std::cout << "\nTop 5 Largest Values:" << std::endl;
        for(size_t i = 0; i < m_stats.top_values.size(); ++i) {
            std::cout << "  " << i+1 << ". " << m_stats.top_values[i] << std::endl;
        }
    }
    
    std::cout << "\n------ Score Contribution Analysis ------" << std::endl;
    double avg_bg_contrib = (m_stats.count > 0) ? m_total_bg_score.load() / m_stats.count : 0.0;
    double avg_fg_contrib = (m_stats.count > 0) ? m_total_fg_score.load() / m_stats.count : 0.0;
    long long total_fg_picks = m_total_fg_picks.load();
    long long total_runs_with_fg = m_total_fg_runs.load();
    double avg_length = (total_runs_with_fg > 0) ? static_cast<double>(total_fg_picks) / total_runs_with_fg : 0.0;
    long long fg_triggers = m_fg_triggered_count.load();
    double trigger_rate = (m_stats.count > 0) ? 100.0 * static_cast<double>(fg_triggers) / m_stats.count : 0.0;
    std::cout << "Avg. BG Score Contribution: " << avg_bg_contrib << std::endl;
    std::cout << "BG Standard Deviation: " << std::fixed << std::setprecision(6) << m_stats.bg_stdDev << std::endl;
    std::cout << "Avg. BG RTP: " << std::fixed << std::setprecision(4) << avg_bg_contrib/(base_bet)*100 << "% " << std::endl;
    std::cout << "BG RTP Std:            " << std::fixed << std::setprecision(6) << m_stats.bg_stdDev/base_bet << std::endl;
    std::cout << "BG RTP Contribution %: " << std::fixed << std::setprecision(4) << avg_bg_contrib/(avg_bg_contrib+avg_fg_contrib)*100 << "% "  << std::endl;
    std::cout << "Avg. FG Score Contribution: " << avg_fg_contrib << std::endl;
    std::cout << "Avg. FG RTP: " << std::fixed << std::setprecision(4) << avg_fg_contrib/(base_bet)*100 << "% " << std::endl;
    std::cout << "Avg. Raw Per Round FG RTP: " << std::fixed << std::setprecision(4) << avg_fg_contrib/(base_bet)/avg_length/trigger_rate*10000 << "% " << std::endl;
    std::cout << "FG RTP Contribution %: " << std::fixed << std::setprecision(4) << avg_fg_contrib/(avg_bg_contrib+avg_fg_contrib)*100 << "% "  << std::endl;

    std::cout << "\n------ FG Trigger and Run Length Statistics ------" << std::endl;

    std::cout << "FG Triggered Count:   " << fg_triggers << " (" << std::fixed << std::setprecision(4) << trigger_rate << "% of rounds)" << std::endl;

    std::cout << "Total FG Picks:       " << total_fg_picks << " (across all FG sessions)" << std::endl;

    std::cout << "Avg. FG Run Length:   " << std::fixed << std::setprecision(4) << avg_length << " (for sessions with FG)" << std::endl;
    std::cout << "Max FG Run Length:    " << m_max_fg_length.load() << std::endl;

    std::cout << "\n------ Maximum Multipliers Observed ------" << std::endl;
    std::cout << "Max BG Multiplier:    " << m_max_bg_multiplier.load() << std::endl;
    std::cout << "Max FG Multiplier:    " << m_max_fg_multiplier.load() << std::endl;

    std::cout << "\n------ Nonzero Value Frequencies ------" << std::endl;
    long long nonzero_bg = m_nonzero_bg_count.load();
    long long nonzero_total = m_nonzero_total_count.load();

    double bg_nonzero_rate = (m_stats.count > 0) ? 100.0 * static_cast<double>(nonzero_bg) / m_stats.count : 0.0;
    double total_nonzero_rate = (m_stats.count > 0) ? 100.0 * static_cast<double>(nonzero_total) / m_stats.count : 0.0;

    std::cout << "BG Nonzero:    " << nonzero_bg << " / " << m_stats.count << " rounds (" << std::fixed << std::setprecision(4) << bg_nonzero_rate << "%)" << std::endl;
    std::cout << "Total Nonzero: " << nonzero_total << " / " << m_stats.count << " rounds (" << std::fixed << std::setprecision(4) << total_nonzero_rate << "%)" << std::endl;

    // --- FG Nonzero Frequencies (Dual-Level Tracking) ---
    // Session-level: Tracks how many FG sessions had non-zero total payout
    // Pick-level: Tracks how many individual FG picks had non-zero value
    std::cout << "\nFG Nonzero (Session-Level):" << std::endl;
    std::cout << "  Measures: Of all FG sessions, how many had non-zero total payout" << std::endl;
    long long nonzero_fg_sessions = m_nonzero_fg_sessions_count.load();
    double fg_sessions_nonzero_rate = (fg_triggers > 0) ? 100.0 * static_cast<double>(nonzero_fg_sessions) / fg_triggers : 0.0;
    std::cout << "  Count:    " << nonzero_fg_sessions << " / " << fg_triggers << " FG sessions (" << std::fixed << std::setprecision(4) << fg_sessions_nonzero_rate << "%)" << std::endl;

    std::cout << "\nFG Nonzero (Pick-Level):" << std::endl;
    std::cout << "  Measures: Of all individual FG picks, how many had non-zero value" << std::endl;
    std::cout << "  Note:     Should match the FG item configuration from input data" << std::endl;
    long long nonzero_fg_picks = m_nonzero_fg_picks_count.load();
    double fg_picks_nonzero_rate = (total_fg_picks > 0) ? 100.0 * static_cast<double>(nonzero_fg_picks) / total_fg_picks : 0.0;
    std::cout << "  Count:    " << nonzero_fg_picks << " / " << total_fg_picks << " FG picks (" << std::fixed << std::setprecision(4) << fg_picks_nonzero_rate << "%)" << std::endl;

    // --- New section for Levels Statistics ---
    std::cout << "\n------ Levels Statistics ------" << std::endl;
    std::cout << "  Note: These statistics track the 'levels' field from configuration data" << std::endl;
    std::cout << "        Items with value=0 have levels=1 by data integrity constraint" << std::endl;
    std::cout << "        'Nonzero Value' means items where level != 1 (i.e., value != 0)" << std::endl;

    // Category 1: BG Items
    std::cout << "\nCategory 1: BG Items (per-item statistics)" << std::endl;
    std::cout << "  Denominator: " << m_stats.count << " BG items (total rounds)" << std::endl;
    std::cout << "  Max BG Level:                  " << m_max_bg_level.load() << std::endl;
    double bg_avg_total = (m_stats.count > 0) ? static_cast<double>(m_total_bg_levels.load()) / m_stats.count : 0.0;
    std::cout << "  Avg BG Level (Total):          " << std::fixed << std::setprecision(4) << bg_avg_total << std::endl;
    long long bg_nonzero_count = m_bg_nonzero_levels_count.load();
    double bg_avg_nonzero = (bg_nonzero_count > 0) ? static_cast<double>(m_bg_nonzero_levels_sum.load()) / bg_nonzero_count : 0.0;
    std::cout << "  Avg BG Level (Nonzero Value):  " << std::fixed << std::setprecision(4) << bg_avg_nonzero << std::endl;
    std::cout << "  Note: Should match BG config baseline from JSON loading" << std::endl;

    // Category 2: FG Picks
    std::cout << "\nCategory 2: FG Picks (per-item statistics)" << std::endl;
    std::cout << "  Denominator: " << total_fg_picks << " FG picks (total items picked)" << std::endl;
    std::cout << "  Max FG Level:                  " << m_max_fg_level.load() << std::endl;
    double fg_avg_total = (total_fg_picks > 0) ? static_cast<double>(m_total_fg_levels.load()) / total_fg_picks : 0.0;
    std::cout << "  Avg FG Level (Total):          " << std::fixed << std::setprecision(4) << fg_avg_total << std::endl;
    long long fg_nonzero_count = m_fg_nonzero_levels_count.load();
    double fg_avg_nonzero = (fg_nonzero_count > 0) ? static_cast<double>(m_fg_nonzero_levels_sum.load()) / fg_nonzero_count : 0.0;
    std::cout << "  Avg FG Level (Nonzero Value):  " << std::fixed << std::setprecision(4) << fg_avg_nonzero << std::endl;
    std::cout << "  Note: Should match FG config baseline from JSON loading" << std::endl;

    // Category 3: Per Run
    std::cout << "\nCategory 3: Per Run (combined BG + FG statistics)" << std::endl;
    long long total_items = m_stats.count + total_fg_picks;
    std::cout << "  Denominator: " << total_items << " total items (BG + FG)" << std::endl;
    std::cout << "  Max Run Level:                 " << m_max_run_level.load() << std::endl;
    double run_avg_total = (total_items > 0) ? static_cast<double>(m_total_run_levels.load()) / total_items : 0.0;
    std::cout << "  Avg Run Level (Total):         " << std::fixed << std::setprecision(4) << run_avg_total << std::endl;
    long long run_nonzero_count = m_run_nonzero_levels_count.load();
    double run_avg_nonzero = (run_nonzero_count > 0) ? static_cast<double>(m_run_nonzero_levels_sum.load()) / run_nonzero_count : 0.0;
    std::cout << "  Avg Run Level (Nonzero Value): " << std::fixed << std::setprecision(4) << run_avg_nonzero << std::endl;
    std::cout << "  Note: Overview of levels when BG and FG are combined" << std::endl;

    // --- New section to print confidence intervals ---
    if (!m_stats.confidence_intervals.empty()) {
        std::cout << "\n------ Confidence Intervals for the Mean ------" << std::endl;
        if (m_mode == MemoryMode::EFFICIENT) {
            std::cout << "        (Method: Batched Means)" << std::endl;
        } else {
            std::cout << "         (Method: Bootstrap)" << std::endl;
        }

        for (const auto& ci : m_stats.confidence_intervals) {
            std::cout << std::fixed << std::setprecision(1) << ci.level << "% Confidence Interval: "
                      << std::setprecision(6) << "[" << ci.lower_bound << ", " << ci.upper_bound << "]" << std::endl;
        }
    }


    std::cout << "\n------ Histogram Distribution ------" << std::endl;
    if (m_mode == MemoryMode::ACCURATE) { std::cout << "         (from fully sorted data)" << std::endl; } 
    else { std::cout << "       (from efficient streaming data)" << std::endl; }
    
    std::cout << std::left << std::setw(20) << "Bin Range" << std::right << std::setw(20) << "Count" << std::setw(25) << "Percentage" << std::endl;
    std::cout << std::string(65, '-') << std::endl;
    
    std::stringstream ss;
    if (m_histogram.underflow > 0) {
         ss << "(< 0)";
         double percentage = 100.0 * m_histogram.underflow / m_stats.count;
         std::cout << std::left << std::setw(20) << ss.str() << std::right << std::setw(20) << m_histogram.underflow << std::setw(24) << std::fixed << std::setprecision(4) << percentage << "%" << std::endl;
         ss.str("");
    }
    
    for(size_t i = 0; i < m_histogram.bins.size(); ++i) {
        if (m_histogram.bins[i] == 0) continue;
        
        if (m_histogram.dividers[i] == 0 && m_histogram.dividers[i+1] == 1) {
             ss << "0";
        } else {
             ss << "[" << m_histogram.dividers[i] << ", " << m_histogram.dividers[i+1] << ")";
        }
        
        double percentage = 100.0 * m_histogram.bins[i] / m_stats.count;
        std::stringstream perc_ss;
        if (percentage < 0.0001 && percentage > 0) {
             perc_ss << std::scientific << std::setprecision(2) << percentage << "%";
        } else {
             perc_ss << std::fixed << std::setprecision(4) << percentage << "%";
        }

        std::cout << std::left << std::setw(20) << ss.str() 
                  << std::right << std::setw(20) << m_histogram.bins[i] 
                  << std::setw(25) << perc_ss.str() << std::endl;
        ss.str("");
    }

    if (m_histogram.overflow > 0) {
         ss << "[" << m_histogram.dividers.back() << "+)";
         double percentage = 100.0 * m_histogram.overflow / m_stats.count;
         std::cout << std::left << std::setw(20) << ss.str() << std::right << std::setw(20) << m_histogram.overflow << std::setw(24) << std::fixed << std::setprecision(4) << percentage << "%" << std::endl;
    }
    
    std::cout << "-----------------------------------------------" << std::endl;
}