#include <cassert>
#include <fstream>
#include <tfhe++.hpp>

using namespace TFHEpp;

int main()
{
    SecretKey sk;
    {
        std::ofstream ofs{"./secret.key", std::ios::binary};
        cereal::PortableBinaryOutputArchive ar(ofs);
        sk.serialize(ar);
    }
    SecretKey ski;
    {
        std::ifstream ifs{"./secret.key", std::ios::binary};
        cereal::PortableBinaryInputArchive ar(ifs);
        ski.serialize(ar);
    }
    for (int i = 0; i < DEF_n; i++) assert(sk.key.lvl0[i] == ski.key.lvl0[i]);
    for (int i = 0; i < DEF_N; i++) assert(sk.key.lvl1[i] == ski.key.lvl1[i]);
    for (int i = 0; i < DEF_nbar; i++)
        assert(sk.key.lvl2[i] == ski.key.lvl2[i]);
}