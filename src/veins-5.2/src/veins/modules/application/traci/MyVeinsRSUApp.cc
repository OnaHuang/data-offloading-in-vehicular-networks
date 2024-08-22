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

        // Initialize a timer for taking per second actions, e.g., ANM queue
        perSecondANMQueueTimer = new cMessage("Per second ANM queue timer", PER_SECOND_ANM_TIMER);
        simtime_t firstPerSecondANMQueueMsg = simTime() + 1;
        scheduleAt(firstPerSecondANMQueueMsg, perSecondANMQueueTimer);

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
    addANMQueue(anm);
    //MyVeinsBaseApp::onANM(anm);
//    Coord evtLocation = anm->getEvtLocation();

//    if(isNodeTrustworthy(anm->getSenderId())){
//        anm->setSenderId(myId);
//        anm->setSenderType(myDeviceType);
//        populateWSM(anm);
//    }
//    else{
//        //not trustworthy
//    }

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
        case PER_SECOND_ANM_TIMER:{
            takePerSecondANMQueueActionByRSU();
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

void MyVeinsRSUApp::takePerSecondANMQueueActionByRSU() {
    simtime_t currentTime = simTime(); // Get the current simulation time

    // Iterate over the vector of event queues
    auto it = allLocalEvents.begin();
    while (it != allLocalEvents.end()) {
        simtime_t firstEvtTime = it->second.firstEvtTime;

        // Check if the time elapsed since the first event is greater than 10 seconds
        if (currentTime - firstEvtTime >= 10) {

            AccidentNoticeMessage* anm = new AccidentNoticeMessage();

            Coord avgLocation = analyseEvtMsgOfOneLocation(it->second);
            anm->setEvtLocation(avgLocation);
            anm->setFirstEvtTime(it->second.firstEvtTime);
            populateWSM(anm);
            // Remove the entire event queue
            it = allLocalEvents.erase(it);
        } else {
            ++it;
        }
    }
    // Schedule the timer again after one second
    scheduleAt(simTime() + 1, perSecondANMQueueTimer);
}


bool MyVeinsRSUApp::isNodeTrustworthy(LAddress::L2Type senderId)
{
    return rand() % 2 == 0;
}

void MyVeinsRSUApp::generateRandomAccidentPos()
{
    for(int i=0; i<3; ++i){
        Coord pos(2050,1000+10*i);
        randomAccidentPosTable.push_back(pos);
    }

}

void MyVeinsRSUApp::addANMQueue(AccidentNoticeMessage* newMsg){
    Coord newCoord = newMsg->getEvtLocation();// Get Coord from the message
    bool found = false;
    double threshold = 10.0; // 10-meter range

    for (auto& localEvent : allLocalEvents) {
        Coord existingCoord = localEvent.first;
        double distance = sqrt(pow(newCoord.x - existingCoord.x, 2) +
                               pow(newCoord.y - existingCoord.y, 2));
        if (distance <= threshold) {
            // Within the range, add to the existing queue
            localEvent.second.evtQueue.push_back(newMsg);

            // Recalculate the Coord as the average x, y of the evtQueueStruct
            double sumX = 0.0, sumY = 0.0;
            for (auto* msg : localEvent.second.evtQueue) {
                Coord msgCoord = msg->getEvtLocation(); // Get the Coord of the message
                sumX += msgCoord.x;
                sumY += msgCoord.y;
            }
            int queueSize = localEvent.second.evtQueue.size();
            localEvent.first.x = sumX / queueSize;
            localEvent.first.y = sumY / queueSize;

            found = true;
            break;
        }
    }

    if (!found) {
        // If no suitable Coord is found, create a new pair
        evtQueueStruct newEvtQueueStruct;
        newEvtQueueStruct.firstEvtTime = simTime(); // Assume using current simulation time
        newEvtQueueStruct.evtQueue.push_back(newMsg);
        allLocalEvents.push_back(make_pair(newCoord, newEvtQueueStruct));
    }
}

Coord MyVeinsRSUApp::analyseEvtMsgOfOneLocation(evtQueueStruct& oneEvtQueueStruct){
    vector<AccidentNoticeMessage*> trustedEvtMsgsQueue;
    auto it = oneEvtQueueStruct.evtQueue.begin();
    while (it != oneEvtQueueStruct.evtQueue.end()) {
        LAddress::L2Type vehicleId = (*it)->getSenderId();
        if (isNodeTrustworthy(vehicleId)) {
            trustedEvtMsgsQueue.push_back(*it);
        }
        ++it;
    }

    Coord sumLocation(0, 0);

    if (trustedEvtMsgsQueue.empty()) {
        return sumLocation;
    }


    for (auto* msg : trustedEvtMsgsQueue) {
        //test addition
        sumLocation += msg->getEvtLocation();
    }
    //test division
    Coord avgLocation = sumLocation / trustedEvtMsgsQueue.size();
    return avgLocation;
}

string MyVeinsRSUApp::calculateEStar(const vector<string>& events) {
    unordered_map<string, int> frequencyMap;

    //count the time of appearance of each event
    for (const string& event : events) {
        frequencyMap[event]++;
    }

    // find the mode
    string modeEvent;
    int maxFrequency = 0;

    for (const auto& entry : frequencyMap) {
        if (entry.second > maxFrequency) {
            maxFrequency = entry.second;
            modeEvent = entry.first;
        }
    }

    return modeEvent;
}

int MyVeinsRSUApp::editDistance(const string& s1, const string& s2) {
    int len1 = s1.size();
    int len2 = s2.size();
    vector<vector<int>> dp(len1 + 1, vector<int>(len2 + 1));

    for (int i = 0; i <= len1; ++i) {
        for (int j = 0; j <= len2; ++j) {
            if (i == 0) {
                dp[i][j] = j;
            } else if (j == 0) {
                dp[i][j] = i;
            } else if (s1[i - 1] == s2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];
            } else {
                dp[i][j] = 1 + min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
            }
        }
    }
    return dp[len1][len2];
}


