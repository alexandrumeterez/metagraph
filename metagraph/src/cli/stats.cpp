#include "stats.hpp"

#include "common/logger.hpp"
#include "common/algorithms.hpp"
#include "common/unix_tools.hpp"
#include "common/serialization.hpp"
#include "common/threads/threading.hpp"
#include "graph/representation/succinct/dbg_succinct.hpp"
#include "graph/representation/succinct/boss.hpp"
#include "graph/graph_extensions/node_weights.hpp"
#include "annotation/representation/row_compressed/annotate_row_compressed.hpp"
#include "annotation/representation/column_compressed/annotate_column_compressed.hpp"
#include "annotation/representation/annotation_matrix/static_annotators_def.hpp"
#include "config/config.hpp"
#include "load/load_graph.hpp"
#include "load/load_annotation.hpp"
#include "parse_sequences.hpp"

using mg::common::logger;
using utils::get_verbose;

typedef annotate::MultiLabelEncoded<std::string> Annotator;


void print_boss_stats(const BOSS &boss_graph,
                      bool count_dummy,
                      size_t num_threads,
                      bool verbose) {
    std::cout << "====================== BOSS STATS ======================" << std::endl;
    std::cout << "k: " << boss_graph.get_k() + 1 << std::endl;
    std::cout << "nodes (k-1): " << boss_graph.num_nodes() << std::endl;
    std::cout << "edges ( k ): " << boss_graph.num_edges() << std::endl;
    std::cout << "state: " << Config::state_to_string(boss_graph.get_state()) << std::endl;

    assert(boss_graph.rank_W(boss_graph.num_edges(), boss_graph.alph_size) == 0);
    std::cout << "W stats: {'" << boss_graph.decode(0) << "': "
              << boss_graph.rank_W(boss_graph.num_edges(), 0);
    for (int i = 1; i < boss_graph.alph_size; ++i) {
        std::cout << ", '" << boss_graph.decode(i) << "': "
                  << boss_graph.rank_W(boss_graph.num_edges(), i)
                        + boss_graph.rank_W(boss_graph.num_edges(), i + boss_graph.alph_size);
    }
    std::cout << "}" << std::endl;

    assert(boss_graph.get_F(0) == 0);
    std::cout << "F stats: {'";
    for (int i = 1; i < boss_graph.alph_size; ++i) {
        std::cout << boss_graph.decode(i - 1) << "': "
                  << boss_graph.get_F(i) - boss_graph.get_F(i - 1)
                  << ", '";
    }
    std::cout << boss_graph.decode(boss_graph.alph_size - 1) << "': "
              << boss_graph.num_edges() - boss_graph.get_F(boss_graph.alph_size - 1)
              << "}" << std::endl;

    if (count_dummy) {
        std::cout << "dummy source edges: "
                  << boss_graph.mark_source_dummy_edges(NULL, num_threads, verbose)
                  << std::endl;
        std::cout << "dummy sink edges: "
                  << boss_graph.mark_sink_dummy_edges()
                  << std::endl;
    }
    std::cout << "========================================================" << std::endl;
}

void print_stats(const DeBruijnGraph &graph) {
    std::cout << "====================== GRAPH STATS =====================" << std::endl;
    std::cout << "k: " << graph.get_k() << std::endl;
    std::cout << "nodes (k): " << graph.num_nodes() << std::endl;
    std::cout << "canonical mode: " << (graph.is_canonical_mode() ? "yes" : "no") << std::endl;

    if (auto weights = graph.get_extension<NodeWeights>()) {
        double sum_weights = 0;
        uint64_t num_non_zero_weights = 0;
        if (const auto *dbg_succ = dynamic_cast<const DBGSuccinct*>(&graph)) {
            // In DBGSuccinct some of the nodes may be masked out
            // TODO: Fix this by using non-contiguous indexing in graph
            //       so that mask of dummy edges does not change indexes.
            for (uint64_t i = 1; i <= dbg_succ->get_boss().num_edges(); ++i) {
                if (uint64_t weight = (*weights)[i]) {
                    sum_weights += weight;
                    num_non_zero_weights++;
                }
            }
        } else {
            if (!weights->is_compatible(graph)) {
                logger->error("Node weights are not compatible with graph");
                exit(1);
            }
            graph.call_nodes([&](auto i) {
                if (uint64_t weight = (*weights)[i]) {
                    sum_weights += weight;
                    num_non_zero_weights++;
                }
            });
        }
        std::cout << "nnz weights: " << num_non_zero_weights << std::endl;
        std::cout << "avg weight: " << static_cast<double>(sum_weights) / num_non_zero_weights << std::endl;

        if (get_verbose()) {
            if (const auto *dbg_succ = dynamic_cast<const DBGSuccinct*>(&graph)) {
                // In DBGSuccinct some of the nodes may be masked out
                // TODO: Fix this by using non-contiguous indexing in graph
                //       so that mask of dummy edges does not change indexes.
                for (uint64_t i = 1; i <= dbg_succ->get_boss().num_edges(); ++i) {
                    if (uint64_t weight = (*weights)[i])
                        std::cout << weight << " ";
                }
            } else {
                graph.call_nodes([&](auto i) { std::cout << (*weights)[i] << " "; });
            }
            std::cout << std::endl;
        }
    }

    std::cout << "========================================================" << std::endl;
}

