#ifndef MEDIENINFO_SAMPLEDWRITER_H
#define MEDIENINFO_SAMPLEDWRITER_H

#include <vector>
#include <functional>
#include <cassert>
#include <cmath>
#include <iostream>
#include "quantisation/quantisationTables.h"
#include "HuffmanEncoder.h"

template<typename T, typename Tout = int16_t>
class Pair {
public:
    T amount;
    Tout value;
    uint16_t bitPattern; // note: The actual amount of used bits is determined by the bit_pattern
    uint8_t category; // DC: Huffcode this category, then write huff(category) and then bit_pattern (unencoded)
    uint8_t pairBitwise; // AC: search this value in the huffman tree and write it to the stream, then write bitPattern
    bool DC;

    void createBitValue() {
        double log;
        if (value < 0) {
            bitPattern = static_cast<uint16_t>(value*-1);
            bitPattern = ~bitPattern;
            log = value * -1;
        } else {
            bitPattern = static_cast<uint16_t>(value);
            log = value;
        }
        log = std::log2(log);
        if (log == (int)log) {
            log++;
        }
        this->category = ceil(log);
        this->bitPattern &= BitStream::ones(this->category);
        assert(category < 16 && category >= 0);
    }

public:
    Pair(T first, Tout second) : amount(first), value(second), bitPattern(0), category(0), pairBitwise(0), DC(false) { // AC
        assert(first < 16);
        assert(first >= 0);

        createBitValue();
        assert(static_cast<int>(abs(second)) == static_cast<int>(second < 0 ? (~bitPattern & BitStream::ones(category)) : bitPattern));

        pairBitwise = amount;
        pairBitwise = pairBitwise << 4;
        pairBitwise |= category & 0x0F;
    }

    Pair(Tout nr) : amount(0), value(nr), bitPattern(0), category(0), pairBitwise(0), DC(true) { // DC
        createBitValue();
    }
};

class ZikZakLookupTable {
private:
    using uint = unsigned int;
    static constexpr void genAcLookupTable(std::array<std::array<uint, 8>, 8>& table) {

        uint x = 1, y = 0;
        bool use_x = false; // to switch between x & y
        // iterate from 0..62 since the actual 0 is dc
        for (uint i = 0; i < 63; ++i) {
            table[x][y] = i;

            if(use_x) {
                if(x == 7) {
                    use_x = false;
                    ++y;
                }
                else {
                    ++x;

                    if(y == 0) {
                        use_x = false;
                    }
                    else {
                        --y;
                    }
                }

            }
            else {
                if(y == 7) {
                    use_x = true;
                    ++x;
                }
                else {
                    ++y;

                    if (x == 0) {
                        use_x = true;
                    } else {
                        --x;
                    }
                }
            }
        }
    }

public:
    std::array<std::array<uint, 8>, 8> acLookupTable {0};
    constexpr ZikZakLookupTable() {
        genAcLookupTable(acLookupTable);
    }

};


template<typename T, typename Tout = int16_t>
class OffsetSampledWriter {
private:
    using uint = unsigned int;

    const uint rowwidth = 8;
    const uint blocksize = rowwidth * rowwidth;
    const uint acBlockSize = blocksize - 1;
    const uint size;

    std::vector<Tout> output_dc;
    std::vector<Tout> output_ac;

public:
    std::vector<Pair<uint8_t,Tout>> runLengthEncoded;
    std::array<uint32_t, 256> huffweight_ac = {0}, huffweight_dc = {0};

private:
    const ZikZakLookupTable acLookupTableGen;
    const std::array<std::array<uint, 8>, 8>& acLookupTable = acLookupTableGen.acLookupTable;
    const QuantisationTable& qTable;

public:
    explicit OffsetSampledWriter(const uint blocks, const QuantisationTable& qTable)
        : size(blocks * blocksize), qTable(qTable) {
        // resize, but substract one for each block because the first coefficient is AC
        output_ac.resize(size - blocks, 0);
        output_dc.resize(blocks, 0);
    }

