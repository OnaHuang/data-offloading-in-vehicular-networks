//
// Copyright (C) 2011 David Eckhoff <eckhoff@cs.fau.de>
//
// Documentation for these modules is at http://veins.car2x.org/
//
// SPDX-License-Identifier: GPL-2.0-or-later
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "MyVeinsBaseApp.h"
#include <cstring>

using namespace veins;
using namespace std;

std::map<LAddress::L2Type, cModule*> veins::MyVeinsBaseApp::L2TocModule;
vector<Coord> veins::MyVeinsBaseApp::randomAccidentPosTable;

void MyVeinsBaseApp::initialize(int stage)
{
    BaseApplLayer::initialize(stage);
    cout<<"------------------------initialise Base-------------------"<<endl;

    if (stage == 0) {

        // Initialise pointers to other modules
        if (FindModule<TraCIMobility*>::findSubModule(getParentModule())) {
            mobility = TraCIMobilityAccess().get(getParentModule());
            traci = mobility->getCommandInterface();
            traciVehicle = mobility->getVehicleCommandInterface();
        }
        else {
            traci = nullptr;
            mobility = nullptr;
            traciVehicle = nullptr;
        }

        annotations = AnnotationManagerAccess().getIfExists();
        ASSERT(annotations);

        mac = FindModule<DemoBaseApplLayerToMac1609_4Interface*>::findSubModule(getParentModule());
        ASSERT(mac);

        // read parameters
        headerLength = par("headerLength");
        sendBeacons = par("sendBeacons").boolValue();
        beaconLengthBits = par("beaconLengthBits");
        beaconUserPriority = par("beaconUserPriority");
        beaconInterval = par("beaconInterval");
        detectAccidentMsgInterval = par("detectAccidentMsgInterval");

        dataLengthBits = par("dataLengthBits");
        dataOnSch = par("dataOnSch").boolValue();
        dataUserPriority = par("dataUserPriority");

        wsaInterval = par("wsaInterval").doubleValue();
        currentOfferedServiceId = -1;

        isParked = false;

        findHost()->subscribe(BaseMobility::mobilityStateChangedSignal, this);
        findHost()->subscribe(TraCIMobility::parkingStateChangedSignal, this);

        sendBeaconEvt = new cMessage("beacon evt", SEND_BEACON_EVT);
        sendWSAEvt = new cMessage("wsa evt", SEND_WSA_EVT);
        detectAccidentEvt = new cMessage("detect accident evt",DETECT_ACCIDENT_EVT);

        generatedBSMs = 0;
        generatedWSAs = 0;
        generatedWSMs = 0;
        receivedBSMs = 0;
        receivedWSAs = 0;
        receivedWSMs = 0;
    }
    else if (stage == 1) {

        // store MAC address for quick access
        myId = mac->getMACAddress();
        host = findHost();
        L2TocModule[myId] = host;
        // simulate asynchronous channel access

        if (dataOnSch == true && !mac->isChannelSwitchingActive()) {
            dataOnSch = false;
            EV_ERROR << "App wants to send data on SCH but MAC doesn't use any SCH. Sending all data on CCH" << std::endl;
        }
        simtime_t firstBeacon = simTime();

        if (par("avoidBeaconSynchronization").boolValue() == true) {

            simtime_t randomOffset = dblrand() * beaconInterval;
            firstBeacon = simTime() + randomOffset;

            if (mac->isChannelSwitchingActive() == true) {
                if (beaconInterval.raw() % (mac->getSwitchingInterval().raw() * 2)) {
                    EV_ERROR << "The beacon interval (" << beaconInterval << ") is smaller than or not a multiple of  one synchronization interval (" << 2 * mac->getSwitchingInterval() << "). This means that beacons are generated during SCH intervals" << std::endl;
                }
                firstBeacon = computeAsynchronousSendingTime(beaconInterval, ChannelType::control);
            }

            if (sendBeacons) {
                scheduleAt(firstBeacon, sendBeaconEvt);
            }
        }
        scheduleAt(simTime(), detectAccidentEvt);
    }
}

