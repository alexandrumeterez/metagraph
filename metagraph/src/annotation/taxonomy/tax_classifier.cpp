#include "tax_classifier.hpp"

#include <cmath>
#include <string>
#include <vector>

#include "annotation/representation/annotation_matrix/annotation_matrix.hpp"
#include "common/unix_tools.hpp"
#include "common/seq_tools/reverse_complement.hpp"
#include "common/utils/string_utils.hpp"

#include "common/logger.hpp"

namespace mtg {
namespace annot {

using mtg::common::logger;

bool TaxonomyBase::assign_label_type(const std::string &sample_label) {
    if (utils::starts_with(sample_label, "gi|")) {
        // e.g.   >gi|1070643132|ref|NC_031224.1| Arthrobacter phage Mudcat, complete genome
        label_type = GEN_BANK;
        return true;
    } else if (utils::starts_with(utils::split_string(sample_label, ":")[1], "taxid|")) {
        // e.g.   >kraken:taxid|2016032|NC_047834.1 Alteromonas virus vB_AspP-H4/4, complete genome
        label_type = TAXID;
        return false;
    }

    logger->error("Can't determine the type of the given label {}. "
                  "Make sure the labels are in a recognized format.", sample_label);
    exit(1);
}

bool TaxonomyBase::get_taxid_from_label(const std::string &label, TaxId *taxid) const {
    if (label_type == TAXID) {
        *taxid = std::stoul(utils::split_string(label, "|")[1]);
        return true;
    } else if (TaxonomyBase::label_type == GEN_BANK) {
        std::string acc_version = get_accession_version_from_label(label);
        if (not accversion_to_taxid_map.count(acc_version)) {
            return false;
        }
        *taxid = accversion_to_taxid_map.at(acc_version);
        return true;
    }

    logger->error("Error: Could not get the taxid for label {}", label);
    exit(1);
}

std::string TaxonomyBase::get_accession_version_from_label(const std::string &label) const {
    if (label_type == TAXID) {
        return utils::split_string(utils::split_string(label, "|")[2], " ")[0];
    } else if (label_type == GEN_BANK) {
        return utils::split_string(label, "|")[3];;
    }

    logger->error("Error: Could not get the accession version for label {}", label);
    exit(1);
}

// TODO improve this by parsing the compressed ".gz" version (or use https://github.com/pmenzel/taxonomy-tools)
void TaxonomyBase::read_accversion_to_taxid_map(const std::string &filepath,
                                                const graph::AnnotatedDBG *anno_matrix = NULL) {
    std::ifstream f(filepath);
    if (!f.good()) {
        logger->error("Failed to open accession to taxid map table {}", filepath);
        exit(1);
    }

    std::string line;
    getline(f, line);
    if (!utils::starts_with(line, "accession\taccession.version\ttaxid\t")) {
        logger->error("The accession to taxid map table is not in the standard (*.accession2taxid) format {}",
                      filepath);
        exit(1);
    }

    tsl::hopscotch_set<std::string> input_accessions;
    if (anno_matrix != NULL) {
        for (const std::string &label : anno_matrix->get_annotation().get_all_labels()) {
            input_accessions.insert(get_accession_version_from_label(label));
        }
    }

    while (getline(f, line)) {
        if (line == "") {
            logger->error("The accession to taxid map table contains empty lines. "
                          "Please make sure that this file was not manually modified {}", filepath);
            exit(1);
        }
        std::vector<std::string> parts = utils::split_string(line, "\t");
        if (parts.size() <= 2) {
            logger->error("The accession to taxid map table contains incomplete lines. "
                          "Please make sure that this file was not manually modified {}", filepath);
            exit(1);
        }
        if (input_accessions.size() == 0 || input_accessions.count(parts[1])) {
            accversion_to_taxid_map[parts[1]] = std::stoul(parts[2]);
        }
    }
}

TaxonomyClsAnno::TaxonomyClsAnno(const graph::AnnotatedDBG &anno,
                                 const double lca_coverage_rate,
                                 const double kmers_discovery_rate,
                                 const std::string &tax_tree_filepath,
                                 const std::string &label_taxid_map_filepath)
             : TaxonomyBase(lca_coverage_rate, kmers_discovery_rate),
               _anno_matrix(&anno) {
    if (!std::filesystem::exists(tax_tree_filepath)) {
        logger->error("Can't open taxonomic tree file {}", tax_tree_filepath);
        exit(1);
    }

    bool require_accversion_to_taxid_map = assign_label_type(_anno_matrix->get_annotation().get_all_labels()[0]);

    Timer timer;
    if (require_accversion_to_taxid_map) {
        logger->trace("Parsing label_taxid_map file...");
        read_accversion_to_taxid_map(label_taxid_map_filepath, _anno_matrix);
        logger->trace("Finished label_taxid_map file in {} sec", timer.elapsed());
    }

    timer.reset();
    logger->trace("Parsing taxonomic tree...");
    ChildrenList tree;
    read_tree(tax_tree_filepath, &tree);
    logger->trace("Finished taxonomic tree read in {} sec.", timer.elapsed());

    timer.reset();
    logger->trace("Calculating tree statistics...");
    std::vector<TaxId> tree_linearization;
    dfs_statistics(root_node, tree, &tree_linearization);
    logger->trace("Finished tree statistics calculation in {} sec.", timer.elapsed());

    timer.reset();
    logger->trace("Starting rmq preprocessing...");
    rmq_preprocessing(tree_linearization);
    logger->trace("Finished rmq preprocessing in {} sec.", timer.elapsed());
}

void TaxonomyClsAnno::read_tree(const std::string &tax_tree_filepath, ChildrenList *tree) {
    std::ifstream f(tax_tree_filepath);
    if (!f.good()) {
        logger->error("Failed to open Taxonomic Tree file {}", tax_tree_filepath);
        exit(1);
    }

    std::string line;
    tsl::hopscotch_map<TaxId, TaxId> full_parents_list;
    while (getline(f, line)) {
        if (line == "") {
            logger->error("The Taxonomic Tree file contains empty lines. "
                          "Please make sure that this file was not manually modified: {}",
                          tax_tree_filepath);
            exit(1);
        }
        std::vector<std::string> parts = utils::split_string(line, "\t");
        if (parts.size() <= 2) {
            logger->error("The Taxonomic tree filepath contains incomplete lines. "
                          "Please make sure that this file was not manually modified: {}",
                          tax_tree_filepath);
            exit(1);
        }
        uint32_t act = std::stoul(parts[0]);
        uint32_t parent = std::stoul(parts[2]);
        full_parents_list[act] = parent;
        node_parent[act] = parent;
    }

    std::vector<TaxId> relevant_taxids;
    // 'considered_relevant_taxids' is used to make sure that there are no duplications in 'relevant_taxids'.
    tsl::hopscotch_set<TaxId> considered_relevant_taxids;

    if (accversion_to_taxid_map.size()) {
        // Store only the taxonomic nodes that exists in the annotation matrix.
        for (const pair<std::string, TaxId> &it : accversion_to_taxid_map) {
            relevant_taxids.push_back(it.second);
            considered_relevant_taxids.insert(it.second);
        }
    } else {
        // If 'this->accversion_to_taxid_map' is empty, store the entire taxonomic tree.
        for (auto it : full_parents_list) {
            relevant_taxids.push_back(it.first);
            considered_relevant_taxids.insert(it.first);
        }
    }
    assert(relevant_taxids.size());

    uint64_t num_taxid_failed = 0; // num_taxid_failed is used for logging only.
    for (uint32_t i = 0; i < relevant_taxids.size(); ++i) {
        const TaxId taxid = relevant_taxids[i];
        if (!full_parents_list.count(taxid)) {
            num_taxid_failed += 1;
            continue;
        }

        if (not considered_relevant_taxids.count(full_parents_list[taxid])) {
            relevant_taxids.push_back(full_parents_list[taxid]);
            considered_relevant_taxids.insert(full_parents_list[taxid]);
        }

        // Check if the current taxid is the root.
        if (taxid == full_parents_list[taxid]) {
            root_node = taxid;
        }
    }
    if (num_taxid_failed) {
        logger->warn("During the tax_tree_filepath {} parsing, {} taxids were not found out of {} total evaluations.",
                     tax_tree_filepath, num_taxid_failed, relevant_taxids.size());
    }

    // Construct the output tree.
    for (const TaxId &taxid : relevant_taxids) {
        if (taxid == root_node) {
            continue;
        }
        (*tree)[full_parents_list[taxid]].push_back(taxid);
    }
}

void TaxonomyClsAnno::dfs_statistics(const TaxId node,
                                     const ChildrenList &tree,
                                     std::vector<TaxId> *tree_linearization) {
    node_to_linearization_idx[node] = tree_linearization->size();
    tree_linearization->push_back(node);
    uint32_t depth = 0;
    if (tree.count(node)) {
        for (const TaxId &child : tree.at(node)) {
            dfs_statistics(child, tree, tree_linearization);
            tree_linearization->push_back(node);
            if (node_depth[child] > depth) {
                depth = node_depth[child];
            }
        }
    }
    node_depth[node] = depth + 1;
}

void TaxonomyClsAnno::rmq_preprocessing(const std::vector<TaxId> &tree_linearization) {
    uint32_t num_rmq_rows = log2(tree_linearization.size()) + 1;

    rmq_data.resize(num_rmq_rows);
    for (uint32_t i = 0; i < num_rmq_rows; ++i) {
        rmq_data[i].resize(tree_linearization.size());
    }

    // Copy tree_linearization to rmq[0].
    for (uint32_t i = 0; i < tree_linearization.size(); ++i) {
        rmq_data[0][i] = tree_linearization[i];
    }

    // Delta represents the size of the RMQ's sliding window (always a power of 2).
    uint32_t delta = 1;
    for (uint32_t row = 1; row < num_rmq_rows; ++row) {
        for (uint32_t i = 0; i + delta < tree_linearization.size(); ++i) {
            // rmq_data[row][i] covers an interval of size delta=2^row and returns the node with the maximal depth among positions [i, i+2^row-1] in the linearization.
            // According to 'this->dfs_statistics()': node_depth[leaf] = 1 and node_depth[root] = maximum distance to a leaf.
            if (node_depth[rmq_data[row - 1][i]] >
                node_depth[rmq_data[row - 1][i + delta]]) {
                rmq_data[row][i] = rmq_data[row - 1][i];
            } else {
                rmq_data[row][i] = rmq_data[row - 1][i + delta];
            }
        }
        delta *= 2;
    }
}

std::vector<TaxId> TaxonomyClsAnno::get_lca_taxids_for_seq(const std::string_view &sequence, bool reversed) const {
    // num_kmers represents the total number of kmers parsed until the current time.
    uint32_t num_kmers = 0;

    // 'kmer_idx' and 'kmer_val' are storing the indexes and values of all the nonzero kmers in the given read.
    // The list of kmers, 'kmer_val', will be further sent to "matrix.getrows()" method;
    // The list of indexes, 'kmer_idx', will be used to associate one row from "matrix.getrows()" with the corresponding kmer index.
    std::vector<uint32_t> kmer_idx;
    std::vector<node_index> kmer_val;

    if (sequence.size() >= std::numeric_limits<uint32_t>::max()) {
        logger->error("The given sequence contains more than 2^32 bp.");
        std::exit(1);
    }

    auto anno_graph = _anno_matrix->get_graph_ptr();
    anno_graph->map_to_nodes(sequence, [&](node_index i) {
        num_kmers++;
        if (i <= 0 || i >= anno_graph->max_index()) {
            return;
        }
        kmer_val.push_back(i - 1);
        kmer_idx.push_back(num_kmers - 1);
    });

    // Compute the LCA normalized taxid for each nonzero kmer in the given read.
    const auto unique_matrix_rows = _anno_matrix->get_annotation().get_matrix().get_rows(kmer_val);
    //TODO make sure that this function works even if we have duplications in 'rows'. Then, delete this error catch.
    if (kmer_val.size() != unique_matrix_rows.size()) {
        throw std::runtime_error("Internal error: There must be no duplications in the received set of 'rows' in 'call_annotated_rows'.");
    }

    if (unique_matrix_rows.size() >= std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("Internal error: There must be less than 2^32 unique rows. Reduce the query batch size.");
    }

    const auto &label_encoder = _anno_matrix->get_annotation().get_label_encoder();

    TaxId taxid;
    uint64_t cnt_kmer_idx = 0;
    std::vector<TaxId> curr_kmer_taxids;
    std::vector<TaxId> seq_taxids(num_kmers);

    for (auto row : unique_matrix_rows) {
        for (auto cell : row) {
            if (get_taxid_from_label(label_encoder.decode(cell), &taxid)) {
                curr_kmer_taxids.push_back(taxid);
            }
        }
        if (curr_kmer_taxids.size() != 0) {
            if (not reversed) {
                seq_taxids[kmer_idx[cnt_kmer_idx]] = find_lca(curr_kmer_taxids);
            } else {
                seq_taxids[num_kmers - 1 - kmer_idx[cnt_kmer_idx]] = find_lca(curr_kmer_taxids);
            }
        }
        cnt_kmer_idx++;
        curr_kmer_taxids.clear();
    }

    return seq_taxids;
}

TaxId TaxonomyBase::assign_class(const std::string &sequence) const {
    std::vector<TaxId> forward_taxids = get_lca_taxids_for_seq(sequence, false);

    std::string reversed_sequence(sequence);
    reverse_complement(reversed_sequence.begin(), reversed_sequence.end());
    std::vector<TaxId> backward_taxids = get_lca_taxids_for_seq(reversed_sequence, true);

    tsl::hopscotch_map<TaxId, uint64_t> num_kmers_per_node;

    // total_discovered_kmers represents the number of nonzero kmers according to both forward and reversed read.
    uint32_t num_discovered_kmers = 0;
    const uint32_t num_total_kmers = forward_taxids.size();

    // Find the LCA taxid for each kmer without any dependency on the orientation of the read.
    for (uint32_t i = 0; i < num_total_kmers; ++i) {
        if (forward_taxids[i] == 0 && backward_taxids[i] == 0) {
            continue;
        }
        TaxId curr_taxid;
        if (backward_taxids[i] == 0) {
            curr_taxid = forward_taxids[i];
        } else if (forward_taxids[i] == 0) {
            curr_taxid = backward_taxids[i];
        } else {
            // In case that both 'forward_taxid[i]' and 'backward_taxids[i]' are nonzero, compute the LCA.
            TaxId forward_taxid = forward_taxids[i];
            TaxId backward_taxid = backward_taxids[i];
            if (forward_taxid == 0) {
                curr_taxid = backward_taxid;
            } else if (backward_taxid == 0) {
                curr_taxid = forward_taxid;
            } else {
                curr_taxid = find_lca({forward_taxid, backward_taxid});
            }
        }
        if (curr_taxid) {
            num_discovered_kmers ++;
            num_kmers_per_node[curr_taxid]++;
        }
    }

    if (num_discovered_kmers <= _kmers_discovery_rate * num_total_kmers) {
        return 0; // 0 is a wildcard for not enough discovered kmers.
    }

    tsl::hopscotch_set<TaxId> nodes_already_propagated;
    tsl::hopscotch_map<TaxId, uint64_t> node_scores;

    uint32_t desired_number_kmers = num_discovered_kmers * _lca_coverage_rate;
    TaxId best_lca = root_node;
    uint32_t best_lca_dist_to_root = 1;

    // Update the nodes' score by iterating through all the nodes with nonzero kmers.
    for (const pair<TaxId, uint64_t> &node_pair : num_kmers_per_node) {
        TaxId start_node = node_pair.first;
        this->update_scores_and_lca(start_node, num_kmers_per_node, desired_number_kmers, &node_scores,
                                    &nodes_already_propagated, &best_lca, &best_lca_dist_to_root);
    }
    return best_lca;
}


void TaxonomyBase::update_scores_and_lca(const TaxId start_node,
                                         const tsl::hopscotch_map<TaxId, uint64_t> &num_kmers_per_node,
                                         const uint64_t desired_number_kmers,
                                         tsl::hopscotch_map<TaxId, uint64_t> *node_scores,
                                         tsl::hopscotch_set<TaxId> *nodes_already_propagated,
                                         TaxId *best_lca,
                                         uint32_t *best_lca_dist_to_root) const {
    if (nodes_already_propagated->count(start_node)) {
        return;
    }
    uint64_t score_from_processed_parents = 0;
    uint64_t score_from_unprocessed_parents = num_kmers_per_node.at(start_node);

    // processed_parents represents the set of nodes on the path start_node->root that have already been processed in the previous iterations.
    std::vector<TaxId> processed_parents;
    std::vector<TaxId> unprocessed_parents;

    TaxId act_node = start_node;
    unprocessed_parents.push_back(act_node);

    while (act_node != root_node) {
        act_node = node_parent.at(act_node);
        if (!nodes_already_propagated->count(act_node)) {
            if (num_kmers_per_node.count(act_node)) {
                score_from_unprocessed_parents += num_kmers_per_node.at(act_node);
            }
            unprocessed_parents.push_back(act_node);
        } else {
            if (num_kmers_per_node.count(act_node)) {
                score_from_processed_parents += num_kmers_per_node.at(act_node);
            }
            processed_parents.push_back(act_node);
        }
    }
    // The score of all the nodes in 'processed_parents' will be updated with 'score_from_unprocessed_parents' only.
    // The nodes in 'unprocessed_parents' will be updated with the sum 'score_from_processed_parents + score_from_unprocessed_parents'.
    for (uint64_t i = 0; i < unprocessed_parents.size(); ++i) {
        TaxId &act_node = unprocessed_parents[i];
        (*node_scores)[act_node] =
                score_from_processed_parents + score_from_unprocessed_parents;
        nodes_already_propagated->insert(act_node);

        uint64_t act_dist_to_root =
                processed_parents.size() + unprocessed_parents.size() - i;

        // Test if the current node's score would be a better LCA result.
        if ((*node_scores)[act_node] >= desired_number_kmers
            && (act_dist_to_root > *best_lca_dist_to_root
                || (act_dist_to_root == *best_lca_dist_to_root && (*node_scores)[act_node] > (*node_scores)[*best_lca]))) {
            *best_lca = act_node;
            *best_lca_dist_to_root = act_dist_to_root;
        }
    }
    for (uint64_t i = 0; i < processed_parents.size(); ++i) {
        TaxId &act_node = processed_parents[i];
        (*node_scores)[act_node] += score_from_unprocessed_parents;

        uint64_t act_dist_to_root = processed_parents.size() - i;
        if ((*node_scores)[act_node] >= desired_number_kmers
            && (act_dist_to_root > *best_lca_dist_to_root
                || (act_dist_to_root == *best_lca_dist_to_root && (*node_scores)[act_node] > (*node_scores)[*best_lca]))) {
            *best_lca = act_node;
            *best_lca_dist_to_root = act_dist_to_root;
        }
    }
}

TaxId TaxonomyClsAnno::find_lca(const std::vector<TaxId> &taxids) const {
    if (taxids.empty()) {
        logger->error("Internal error: Can't find LCA for an empty set of normalized taxids.");
        std::exit(1);
    }
    uint64_t left_idx = node_to_linearization_idx.at(taxids[0]);
    uint64_t right_idx = node_to_linearization_idx.at(taxids[0]);

    for (const TaxId &taxid : taxids) {
        if (node_to_linearization_idx.at(taxid) < left_idx) {
            left_idx = node_to_linearization_idx.at(taxid);
        }
        if (node_to_linearization_idx.at(taxid) > right_idx) {
            right_idx = node_to_linearization_idx.at(taxid);
        }
    }
    // The node with maximum node_depth in 'linearization[left_idx : right_idx+1]' is the LCA of the given set.

    // Find the maximum node_depth between the 2 overlapping intervals of size 2^log_dist.
    uint32_t log_dist = sdsl::bits::hi(right_idx - left_idx);
    if (rmq_data.size() <= log_dist) {
        logger->error("Internal error: the RMQ was not precomputed before the LCA queries.");
        std::exit(1);
    }

    uint32_t left_lca = rmq_data[log_dist][left_idx];
    uint32_t right_lca = rmq_data[log_dist][right_idx - (1 << log_dist) + 1];

    if (node_depth.at(left_lca) > node_depth.at(right_lca)) {
        return left_lca;
    }
    return right_lca;
}

std::vector<TaxId> TaxonomyClsImportDB::get_lca_taxids_for_seq(const std::string_view &sequence, bool reversed) const {
    cerr << "Assign class not implemented reversed = " << reversed << "\n";
    throw std::runtime_error("get_lca_taxids_for_seq TaxonomyClsImportDB not implemented. Received seq size"
                             + to_string(sequence.size()));
}

TaxId TaxonomyClsAnno::find_lca(const std::vector<TaxId> &taxids) const {
    throw std::runtime_error("find_lca TaxonomyClsAnno not implemented. Received taxids size"
                             + to_string(taxids.size()));
}

TaxId TaxonomyClsImportDB::find_lca(const std::vector<TaxId> &taxids) const {
    throw std::runtime_error("find_lca TaxonomyClsImportDB not implemented. Received taxids size"
                             + to_string(taxids.size()));
}

} // namespace annot
} // namespace mtg