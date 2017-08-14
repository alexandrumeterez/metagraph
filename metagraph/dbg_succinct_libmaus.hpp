#ifndef __DBG_SUCCINCT_LIBM_HPP__
#define __DBG_SUCCINCT_LIBM_HPP__

#include <zlib.h>
#include <unordered_map>

#include <libmaus2/bitbtree/bitbtree.hpp>
#include <libmaus2/wavelet/DynamicWaveletTree.hpp>

#include "config.hpp"
#include "datatypes.hpp"

#include <sdsl/wavelet_trees.hpp>
#include <boost/multiprecision/cpp_int.hpp>

typedef boost::multiprecision::uint256_t ui256;

class rs_bit_vector: public sdsl::bit_vector {
    private:
        sdsl::rank_support_v5<> rk;
        sdsl::select_support_mcl<> slct;
        bool update_rs = true;
        void init_rs() {
            rk = sdsl::rank_support_v5<>(this);
            slct = sdsl::select_support_mcl<>(this);
            update_rs = false;
        }
    public:
        rs_bit_vector(size_t size, bool def) : sdsl::bit_vector(size, def) {
        }
        rs_bit_vector() : sdsl::bit_vector() {
        }
        void set(size_t id, bool val) {
            this->operator[](id) = val;
            update_rs = true;
        }
        void setBitQuick(size_t id, bool val) {
            this->operator[](id) = val;
            update_rs = true;
        }
        void insertBit(size_t id, bool val) {
            this->resize(this->size()+1);
            if (this->size() > 1)
                std::copy_backward(this->begin()+id,(this->end())-1,this->end());
            set(id, val);
            update_rs = true;
        }
        void deleteBit(size_t id) {
            if (this->size() > 1)
                std::copy(this->begin()+id+1,this->end(),this->begin()+id);
            this->resize(this->size()-1);
            update_rs = true;
        }
        void deserialise(std::istream &in) {
            this->load(in);
        }
        void serialise(std::ostream &out) {
            this->serialize(out);
        }
        uint64_t select1(size_t id) {
            if (update_rs)
                init_rs();
            //compensating for libmaus weirdness
            id++;
            size_t maxrank = rk(this->size());
            if (id > maxrank) {
                //TODO: should this line ever be reached?
                return this->size();
            }
            return slct(id);
        }
        uint64_t rank1(size_t id) {
            if (update_rs)
                init_rs();
            //the rank method in SDSL does not include id in the count
            return rk(id >= this->size() ? this->size() : id+1);
        }
};

class dyn_wavelet: public sdsl::int_vector<> {
    private:
        sdsl::wt_int<> wwt;
        size_t logsigma;
        bool update_rs;
        void init_wt() {
            this->resize(n);
            sdsl::construct_im(wwt, *this);
            update_rs=false;
        }
    public:
        size_t n;
        dyn_wavelet(size_t logsigma, size_t size, uint64_t def) 
            : sdsl::int_vector<>(2 * size + 1, def, 1<< logsigma) {
            n = size;
            update_rs = true;
        }
        dyn_wavelet(size_t logsigma)
            : dyn_wavelet(logsigma, 0, 0) {
        }
        void deserialise(std::istream &in) {
            wwt.load(in);
            this->load(in);
            n = this->size();
        }
        dyn_wavelet(std::istream &in) {
            this->deserialise(in);
        }
        void insert(uint64_t val, size_t id) {
            if (n == this->size()) {
                this->resize(2*n+1);
            }
            n++;
            if (this->size() > 1)
                std::copy_backward(this->begin()+id,this->begin()+n-1,this->begin()+n);
            this->operator[](id) = val;
            update_rs = true;
        }
        void remove(size_t id) {
            if (this->size() > 1)
                std::copy(this->begin()+id+1,this->begin()+n,this->begin()+id);
            n--;
            update_rs = true;
        }
        uint64_t rank(uint64_t c, uint64_t i) {
            if (update_rs)
                init_wt();
            return wwt.rank(i >= wwt.size() ? wwt.size() : i+1, c);
        }
        uint64_t select(uint64_t c, uint64_t i) {
            if (update_rs)
                init_wt();
            i++;
            uint64_t maxrank = wwt.rank(wwt.size(), c);
            if (i > maxrank) {
                //TODO: should this line ever be reached?
                return wwt.size();
            }
            return wwt.select(i, c);
        }
        void serialise(std::ostream &out) {
            this->resize(n);
            wwt.serialize(out);
            this->serialize(out);
        }
        sdsl::int_vector<>::reference operator[](const size_t id) {
            update_rs = true;
            return sdsl::int_vector<>::operator[](id);
        }
};