simtime_t MyVeinsBaseApp::computeAsynchronousSendingTime(simtime_t interval, ChannelType chan)
{

    /*
     * avoid that periodic messages for one channel type are scheduled in the other channel interval
     * when alternate access is enabled in the MAC
     */

    simtime_t randomOffset = dblrand() * interval;
    simtime_t firstEvent;
    simtime_t switchingInterval = mac->getSwitchingInterval(); // usually 0.050s
    simtime_t nextCCH;

    /*
     * start event earliest in next CCH (or SCH) interval. For alignment, first find the next CCH interval
     * To find out next CCH, go back to start of current interval and add two or one intervals
     * depending on type of current interval
     */

    if (mac->isCurrentChannelCCH()) {
        nextCCH = simTime() - SimTime().setRaw(simTime().raw() % switchingInterval.raw()) + switchingInterval * 2;
    }
    else {
        nextCCH = simTime() - SimTime().setRaw(simTime().raw() % switchingInterval.raw()) + switchingInterval;
    }

    firstEvent = nextCCH + randomOffset;

    // check if firstEvent lies within the correct interval and, if not, move to previous interval

    if (firstEvent.raw() % (2 * switchingInterval.raw()) > switchingInterval.raw()) {
        // firstEvent is within a sch interval
        if (chan == ChannelType::control) firstEvent -= switchingInterval;
    }
    else {
        // firstEvent is within a cch interval, so adjust for SCH messages
        if (chan == ChannelType::service) firstEvent += switchingInterval;
    }

    return firstEvent;
}

void MyVeinsBaseApp::populateWSM(BaseFrame1609_4* wsm, LAddress::L2Type rcvId, int serial)
{
    wsm->setRecipientAddress(rcvId);
    wsm->setBitLength(headerLength);

    if (DemoSafetyMessage* bsm = dynamic_cast<DemoSafetyMessage*>(wsm)) {
        bsm->setSenderPos(curPosition);
        bsm->setSenderSpeed(curSpeed);
        bsm->setPsid(-1);
        bsm->setChannelNumber(static_cast<int>(Channel::cch));
        bsm->addBitLength(beaconLengthBits);
        wsm->setUserPriority(beaconUserPriority);
        bsm->setSenderId(myId);
        bsm->setSenderType(myDeviceType);
    }
    else if (DemoServiceAdvertisment* wsa = dynamic_cast<DemoServiceAdvertisment*>(wsm)) {
        wsa->setChannelNumber(static_cast<int>(Channel::cch));
        wsa->setTargetChannel(static_cast<int>(currentServiceChannel));
        wsa->setPsid(currentOfferedServiceId);
        wsa->setServiceDescription(currentServiceDescription.c_str());
    }
    else if(AccidentNoticeMessage* anm = dynamic_cast<AccidentNoticeMessage*>(wsm)){
        anm->setSenderId(myId);

    }
    else {
        if (dataOnSch)
            wsm->setChannelNumber(static_cast<int>(Channel::sch1)); // will be rewritten at Mac1609_4 to actual Service Channel. This is just so no controlInfo is needed
        else
            wsm->setChannelNumber(static_cast<int>(Channel::cch));
        wsm->addBitLength(dataLengthBits);
        wsm->setUserPriority(dataUserPriority);
    }
}

void MyVeinsBaseApp::receiveSignal(cComponent* source, simsignal_t signalID, cObject* obj, cObject* details)
{
    Enter_Method_Silent();
    if (signalID == BaseMobility::mobilityStateChangedSignal) {
        handlePositionUpdate(obj);
    }
    else if (signalID == TraCIMobility::parkingStateChangedSignal) {
        handleParkingUpdate(obj);
    }
}

void MyVeinsBaseApp::handlePositionUpdate(cObject* obj)
{
    ChannelMobilityPtrType const mobility = check_and_cast<ChannelMobilityPtrType>(obj);
    curPosition = mobility->getPositionAt(simTime());
    curSpeed = mobility->getCurrentSpeed();
}

void MyVeinsBaseApp::handleParkingUpdate(cObject* obj)
{
    isParked = mobility->getParkingState();
}

