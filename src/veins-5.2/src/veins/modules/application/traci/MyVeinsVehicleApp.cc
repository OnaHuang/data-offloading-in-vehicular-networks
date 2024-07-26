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

#include "veins/modules/application/traci/MyVeinsVehicleApp.h"
#include "veins/modules/messages/DemoSafetyMessage_m.h"
#include "veins/modules/application/traci/TraCIDemo11pMessage_m.h"
#include "veins/modules/application/traci/MyVeinsRSUApp.h"

using namespace veins;
using namespace std;

Define_Module(veins::MyVeinsVehicleApp);

void MyVeinsVehicleApp::initialize(int stage)
{
    //cout<<"-----------------initialise--------------------"<<endl;
    MyVeinsBaseApp::initialize(stage);
    if (stage == 0) {
        cout<<"-----------I am stage 0-----Vehicle----------"<<simTime()<<endl;
        sentMessage = false;
        lastDroveAt = simTime();
        currentSubscribedServiceId = -1;

        finishInterval = par("finishInterval");
    }
    else if (stage == 1) {
        cout<<"-----------I am stage 1-----Vehicle----------"<<simTime()<<endl;
    }
    if(myId == 40){
        isEvent = true;
    }
}

void MyVeinsVehicleApp::onWSA(DemoServiceAdvertisment* wsa)
{
    if (currentSubscribedServiceId == -1) {
        mac->changeServiceChannel(static_cast<Channel>(wsa->getTargetChannel()));
        currentSubscribedServiceId = wsa->getPsid();
        if (currentOfferedServiceId != wsa->getPsid()) {
            stopService();
            startService(static_cast<Channel>(wsa->getTargetChannel()), wsa->getPsid(), "Mirrored Traffic Service");
        }
    }
}

void MyVeinsVehicleApp::onWSM(BaseFrame1609_4* frame)
{
    TraCIDemo11pMessage* wsm = check_and_cast<TraCIDemo11pMessage*>(frame);

    findHost()->getDisplayString().setTagArg("i", 1, "green");

    if (mobility->getRoadId()[0] != ':') traciVehicle->changeRoute(wsm->getDemoData(), 9999);
    if (!sentMessage) {
        sentMessage = true;
        // repeat the received traffic update once in 2 seconds plus some random delay
        wsm->setSenderAddress(myId);
        wsm->setSerial(3);
        scheduleAt(simTime() + 2 + uniform(0.01, 0.2), wsm->dup());
    }
}

void MyVeinsVehicleApp::onBSM(DemoSafetyMessage* bsm)
{
    MyVeinsBaseApp::onBSM(bsm);
    //cout<<"I, vehicle ,id: "<< myId <<", name: "<< L2TocModule[myId]->getFullName() <<", have received a beacon from "<<bsm->getSenderId()<<" ,name: "<< L2TocModule[bsm->getSenderId()]->getFullName() <<endl;
    if(bsm->isEvent()){
        cout<<"receive event message from: "<<L2TocModule[bsm->getSenderId()]->getFullName()<<" Msg: ";
        cout<<bsm->getEventMsg()<<endl;
        isEvent = true;
    }
    myNbs.clear();
    for (const auto& neighbor : neighbors) {
        myNbs.push_back(neighbor.first);
    }
    MyVeinsRSUApp::globalAllNbs[myId] = myNbs;
}

void MyVeinsVehicleApp::handleSelfMsg(cMessage* msg)
{
    switch (msg->getKind()){
        case REMOVE_ME_AS_YOUR_NB:{
            handleLeaveMessage(dynamic_cast<DemoSafetyMessage*>(msg));
            break;
        }
        default:{
            if (TraCIDemo11pMessage* wsm = dynamic_cast<TraCIDemo11pMessage*>(msg)) {
                // send this message on the service channel until the counter is 3 or higher.
                // this code only runs when channel switching is enabled
                sendDown(wsm->dup());
                wsm->setSerial(wsm->getSerial() + 1);
                if (wsm->getSerial() >= 3) {
                    // stop service advertisements
                    stopService();
                    delete (wsm);
                }
                else {
                    scheduleAt(simTime() + 1, wsm);
                }
            }
            else {
                MyVeinsBaseApp::handleSelfMsg(msg);
            }
        }
    }
}

void MyVeinsVehicleApp::handlePositionUpdate(cObject* obj)
{
    MyVeinsBaseApp::handlePositionUpdate(obj);

    // stopped for for at least 10s?
    if (mobility->getSpeed() < 1) {
        if (simTime() - lastDroveAt >= 10 && sentMessage == false) {
            findHost()->getDisplayString().setTagArg("i", 1, "red");
            sentMessage = true;

            TraCIDemo11pMessage* wsm = new TraCIDemo11pMessage();
            populateWSM(wsm);
            wsm->setDemoData(mobility->getRoadId().c_str());

            // host is standing still due to crash
            if (dataOnSch) {
                startService(Channel::sch2, 42, "Traffic Information Service");
                // started service and server advertising, schedule message to self to send later
                scheduleAt(computeAsynchronousSendingTime(1, ChannelType::service), wsm);
            }
            else {
                // send right away on CCH, because channel switching is disabled
                sendDown(wsm);
            }
        }
    }
    else {
        lastDroveAt = simTime();
    }
}

void MyVeinsVehicleApp::populateWSM(BaseFrame1609_4* wsm, LAddress::L2Type rcvId, int serial)
{
    MyVeinsBaseApp::populateWSM(wsm, rcvId, serial);
    //cout<<"I, vehicle "<< myId <<", have sent a beacon"<<endl;
}

void MyVeinsVehicleApp::finish()
{
    //remove this vehicle from other vehicles' neighbour table
    cout<<"---------------"<<L2TocModule[myId]->getFullName()<<" will finish-------------"<<endl;
    removeMeFromOtherNBTable = new DemoSafetyMessage("remove me from your nb table", REMOVE_ME_AS_YOUR_NB);

    //removeMeFromOtherNBTable->setKind(REMOVE_ME_AS_YOUR_NB);
    removeMeFromOtherNBTable->setSenderId(myId);
    scheduleAt(simTime() + finishInterval, removeMeFromOtherNBTable);

    MyVeinsBaseApp::finish();
}


void MyVeinsVehicleApp::handleLeaveMessage(DemoSafetyMessage* msg) {
    int leavingNodeId = msg->getSenderId();
    auto it = neighbors.find(leavingNodeId);
    if (it != neighbors.end()) {
        cout<<"---------------erase: "<<it->first<<"--------------"<<endl;
        neighbors.erase(it);
    }
}