//Child of DynamicWaveletTree allowing for construction from an int vector
class dyn_wavelet2 : public libmaus2::wavelet::DynamicWaveletTree<6, 64> {
    public:
        dyn_wavelet2(size_t b)
            : libmaus2::wavelet::DynamicWaveletTree<6, 64>(b) {
        }
        dyn_wavelet2(std::istream &in)
            : libmaus2::wavelet::DynamicWaveletTree<6, 64>(in) {
        }
        libmaus2::bitbtree::BitBTree<6, 64>* makeTree(std::vector<uint8_t> &W_stat, size_t b, unsigned int parallel=1) {
            uint64_t n = W_stat.size();
            //uint64_t n = W_stat->size();
            std::vector<uint64_t> offsets((1ull << (b-1)) - 1, 0);
            //#pragma omp parallel num_threads(parallel) shared(offsets)
            //{
                //#pragma omp for
                for (uint64_t i=0;i<n;++i) {
                    uint64_t m = (1ull << (b - 1));
                    uint64_t v = (uint64_t) W_stat.at(i);
                    //v = (uint64_t) W_stat->at(i);
                    uint64_t o = 0;
                    for (uint64_t ib = 1; ib < b; ++ib) {
                        bool const bit = m & v;
                        if (!bit)
                            offsets.at(o) += 1;
                        o = 2*o + 1 + bit;
                        m >>= 1;
                    }
                }
            //}
            libmaus2::bitbtree::BitBTree<6, 64> *tmp = new libmaus2::bitbtree::BitBTree<6, 64>(n*b, false);
            //libmaus2::bitbtree::BitBTree<6, 64> tmp(n*b, false);
            std::vector<uint64_t> upto_offsets ((1ull << (b - 1)) - 1, 0);
            //#pragma omp parallel num_threads(parallel)
            //{
                //#pragma omp for
                for (uint64_t i = 0; i < n; ++i) {
                    uint64_t m = (1ull << (b - 1));
                    uint64_t v = (uint64_t) W_stat.at(i);
                    //v = (uint64_t) W_stat->at(i);
                    uint64_t o = 0;
                    uint64_t p = i;
                    uint64_t co = 0;
                    bool bit;
                    for (uint64_t ib = 0; ib < b - 1; ++ib) {
                        bit = m & v;
                        if (bit) {
                            //tmp.setBitQuick(ib * n + p + co, true);
                            tmp->setBitQuick(ib * n + p + co, true);
                            //assert((*tmp)[ib * n + p + co]);
                            co += offsets.at(o);
                            p -= upto_offsets.at(o);
                        } else {
                            p -= (p - upto_offsets.at(o)); 
                            upto_offsets.at(o) += 1;
                        }
                        //std::cerr << "o: " << o << " offset[o]: " << offsets.at(o) << std::endl;
                        o = 2*o + 1 + bit;
                        m >>= 1;
                    }
                    bit = m & v;
                    if (bit) {
                       // std::cerr << "b - 1: " << b - 1 << " n: " << n << " p: " << p << " co: " << co << std::endl;
                        tmp->setBitQuick((b - 1) * n + p + co, true); 
                        //tmp.setBitQuick((b - 1) * n + p + co, true); 
                        //assert((*tmp)[(b - 1) * n + p + co]);
                    }
                }
            //}
            return tmp;
        }

        dyn_wavelet2(std::vector<uint8_t> &W_stat, size_t b, unsigned int parallel=1)
            : libmaus2::wavelet::DynamicWaveletTree<6, 64>(makeTree(W_stat, b, parallel), b, W_stat.size()) {
        }

        dyn_wavelet2(libmaus2::bitbtree::BitBTree<6, 64>* bt, size_t b, size_t n)
            : libmaus2::wavelet::DynamicWaveletTree<6, 64>(bt, b, n) {
        }

};

//libmaus2 structures
typedef libmaus2::bitbtree::BitBTree<6, 64> BitBTree;
//typedef libmaus2::wavelet::DynamicWaveletTree<6, 64> WaveletTree;