void MyVeinsBaseApp::handleLowerMsg(cMessage* msg)
{

    BaseFrame1609_4* wsm = dynamic_cast<BaseFrame1609_4*>(msg);
    ASSERT(wsm);

    if (DemoSafetyMessage* bsm = dynamic_cast<DemoSafetyMessage*>(wsm)) {
        receivedBSMs++;
        onBSM(bsm);
    }
    else if (DemoServiceAdvertisment* wsa = dynamic_cast<DemoServiceAdvertisment*>(wsm)) {
        receivedWSAs++;
        onWSA(wsa);
    }
    else if(AccidentNoticeMessage* anm = dynamic_cast<AccidentNoticeMessage*>(wsm)){
        onANM(anm);
    }
    else {
        receivedWSMs++;
        onWSM(wsm);
    }

    delete (msg);
}

void MyVeinsBaseApp::handleSelfMsg(cMessage* msg)
{
    switch (msg->getKind()) {
    case SEND_BEACON_EVT: {
        DemoSafetyMessage* bsm = new DemoSafetyMessage();
        populateWSM(bsm);
        sendDown(bsm);
        scheduleAt(simTime() + beaconInterval, sendBeaconEvt);
        break;
    }
    case SEND_WSA_EVT: {
        DemoServiceAdvertisment* wsa = new DemoServiceAdvertisment();
        populateWSM(wsa);
        sendDown(wsa);
        scheduleAt(simTime() + wsaInterval, sendWSAEvt);
        break;
    }
    case DETECT_ACCIDENT_EVT:{
        // a function to check event that returns true or false
        // e.g., bool isAccidentHappened = checkAccident()
        bool isAccidentHappened = checkAccident();
        // if (isAccidentHappened is true) {
        if(isAccidentHappened){
            AccidentNoticeMessage* anm = new AccidentNoticeMessage();
            anm->setIsEvent(true);
            anm->setEventMsg("Accident Happened");
            anm->setSenderType(myDeviceType);
            int RSUId = checkRSUInRange();
            if(RSUId!=-1){//in rsu range
                populateWSM(anm,RSUId);//send msg to rsu
            }
            else{//not in rsu range
                populateWSM(anm); //broadcast msg to nb vehicles

            }
        }
        //     bool isRSUinRange =  a function to check if RSU is in range
        //          if (isRSUinRange is true) {
        //                  create a new message
        //                  send msg to rsu
        //          else {
        //                  create a new message
        //                  broadcast msg to nb vehicles
        //          }


        cout<<"---------------DETECT_ACCIDENT_EVT--------------------"<<"myId: "<<myId<<" time: "<<simTime()<<endl;
        scheduleAt(simTime()+ detectAccidentMsgInterval, detectAccidentEvt);// schedule a new timer
        break;
    }
    default: {
        if (msg) EV_WARN << "APP: Error: Got Self Message of unknown kind! Name: " << msg->getName() << endl;
        break;
    }
    }
}

void MyVeinsBaseApp::finish()
{

    cout << "myId: " << myId << ", " << L2TocModule[myId]->getFullName() << ", left the network at simTime: " << simTime() << endl;
    recordScalar("generatedWSMs", generatedWSMs);
    recordScalar("receivedWSMs", receivedWSMs);

    recordScalar("generatedBSMs", generatedBSMs);
    recordScalar("receivedBSMs", receivedBSMs);

    recordScalar("generatedWSAs", generatedWSAs);
    recordScalar("receivedWSAs", receivedWSAs);
}

MyVeinsBaseApp::~MyVeinsBaseApp()
{
    cancelAndDelete(sendBeaconEvt);
    cancelAndDelete(sendWSAEvt);
    findHost()->unsubscribe(BaseMobility::mobilityStateChangedSignal, this);
}

void MyVeinsBaseApp::startService(Channel channel, int serviceId, std::string serviceDescription)
{
    if (sendWSAEvt->isScheduled()) {
        throw cRuntimeError("Starting service although another service was already started");
    }

    mac->changeServiceChannel(channel);
    currentOfferedServiceId = serviceId;
    currentServiceChannel = channel;
    currentServiceDescription = serviceDescription;

    simtime_t wsaTime = computeAsynchronousSendingTime(wsaInterval, ChannelType::control);
    scheduleAt(wsaTime, sendWSAEvt);
}

