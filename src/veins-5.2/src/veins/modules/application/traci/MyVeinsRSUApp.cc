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

using namespace veins;
using namespace std;

Define_Module(veins::MyVeinsRSUApp);

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
    //cout<<"I RSU ,id: "<< myId <<", name: "<< L2TocModule[myId]->getFullName() <<", have received a beacon from "<<bsm->getSenderId()<<" ,name: "<< L2TocModule[bsm->getSenderId()]->getFullName() <<endl;
    if(bsm->isEvent()){
        cout<<"RSU receive event message from: "<<L2TocModule[bsm->getSenderId()]->getFullName()<<" Msg: ";
        cout<<bsm->getEventMsg()<<endl;
        isEvent = true;
    }
}

void MyVeinsRSUApp::populateWSM(BaseFrame1609_4* wsm, LAddress::L2Type rcvId, int serial)
{
    MyVeinsBaseApp::populateWSM(wsm, rcvId, serial);
    //cout<<"I, RSU "<< myId <<", have sent a beacon"<<endl;
}