//SDSL-based structures
//typedef rs_bit_vector BitBTree;
//typedef dyn_wavelet WaveletTree;
typedef dyn_wavelet2 WaveletTree;

class DBG_succ {

    // define an extended alphabet for W --> somehow this does not work properly as expected
    typedef uint64_t TAlphabet;

    private:
    std::vector<ui256> kmers;

    public:

    AnnotationHash hasher;

    // the bit array indicating the last outgoing edge of a node
    BitBTree *last = new BitBTree();
    //libmaus2::bitbtree::BitBTree<6, 64> *last = new libmaus2::bitbtree::BitBTree<6, 64>();

    // the array containing the edge labels
    //libmaus2::wavelet::DynamicWaveletTree<6, 64> *W = new libmaus2::wavelet::DynamicWaveletTree<6, 64>(4); // 4 is log (sigma)
    WaveletTree *W = new WaveletTree(4);

    // the bit array indicating the last outgoing edge of a node (static container for full init)
    std::vector<bool> last_stat;

    // the array containing the edge labels
    std::vector<uint8_t> W_stat;

    // the offset array to mark the offsets for the last column in the implicit node list
    std::vector<TAlphabet> F; 

    // k-mer size
    size_t k;
    // index of position that marks end in graph
    uint64_t p;
    // alphabet size
    size_t alph_size = 7;

    // infile base when loaded from file
    std::string infbase;

    // config object
    Config* config;

    // annotation containers
    std::deque<uint32_t> annotation; // list that associates each node in the graph with an annotation hash
    std::vector<std::string> id_to_label; // maps the label ID back to the original string
    std::unordered_map<std::string, uint32_t> label_to_id_map; // maps each label string to an integer ID
    std::map<uint32_t, uint32_t> annotation_map; // maps the hash of a combination to the position in the combination vector
    std::vector<uint32_t> combination_vector; // contains all known combinations
    uint64_t combination_count = 0;

#ifdef DBGDEBUG
    bool debug = true;
#else
    bool debug = false;
#endif 

    // construct empty graph instance
    DBG_succ(size_t k_,
             Config* config_, 
             bool sentinel = true);

    // load graph instance from a provided file name base
    DBG_succ(std::string infbase_, 
             Config* config_);

    // destructor
    ~DBG_succ();
    
    //
    //
    // QUERY FUNCTIONS
    //
    //
    
    /** 
     * Uses the object's array W, a given position i in W and a character c
     * from the alphabet and returns the number of occurences of c in W up to
     * position i.
     */
    uint64_t rank_W(uint64_t i, TAlphabet c);

    /**
     * Uses the array W and gets a count i and a character c from 
     * the alphabet and returns the positions of the i-th occurence of c 
     * in W.
     */
    uint64_t select_W(uint64_t i, TAlphabet c);

    /**
     * This is a convenience function that returns for array W, a position i and 
     * a character c the last index of a character c preceding in W[1..i].
     */
    uint64_t pred_W(uint64_t i, TAlphabet c);

    /**
     * This is a convenience function that returns for array W, a position i and 
     * a character c the first index of a character c in W[i..N].
     */
    uint64_t succ_W(uint64_t i, TAlphabet c);

    /** 
     * Uses the object's array last and a position and
     * returns the number of set bits up to that postion.
     */
    uint64_t rank_last(uint64_t i);

    /**
     * Uses the object's array last and a given position i and
     * returns the position of the i-th set bit in last[1..i].
     */
    uint64_t select_last(uint64_t i);

    /**
     * This is a convenience function that returns for the object's array last
     * and a given position i the position of the last set bit in last[1..i].
     */
    uint64_t pred_last(uint64_t i);

    /**
     * This is a convenience function that returns for the object's array last
     * and a given position i the position of the first set bit in last[i..N].
     */
    uint64_t succ_last(uint64_t i);

    /**
     * This function gets a position i that reflects the i-th node and returns the
     * position in W that corresponds to the i-th node's last character. 
     */
    uint64_t bwd(uint64_t i);

    /**
     * This functions gets a position i reflecting the r-th occurence of the corresponding
     * character c in W and returns the position of the r-th occurence of c in last.
     */
    uint64_t fwd(uint64_t i);