template <class KmerHasher>
void print_bloom_filter_stats(const KmerBloomFilter<KmerHasher> *kmer_bloom) {
    if (!kmer_bloom)
        return;

    std::cout << "====================== BLOOM STATS =====================" << std::endl;
    std::cout << "Size (bits):\t" << kmer_bloom->size() << std::endl
              << "Num hashes:\t" << kmer_bloom->num_hash_functions() << std::endl;
    std::cout << "========================================================" << std::endl;
}

void print_stats(const Annotator &annotation) {
    std::cout << "=================== ANNOTATION STATS ===================" << std::endl;
    std::cout << "labels:  " << annotation.num_labels() << std::endl;
    std::cout << "objects: " << annotation.num_objects() << std::endl;
    std::cout << "density: " << static_cast<double>(annotation.num_relations())
                                    / annotation.num_objects()
                                    / annotation.num_labels() << std::endl;
    std::cout << "representation: ";

    if (dynamic_cast<const annotate::ColumnCompressed<std::string> *>(&annotation)) {
        std::cout << Config::annotype_to_string(Config::ColumnCompressed) << std::endl;

    } else if (dynamic_cast<const annotate::RowCompressed<std::string> *>(&annotation)) {
        std::cout << Config::annotype_to_string(Config::RowCompressed) << std::endl;

    } else if (dynamic_cast<const annotate::MultiBRWTAnnotator *>(&annotation)) {
        std::cout << Config::annotype_to_string(Config::BRWT) << std::endl;
        const auto &brwt = dynamic_cast<const annotate::MultiBRWTAnnotator &>(annotation).get_matrix();
        std::cout << "=================== Multi-BRWT STATS ===================" << std::endl;
        std::cout << "num nodes: " << brwt.num_nodes() << std::endl;
        std::cout << "avg arity: " << brwt.avg_arity() << std::endl;
        std::cout << "shrinkage: " << brwt.shrinking_rate() << std::endl;
        if (get_verbose()) {
            std::cout << "==================== Multi-BRWT TREE ===================" << std::endl;
            brwt.print_tree_structure(std::cout);
        }

    } else if (dynamic_cast<const annotate::BinRelWT_sdslAnnotator *>(&annotation)) {
        std::cout << Config::annotype_to_string(Config::BinRelWT_sdsl) << std::endl;

    } else if (dynamic_cast<const annotate::BinRelWTAnnotator *>(&annotation)) {
        std::cout << Config::annotype_to_string(Config::BinRelWT) << std::endl;

    } else if (dynamic_cast<const annotate::RowFlatAnnotator *>(&annotation)) {
        std::cout << Config::annotype_to_string(Config::RowFlat) << std::endl;

    } else if (dynamic_cast<const annotate::RainbowfishAnnotator *>(&annotation)) {
        std::cout << Config::annotype_to_string(Config::RBFish) << std::endl;

    } else {
        assert(false);
        throw std::runtime_error("Unknown annotator");
    }
    std::cout << "========================================================" << std::endl;
}


int print_stats(Config *config) {
    assert(config);

    const auto &files = config->fnames;

    for (const auto &file : files) {
        std::shared_ptr<DeBruijnGraph> graph;

        graph = load_critical_dbg(file);
        graph->load_extension<NodeWeights>(file);

        logger->info("Statistics for graph '{}'", file);

        print_stats(*graph);

        if (auto dbg_succ = dynamic_cast<DBGSuccinct*>(graph.get())) {
            const auto &boss_graph = dbg_succ->get_boss();

            print_boss_stats(boss_graph,
                             config->count_dummy,
                             get_num_threads(),
                             get_verbose());

            if (config->print_graph_internal_repr) {
                logger->info("Printing internal representation");
                boss_graph.print_internal_representation(std::cout);
            }
            print_bloom_filter_stats(dbg_succ->get_bloom_filter());
        }

        if (config->print_graph)
            std::cout << *graph;
    }

    for (const auto &file : config->infbase_annotators) {
        auto annotation = initialize_annotation(file, *config);

        if (config->print_column_names) {
            annotate::LabelEncoder<std::string> label_encoder;

            logger->info("Scanning annotation '{}'", file);

            try {
                std::ifstream instream(file, std::ios::binary);

                // TODO: make this more reliable
                if (dynamic_cast<const annotate::ColumnCompressed<> *>(annotation.get())) {
                    // Column compressed dumps the number of rows first
                    // skipping it...
                    load_number(instream);
                }

                if (!label_encoder.load(instream))
                    throw std::ios_base::failure("");

            } catch (...) {
                logger->error("Cannot read label encoder from file '{}'", file);
                exit(1);
            }

            std::cout << "Number of columns: " << label_encoder.size() << std::endl;
            for (size_t c = 0; c < label_encoder.size(); ++c) {
                std::cout << label_encoder.decode(c) << '\n';
            }

            continue;
        }

        if (!annotation->load(file)) {
            logger->error("Cannot load annotations from file '{}'", file);
            exit(1);
        }

        logger->info("Statistics for annotation '{}'", file);
        print_stats(*annotation);
    }

    return 0;
}

int compare(Config *config) {
    assert(config);

    const auto &files = config->fnames;

    assert(files.size());

    logger->info("Loading graph                '{}'", files.at(0));
    auto graph = load_critical_dbg(files.at(0));

    for (size_t f = 1; f < files.size(); ++f) {
        logger->info("Loading graph for comparison '{}'", files[f]);
        auto second = load_critical_dbg(files[f]);
        if (*graph == *second) {
            logger->info("Graphs are identical");
        } else {
            logger->info("Graphs are not identical");
        }
    }

    return 0;
}