#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup(){

    minimised=false;
    ofSetVerticalSync(false);
    
    XML.loadFile("cameraSettings.xml");
    auto getCameraSettingInt = [this](const string & tag, int defaultValue) {
        if (XML.tagExists(tag)) {
            return XML.getValue(tag, defaultValue);
        }

        return XML.getValue("settings:" + tag, defaultValue);
    };
    auto getCameraSettingString = [this](const string & tag, const string & defaultValue) {
        if (XML.tagExists(tag)) {
            return XML.getValue(tag, defaultValue);
        }

        return XML.getValue("settings:" + tag, defaultValue);
    };
    
    camWidth		= getCameraSettingInt("CAMWIDTH", 640);
    camHeight	= getCameraSettingInt("CAMHEIGHT", 480);
    frameRate	= getCameraSettingInt("FRAMERATE", 60);
    recievePort	= getCameraSettingInt("RECIEVEPORT", 12334);
    sendPort = getCameraSettingInt("SENDPORT", 12335);
    sendIp = getCameraSettingString("SENDIP", "127.0.0.1");
    minimised = getCameraSettingInt("MINIMISED", 0);
    
    receiver.setup(recievePort);
    sender.setup(sendIp, sendPort);
    bHide=false;
    
    drawColour.set(0, 0, 0);
    
    setIncomingPort=false;
    setOutGoingPort=false;
    
    
//    QVGA - 15, 30, 60, 75, 100, 125, 200
//    VGA - 15, 30, 40, 50, 60, 75
    
    parameters.setName("settings");
    vector<ofVideoDevice> deviceList = ofxPS3EyeGrabber().listDevices();
    
    for (int i = 0; i < deviceList.size(); i++) {
        ofLogNotice("ofApp::setup") << "Found PS3 Eye id 0x" << ofToHex(deviceList[i].id)
                                    << " available=" << deviceList[i].bAvailable;
        if (!deviceList[i].bAvailable) {
            continue;
        }
        
        ofxSyphonServer * server = new ofxSyphonServer();
        std::shared_ptr<ofxPS3EyeGrabber> camera = std::shared_ptr<ofxPS3EyeGrabber>(new ofxPS3EyeGrabber());
        ofTexture * texture = new ofTexture();
        
        camera->setDeviceID(deviceList[i].id);
        camera->setDesiredFrameRate(frameRate);
        if (!camera->setup(camWidth, camHeight)) {
            ofLogWarning("ofApp::setup") << "Could not initialize PS3 Eye id 0x" << ofToHex(deviceList[i].id);
            delete server;
            delete texture;
            continue;
        }
        
        int camIndex = cameras.size();
        camParameterGroup params;
        params.setup(camIndex);
        camParams.push_back(params);
        camParams[camIndex].camExposure.addListener(this, &ofApp::onShutterChange);
        camParams[camIndex].camGain.addListener(this, &ofApp::onGainChange);
        camParams[camIndex].camSharpness.addListener(this, &ofApp::onSharpnessChanged);
        camParams[camIndex].camBrightness.addListener(this, &ofApp::onBrightnessChange);
        camParams[camIndex].camContrast.addListener(this, &ofApp::onContrastChange);
        camParams[camIndex].camRedBalance.addListener(this, &ofApp::onRedBalanceChanged);
        camParams[camIndex].camBlueBalance.addListener(this, &ofApp::onBlueBalanceChanged);
        camParams[camIndex].camGreenBalance.addListener(this, &ofApp::onGreenBalanceChanged);
        camParams[camIndex].camHue.addListener(this, &ofApp::onHueChange);
        camParams[camIndex].drawcam.addListener(this, &ofApp::onCamDrawChanged);
        camParams[camIndex].camAutoGain.addListener(this, &ofApp::onAutoGainAndShutterChange);
        camParams[camIndex].camAutoBalance.addListener(this, &ofApp::onAutoBalanceChanged);
        camParams[camIndex].camflipHoriz.addListener(this, &ofApp::onFlipHorizChanged);
        camParams[camIndex].camflipVert.addListener(this, &ofApp::onFlipVertChanged);
        parameters.add(camParams[camIndex].parameters);
        
        camera->setAutogain(true);
        camera->setAutoWhiteBalance(true);
        cameras.push_back(camera);
        
        server->setName("Camera " +ofToString(camIndex+1)+" Output");
        servers.push_back(server);
        
        texture->allocate(camWidth, camHeight, GL_RGBA);
        if (texture->isAllocated()) {
            textures.push_back(texture);
        }
        
        
        applyCameraSettings(camIndex);
    }
    
    camCounter=cameras.size();
    gui.setup(parameters);
    ofBackground(0, 0, 0);
    
    portInputOutgoing.setup();
    portInputOutgoing.text = ofToString(sendPort);
    portInputOutgoing.bounds.x =  20;
    portInputOutgoing.bounds.y = 40 + gui.getHeight();
    portInputOutgoing.bounds.height = 20;
    portInputOutgoing.bounds.width = 100;
    portInputOutgoing.drawCursor=true;
    
    portInputIncoming.setup();
    portInputIncoming.text = ofToString(recievePort);
    portInputIncoming.bounds.x =  20;
    portInputIncoming.bounds.y = 40 + gui.getHeight()+25;
    portInputIncoming.bounds.height = 20;
    portInputIncoming.bounds.width = 100;
    portInputIncoming.drawCursor=true;
    
    
    if (!minimised) {
        if (cameras.size()*camHeight<gui.getShape().height+80) {
            ofSetWindowShape(gui.getWidth()+80+camWidth * cameras.size(), gui.getShape().height+80);
        }
        if (cameras.size()*camHeight>gui.getShape().height+80) {
            ofSetWindowShape(gui.getWidth()+80+camWidth * cameras.size(), cameras.size()*camHeight);
        }
    }
    if (minimised) {
        ofSetWindowShape(300, 30);
    }
    
    gui.loadFromFile("settings.xml");
    applyCameraSettings();

    
}