    /**
     * Using the offset structure F this function returns the value of the last 
     * position of node i.
     */
    TAlphabet get_node_end_value(uint64_t i);
    
    /**
     * Given index of node i, the function returns the 
     * first character of the node.
     */
    TAlphabet get_node_begin_value(uint64_t i);

    /**
     * Given a position i in W and an edge label c, this function returns the
     * index of the node the edge is pointing to.
     */
    uint64_t outgoing(uint64_t i, TAlphabet c);

    /**
     * Given a node index i and an edge label c, this function returns the
     * index of the node the incoming edge belongs to.
     */
    uint64_t incoming(uint64_t i, TAlphabet c);

    /**
     * Given a node index i, this function returns the number of outgoing
     * edges from node i.
     */
    uint64_t outdegree(uint64_t i);

    /**
     * Given a node index i, this function returns the number of incoming
     * edges to node i.
     */
    uint64_t indegree(uint64_t i);

    /**
     * Given a node label s, this function returns the index
     * of the corresponding node, if this node exists and 0 otherwise.
     */
    uint64_t index(std::string &s_);

    uint64_t index(std::deque<TAlphabet> str);

    std::vector<HitInfo> index_fuzzy(std::string &str, uint64_t eops);

    std::pair<uint64_t, uint64_t> index_range(std::deque<TAlphabet> str);

    uint64_t index_predecessor(std::deque<TAlphabet> str);

    /**
     * Given a position i, this function returns the boundaries of the interval
     * of nodes identical to node i (ignoring the values in W).
     */
    std::pair<uint64_t, uint64_t> get_equal_node_range(uint64_t i);

    /**
     * Given index i of a node and a value k, this function 
     * will return the k-th last character of node i.
     */
    std::pair<TAlphabet, uint64_t> get_minus_k_value(uint64_t i, uint64_t k);

    /** 
     * This function gets two node indices and returns if the
     * node labels share a k-1 suffix.
     */
    bool compare_node_suffix(uint64_t i1, uint64_t i2);

    /**
     * This function returns true if node i is a terminal node.
     */
    bool is_terminal_node(uint64_t i);

    /**
    * Given a node index k_node, this function returns the k-mer sequence of the
    * node in a deque data structure.
    */
    std::deque<TAlphabet> get_node_seq(uint64_t k_node); 

    /**
    * Given a node index k_node, this function returns the k-mer sequence of the 
    * node as a string.
    */
    std::string get_node_str(uint64_t k_node); 

    /**
     * Return number of edges in the current graph.
     * TODO: delete
     */
    uint64_t get_size();

    /**
     * Return number of nodes in the current graph.
     */
    uint64_t get_nodes();

    /**
     * Return k-mer length of current graph.
     * TODO: delete
     */
    uint64_t get_k();

    /**
     * Return value of W at position k.
     */
    TAlphabet get_W(uint64_t k);

    /** 
     * Return value of F vector at index k.
     * The index is over the alphabet!
     */
    TAlphabet get_F(uint64_t k);

    /**
     * Return value of last at position k.
     */
    bool get_last(uint64_t k);

    // Given the alphabet index return the corresponding symbol
    char get_alphabet_symbol(uint64_t s);

    // Given the alphabet character return its corresponding number
    TAlphabet get_alphabet_number(char s);

    /**
     * Breaks the seq into k-mers and searches for the index of each
     * k-mer in the graph. Returns these indices.
     */
    std::vector<uint64_t> align(kstring_t seq);

    std::vector<std::vector<HitInfo> > align_fuzzy(kstring_t seq, uint64_t max_distance = 0, uint64_t alignment_length = 0);

    uint64_t get_node_count();

    uint64_t get_edge_count();


    //
    //
    // APPEND
    // 
    //
    
    /**
     * This function gets a value of the alphabet c and updates the offset of 
     * all following values by +1 is positive is true and by -1 otherwise.
     */
    void update_F(TAlphabet c, bool positive);

    /**
     * This function gets a local range in W from lower bound l
     * to upper bound u and swaps the inserted element to the
     * righ location.
     */
    void sort_W_locally(uint64_t l, uint64_t u);

    /** 
     * This is a convenience function to replace the value at
     * position i in W with val.
     */
    void replaceW(size_t i, TAlphabet val);

