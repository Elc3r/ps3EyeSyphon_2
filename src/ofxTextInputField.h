#pragma once

#include "ofRectangle.h"
#include <string>

class ofxTextInputField {
public:
    void setup() {}
    bool isEditing() const { return false; }

    std::string text;
    ofRectangle bounds;
    bool drawCursor = false;
};
