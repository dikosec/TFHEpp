#include <bitset>
#include <cassert>
#include <chrono>
#include <iostream>
#include <random>
#include <tfhe++.hpp>

using namespace std;
using namespace TFHEpp;

template <uint32_t address_bit>
void RAMUX(TRLWElvl1 &res, const array<TRGSWFFTlvl1, address_bit> &invaddress,
           const array<TRLWElvl1, 1 << address_bit> &data)
{
    constexpr uint32_t num_trlwe = 1 << address_bit;
    array<TRLWElvl1, num_trlwe / 2> temp;

    for (uint32_t index = 0; index < num_trlwe / 2; index++) {
        CMUXFFTlvl1(temp[index], invaddress[0], data[2 * index],
                    data[2 * index + 1]);
    }

    for (uint32_t bit = 0; bit < (address_bit - 2); bit++) {
        const uint32_t stride = 1 << bit;
        for (uint32_t index = 0; index < (num_trlwe >> (bit + 2)); index++) {
            CMUXFFTlvl1(temp[(2 * index) * stride], invaddress[bit + 1],
                        temp[(2 * index) * stride],
                        temp[(2 * index + 1) * stride]);
        }
    }
    constexpr uint32_t stride = 1 << (address_bit - 2);
    CMUXFFTlvl1(res, invaddress[address_bit - 1], temp[0], temp[stride]);
}

int main()
{
    constexpr uint32_t address_bit = 9;  // Address by bytes.
    constexpr uint32_t memsize = 1 << address_bit;
    random_device seeder;
    default_random_engine engine(seeder());
    uniform_int_distribution<uint8_t> binary(0, 1);

    SecretKey *sk = new SecretKey;
    CloudKey *ck = new CloudKey(*sk);
    vector<uint8_t> pmemory(memsize);
    vector<array<uint32_t, DEF_N>> pmu(memsize);
    vector<uint8_t> address(address_bit);
    uint8_t pres;
    uint8_t wrflag;
    uint8_t writep;

    for (uint8_t &p : pmemory) p = binary(engine);

    for (int i = 0; i < memsize; i++) {
        pmu[i] = {};
        pmu[i][0] = pmemory[i] ? DEF_μ : -DEF_μ;
    }
    for (uint8_t &p : address) p = binary(engine);
    uint32_t addressint = 0;
    for (int i = 0; i < address_bit; i++)
        addressint += static_cast<uint32_t>(address[i]) << i;

    wrflag = binary(engine);
    writep = pmemory[addressint] > 0 ? 0 : 1;

    array<array<TRGSWFFTlvl1, address_bit>, 2> *bootedTGSW =
        new array<array<TRGSWFFTlvl1, address_bit>, 2>;
    vector<TLWElvl0> encaddress(address_bit);
    array<TRLWElvl1, memsize> encmemory;
    TLWElvl1 encreadreslvl1;
    TLWElvl0 encreadres;
    TRLWElvl1 encumemory;
    TLWElvl0 cs;
    TLWElvl0 c1;
    TRLWElvl1 writed;

    encaddress = bootsSymEncrypt(address, *sk);
    for (int i = 0; i < memsize; i++)
        encmemory[i] = trlweSymEncryptlvl1(pmu[i], DEF_α, (*sk).key.lvl1);
    cs = tlweSymEncryptlvl0(wrflag ? DEF_μ : -DEF_μ, DEF_α, (*sk).key.lvl0);
    c1 = tlweSymEncryptlvl0(writep ? DEF_μ : -DEF_μ, DEF_α, (*sk).key.lvl0);

    chrono::system_clock::time_point start, end;
    start = chrono::system_clock::now();
    // Addres CB
    for (int i = 0; i < address_bit; i++) {
        CircuitBootstrappingFFTwithInv((*bootedTGSW)[1][i], (*bootedTGSW)[0][i],
                                       encaddress[i], (*ck).ck);
    }

    // Read
    RAMUX<address_bit>(encumemory, (*bootedTGSW)[0], encmemory);
    SampleExtractIndexlvl1(encreadreslvl1, encumemory, 0);
    IdentityKeySwitchlvl10(encreadres, encreadreslvl1, (*ck).gk.ksk);

    // Write
    HomMUXwoSE(writed, cs, c1, encreadres, (*ck).gk);
    for (int i = 0; i < memsize; i++) {
        const bitset<address_bit> addressbitset(i);
        TRLWElvl1 temp = writed;
        for (int j = 0; j < address_bit; j++)
            CMUXFFTlvl1(temp, (*bootedTGSW)[addressbitset[j]][j], temp,
                        encmemory[i]);
        TLWElvl1 temp2;
        SampleExtractIndexlvl1(temp2, temp, 0);
        TLWElvl0 temp3;
        IdentityKeySwitchlvl10(temp3, temp2, (*ck).gk.ksk);
        GateBootstrappingTLWE2TRLWEFFTlvl01(encmemory[i], temp3, (*ck).gk);
    }

    end = chrono::system_clock::now();
    double elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count();
    cout << elapsed << "ms" << endl;
    pres = tlweSymDecryptlvl0(encreadres, (*sk).key.lvl0);

    assert(static_cast<int>(pres) == static_cast<int>(pmemory[addressint]));

    array<bool, DEF_N> pwriteres =
        trlweSymDecryptlvl1(encmemory[addressint], (*sk).key.lvl1);
    assert(static_cast<int>(pwriteres[0]) ==
           static_cast<int>((wrflag > 0) ? writep : pmemory[addressint]));

    cout << "Passed" << endl;
}