#ifndef MEDIENINFO_DISCRETECOSINTRANSFORM_H
#define MEDIENINFO_DISCRETECOSINTRANSFORM_H

#include <math.h>
#include "AbstractCosinusTransform.h"

template<typename T>
class DirectCosinusTransform : AbstractCosinusTransform<T> {

    void transformChannel(const ColorChannel<T> &channel, std::vector<T> &output) override {
        int size = 8;
        for(int i = 0; i < output.size(); i++) {
            for(int j = 0; j < output.size(); j++) {
               double sum = 0;
                for (int x = 0; x < size; x++) {
                    for (int y = 0; y < size; y++) {
                        sum += channel[x*y]*cos(((2*x+1)*i*M_PI)/2*size) * cos(((2*y +1)*j*M_PI)/2*size);
                     }
                }
                output[i*j] = 2/size * C(i)*C(j)*sum;
            }
        }

    }

    double C(int value) {
        if (value == 0) {
            return M_SQRT1_2;
        } else {
            return 1;
        }
    }
};


#endif //MEDIENINFO_DISCRETECOSINTRANSFORM_H