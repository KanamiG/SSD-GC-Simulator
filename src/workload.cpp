
#include "ssd/workload.hpp"

#include <random>
#include <stdexcept>

namespace ssd {

    std::vector<LogicalPageId>
    generateWorkload(const WorkloadConfig& config) {

        if(!config.logical_page_count) {
            throw std::invalid_argument(
                "WorkloadConfig.logical_page_count must be > 0"
            );
        }
        if(config.kind == WorkloadKind::HotCold){
            if(config.hot_set_size == 0 || config.hot_set_size >= config.logical_page_count) {
                throw std::invalid_argument(
                    "WorkloadConfig.hot_set_size must be > 0 and < logical_page_count"
                );
            }
            if(config.hot_probability <= 0.0 || config.hot_probability >= 1.0) {
                throw std::invalid_argument(
                    "WorkloadConfig.hot_probability must be > 0.0 and < 1.0"
                );
            }
        }


        std::vector<LogicalPageId> workload;
        workload.reserve(config.operations);

        std::mt19937_64 rng{config.seed};

        switch(config.kind) {
            //Sequential
            case WorkloadKind::Sequential: {
                for(std::size_t i = 0; i < config.operations; ++i) {
                    workload.push_back(i % config.logical_page_count);
                }
                break;
            }
            //Random
            case WorkloadKind::Random: {
                std::uniform_int_distribution<LogicalPageId> dist(0, config.logical_page_count - 1);
                for(std::size_t i = 0; i < config.operations; ++i) {
                    workload.push_back(dist(rng));
                }
                break;
            }
            //HotCold
            case WorkloadKind::HotCold: {
                std::bernoulli_distribution hot_cold_dist(config.hot_probability);
                std::uniform_int_distribution<LogicalPageId> hot_dist(0, config.hot_set_size - 1);
                std::uniform_int_distribution<LogicalPageId> cold_dist(config.hot_set_size, config.logical_page_count - 1);
                for(std::size_t i = 0; i < config.operations; ++i) {
                    if(hot_cold_dist(rng)) {
                        workload.push_back(hot_dist(rng));
                    } else {
                        workload.push_back(cold_dist(rng));
                    }
                }
                break;
            }
            default:
                throw std::logic_error(
                    "Unknown WorkloadKind"
                );
        }
    return workload;
    }  


} // namespace ssd
