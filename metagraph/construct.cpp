#include "construct.hpp"

#include <unordered_set>
#include <unordered_map>
#include <sstream>


namespace construct {

// add a full sequence to the graph
void add_seq(DBG_succ *G, kstring_t &seq, bool append) {

#ifdef DBGDEBUG
    G->print_seq();
    G->print_state();
    std::cout << "======================================" << std::endl;
#endif

    // Padding of the input genome / read
    if (G->W->size() == 2) {
        for (size_t j = 0; j < G->k; j++) {
            append_pos(G, 6);
#ifdef DBGDEBUG
            G->print_seq();
            G->print_state();
            std::cout << "======================================" << std::endl;
#endif
        }
    }

    /** Iterate over input sequence and enumerae all
     * k-mers.
     */

    size_t i;
    uint64_t *ckmer = new uint64_t[G->k+1];
    for (i = 0; i < std::min(seq.l, G->k); ++i) {
        ckmer[i] = 6;
    }
    ckmer[i] = 0;
    uint64_t c;
    uint64_t ind;
    if (!append) {
        ind = G->index(ckmer, i);
        if (!ind) {
            ind = G->p;
            i = 0;
        } else {
            if (G->k >= seq.l) {
                delete[] ckmer;
                return;
            }
            i = G->k;
        }
    } else {
        i = 0;
        ind = 0;
    }
    for (; i < seq.l; ++i) {
        if (i > 0 && i % 1'000 == 0) {
            std::cout << "." << std::flush;
            if (i % 10'000 == 0) {
                fprintf(stdout, "%lu - edges %lu / nodes %lu\n", i, G->get_edge_count(), G->get_node_count());
            }
        }

        c = G->get_alphabet_number(seq.s[i]);
        ind = append_pos(G, c, ckmer, ind) * !append;
        memmove(ckmer, &ckmer[1], sizeof(uint64_t)*(G->k-1));
        ckmer[G->k-1] = c;

#ifdef DBGDEBUG
        G->print_seq();
        G->print_state();
        std::cout << "======================================" << std::endl;
#endif
    }
    assert(!append || G->W->operator[](ind)==0);
    // Padding after sequence to get back into default state.
    if (G->W->operator[](ind)==0) {
        for (size_t j = 0; j < G->k; j++) {
            ind = append_pos(G, 6, ckmer, ind) * !append;
            memmove(ckmer, &ckmer[1], sizeof(uint64_t)*(G->k-1));
            ckmer[G->k-1] = 6;
#ifdef DBGDEBUG
            G->print_seq();
            G->print_state();
            std::cout << "======================================" << std::endl;
#endif
        }
        if (!append)
            ind = append_pos(G, 0, ckmer, ind);
    }

    fprintf(stdout, "edges %lu / nodes %lu\n", G->get_edge_count(), G->get_node_count());
    delete[] ckmer;
}


bool check_suffix(DBG_succ* G, char* target, std::string& suffix, const char* nt_lookup) {

    std::string cursuff = std::string(target + G->k - suffix.length(), target + G->k);

    for (std::string::iterator it = cursuff.begin(); it != cursuff.end(); ++it) {
        *it = G->alphabet[(uint8_t) nt_lookup[(uint8_t) *it]];
    }
    return cursuff == suffix;
}


void add_seq_fast(DBG_succ* G, kstring_t &seq, kstring_t &name, bool add_bridge, unsigned int parallel, std::string suffix, bool add_anno) {

#ifdef DBGDEBUG
        G->print_seq();
        G->print_state();
        std::cout << "======================================" << std::endl;
#endif

    std::vector<uint32_t> label_id;
    if (add_anno) {
        // annotation data
        std::string cur_label, label_str = (name.s ? std::string(name.s) : "");
        std::vector<std::string> labels;
        uint32_t max_label_id = 0;

        //TODO: hack to get VCFs to work
        size_t vcfind = label_str.find("VCF:");
        //std::cout << label_str << " " << vcfind << "\n";
        if (vcfind == 0) {
            //std::cout << "VCF found\n";
            std::istringstream is(label_str);
            std::getline(is, cur_label, ':');
            while (std::getline(is, cur_label, ':')) {
                labels.push_back(cur_label);
                //std::cout << cur_label << "\n";
            }
            //std::cout << "\n";
        } else {
            labels.push_back(label_str);
        }

        // translate label strings into label IDs
        for (auto it = labels.begin(); it!= labels.end(); ++it) {
            std::unordered_map<std::string, uint32_t>::iterator id_it = G->label_to_id_map.find(*it);
            if (id_it == G->label_to_id_map.end()) {
                label_id.push_back((uint32_t) G->id_to_label.size());
                G->id_to_label.push_back(*it);
                G->label_to_id_map[*it] = label_id.back();
                max_label_id = std::max(max_label_id, label_id.back());
            } else {
                label_id.push_back(id_it->second);
            }
        }

        if (max_label_id >= G->annotation_full.size()) {
            G->annotation_full.resize(max_label_id + 1);
        }
    }

    // translate from ascii into talphabet
    const char nt_lookup[128] = {
        5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
        5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
        5, 5, 5, 5,  0, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
        5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,  5, 5, 5, 5,
        5, 1, 5, 2,  5, 5, 5, 3,  5, 5, 5, 5,  5, 5, 5, 5,
        5, 5, 5, 5,  4, 4, 5, 5,  6, 5, 5, 5,  5, 5, 5, 5,
        5, 1, 5, 2,  5, 5, 5, 3,  5, 5, 5, 5,  5, 5, 5, 5,
        5, 5, 5, 5,  4, 4, 5, 5,  6, 5, 5, 5,  5, 5, 5, 5
    };

    // ther is nothing to parse
    if (!seq.l) {
        return;
    }

	char *bridge = (char*) malloc(G->k+2);
    memset(bridge, 'X', G->k);
    bridge[G->k] = seq.s[0];
    bridge[G->k+1] = 0;

    size_t i = 0;
    //std::cout << "Loading next sequence with " << parallel << " threads\n";
    if (add_bridge) {
        for (i = 0; i < std::min(G->k, seq.l); ++i) {
            if (check_suffix(G, bridge, suffix, nt_lookup)) {
                G->kmers.push_back(kmer_boost::KMer{kmer_boost::stokmer(bridge, G->k+1, nt_lookup), 0});
            }
            memmove(bridge, bridge+1, G->k);
            bridge[G->k] = (i+1 < seq.l) ? seq.s[i+1] : 'X';
        }
    }
    if (G->k < seq.l) {
        #pragma omp parallel num_threads(parallel)
        {
            std::vector<kmer_boost::KMer> kmer_priv;
            #pragma omp for nowait
            for (i = 0; i < seq.l - G->k; ++i) {
                if (check_suffix(G, seq.s + i, suffix, nt_lookup)) {
                    if (add_anno) {
                        for (auto it = label_id.begin(); it != label_id.end(); ++it) {
                            kmer_priv.push_back(kmer_boost::KMer{kmer_boost::stokmer(seq.s+i, G->k+1, nt_lookup), *it});
                        }
                    } else {
                        kmer_priv.push_back(kmer_boost::KMer{kmer_boost::stokmer(seq.s+i, G->k+1, nt_lookup), 0});
                    }
                }
            }
            #pragma omp critical
            G->kmers.insert(G->kmers.end(), std::make_move_iterator(kmer_priv.begin()), std::make_move_iterator(kmer_priv.end()));
        }
        memcpy(bridge, seq.s + seq.l - G->k, G->k);
        bridge[G->k]='X';
    }
    if (add_bridge) {
        for (i = 0; i < G->k; ++i) {
            if (check_suffix(G, bridge, suffix, nt_lookup)) {
                G->kmers.push_back(kmer_boost::KMer{kmer_boost::stokmer(bridge, G->k+1, nt_lookup), 0});
            }
            memmove(bridge, bridge+1, G->k);
            bridge[G->k] = 'X';
        }
    }
    free(bridge);
}

//removes duplicate kmers, counts coverage, and assigns labels
std::vector<kmer_boost::KMer>::iterator unique_count(std::vector<kmer_boost::KMer>::iterator first, std::vector<kmer_boost::KMer>::iterator last, bool add_anno) {

    //Adapted from http://en.cppreference.com/w/cpp/algorithm/unique
    if (first == last)
        return last;

    std::vector<kmer_boost::KMer>::iterator result = first;
    if (add_anno) {
        /*
        cpp_int annot;
        if (result->annot != 0) {
            annot = (cpp_int(1) << static_cast<size_t>(result->annot-1));
        } else {
            annot = 0;
        }
        */
        std::vector<uint32_t> annot;
        if (result->annot.size() > 0) {
            annot = result->annot;
        }

        while (++first != last) {
            if (!(result->seq == first->seq)) {
                result->annot = annot;
                if (++result != first) {
                    *result = std::move(*first);
                }
                /*
                if (result->annot != 0) {
                    annot = (cpp_int(1) << static_cast<size_t>(result->annot-1));
                } else {
                    annot = 0;
                }
                */
                if (result->annot.size() > 0) {
                    annot = result->annot;
                } else {
                    annot.clear();
                }

            }
            //if (first->annot != 0) {
            if (first->annot.size() > 0) {
                //num_annots = std::max(num_annots, static_cast<size_t>(first->annot));
                //TODO: add check for first->annot being too big
                //annot |= (cpp_int(1) << static_cast<size_t>(first->annot-1));
                annot.push_back(first->annot.back());
            }
        }
        result->annot = annot;
    } else {
        while (++first != last) {
            if (!(result->seq == first->seq) && (++result != first)) {
                *result = std::move(*first);
            }
        }
    }
    return ++result;
}


void construct_succ(DBG_succ* G, unsigned int parallel, bool add_anno) {

    // parallel sort of all kmers
    omp_set_num_threads(std::max((int)parallel,1));
    __gnu_parallel::sort(G->kmers.begin(),G->kmers.end());
    std::vector<kmer_boost::KMer>::iterator uniq_count = unique_count(G->kmers.begin(), G->kmers.end(), add_anno);
    G->kmers.erase(uniq_count, G->kmers.end() );

    // Annotation:
    //TODO: set the bit vector, then store it
    //TODO: when merging into G, keep track of the fact that this may have a different number of annotations that the rest of hte graph
    size_t max_anno_id = G->annotation_full.size();

    //DEBUG: output kmers in current bin
    /*
    std::cerr << "\n";
    for (size_t i=0;i<G->kmers.size();++i) {
        char* curseq = kmertos(G->kmers[i].first, G->alphabet, G->alph_size);
        std::cerr << G->kmers[i].first << "\t" << curseq+1 << " " << curseq[0] << " " << G->kmers[i].second << "\n";
        free(curseq);
    }
    */

    size_t curpos = G->W_stat.size();
    G->W_stat.resize(G->W_stat.size() + G->kmers.size());
    G->last_stat_safe.resize(G->last_stat_safe.size() + G->kmers.size(), true);
    G->coverage.resize(G->coverage.size() + G->kmers.size(),0);
    //G->bridge_stat.resize(G->bridge_stat.size() + G->kmers.size(), false);

    #pragma omp parallel num_threads(parallel)
    {
        #pragma omp for nowait
        for (size_t i = 0; i < G->kmers.size(); ++i) {
            //set last
            if (i + 1 < G->kmers.size()) {
                bool dup = kmer_boost::compare_kmer_suffix(G->kmers[i].seq, G->kmers[i+1].seq);
                if (dup) {
                    G->last_stat_safe[curpos + i] = false;
                }
            }
            //set W
            uint8_t curW = kmer_boost::getW(G->kmers[i].seq);
            if (curW == 127) {
                char* curseq = kmer_boost::kmertos(G->kmers[i].seq, G->alphabet, G->alph_size);
                std::cerr << "Failure decoding kmer " << i << "\n" << G->kmers[i].seq << "\n" << curseq << "\n";
                free(curseq);
                exit(1);
            }
            if (!curW && curpos+i)
                G->p = curpos + i;
            if (i) {
                for (size_t j = i - 1; kmer_boost::compare_kmer_suffix(G->kmers[j].seq, G->kmers[i].seq, 1); --j) {
                    //TODO: recalculating W is probably faster than doing a pragma for ordered
                    if (kmer_boost::getW(G->kmers[j].seq) == curW) {
                        curW += G->alph_size;
                        break;
                    }
                    if (!j)
                        break;
                }
            }
            G->W_stat[curpos+i] = curW;
            /*
            if (add_anno && G->kmers[i].annot > 0)
                max_anno_id = std::max(max_anno_id, (size_t) msb(G->kmers[i].annot));
            */
            if (add_anno && G->kmers[i].annot.size() > 0)
                max_anno_id = std::max(max_anno_id, (size_t) *std::max_element(G->kmers[i].annot.begin(), G->kmers[i].annot.end()));
        }
    }
    for (size_t i = 0; i < G->kmers.size(); ++i) {
        char cF = kmer_boost::getPos(G->kmers[i].seq, G->k-1, G->alphabet, G->alph_size);
        if (cF != G->alphabet[G->lastlet]) {
            for ((G->lastlet)++; G->lastlet<G->alph_size; (G->lastlet)++) {
                G->F[G->lastlet] = curpos + i - 1;
                if (G->alphabet[G->lastlet] == cF) {
                    break;
                }
            }
        }
    }
    if (add_anno) {
        for (size_t i = G->annotation_full.size(); i <= max_anno_id; ++i)
            G->annotation_full.push_back(NULL);

        #pragma omp parallel num_threads(parallel)
        {
            #pragma omp for nowait
            // loop over annotation columns
            for (size_t i = 0; i <= max_anno_id; ++i) {
                //std::cerr << i << std::endl;
                sdsl::bit_vector* bv = new sdsl::bit_vector(G->W_stat.size(), 0);
                if (G->annotation_full.at(i) != NULL) {
                    sdsl::select_support_sd<> slct = sdsl::select_support_sd<>(G->annotation_full.at(i));
                    sdsl::rank_support_sd<> rank = sdsl::rank_support_sd<>(G->annotation_full.at(i));
                    size_t maxrank = rank(G->annotation_full.at(i)->size());
                    size_t idx;
                    for (size_t j = 1; j <= maxrank; ++j) {
                        idx = slct(j);
                        if (idx < G->annotation_full.at(i)->size()) {
                            bv->operator[](idx) = G->annotation_full.at(i)->operator[](idx);
                        }
                    }
                }
                for (size_t k = 0; k < G->kmers.size(); ++k) {
                    std::vector<uint32_t>::iterator it = std::find(G->kmers.at(k).annot.begin(), G->kmers.at(k).annot.end(), i);
                    if ((G->kmers.at(k).annot.size() > 0) && (it != G->kmers.at(k).annot.end()))
                        bv->operator[](k + curpos) = true;
                    /*
                    if ((G->kmers.at(k).annot > 0) && (msb(G->kmers.at(k).annot) >= i) && ((cpp_int(1)<<i) & G->kmers.at(k).annot))
                        bv->operator[](k + curpos) = true;
                    */
                }
                if (G->annotation_full.at(i) != NULL)
                    delete G->annotation_full.at(i);
                G->annotation_full.at(i) = new sdsl::sd_vector<>(*bv);
                delete bv;
            }
        }
    }
    G->kmers.clear();
}


/** This function takes a character c and appends it to the end of the graph sequence
 * given that the corresponding note is not part of the graph yet.
 */
uint64_t append_pos(DBG_succ* G, uint64_t c, uint64_t *ckmer, uint64_t i) {

    // check that the last position of the graph is indeed a terminal
    assert((*(G->W))[G->p] == 0);
    uint64_t *p;
    bool append = false;
    uint64_t c_p = G->get_node_end_value(G->p);
    // get range of identical nodes (without W) pos current end position
    std::pair<uint64_t, uint64_t> R;
    //std::pair<uint64_t, uint64_t> R = G->get_equal_node_range(G->p);
    //fprintf(stdout, "range [%i %i]\n", (int) R.first, (int) R.second);

    if (!i) {
        p = &(G->p);
        append = true;
        R.second = G->succ_last(*p);
    } else {
        append = false;
        R = G->get_equal_node_range(G->p);
        p = &(R.first);
    }

    // get position of first occurence of c in W after p
    uint64_t next_c = G->succ_W(*p, c);
    // check if c is part of range
    bool exist_c = (next_c <= R.second);
    if (!exist_c) {
        // get position of first occurence of c- in W after p
        next_c = G->succ_W(*p, c + G->alph_size);
        // check if c- is part of range
        exist_c = (next_c <= R.second);
    }

    /**
     * if the character already exists in the range, we delete the terminal symbol
     * at p, insert c at fwd(next_c) and update p.
     */
    if (exist_c) {
        if (!append) {
            //if not appending, then this has already been observed and we only need to delete the extra pointer
            for (i=0;G->W->operator[](*p)==0;++i,++(*p)) {
                if (*p==(G->p))
                    continue;
                if (G->debug) {
                    assert(i<2);
                    assert(*p < R.second);
                }
                G->W->remove(*p);
                G->last->deleteBit(*p);
                G->update_F(c_p, false);
                if (*p <= G->p) {
                    (G->p)--;
                    assert(G->W->operator[](*p) == 0);
                }
                if (*p <= next_c)
                    next_c--;
            }
            return G->fwd(next_c);
        }
        uint64_t p_new = G->fwd(next_c);
        // remove old terminal symbol
        G->last->deleteBit(*p);
        G->W->remove(*p);
        // adapt position if altered by previous deletion
        p_new -= (*p < p_new);
        // insert new terminal symbol
        // we have to insert 0 into last as the node already existed in the range
        // and the terminal symbol is always first
        G->last->insertBit(p_new, false);
        G->W->insert(0, p_new);
        // update new terminal position
        *p = p_new;
        // take care of updating the offset array F
        G->update_F(c_p, false);
        //assert(get_node_end_value(p) == c);
        G->update_F(c, true);
    } else {
        /**
         * We found that c does not yet exist in the current range and now have to
         * figure out if we need to add c or c- to the range.
         * To do this, we check if there is a previous position j1 with W[j1] == c
         * whose node shares a k-1 suffix with the current node. If yes, we add c-
         * instead of c.
         */
        if (!append) {
            //we need to insert a new pointer
            if (*p == G->p)
                (*p)++;
            if (G->W->operator[](*p)) {
                //if no placeholder exists
                G->W->insert(0, *p);
                G->update_F(c_p, true);
                G->last->insertBit(*p, false);
                R.second++;
                if (*p <= G->p) {
                    (G->p)++;
                    assert(G->W->operator[](G->p) == 0);
                }
            }
        }
        // get position of last occurence of c before p (including p - 1)
        uint64_t last_c = G->pred_W(*p - 1, c);
        // if this position exists
        if (last_c > 0) {
            uint64_t x = G->fwd(last_c);
            assert((*(G->last))[x]); // this should always be true - unless x is 0 - I do not get the logic in the reference implementation

            // check, if there are any c or c- symbols following after position p
            uint64_t next_c = G->succ_W(*p + 1, c);
            uint64_t next_cm = G->succ_W(*p + 1, c + G->alph_size);
            // there is no c between p and next_cm and next_cm is a c- ==> we should add a c-
            // all nodes between W[i] = c and W[j] = c- share a common suffix of length k-1
            bool minus1 = (next_cm < next_c);
            // check, if we share a k-1 suffix with last_c
            if (!minus1) {
                if (ckmer) {
                    minus1 = G->compare_node_suffix(ckmer, last_c);
                } else {
                    minus1 = G->compare_node_suffix(*p, last_c);
                }
            }

            // adding a new node can influence following nodes that share a k-1 suffix with the
            // new node -> need to adapt the respektive cc to a cc-
            bool minus2 = false;
            if (next_c < G->W->size()) {
                if (ckmer) {
                    minus2 = G->compare_node_suffix(ckmer, next_c);
                } else {
                    minus2 = G->compare_node_suffix(*p, next_c);
                }
                if (minus2) {
                    G->replaceW(next_c, (*(G->W))[next_c] + G->alph_size);
                }
            }

            G->replaceW(*p, minus1 ? c + G->alph_size : c);
            // after we are done, assert that the order within the range we created
            // is still valid within W
            if (*p - R.second > 0) {
                G->sort_W_locally(*p, R.second);
            }

            // if minus1 is true, we share a k-1 suffix with the node at
            // last_c and thus need to adapt our insertion position by -1,
            // as we would like to insert before it. Otherwise we insert directly after
            // it as we are now sorted after it.
            if (minus1) {
                *p = x;
                G->last->insertBit(x, false);
                G->W->insert(0, x);
            } else if (minus2) {
                *p = x + 1;
                G->last->insertBit(x + 1, false);
                G->W->insert(0, x + 1);
            // no node shares a k-1 suffix with last_c and thus the new node comes after
            // the forward of last_c (as the current node came after last_c as well)
            } else {
                *p = x + 1;
                G->last->insertBit(x + 1, true);
                G->W->insert(0, x + 1);
            }
        } else {
            uint64_t x = G->F[c] + 1;
            uint64_t next_c = G->succ_W(*p + 1, c);
            bool minus = false;
            if (next_c < G->W->size()) {
                if (ckmer) {
                    minus = G->compare_node_suffix(ckmer, next_c);
                } else {
                    minus = G->compare_node_suffix(*p, next_c);
                }
            }
            G->replaceW(*p, c);
            if (*p - R.second > 0) {
                G->sort_W_locally(*p, R.second);
            }
            *p = x;
            if (minus) {
                G->replaceW(next_c, (*(G->W))[next_c] + G->alph_size);
                G->last->insertBit(x, false);
            } else {
                G->last->insertBit(x, true);
            }
            G->W->insert(0, x);
        }
        if (*p < G->p || (!append && *p == G->p)) {
            (G->p)++;
            assert(G->W->operator[](G->p) == 0);
        }
        G->update_F(c, true);
    }
    // update sorting at new location of p
    // with this we assert that $ is always inserted at the first position
    // of a range of equal nodes --> this will help us to prevent multiple insertions
    // of already existing nodes
    R = G->get_equal_node_range(*p);
    if (R.second - R.first > 0) {
        G->sort_W_locally(R.first, R.second);
        *p = R.first;
        if (!append && *p == G->p)
            (*p)++;
        /*
        while ((*(G->W))[*p] != 0)
            (*p)--;
        */
        assert(G->W->operator[](*p)==0);
    }
    return *p;
}


/** This function takes a pointer to a graph structure and concatenates the arrays W, last
 * and F to this graph's arrays. In almost all cases this will not produce a valid graph and
 * should only be used as a helper in the parallel merge procedure.
 */
void append_graph(DBG_succ* G_s, DBG_succ* G_t) {

    size_t curr_pos = G_t->get_size();

    if (G_t->config->verbose)
        std::cout << "    adding " << G_s->get_size() << " edges" << std::endl;
    // handle last and W
    for (size_t j = 1; j < G_s->get_size(); ++j) {
        G_t->last->insertBit(curr_pos, G_s->get_last(j));
        G_t->W->insert(G_s->get_W(j), curr_pos);
        ++curr_pos;
    }
    if (G_t->config->verbose)
        std::cout << "new total edges: " << G_s->W->size() << std::endl;

    // handle F
    assert(G_t->F.size() == G_s->F.size());
    for (size_t j = 0; j < G_t->F.size(); ++j) {
        G_t->F.at(j) += G_s->F.at(j);
    }
}


/**
 * This function takes a pointer to a graph structure and concatenates the arrays W, last
 * and F to this graph's static containers last_stat and W_stat. In almost all cases
 * this will not produce a valid graph and should only be used as a helper in the
 * parallel merge procedure.
 */
void append_graph_static(DBG_succ *G_t, DBG_succ *G_s) {

    size_t n = G_s->get_size();
    if (G_t->config->verbose)
        std::cout << "    adding " << n << " edges" << std::endl;

    //size_t n_old = this->last_stat.size();
    //this->last_stat.resize(n_old + n);
    //this->W_stat.resize(n_old + n);

    const size_t b = 4;
    std::vector<uint64_t> offsets (1ull << b, 0);
    std::queue<uint64_t> blocks;
    std::queue<uint64_t> new_blocks;
    blocks.push(n);
    size_t pos = 0;
    uint64_t o = 0;
    for (size_t ib = 0; ib < b; ++ib) {
        while (!blocks.empty()) {
            uint64_t cnt = blocks.front();
            blocks.pop();
            uint64_t epos = pos + cnt;
            for ( ; pos < epos; ++pos) {
                offsets.at(o) += !G_s->W->get_bit_raw(pos);
            }
            if (ib < b - 1) {
                new_blocks.push(offsets.at(o));
                new_blocks.push(cnt - offsets.at(o));
            }
            o++;
        }
        if (ib < b - 1)
            blocks.swap(new_blocks);
    }

    //std::cerr << "R size: " << G_s->W->R->size() << std::endl;

    bool bit;
    std::vector<uint64_t> upto_offsets ((1ull << (b - 1)) - 1, 0);
    uint64_t p, co, v, m;
    for (size_t i = 0; i < n; ++i) {
        m = (1ull << (b - 1));
        //v = (uint64_t) W_stat.at(i);
        v = 0;
        o = 0;
        p = i;
        co = 0;
        for (size_t ib = 0; ib < b - 1; ++ib) {
            bit = G_s->W->get_bit_raw(ib * n + p + co);
            if (bit) {
                v |= m;
                co += offsets.at(o);
                p -= upto_offsets.at(o);
            } else {
                p -= (p - upto_offsets.at(o));
                upto_offsets.at(o) += 1;
            }
            o = 2*o + 1 + bit;
            m >>= 1;
        }
        bit = G_s->W->get_bit_raw((b - 1) * n + p + co);
        if (bit) {
            v |= m;
        }
        if (i == 0)
            continue;
        G_t->W_stat.push_back(v);
        G_t->last_stat.push_back(G_s->get_last(i));
    }

    // handle F
    assert(G_t->F.size() == G_s->F.size());
    for (size_t j = 0; j < G_t->F.size(); ++j) {
        G_t->F.at(j) += G_s->F.at(j);
    }
}

// Given an edge list, remove them from the graph.
// If a ref_point is given, it's updated index is return, otherwise return 0
uint64_t remove_edges(DBG_succ *G, std::set<uint64_t> &edges, uint64_t ref_point) {
    uint64_t offset = 0;
    uint64_t i, j, d;

    for (auto x = edges.begin(); x != edges.end(); ++x) {
        assert(*x >= offset);
        d = G->W->operator[](*x - offset);
        if (d < G->alph_size) {
            //fix W array
            j = G->succ_W(*x-offset + 1, d);
            for (i = *x - offset + 1; i < j; ++i) {
                if (G->W->operator[](i) == d + G->alph_size) {
                    G->replaceW(i, d);
                    break;
                }
            }
        }
        G->W->remove(*x - offset);
        G->update_F(G->get_node_end_value(*x - offset), false);
        //if the current node has multiple outgoing edges, remove one of the 0s from
        //last instead of 1
        if (G->get_last(*x-offset) && (*x >= offset + 1) && !G->get_last(*x - offset - 1)) {
            G->last->deleteBit(*x - offset - 1);
        } else {
            G->last->deleteBit(*x - offset);
        }
        //fix pointers
        if (ref_point && *x-offset <= ref_point)
            ref_point--;
        if (*x-offset <= G->p) {
            (G->p)--;
            assert(G->W->operator[](G->p) == 0);
        }
        offset++;
    }
    return ref_point;
}

//Given a graph and a minimum number of splits, generate a list of suffices from the alphabet
std::deque<std::string> generate_suffices(DBG_succ *G, unsigned int nsplits) {
    unsigned int suffix_len = (unsigned int) ceil(log2(nsplits) / log2(G->alph_size - 1));

    //should be set to at most k-1 so that W calculation is correct
    suffix_len = std::min(suffix_len, (unsigned int) G->k - 1);
    std::deque<std::string> suffices = {""};
    for (size_t i = 0; i < suffix_len; ++i) {
         while (suffices[0].length() < suffix_len) {
             for (size_t j = 0; j < G->alph_size; ++j) {
                  suffices.push_back(G->alphabet[j] + suffices[0]);
             }
             suffices.pop_front();
         }
    }
    assert(suffices.size() == pow(G->alph_size, suffix_len));
    return suffices;
}

void add_sink(DBG_succ* G, unsigned int parallel, std::string suffix, bool add_anno) {
    add_seq_fast(G, G->start, G->blank, false, parallel, suffix, add_anno);
    add_seq_fast(G, G->graphsink, G->blank, true, parallel, suffix, add_anno);
}

} // construct