void ofApp::exit(){
    for (auto * texture: textures) {
        delete texture;
    }
    textures.clear();
    
    for (auto * server: servers) {
        delete server;
    }
    servers.clear();
    
    cameras.clear();
}

void ofApp::update(){

    if (!portInputIncoming.isEditing()) {
        drawColour.set(0, 0, 0);
    }
    if (camCounter>0) {
        for (int i = 0; i < cameras.size(); i++) {
            cameras[i]->update();
            
            if (cameras[i]->isFrameNew()){
                textures[i]->loadData(cameras[i]->getPixels());
                servers[i]->publishTexture(textures[i]);
            }
        }
    }
    
    if (camCounter>0) {
        
        while(receiver.hasWaitingMessages()){
            ofxOscMessage m;
            receiver.getNextMessage(m);
            
            for (int i = 0; i < camCounter; i++) {
                
                if ( m.getAddress() == "/minimise" ){
                    minimised=m.getArgAsInt32(0);
                }
                
                if ( m.getAddress() == "/saveSettings" ){
                    gui.saveToFile("settings.xml");
                }
                
                if ( m.getAddress() == "/recallSaved" ){
                    gui.loadFromFile("settings.xml");
                    applyCameraSettings();
                }
                
                if ( m.getAddress() == "/"+ofToString(i+1)+"/autoGain" ){
                    camParams[i].camAutoGain =m.getArgAsInt32( 0 );
                }
                if ( m.getAddress() == "/"+ofToString(i+1)+"/autoBalance" ){
                    camParams[i].camAutoBalance =m.getArgAsInt32( 0 );
                }
                if ( m.getAddress() == "/"+ofToString(i+1)+"/drawCam" ){
                    camParams[i].drawcam =m.getArgAsInt32( 0 );
                }
                
                if ( m.getAddress() == "/"+ofToString(i+1)+"/gain" ){
                    camParams[i].camGain =m.getArgAsInt32( 0 );
                }
                
                if ( m.getAddress() == "/"+ofToString(i+1)+"/draw" ){
                    camParams[i].drawcam =m.getArgAsInt32( 0 );
                }
                
                if ( m.getAddress() == "/"+ofToString(i+1)+"/exposure" ){
                    camParams[i].camExposure = m.getArgAsInt32( 0 );
                }
                
                if ( m.getAddress() == "/"+ofToString(i)+"/sharpness" ){
                    camParams[i].camSharpness = m.getArgAsInt32( 0 );
                }
                
                if ( m.getAddress() == "/"+ofToString(i)+"/brightness" ){
                    camParams[i].camBrightness = m.getArgAsInt32( 0 );
                }
                
                if ( m.getAddress() == "/"+ofToString(i+1)+"/contrast" ){
                    camParams[i].camContrast = m.getArgAsInt32( 0 );
                }
                
                if ( m.getAddress() == "/"+ofToString(i+1)+"/hue" ){
                    camParams[i].camHue = m.getArgAsInt32( 0 );
                }
                
                if ( m.getAddress() == "/"+ofToString(i+1)+"/blueBalance" ){
                    camParams[i].camBlueBalance = m.getArgAsInt32( 0 );
                }
                
                if ( m.getAddress() == "/"+ofToString(i+1)+"/redBalance" ){
                    camParams[i].camRedBalance = m.getArgAsInt32( 0 );
                }
                
                if ( m.getAddress() == "/"+ofToString(i+1)+"/autoGain" ){
                    camParams[i].camAutoGain = m.getArgAsInt32( 0 );
                }
                
                if ( m.getAddress() == "/"+ofToString(i+1)+"/autoWhiteBalance" ){
                    camParams[i].camAutoBalance = m.getArgAsInt32( 0 );
                }
                
                if ( m.getAddress() == "/"+ofToString(i+1)+"/flip" ){
                    camParams[i].camflipVert = m.getArgAsInt32( 0 );
                    camParams[i].camflipHoriz = m.getArgAsInt32(1);
                }
            }
        }
    }
}

