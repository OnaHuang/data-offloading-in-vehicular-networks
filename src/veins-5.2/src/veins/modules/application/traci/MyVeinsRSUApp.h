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

#pragma once

#include "veins/veins.h"

#include "MyVeinsBaseApp.h"

using namespace omnetpp;
using namespace std;

namespace veins {

/**
 * @brief
 * This is a stub for a typical Veins application layer.
 * Most common functions are overloaded.
 * See MyVeinsRSUApp.cc for hints
 *
 * @author David Eckhoff
 *
 */

class VEINS_API MyVeinsRSUApp : public MyVeinsBaseApp {
public:
    static unordered_map<int,vector<int>> globalAllNbs;

private:
    ofstream csvFile;
    vector<pair<double, double>> accidentLocations;

public:
    const vector<pair<double, double>>& getAccidentLocations() const;

protected:
    void initialize(int stage) override;
    void onWSM(BaseFrame1609_4* wsm) override;
    void onWSA(DemoServiceAdvertisment* wsa) override;
    void onBSM(DemoSafetyMessage* bsm) override;
    void populateWSM(BaseFrame1609_4* wsm, LAddress::L2Type rcvId = LAddress::L2BROADCAST(), int serial = 0) override;
    void finish() override;
    void handleSelfMsg(cMessage* msg) override;
    void takePerSecondCountNbActionByRSU();
    void generateAccidents();

    simtime_t lastEditglobalAllNbsTime = 0;

    cMessage* perSecondNbCountTimer;

};

} // namespace veins
