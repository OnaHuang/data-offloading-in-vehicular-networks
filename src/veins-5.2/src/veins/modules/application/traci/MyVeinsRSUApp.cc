//
// Copyright (C) 2016 David Eckhoff <david.eckhoff@fau.de>
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

#include "veins/modules/application/traci/MyVeinsRSUApp.h"
//#include "veins/modules/application/traci/MyVeinsMessage_m.h"
#include "veins/modules/application/traci/TraCIDemo11pMessage_m.h"
#include <fstream>
#include <cstdlib>

using namespace veins;
using namespace std;

Define_Module(veins::MyVeinsRSUApp);
unordered_map<int, vector<int>> veins::MyVeinsRSUApp::globalAllNbs;


void MyVeinsRSUApp::initialize(int stage)
{
    MyVeinsBaseApp::initialize(stage);
    if (stage == 0) {
        // Nothing is required here when stage=0
        myDeviceType = RSU;
        cout<<"-----------I am stage 0----RSU-----------"<<simTime()<<endl;
    }
    else if (stage == 1) {
        cout<<"-----------I am stage 1-----RSU----------"<<simTime()<<endl;
        cout<<"----------------------------initialise-----------------------------------"<<endl;
        // Initialize a timer for taking per second actions, e.g., count num nbs
        perSecondNbCountTimer = new cMessage("Per second nb counting timer", PER_SECOND_NB_COUNT_TIMER);
        simtime_t firstPerSecondNbCountMsg = simTime() + 1;
        scheduleAt(firstPerSecondNbCountMsg, perSecondNbCountTimer);

        if(myId==11){
//            csvFile.open("D:/OneDrive - Aberystwyth University/Aberystwyth University/MSc/MSc Project/data.csv");
            csvFile.open("log/data.csv");
            if (!csvFile.is_open()) {
                cerr << "Failed to open data.csv" << endl;
            }
            generateRandomAccidentPos();
        }
    }
}

void MyVeinsRSUApp::onWSA(DemoServiceAdvertisment* wsa)
{
    // if this RSU receives a WSA for service 42, it will tune to the chan
    if (wsa->getPsid() == 42) {
        mac->changeServiceChannel(static_cast<Channel>(wsa->getTargetChannel()));
    }
}

void MyVeinsRSUApp::onWSM(BaseFrame1609_4* frame)
{
    TraCIDemo11pMessage* wsm = check_and_cast<TraCIDemo11pMessage*>(frame);

    // this rsu repeats the received traffic update in 2 seconds plus some random delay
    sendDelayedDown(wsm->dup(), 2 + uniform(0.01, 0.2));
}

void MyVeinsRSUApp::onBSM(DemoSafetyMessage* bsm)
{
    MyVeinsBaseApp::onBSM(bsm);
    //cout<<"I RSU ,id: "<< myId <<", name: "<< L2TocModule[myId]->getFullName() <<", have received a beacon from "<<bsm->getSenderId()<<" ,name: "<< L2TocModule[bsm->getSenderId()]->getFullName() <<endl;
    if(bsm->isEvent()){
        cout<<"RSU receive event message from: "<<L2TocModule[bsm->getSenderId()]->getFullName()<<" Msg: ";
        cout<<bsm->getEventMsg()<<endl;
        isEvent = true;
    }
//    simtime_t currentTime = simTime();
//    if(currentTime-lastEditglobalAllNbsTime>1){
//        for (const auto& entry : globalAllNbs) {
//            int id = entry.first;
//            vector<int> neighborList = entry.second;
//
//            for (int neighbor : neighborList) {
//                csvFile << id << "," << neighbor << "," << currentTime << "\n";
//            }
//        }
//    }
//    lastEditglobalAllNbsTime = currentTime;
}

void MyVeinsRSUApp::onANM(AccidentNoticeMessage* anm)
{
    //MyVeinsBaseApp::onANM(anm);
    if(isNodeTrustworthy(anm->getSenderId())){
        anm->setSenderId(myId);
        anm->setSenderType(myDeviceType);
        populateWSM(anm);
    }
    else{
        //not trustworthy
    }

}

void MyVeinsRSUApp::populateWSM(BaseFrame1609_4* wsm, LAddress::L2Type rcvId, int serial)
{
    MyVeinsBaseApp::populateWSM(wsm, rcvId, serial);
    //cout<<"I, RSU "<< myId <<", have sent a beacon"<<endl;
}

void MyVeinsRSUApp::finish()
{
    MyVeinsBaseApp::finish();
    if(myId==11){
        csvFile.close();
        cout << "CSV file created successfully."<<endl;
    }
}

void MyVeinsRSUApp::handleSelfMsg(cMessage* msg)
{
    switch (msg->getKind()) {
        case PER_SECOND_NB_COUNT_TIMER: {
            takePerSecondCountNbActionByRSU();
            break;
        }
        default: {
            MyVeinsBaseApp::handleSelfMsg(msg);
        }
    }


}

void MyVeinsRSUApp::takePerSecondCountNbActionByRSU()
{
    simtime_t currentTime = simTime();

    for (const auto& entry : globalAllNbs) {
        int id = entry.first;
        vector<int> neighborList = entry.second;

        for (int neighbor : neighborList) {
            csvFile << id << "," << neighbor << "," << currentTime << "\n";
        }
    }
    // Schedule the timer again after one second
    scheduleAt(simTime() + 1, perSecondNbCountTimer);
}

bool MyVeinsRSUApp::isNodeTrustworthy(LAddress::L2Type senderId)
{
    return rand() % 2 == 0;
}

void MyVeinsRSUApp::generateRandomAccidentPos()
{
    for(int i=0; i<3; ++i){
        Coord pos(i,i+1);
        randomAccidentPosTable.push_back(pos);
    }

}