//--------------------------------------------------------------
void ofApp::draw(){
    if (minimised) {
        ofPushStyle();
        ofSetColor(255, 255, 255);
        if(cameras.size() == 0){
            ofDrawBitmapString("No PS3Eye found. :(", 10, 10);
        }
        if(cameras.size() != 0){
            ofDrawBitmapString(ofToString(cameras.size()) + " Cameras connected", 10, 10);
        }
        ofPopStyle();
    }
    if (!minimised) {
        ofPushStyle();
        ofBackground(0, 0, 0);
        ofSetColor(123,123,123);
        ofSetColor(255, 255, 255);
        if(cameras.size() == 0){
            ofDrawBitmapString("No PS3Eye found. :(", 20, 50);
        }
        ofPopStyle();
        
        if (camCounter>0) {
            for (int i = 0; i < cameras.size(); i++) {
                if (camParams[i].drawcam==true) {
                    
                    textures[i]->draw(gui.getWidth()+80+i * cameras[i]->getWidth(),0);
                    ofDrawBitmapString("FPS="+ofToString(cameras[i]->getFPS()) + " Dimension=" + ofToString(cameras[i]->getWidth())  +" * "+ ofToString(cameras[i]->getHeight()), gui.getWidth()+80+ i * cameras[i]->getWidth() + 20, 20);
                }
            }
        }
        if( !bHide ){
            gui.draw();
        }
    }
    
}


//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if( key == 'g' ){
        bHide = !bHide;
    }
    
    if (key=='m') {
        minimised=!minimised;
        if (minimised) {
            ofSetWindowShape(300, 30);
        }
        if (!minimised) {
            if (cameras.size()*camHeight<gui.getShape().height+80) {
                ofSetWindowShape(gui.getWidth()+80+camWidth * cameras.size(), gui.getShape().height+80);
            }
            if (cameras.size()*camHeight>gui.getShape().height+80) {
                ofSetWindowShape(gui.getWidth()+80+camWidth * cameras.size(), cameras.size()*camHeight);
            }
        }
    }
    
    if (key=='s') {
        gui.saveToFile("settings.xml");
    }
    
    if (key=='r') {
        gui.loadFromFile("settings.xml");
        applyCameraSettings();
    }
    
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){
    
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y){
    
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){
    
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){
    
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){
    
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){
    
}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){
    
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){
    
}