void MyVeinsBaseApp::stopService()
{
    cancelEvent(sendWSAEvt);
    currentOfferedServiceId = -1;
}

void MyVeinsBaseApp::sendDown(cMessage* msg)
{
    checkAndTrackPacket(msg);
    BaseApplLayer::sendDown(msg);
}

void MyVeinsBaseApp::sendDelayedDown(cMessage* msg, simtime_t delay)
{
    checkAndTrackPacket(msg);
    BaseApplLayer::sendDelayedDown(msg, delay);
}

void MyVeinsBaseApp::checkAndTrackPacket(cMessage* msg)
{
    if (dynamic_cast<DemoSafetyMessage*>(msg)) {
        EV_TRACE << "sending down a BSM" << std::endl;
        generatedBSMs++;
    }
    else if (dynamic_cast<DemoServiceAdvertisment*>(msg)) {
        EV_TRACE << "sending down a WSA" << std::endl;
        generatedWSAs++;
    }
    else if (dynamic_cast<BaseFrame1609_4*>(msg)) {
        EV_TRACE << "sending down a wsm" << std::endl;
        generatedWSMs++;
    }
}

void MyVeinsBaseApp::onBSM(DemoSafetyMessage* bsm){
    updateNeighbor(bsm->getSenderId(), simTime(),myDeviceType);
    cout << L2TocModule[myId]->getFullName()<<" Neighbor Table:"<<endl;
    cout<<"before removing Node ID: "<<endl;
    for (const auto& neighbor : neighbors) {
        cout<< neighbor.first << ", Last Beacon: " << neighbor.second.lastBeaconTime << endl;
    }
    MyVeinsBaseApp::removeExpiredNeighbors();
    cout << "after removing Node ID: "<<endl;
    for (const auto& neighbor : neighbors) {
        cout<< neighbor.first << ", Last Beacon: " << neighbor.second.lastBeaconTime << endl;
    }
}

void MyVeinsBaseApp::updateNeighbor(int node_id, simtime_t last_beacon, bool neighborType) {
    nodeInfo neighborInfo = {node_id, neighborType, simTime()};
    neighbors[node_id] = neighborInfo;
}

void MyVeinsBaseApp::removeExpiredNeighbors() {
    simtime_t currentTime = simTime();
    for (auto it = neighbors.begin(); it != neighbors.end(); ) {
        if (currentTime - it->second.lastBeaconTime > 3) {
            cout << "Sim time: " << simTime() << endl;
            if (L2TocModule[it->first]) {
                //cout << "Removing Node ID: " << it->first << ", " << L2TocModule[it->first]->getFullName()<< " due to timeout."<<endl;
                it = neighbors.erase(it);
            }
        } else {
            ++it;
        }
    }
}

bool MyVeinsBaseApp::checkAccident(){

    Coord currentPos = getCurrentPosition();
    for (const auto& accidentPos : MyVeinsBaseApp::randomAccidentPosTable) {
        double distance = calculateDistance(currentPos, accidentPos);
        cout << "Distance between current position and accident position: " << distance << endl;

        double distanceThreshold = 99999999.0;
        if (distance <= distanceThreshold) {
            cout << "Current position is within the distance threshold of an accident position." << endl;
            return true;
        }
    }
    return false;
}

int MyVeinsBaseApp::checkRSUInRange(){//if there is an rsu, return rsu's node id. If not, return -1 broadcast
    int RSUId = -1;
    for (const auto& pair : neighbors) {
        const nodeInfo& info = pair.second;
            if (info.nodeType == 0) {
                RSUId = info.nodeId;
                break;
            }
    }
    return RSUId;
}


Coord MyVeinsBaseApp::getCurrentPosition()
{
    return curPosition;
}

double MyVeinsBaseApp::calculateDistance(const Coord& pos1, const Coord& pos2) {
    return sqrt(pow(pos1.x - pos2.x, 2) + pow(pos1.y - pos2.y, 2));
}