double MyVeinsRSUApp::calculateSimilarity(const string& e_i, const string& e_star) {
    int maxLen = max(e_i.length(), e_star.length());
    if (maxLen == 0) return 0; // both are empty
    int distance = editDistance(e_i, e_star);
    return 1.0 - static_cast<double>(distance) / maxLen;
}

double MyVeinsRSUApp::calculate_fi(const string& e_i, const string& e_star) {
    if (e_i.empty()) {
        return 0.0;
    } else if (e_i == e_star) {
        return 1.0;
    } else {
        return calculateSimilarity(e_i, e_star);
    }
}

//double MyVeinsRSUApp::sim_func(const string& e_i, const string& e_star) {
//    //calculate similarity
//    return 0.5;
//}

//double MyVeinsRSUApp::calculate_Gi(const vector<double>& feedbacks, double f_th) {
//    int count = 0;
//    for (double fi : feedbacks) {
//        if (fi > f_th) {
//            count++;
//        }
//    }
//    return static_cast<double>(count) / feedbacks.size();
//}
//
//double MyVeinsRSUApp::calculate_xi_res(int N_i, int N_is, double rho) {
//    double xi_res = 0.0;
//    for (int k = N_is + 1; k <= N_i; ++k) {
//        xi_res += rho * pow(1 - rho, k - 1);
//    }
//    return xi_res;
//}
//
//double MyVeinsRSUApp::calculate_Pi(const vector<double>& feedbacks, int N_is, double rho, double xi_res) {
//    double Pi = 0.0;
//    for (int l = 1; l <= N_is; ++l) {
//        double wl = rho * pow(1 - rho, l - 1) + xi_res / feedbacks.size();
//        Pi += wl * feedbacks[l - 1];
//    }
//    return Pi;
//}
//
//
//double MyVeinsRSUApp::calculate_Ti(double T_init, int N_i, double Gi, double Pi, double mu) {
//    if (N_i == 0) {
//        return T_init;
//    } else {
//        return mu * Gi + (1 - mu) * Pi;
//    }
//}