int ofApp::cameraIndexFromSender(const void * guiSender) const {
    const auto * parameter = static_cast<const ofAbstractParameter *>(guiSender);
    string idName = parameter->getName();
    std::size_t marker = idName.rfind(" Cam ");
    if (marker == string::npos) {
        ofLogWarning("ofApp::cameraIndexFromSender") << "Could not parse camera id from parameter: " << idName;
        return -1;
    }

    return ofToInt(idName.substr(marker + 5)) - 1;
}

bool ofApp::isValidCameraIndex(int camIndex) const {
    if (camIndex < 0 || camIndex >= cameras.size() || camIndex >= camParams.size()) {
        ofLogWarning("ofApp::isValidCameraIndex") << "Invalid camera index: " << camIndex;
        return false;
    }

    return true;
}

void ofApp::applyCameraSettings() {
    for (int i = 0; i < cameras.size(); i++) {
        applyCameraSettings(i);
    }
}

void ofApp::applyCameraSettings(int camIndex) {
    if (!isValidCameraIndex(camIndex)) {
        return;
    }

    cameras[camIndex]->setAutogain(camParams[camIndex].camAutoGain);
    cameras[camIndex]->setAutoWhiteBalance(camParams[camIndex].camAutoBalance);
    cameras[camIndex]->setBrightness(uint8_t(camParams[camIndex].camBrightness));
    cameras[camIndex]->setContrast(uint8_t(camParams[camIndex].camContrast));
    cameras[camIndex]->setSharpness(uint8_t(camParams[camIndex].camSharpness));
    cameras[camIndex]->setFlipHorizontal(camParams[camIndex].camflipHoriz);
    cameras[camIndex]->setFlipVertical(camParams[camIndex].camflipVert);

    if (!camParams[camIndex].camAutoGain) {
        cameras[camIndex]->setGain(uint8_t(camParams[camIndex].camGain));
        cameras[camIndex]->setExposure(uint8_t(camParams[camIndex].camExposure));
    }

    if (!camParams[camIndex].camAutoBalance) {
        cameras[camIndex]->setRedBalance(uint8_t(camParams[camIndex].camRedBalance));
        cameras[camIndex]->setBlueBalance(uint8_t(camParams[camIndex].camBlueBalance));
        cameras[camIndex]->setGreenBalance(uint8_t(camParams[camIndex].camGreenBalance));
        cameras[camIndex]->setHue(uint8_t(camParams[camIndex].camHue));
    }
}

void ofApp:: onAutoGainAndShutterChange(const void * guiSender,bool & value){
    int camIdNumber = cameraIndexFromSender(guiSender);
    if (!isValidCameraIndex(camIdNumber)) {
        return;
    }
    
    
    cameras[camIdNumber]->setAutogain(camParams[camIdNumber].camAutoGain);
    
    ofxOscMessage autoGainMessage;
    autoGainMessage.setAddress("/"+ofToString(camIdNumber+1)+"/autoGain" );
    autoGainMessage.addIntArg(camParams[camIdNumber].camAutoGain);
    sender.sendMessage(autoGainMessage);
    if (!cameras[camIdNumber]->getAutogain()) {
        cameras[camIdNumber]->setExposure(uint8_t(camParams[camIdNumber].camExposure));
        cameras[camIdNumber]->setGain(uint8_t(camParams[camIdNumber].camGain));
        
    }
}
void ofApp:: onGainChange(const void * guiSender,int & value){
    int camIdNumber = cameraIndexFromSender(guiSender);
    if (!isValidCameraIndex(camIdNumber)) {
        return;
    }
    if (!camParams[camIdNumber].camAutoGain) {
        cameras[camIdNumber]->setGain(uint8_t(camParams[camIdNumber].camGain));
        
    }
    ofxOscMessage gainMessage;
    gainMessage.setAddress("/"+ofToString(camIdNumber+1)+"/gain" );
    gainMessage.addIntArg(camParams[camIdNumber].camGain);
    sender.sendMessage(gainMessage);
    
}
void ofApp:: onShutterChange(const void * guiSender,int & value){
    int camIdNumber = cameraIndexFromSender(guiSender);
    if (!isValidCameraIndex(camIdNumber)) {
        return;
    }
    
    if (!camParams[camIdNumber].camAutoGain) {
        cameras[camIdNumber]->setExposure(uint8_t(camParams[camIdNumber].camExposure));
        
    }
    ofxOscMessage shutterMesage;
    shutterMesage.setAddress("/"+ofToString(camIdNumber+1)+"/exposure" );
    shutterMesage.addIntArg(camParams[camIdNumber].camExposure);
    sender.sendMessage(shutterMesage);
    
}

