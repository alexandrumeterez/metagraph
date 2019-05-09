#ifndef __PATH_ENCODER_HPP__
#define __PATH_ENCODER_HPP__

//
// Created by Jan Studený on 2019-03-08.
//


#include <utility>
#include <iostream>
#include <map>
#include <filesystem>
#include <vector>
#include <nlohmann/json.hpp>
#include "sequence_graph.hpp"
#include "boss_construct.hpp"
#include "dbg_succinct.hpp"

#include "utilities.hpp"

template <typename _path_id,typename GraphT=DBGSuccinct>
class PathDatabase {
    // This is an abstract class, for implementation, use PathDatabaseWavelet.
  public:
    // convenience constructor
    explicit PathDatabase(const vector<string> &raw_reads,
                    size_t k_kmer = 21 /* default kmer */)
            : PathDatabase(std::shared_ptr<const GraphT> {
                                [](GraphT* g){ g->mask_dummy_kmers(1,false); return g; }
                                (new GraphT(dbg_succ_graph_constructor(raw_reads, k_kmer)))
                                })
                {}


    PathDatabase(std::shared_ptr<const GraphT> graph)
          : graph_(graph) {
    }

     ~PathDatabase() = default;

    using node_index = DeBruijnGraph::node_index;
    using path_id = _path_id;

    // compress a batch of sequences
    virtual std::vector<path_id>
    encode(const std::vector<std::string> &sequences) = 0;

    // // All k-mers from the sequence must be represented in graph
    // path_id encode(const std::string &sequence);
    // void compress();

    virtual size_t num_paths() const = 0;

    virtual std::string decode(path_id path) const = 0;

    virtual node_index get_first_node(path_id path) const {
      auto decoded_path = decode(path);
      return graph_->kmer_to_node(decoded_path.substr(0,graph_->get_k()));
    };

    virtual node_index get_last_node(path_id path) const {
      auto decoded_path = decode(path);
      return graph_->kmer_to_node(decoded_path.substr(decoded_path.size()-graph_->get_k()));
    }

    // returns ids of all paths that go through sequence |str|
    virtual std::vector<path_id> get_paths_going_through(const std::string &str) const = 0;
    virtual std::vector<path_id> get_paths_going_through(node_index node) const = 0;

    // make one traversal step through the selected path
    // TODO: Figure out what to do if the node is visited multiple times in the path.
    virtual node_index get_next_node(node_index node, path_id path) const = 0;

    // transition to the next node consistent with the history
    // return npos if there is no transition consistent with the history
    virtual node_index get_next_consistent_node(const std::string &history) const = 0;

    virtual void serialize(const fs::path& folder) const = 0;

  protected:
    std::shared_ptr<const GraphT> graph_;

    static BOSS* dbg_succ_graph_constructor(const vector<string> &raw_reads,
                                                size_t k_kmer) {
        auto graph_constructor = BOSSConstructor(k_kmer - 1);// because BOSS has smaller kmers
        for(auto &read : raw_reads) {
            assert(read.size() >= k_kmer);
            graph_constructor.add_sequence(read);
        }
        return new BOSS(&graph_constructor);
    }
};


#endif // __PATH_ENCODER_HPP__
