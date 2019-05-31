#ifndef __PATH_HPP__
#define __PATH_HPP__

#include <vector>
#include <regex>
#include <cstdlib>
#include <iostream>

template <typename NodeType, typename VLabels>
class Path {

  public:
    Path(uint64_t k, std::string::const_iterator query_begin_it,
         std::string::const_iterator query_it) :
            k_(k),
            query_begin_it_(query_begin_it),
            query_it_(query_it) {
        score_ = 0.0;
        num_matches_ = 0;
        is_similar_ = false;
        is_score_updated_ = false; }

    Path(const Path&) = default;
    Path(Path&&) = default;
    Path& operator= (const Path&) = default;
    Path& operator= (Path&&) = default;

    void seed(NodeType node, const VLabels &labels, std::string sequence, float score=0) {
        push_back(node, labels, score);
        num_matches_ += sequence.size();
        sequence_ += sequence;
    }
    void extend(NodeType node, const VLabels &labels, char extention, float score=0) {
        push_back(node, labels, score);
        //In case of a match:
        if (score > 0) {
            num_matches_ += 1;
        } else {
            is_score_updated_ = false;
        }
        sequence_ += extention;
    }
    // Append a path to the current object. Fills in 0 in case of spaced nodes.
    // Avoid adding duplicate nodes and chars in case of positive overlap.
    void append_path(const Path& path, int64_t overlap_length, uint64_t score) {
        assert(path.nodes_.size() - overlap_length >= 0);
        // TODO: Properly close the gap.
        // Example test case in dbg_aligner.map_to_nodes_jump and dbg_aligner.map_to_nodes_insert.
        while (overlap_length < 0) {
            extend(0, {}, 'N', 0);
            ++ overlap_length;
        }
        nodes_.insert(std::end(nodes_), std::begin(path.nodes_) + overlap_length, std::end(path.nodes_));
        sequence_ += path.sequence_.substr(k_ - 1 + overlap_length);
        label_set_.insert(std::end(label_set_), std::begin(path.label_set_),
                          std::end(path.label_set_));
        query_it_ = path.query_it_;
        score_ = score;
    }
    // Note: Assume score is updated by user accordingly.
    void trim(uint64_t query_trim_length, uint64_t path_trim_length) {
        query_it_ -= query_trim_length;
        sequence_.resize(sequence_.size() - path_trim_length);
        nodes_.resize(nodes_.size() - path_trim_length);
    }

    void update_total_score(float score) { score_ = score; is_score_updated_ = true; }
    void set_query_begin_it(std::string::const_iterator query_begin_it) {
        query_begin_it_ = query_begin_it; }
    void set_query_it(std::string::const_iterator query_it) {
        query_it_ = query_it; }
    void set_cigar(const std::string& cigar) {
        cigar_ = cigar;
        // Extracting the number of matches.
        std::regex match_regex("[0-9]+[M=]");
        std::smatch match;
        uint64_t match_counter = 0;
            std::string cigar_cpy = cigar_;
            while (std::regex_search(cigar_cpy, match, match_regex)) {
                std::string position_str = match.str();
                position_str.pop_back();
                match_counter += atoi(position_str.c_str());
                cigar_cpy = match.suffix();
            }
    }
    void set_similar() { is_similar_ = true; }

    NodeType back() const { return nodes_.back(); }
    NodeType front() const { return nodes_.front(); }
    NodeType last_parent() const { return nodes_.at(nodes_.size() - 1); }
    size_t size() const { return nodes_.size(); }
    size_t query_size() const { return query_it_ - query_begin_it_ + k_ - 1; }
    size_t num_kmers_in_query() const { return query_it_ - query_begin_it_; }

    float get_total_score() const { return score_; }
    VLabels get_labels() const { return label_set_; }
    std::vector<NodeType> get_nodes() const { return nodes_; }

    std::string::const_iterator get_query_it() const { return query_it_; }
    std::string::const_iterator get_query_begin_it() const { return query_begin_it_; }
    std::string get_sequence() const { return sequence_; }
    std::string get_query_sequence() const { return std::string(query_begin_it_, query_it_ + k_ - 1); }
    std::string get_cigar() const { return cigar_; }
    bool get_similar() const { return is_similar_; }
    bool is_score_updated() const { return is_score_updated_; }
    // The paths are sorted in BoundedPriorityQueue in increasing order of score
    // per number of nodes. This gives paths with lower absolute score, but higher
    // score per node to appear at the top of the queue.
    bool operator< (const Path &other) const {
        return ((this->num_matches_ + this->score_)/this->size() < (other.num_matches_ + other.score_)/other.size());
        //return (this->num_matches_/this->size() < other.num_matches_/other.size());
        //return (this->score_/this->size() < other.score_/other.size());
        //return (this->score_ < other.score_);
    }
    bool operator<= (const Path &other) const {
        return (this->operator<(other) ||
                (this->score_ == other.score_ &&
                 this->num_matches_ == other.num_matches_ &&
                 this->size() == other.size()));
    }

  private:
    float score_;
    uint64_t k_;
    std::vector<NodeType> nodes_;
    VLabels label_set_;
    std::string::const_iterator query_begin_it_;
    std::string::const_iterator query_it_;
    std::string sequence_;
    std::string cigar_;
    bool is_similar_;
    uint64_t num_matches_;
    bool is_score_updated_;

    void push_back(NodeType node, const VLabels &labels, float score=0) {
        nodes_.push_back(node);
        score_ += score;
        label_set_.insert(std::end(label_set_), std::begin(labels),
                          std::end(labels));
        ++ query_it_;
    }

};

#endif  // __PATH_HPP__