void ofApp::onRedBalanceChanged(const void * guiSender,int & value){
    int camIdNumber = cameraIndexFromSender(guiSender);
    if (!isValidCameraIndex(camIdNumber)) {
        return;
    }
    if (!camParams[camIdNumber].camAutoBalance) {
        cameras[camIdNumber]->setRedBalance(uint8_t(camParams[camIdNumber].camRedBalance));
        
        
    }
    ofxOscMessage redBalance;
    redBalance.setAddress("/"+ofToString(camIdNumber+1)+"/redBalance" );
    redBalance.addIntArg(camParams[camIdNumber].camRedBalance);
    sender.sendMessage(redBalance);
    
}

void ofApp::onBlueBalanceChanged(const void * guiSender,int & value){
    int camIdNumber = cameraIndexFromSender(guiSender);
    if (!isValidCameraIndex(camIdNumber)) {
        return;
    }
    if (!camParams[camIdNumber].camAutoBalance) {
    cameras[camIdNumber]->setBlueBalance(uint8_t(camParams[camIdNumber].camBlueBalance));
    
    
    }
    ofxOscMessage blueBalance;
    blueBalance.setAddress("/"+ofToString(camIdNumber+1)+"/blueBalance" );
    blueBalance.addIntArg(camParams[camIdNumber].camBlueBalance);
    sender.sendMessage(blueBalance);
}

void ofApp::onGreenBalanceChanged(const void * guiSender,int & value){
    int camIdNumber = cameraIndexFromSender(guiSender);
    if (!isValidCameraIndex(camIdNumber)) {
        return;
    }
    if (!camParams[camIdNumber].camAutoBalance) {
        cameras[camIdNumber]->setGreenBalance(uint8_t(camParams[camIdNumber].camGreenBalance));
        
        
    }
    ofxOscMessage greenBalance;
    greenBalance.setAddress("/"+ofToString(camIdNumber+1)+"/greenBalance" );
    greenBalance.addIntArg(camParams[camIdNumber].camGreenBalance);
    sender.sendMessage(greenBalance);
    
}

void ofApp::onFlipVertChanged(const void * guiSender,bool & value){
    int camIdNumber = cameraIndexFromSender(guiSender);
    if (!isValidCameraIndex(camIdNumber)) {
        return;
    }
    
    cameras[camIdNumber]->setFlipVertical(camParams[camIdNumber].camflipVert);
    
    ofxOscMessage flipMessage;
    flipMessage.setAddress("/"+ofToString(camIdNumber+1)+"/flip" );
    flipMessage.addIntArg(camParams[camIdNumber].camflipVert);
    flipMessage.addIntArg(camParams[camIdNumber].camflipHoriz);
    sender.sendMessage(flipMessage);
}
void ofApp::onFlipHorizChanged(const void * guiSender,bool & value){
    int camIdNumber = cameraIndexFromSender(guiSender);
    if (!isValidCameraIndex(camIdNumber)) {
        return;
    }
    
    cameras[camIdNumber]->setFlipHorizontal(camParams[camIdNumber].camflipHoriz);
    
    ofxOscMessage flipMessage;
    flipMessage.setAddress("/"+ofToString(camIdNumber+1)+"/flip" );
    flipMessage.addIntArg(camParams[camIdNumber].camflipVert);
    flipMessage.addIntArg(camParams[camIdNumber].camflipHoriz);
    sender.sendMessage(flipMessage);
}
void ofApp::onSharpnessChanged(const void * guiSender,int & value){
    int camIdNumber = cameraIndexFromSender(guiSender);
    if (!isValidCameraIndex(camIdNumber)) {
        return;
    }
    
    cameras[camIdNumber]->setSharpness(uint8_t(camParams[camIdNumber].camSharpness));
    
    ofxOscMessage sharpnessMessage;
    sharpnessMessage.setAddress("/"+ofToString(camIdNumber+1)+"/sharpness" );
    sharpnessMessage.addIntArg(camParams[camIdNumber].camSharpness);
    sender.sendMessage(sharpnessMessage);
}

