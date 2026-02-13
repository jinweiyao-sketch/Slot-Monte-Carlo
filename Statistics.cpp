#include "Statistics.h"
#include <numeric>   // For std::accumulate
#include <cmath>     // For std::sqrt, std::pow
#include <algorithm> // For std::sort, std::lower_bound
#include <stdexcept> // For std::invalid_argument
#include <map>

namespace Statistics {

    double calculateMean(const std::vector<double>& data) {
        if (data.empty()) return 0.0;
        long double sum = std::accumulate(data.begin(), data.end(), 0.0L);
        return static_cast<double>(sum / data.size());
    }

    double calculateVariance(const std::vector<double>& data, double mean) {
        if (data.size() < 2) return 0.0;
        long double squaredDiffSum = 0.0;
        for (const double val : data) {
            squaredDiffSum += (val - mean) * (val - mean);
        }
        return static_cast<double>(squaredDiffSum / data.size()); // Population variance
    }

    double calculateStdDev(double variance) {
        return std::sqrt(variance);
    }

    double calculateSkewness(const std::vector<double>& data, double mean, double stdDev) {
        if (data.size() < 3 || stdDev == 0) return 0.0;
        long double skewSum = 0.0;
        for (const double val : data) {
            skewSum += std::pow((val - mean) / stdDev, 3);
        }
        // Apply Bessel's correction for sample skewness
        size_t n = data.size();
        double correction = std::sqrt(n * (n - 1.0)) / (n - 2.0);
        return static_cast<double>(skewSum / n) * correction;
    }

    double calculateKurtosis(const std::vector<double>& data, double mean, double stdDev) {
        if (data.size() < 4 || stdDev == 0) return 0.0;
        long double kurtosisSum = 0.0;
        for (const double val : data) {
            kurtosisSum += std::pow((val - mean) / stdDev, 4);
        }
        // Calculate excess kurtosis for a sample
        size_t n = data.size();
        double term1 = (n + 1.0) * n / ((n - 1.0) * (n - 2.0) * (n - 3.0));
        double term2 = kurtosisSum;
        double term3 = 3.0 * std::pow(n - 1.0, 2) / ((n - 2.0) * (n - 3.0));
        return static_cast<double>(term1 * term2 - term3);
    }

    double findValueAtPercentile(std::vector<double>& data, double percentile) {
        if (data.empty() || percentile < 0.0 || percentile > 100.0) {
            throw std::invalid_argument("Data cannot be empty and percentile must be between 0 and 100.");
        }
        
        std::sort(data.begin(), data.end());

        if (percentile == 100.0) {
            return data.back();
        }
        
        // Using (N-1) method for index calculation
        double rank = (percentile / 100.0) * (data.size() - 1);
        size_t lower_index = static_cast<size_t>(rank);
        double fraction = rank - lower_index;

        if (lower_index + 1 >= data.size()) {
            return data.back();
        }

        // Linear interpolation between the two closest ranks
        return data[lower_index] + fraction * (data[lower_index + 1] - data[lower_index]);
    }

    double findPercentileOfValue(const std::vector<double>& sortedData, double value) {
        if (sortedData.empty()) {
            throw std::invalid_argument("Data cannot be empty.");
        }
        
        // std::lower_bound finds the first element not less than 'value'
        auto it = std::lower_bound(sortedData.begin(), sortedData.end(), value);
        
        // Calculate the distance (count of elements <= value)
        // This gives us the number of elements less than or equal to the value
        size_t count = std::distance(sortedData.begin(), it);
        
        // Add count of elements exactly equal to 'value' for better positioning
        if (it != sortedData.end() && *it == value) {
            auto upper_it = std::upper_bound(sortedData.begin(), sortedData.end(), value);
            count += std::distance(it, upper_it) / 2;
        }

        // Calculate percentile
        return (static_cast<double>(count) / sortedData.size()) * 100.0;
    }


    // Implementation of the t-value finder
    double findTValue(double confidence_level, int df) {
        if (df < 1) {
            // Degrees of freedom must be at least 1.
            return std::nan(""); 
        }

        // This function uses a combination of a lookup table for common, small df values
        // and approximation with Z-scores for larger df, which is a standard practice.
        // The "alpha" is the area in the tails of the distribution.
        double alpha = 1.0 - (confidence_level / 100.0);
        double two_tailed_alpha = alpha / 2.0;

        // For larger degrees of freedom (df > 100 is a common rule of thumb),
        // the t-distribution is very close to the normal distribution (Z-distribution).
        // We use the Z-scores as an approximation.
        if (df > 100) {
            if (confidence_level == 90.0) return 1.645; // Z-score for 90%
            if (confidence_level == 95.0) return 1.960; // Z-score for 95%
            if (confidence_level == 99.0) return 2.576; // Z-score for 99%
        }

        // For smaller df, we can use a lookup table. This is a simplified table.
        // A real-world library would use a more complex inverse CDF function.
        // Map key: degrees of freedom
        // Map value: {t-value for 90%, 95%, 99%}
        // These are two-tailed critical values from the Student's t-distribution.
        static const std::map<int, std::vector<double>> t_table = {
            {1,  {6.314, 12.706, 63.657}},
            {2,  {2.920, 4.303,  9.925}},
            {3,  {2.353, 3.182,  5.841}},
            {4,  {2.132, 2.776,  4.604}},
            {5,  {2.015, 2.571,  4.032}},
            {6,  {1.943, 2.447,  3.707}},
            {7,  {1.895, 2.365,  3.499}},
            {8,  {1.860, 2.306,  3.355}},
            {9,  {1.833, 2.262,  3.250}},
            {10, {1.812, 2.228,  3.169}},
            {15, {1.753, 2.131,  2.947}},
            {20, {1.725, 2.086,  2.845}},
            {25, {1.708, 2.060,  2.787}},
            {30, {1.697, 2.042,  2.750}},
            {40, {1.684, 2.021,  2.704}},
            {50, {1.676, 2.009,  2.678}},
            {60, {1.671, 2.000,  2.660}},
            {80, {1.664, 1.990,  2.639}},
            {100,{1.660, 1.984,  2.626}}
        };

        auto it = t_table.lower_bound(df);
        if (it == t_table.end()) {
            it--; // Use the largest available entry if df is between map keys
        }

        if (confidence_level == 90.0) return it->second[0];
        if (confidence_level == 95.0) return it->second[1];
        if (confidence_level == 99.0) return it->second[2];

        return std::nan(""); // Return NaN if confidence level is not supported
    }

} // namespace Statistics
