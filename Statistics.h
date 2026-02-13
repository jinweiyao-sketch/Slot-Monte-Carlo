#ifndef STATISTICS_H
#define STATISTICS_H

#include <vector>

namespace Statistics {

    /**
     * @brief Calculates the mean (average) of a dataset.
     * @param data The vector of data points.
     * @return The mean of the data.
     */
    double calculateMean(const std::vector<double>& data);

    /**
     * @brief Calculates the population variance of a dataset.
     * @param data The vector of data points.
     * @param mean The pre-calculated mean of the data.
     * @return The variance of the data.
     */
    double calculateVariance(const std::vector<double>& data, double mean);

    /**
     * @brief Calculates the population standard deviation.
     * @param variance The pre-calculated variance of the data.
     * @return The standard deviation.
     */
    double calculateStdDev(double variance);

    /**
     * @brief Calculates the sample skewness of a dataset.
     * @param data The vector of data points.
     * @param mean The pre-calculated mean of the data.
     * @param stdDev The pre-calculated standard deviation of the data.
     * @return The skewness of the data.
     */
    double calculateSkewness(const std::vector<double>& data, double mean, double stdDev);

    /**
     * @brief Calculates the excess kurtosis of a dataset.
     * @param data The vector of data points.
     * @param mean The pre-calculated mean of the data.
     * @param stdDev The pre-calculated standard deviation of the data.
     * @return The excess kurtosis of the data.
     */
    double calculateKurtosis(const std::vector<double>& data, double mean, double stdDev);

    /**
     * @brief Finds the value at a given percentile in the dataset.
     * @note This function will sort the input vector `data` in-place to save memory.
     * @param data The vector of data points. Will be sorted.
     * @param percentile The percentile to find (e.g., 50.0 for median). Must be in [0, 100].
     * @return The value at the specified percentile.
     */
    double findValueAtPercentile(std::vector<double>& data, double percentile);

    /**
     * @brief Finds the percentile rank of a given value in a sorted dataset.
     * @param sortedData The vector of data points, which MUST be pre-sorted.
     * @param value The value whose percentile rank is to be found.
     * @return The percentile rank of the value (0-100).
     */
    double findPercentileOfValue(const std::vector<double>& sortedData, double value);

    /**
     * @brief Finds the critical value from a Student's t-distribution.
     * @param confidence_level The level of confidence percentage we want. Alpha and Two-tailed Alpha will be calculated based on this. Currently accepts only 90, 95, 99
     * @param degrees_of_freedom A interger representing the degress of freedom, typically the sample size. When large enough, the function returns normal value. Currently accepts fixed values.
     * @return The corresponding critical value for a t-distribution
     */
    double findTValue(double confidence_level, int degrees_of_freedom);

} // namespace Statistics

#endif // STATISTICS_H