void ofApp::onCamDrawChanged(const void * guiSender,bool & value){
    int camIdNumber = cameraIndexFromSender(guiSender);
    if (!isValidCameraIndex(camIdNumber)) {
        return;
    }
    
    
    ofxOscMessage drawMessage;
    drawMessage.setAddress("/"+ofToString(camIdNumber+1)+"/draw" );
    drawMessage.addIntArg(camParams[camIdNumber].drawcam);
    sender.sendMessage(drawMessage);
}

void ofApp::onAutoBalanceChanged(const void * guiSender,bool & value){
    int camIdNumber = cameraIndexFromSender(guiSender);
    if (!isValidCameraIndex(camIdNumber)) {
        return;
    }
    
    cameras[camIdNumber]->setAutoWhiteBalance(camParams[camIdNumber].camAutoBalance);
    
    ofxOscMessage autoBalanceMessage;
    autoBalanceMessage.setAddress("/"+ofToString(camIdNumber+1)+"/autoBalance" );
    autoBalanceMessage.addIntArg(camParams[camIdNumber].camAutoBalance);
    sender.sendMessage(autoBalanceMessage);
    
    if (!cameras[camIdNumber]->getAutoWhiteBalance()) {
        cameras[camIdNumber]->setHue(uint8_t(camParams[camIdNumber].camHue));
        cameras[camIdNumber]->setBlueBalance(uint8_t(camParams[camIdNumber].camBlueBalance));
        cameras[camIdNumber]->setRedBalance(uint8_t(camParams[camIdNumber].camRedBalance));
        cameras[camIdNumber]->setGreenBalance(uint8_t(camParams[camIdNumber].camGreenBalance));
    }
}

void ofApp:: onBrightnessChange(const void * guiSender,int & value){
    int camIdNumber = cameraIndexFromSender(guiSender);
    if (!isValidCameraIndex(camIdNumber)) {
        return;
    }
    
    cameras[camIdNumber]->setBrightness(uint8_t(camParams[camIdNumber].camBrightness));
    
    ofxOscMessage brightnessMessage;
    brightnessMessage.setAddress("/"+ofToString(camIdNumber+1)+"/brightness" );
    brightnessMessage.addIntArg(camParams[camIdNumber].camBrightness);
    sender.sendMessage(brightnessMessage);
}

void ofApp:: onContrastChange(const void * guiSender,int & value){
    int camIdNumber = cameraIndexFromSender(guiSender);
    if (!isValidCameraIndex(camIdNumber)) {
        return;
    }
    
    cameras[camIdNumber]->setContrast(uint8_t(camParams[camIdNumber].camContrast));
    
    ofxOscMessage contrastMessage;
    contrastMessage.setAddress("/"+ofToString(camIdNumber+1)+"/contrast" );
    contrastMessage.addIntArg(camParams[camIdNumber].camContrast);
    sender.sendMessage(contrastMessage);
}

void ofApp:: onHueChange(const void * guiSender,int & value){
    int camIdNumber = cameraIndexFromSender(guiSender);
    if (!isValidCameraIndex(camIdNumber)) {
        return;
    }
    if (!camParams[camIdNumber].camAutoBalance) {
    cameras[camIdNumber]->setHue(uint8_t(camParams[camIdNumber].camHue));
    
    
    }
    ofxOscMessage hueMessage;
    hueMessage.setAddress("/"+ofToString(camIdNumber+1)+"/hue" );
    hueMessage.addIntArg(camParams[camIdNumber].camHue);
    sender.sendMessage(hueMessage);
}
