#ifndef __CONFIG_HPP__
#define __CONFIG_HPP__

    struct CFG 
    {
        //String<char> > fname;
        std::vector<std::string> fname;
        std::string outfbase;
        std::string infbase;
        std::string sqlfbase;
        bool verbose;
        bool integrate;
        unsigned int k;

        CFG() :
            verbose(false), k(31)
        {}
    };




#endif
