#ifndef MEDIENINFO_ENCODINGPROCESSOR_H
#define MEDIENINFO_ENCODINGPROCESSOR_H

#include "Image.h"
#include "dct/AbstractCosinusTransform.h"
#include "SampledWriter.h"

template<typename  T>
class EncodingProcessor {
public:
    EncodingProcessor() = default;

    template<typename Transform, typename Channel = ColorChannel<T>>
    void processChannel(const Channel& channel, const Transform& dct, SampledWriter<T>& output) {
        unsigned int blocksx = channel.widthPadded >> 3;
        unsigned int blocksy = channel.heightPadded >> 3;

        for(unsigned int x = 0; x < blocksx; ++x) {
            for(unsigned int y = 0; y < blocksy; ++y) {

                auto setter = output.getBlockSetter(x, y);
                const auto getter = channel.getBlockGetter(x, y);

                dct.transformBlock(getter, setter);
            }
        }
    }

};

#endif //MEDIENINFO_ENCODINGPROCESSOR_H