    void set(const T& val, const uint block, const uint x, const uint y) {
        const auto divisor =  qTable[(x << 3) + y];
        const auto extra = val < 0 ? -(divisor >> 1) : (divisor >> 1);
        const Tout valm = static_cast<Tout>((val + extra) / divisor);
        assert(abs(static_cast<T>(val)/divisor) < 32767);

        if(x == 0 && y == 0) {
#ifdef NDEBUG
            output_dc[block] = valm;
#else
            output_dc.at(block) = valm;
#endif
        }
        else {
#ifdef NDEBUG
            output_ac[(block * acBlockSize) + acLookupTable[x][y]] = valm;
#else

            output_ac.at((block * acBlockSize) + acLookupTable[x][y]) = valm;
#endif
        }
    }

    void runLengthEncoding() {
        partialRunLengthEncoding(0, output_dc.size());
    }

    void partialRunLengthEncoding(const int start, int stop) {
        Tout prev_dc = (start == 0) ? 0 : output_dc[start - 1];
        int dc_offset = start;

        stop *= 63;
        for(int b = (start * 63); b < stop; b += 63) {

            Tout cur_dc = output_dc[dc_offset++];
            auto p = Pair<uint8_t, Tout>(cur_dc - prev_dc);
            ++huffweight_dc[p.category];
            this->runLengthEncoded.emplace_back(p);
            prev_dc = cur_dc;

            uint amountZeros = 0;
            for(int k = b; k < b+63; ++k) {
                const auto value = output_ac[k];
                if (value != 0) {
                    while(amountZeros > 15) {
                        Pair<uint8_t, Tout> temp(15,0);
                        ++huffweight_ac[temp.pairBitwise];
                        this->runLengthEncoded.emplace_back(temp);
                        amountZeros -= 16;
                    }
                    Pair<uint8_t, Tout> temp(amountZeros,value);
                    ++huffweight_ac[temp.pairBitwise];
                    runLengthEncoded.emplace_back(temp);
                    amountZeros = 0;
                } else {
                    amountZeros++;
                }
            }

            if (amountZeros != 0) {
                Pair<uint8_t, Tout> temp(0, 0);
                ++huffweight_ac[temp.pairBitwise];
                runLengthEncoded.emplace_back(temp);
            }
        }
    }
};

template<typename T>
class StreamWriter {
private:
    using HuffmanEncoder = IsoHuffmanEncoder<256, uint8_t, 16>;
    const OffsetSampledWriter<T, int16_t> channel;
    const HuffmanEncoder& ac_encoder, dc_encoder;
    const uint32_t rowWidth;
    uint32_t offset = 0;
    BitStream& stream;

public:
    StreamWriter(const OffsetSampledWriter<T, int16_t> &channel, const HuffmanEncoder &ac_encoder,
                 const HuffmanEncoder &dc_encoder, BitStream& bs, const uint32_t rowWidth) :
                 channel(channel), ac_encoder(ac_encoder), dc_encoder(dc_encoder), rowWidth(rowWidth), stream(bs) {

    }

    void skip(uint32_t amount) {
        while(amount > 0) {
#ifdef NDEBUG
            while(!channel.runLengthEncoded[++offset].DC);
#else
            while(!channel.runLengthEncoded.at(++offset).DC);
#endif
            --amount;
        }
    }

    void skipRow() {
        skip(rowWidth);
    }

    void writeBlock() {
        assert(channel.runLengthEncoded.at(offset).DC);
        writeDcPair(channel.runLengthEncoded[offset]);

        while(offset < (channel.runLengthEncoded.size() - 1)) {
            const auto& p =
#ifdef NDEBUG
                    channel.runLengthEncoded[++offset];
#else
                    channel.runLengthEncoded.at(++offset);
#endif
            if(p.DC)
                break;
            writeAcPair(p);
        }
    }

    template<typename T1>
    inline void writeAcPair(const Pair<T1, int16_t>& p) {
        ac_encoder.write(stream, p.pairBitwise);
        if (p.category != 0)
            stream.appendU16(p.bitPattern, p.category);
    }

    template<typename T1>
    inline void writeDcPair(const Pair<T1, int16_t>& p) {
        dc_encoder.write(stream, p.category);
        if (p.category != 0)
            stream.appendU16(p.bitPattern, p.category);
    }
};

#endif //MEDIENINFO_SAMPLEDWRITER_H
