#ifndef MEDIENINFO_PPMPARSER_H
#define MEDIENINFO_PPMPARSER_H

#include <fstream>
#include <iostream>

#include "PixelTypes.h"


using namespace std;

class PPMParser {
public:

    PPMParser() {

    }

    void parsePPM() {
        fstream FileBin("../output/test.ppm", ios::in|ios::binary);
        if (FileBin.is_open()) {
            char buffer[80] = {};
            FileBin.read(buffer, 80);
            cout <<"here it is:"<< buffer;
        }
    }
};

#endif //MEDIENINFO_PPMPARSER_H
