#include "clean.hpp"

#include "common/logger.hpp"
#include "common/algorithms.hpp"
#include "common/unix_tools.hpp"
#include "common/threads/threading.hpp"
#include "common/vectors/int_vector_algorithm.hpp"
#include "graph/representation/succinct/dbg_succinct.hpp"
#include "graph/representation/masked_graph.hpp"
#include "graph/graph_extensions/node_weights.hpp"
#include "graph/graph_cleaning.hpp"
#include "config/config.hpp"
#include "load/load_graph.hpp"
#include "sequence_reader.hpp"

using mg::common::logger;


int clean_graph(Config *config) {
    assert(config);

    const auto &files = config->fnames;

    assert(files.size() == 1);
    assert(config->outfbase.size());

    config->min_count = std::max(1u, config->min_count);

    if (!config->to_fasta) {
        logger->error("Clean graph can be serialized only in form "
                      "of its contigs or unitigs. Add flag --to-fasta");
        exit(1);
    }

    Timer timer;
    logger->trace("Graph loading...");

    auto graph = load_critical_dbg(files.at(0));

    if (config->min_count > 1
            || config->max_count < std::numeric_limits<unsigned int>::max()
            || config->min_unitig_median_kmer_abundance != 1
            || config->count_slice_quantiles[0] != 0
            || config->count_slice_quantiles[1] != 1) {
        // load k-mer counts
        auto node_weights = graph->load_extension<NodeWeights>(files.at(0));

        if (!(node_weights)) {
            logger->error("Cannot load k-mer counts from file '{}'",
                          files.at(0));
            exit(1);
        }

        if (auto *dbg_succ = dynamic_cast<DBGSuccinct*>(graph.get()))
            dbg_succ->reset_mask();

        if (!node_weights->is_compatible(*graph)) {
            logger->error("k-mer counts are not compatible with graph '{}'",
                          files.at(0));
            exit(1);
        }

        if (config->min_count > 1
                || config->max_count < std::numeric_limits<unsigned int>::max()) {
            const auto &weights = *graph->get_extension<NodeWeights>();

            graph = std::make_shared<MaskedDeBruijnGraph>(graph,
                [&](auto i) { return weights[i] >= config->min_count
                                    && weights[i] <= config->max_count; });
            graph->add_extension(node_weights);

            assert(node_weights->is_compatible(*graph));
        }

        if (config->min_unitig_median_kmer_abundance == 0) {
            // skip zero k-mer counts for dummy k-mers in DBGSuccinct
            const auto _graph = dynamic_cast<DBGSuccinct*>(graph.get())
                    ? std::make_shared<MaskedDeBruijnGraph>(graph, [&](auto i) { return (*node_weights)[i] > 0; })
                    : graph;

            config->min_unitig_median_kmer_abundance
                = estimate_min_kmer_abundance(*_graph, *node_weights,
                                              config->fallback_abundance_cutoff);
        }
    }

    logger->trace("Graph loaded in {} sec", timer.elapsed());

    if (dynamic_cast<const MaskedDeBruijnGraph *>(graph.get())) {
        logger->trace("Extracting sequences from subgraph...");
    } else {
        logger->trace("Extracting sequences from graph...");
    }

    timer.reset();

    auto call_clean_contigs = [&](auto callback) {
        if (config->min_unitig_median_kmer_abundance != 1) {
            auto node_weights = graph->get_extension<NodeWeights>();
            assert(node_weights);
            if (!node_weights->is_compatible(*graph)) {
                logger->error("k-mer counts are not compatible with the subgraph");
                exit(1);
            }

            logger->info("Threshold for median k-mer abundance in unitigs: {}",
                         config->min_unitig_median_kmer_abundance);

            graph->call_unitigs([&](const std::string &unitig, const auto &path) {
                if (!is_unreliable_unitig(path,
                                          *node_weights,
                                          config->min_unitig_median_kmer_abundance))
                    callback(unitig, path);
            }, config->min_tip_size);

        } else if (config->unitigs || config->min_tip_size > 1) {
            graph->call_unitigs(callback, config->min_tip_size);

        } else {
            graph->call_sequences(callback);
        }
    };

    assert(config->count_slice_quantiles.size() >= 2);

    if (config->count_slice_quantiles[0] == 0
            && config->count_slice_quantiles[1] == 1) {
        FastaWriter writer(utils::remove_suffix(config->outfbase, ".gz", ".fasta") + ".fasta.gz",
                           config->header, true);

        call_clean_contigs([&](const std::string &contig, const auto &) {
            writer.write(contig);
        });

    } else {
        auto node_weights = graph->get_extension<NodeWeights>();
        if (!node_weights) {
            logger->error("Need k-mer counts for binning k-mers by abundance");
            exit(1);
        }
        assert(node_weights->is_compatible(*graph));

        auto &weights = node_weights->get_data();

        assert(graph->max_index() + 1 == weights.size());

        // compute clean count histogram
        tsl::hopscotch_map<uint64_t, uint64_t> count_hist;

        if (config->min_unitig_median_kmer_abundance != 1 || config->min_tip_size > 1) {
            // cleaning required
            sdsl::bit_vector removed_nodes(weights.size(), 1);

            call_clean_contigs([&](const std::string&, const auto &path) {
                for (auto i : path) {
                    assert(weights[i]);
                    count_hist[weights[i]]++;
                    removed_nodes[i] = 0;
                }
            });

            call_ones(removed_nodes, [&weights](auto i) { weights[i] = 0; });

        } else if (auto dbg_succ = std::dynamic_pointer_cast<DBGSuccinct>(graph)) {
            // use entire graph without dummy BOSS edges
            graph->call_nodes([&](auto i) {
                if (uint64_t count = weights[i])
                    count_hist[count]++;
            });
        } else {
            // use entire graph
            graph->call_nodes([&](auto i) {
                assert(weights[i]);
                count_hist[weights[i]]++;
            });
        }
        // must not have any zero weights
        assert(!count_hist.count(0));

        std::vector<std::pair<uint64_t, uint64_t>> count_hist_v(count_hist.begin(),
                                                                count_hist.end());
        count_hist.clear();

        ips4o::parallel::sort(count_hist_v.begin(), count_hist_v.end(),
                              utils::LessFirst(), get_num_threads());

        #pragma omp parallel for num_threads(get_num_threads())
        for (size_t i = 1; i < config->count_slice_quantiles.size(); ++i) {
            // extract sequences for k-mer counts bin |i|
            assert(config->count_slice_quantiles[i - 1] < config->count_slice_quantiles[i]);

            FastaWriter writer(utils::remove_suffix(config->outfbase, ".gz", ".fasta")
                                + "." + std::to_string(config->count_slice_quantiles[i - 1])
                                + "." + std::to_string(config->count_slice_quantiles[i]) + ".fasta.gz",
                               config->header, true);

            if (!count_hist_v.size())
                continue;

            uint64_t min_count = config->count_slice_quantiles[i - 1] > 0
                ? utils::get_quantile(count_hist_v, config->count_slice_quantiles[i - 1])
                : 1;
            uint64_t max_count = config->count_slice_quantiles[i] < 1
                ? utils::get_quantile(count_hist_v, config->count_slice_quantiles[i])
                : std::numeric_limits<uint64_t>::max();

            logger->info("k-mer count thresholds:\n"
                         "min (including): {}\n"
                         "max (excluding): ", min_count, max_count);

            assert(node_weights->is_compatible(*graph));

            MaskedDeBruijnGraph graph_slice(graph,
                [&](auto i) { return weights[i] >= min_count && weights[i] < max_count; });

            graph_slice.call_unitigs([&](const auto &contig, auto&&) { writer.write(contig); });
        }
    }

    logger->trace("Graph cleaning finished in {} sec", timer.elapsed());

    return 0;
}