    // add a full sequence to the graph
    void add_seq (kstring_t &seq);
    void add_seq_alt (kstring_t &seq, bool bridge=true, unsigned int parallel=1, std::string suffix="");
    void construct_succ(unsigned int parallel=1);

    /** This function takes a character c and appends it to the end of the graph sequence
     * given that the corresponding note is not part of the graph yet.
     */
    void append_pos(TAlphabet c);

    /** This function takes a pointer to a graph structure and concatenates the arrays W, last 
     * and F to this graph's arrays. In almost all cases this will not produce a valid graph and 
     * should only be used as a helper in the parallel merge procedure.
     */
    void append_graph(DBG_succ *g);

    /** 
     * This function takes a pointer to a graph structure and concatenates the arrays W, last 
     * and F to this graph's static containers last_stat and W_stat. In almost all cases 
     * this will not produce a valid graph and should only be used as a helper in the 
     * parallel merge procedure.
     */
    void append_graph_static(DBG_succ *g);

    void toDynamic();


    //
    //
    // ANNOTATE
    //
    //

    void annotate_seq(kstring_t &seq, kstring_t &label, uint64_t start=0, uint64_t end=0, pthread_mutex_t* anno_mutex=NULL);

    void annotate_kmer(std::string &kmer, uint32_t &label, uint64_t &previous, pthread_mutex_t* anno_mutex, bool ignore=false);

    std::vector<uint32_t> classify_path(std::vector<uint64_t> path);

    std::set<uint32_t> classify_read(kstring_t &read, uint64_t max_distance);


    //
    //
    // MERGE
    //
    //

   
    uint64_t next_non_zero(std::vector<uint64_t> v, uint64_t pos);
    uint64_t next_non_zero(std::vector<std::pair<uint64_t, std::deque<TAlphabet> > > v, uint64_t pos);

    /**
    * Given a pointer to a graph structure G, the function compares its elements to the
    * current graph. It will perform an element wise comparison of the arrays W, last and
    * F and will only check for identity. If any element differs, the function will return 
    * false and true otherwise.
    */
    bool compare(DBG_succ* G); 

    /*
     * Helper function that will split up a given range in the graph
     * into bins, one for each character in the alphabet. The split is performed based
     * on the k - d column of the node label. It is assumed that the all nodes in the
     * given range share a common suffix of length d.
     */
    std::vector<uint64_t> split_range(uint64_t start, uint64_t end, uint64_t d /*depth*/);
    void split_range(std::deque<TAlphabet>* str, std::pair<uint64_t, uint64_t>& range);

    /* 
     * Helper function to determine the bin boundaries, given 
     * a number of threads.
     */
    std::vector<std::pair<uint64_t, uint64_t> > get_bins(uint64_t threads, uint64_t bins_per_thread, DBG_succ* G);
    std::vector<std::pair<uint64_t, uint64_t> > get_bins(uint64_t bins);

    std::vector<std::pair<uint64_t, uint64_t> > get_bins_relative(DBG_succ* G, std::vector<std::pair<uint64_t, uint64_t> > ref_bins, uint64_t first_pos, uint64_t last_pos);

    /*
     * Helper function to generate the prefix corresponding to 
     * a given bin ID.
     */
    std::deque<TAlphabet> bin_id_to_string(uint64_t bin_id, uint64_t binlen);

    /*
     * Distribute the merging of two graph structures G1 and G2 over
     * bins, such that n parallel threads are used. The number of bins
     * is determined dynamically.
     */
    //void merge_parallel(DBG_succ* G1, DBG_succ* G2, uint64_t k1, uint64_t k2, uint64_t n1, uint64_t n2, uint64_t threads);


    //
    //
    // SERIALIZE
    //
    //
    
    /**
     * This is a debug function that prints the current state of the graph arrays to
     * the screen.
     */
    void print_state();
    
    
    /**
     * This is a debug function that prints the current representation of the graph to
     * the screen.
     */
    void print_state_str();

    /*
     * Returns the sequence stored in W and prints the node
     * information in an overview. 
     * Useful for debugging purposes.
     */
    void print_seq();

    void print_adj_list();

    /**
     * Take the current graph content and store in a file.
     *
     */
    void toFile(unsigned int total = 1, unsigned int idx = 0); 

    /**
     * Visualization, Serialization and Deserialization of annotation content.
     */
    void annotationToScreen();
    void annotationToFile();
    void annotationFromFile();

};
#endif
