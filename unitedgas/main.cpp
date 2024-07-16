#include <iostream>
#include "Well.h"

int main() {
    std::cout << "Working classes!" << std::endl;

    UnitedGas::Well myobj(2, 1, 295, 0, 0, 0, 10, 48, 20, 0, 0, "Number 2", "8/10/2016", "National 67B", std::string(), "1710??", "2 7/8", "2 3/4", "1697 - 1701", std::string(), std::string(), "Packer @600'");

    /*
    myobj.wellno = 2;
    myobj.dailyOil = 1;
    myobj.dailyWater = 295;
    myobj.dailyGas = 0;
    myobj.opPressureTubing = 0;
    myobj.opPressureCasing = 0;
    myobj.strokesPerMin = 10;
    myobj.strokeLength = 48;
    myobj.motorHp = 20;
    myobj.pumpingRatio = 0;
    myobj.unitGearRatio = 0;
    myobj.wellname = "Number 2";
    myobj.dateOfRecentTest = "8/10/2016";
    myobj.pumpingUnitSize = "National 67B";
    myobj.casingSize = std::string();
    myobj.depth = "1710??";
    myobj.tubingSize = "2 7/8";
    myobj.pumpSize = "2 3/4";
    myobj.firstCole = "1697 - 1701";
    myobj.secondCole = std::string();
    myobj.thirdCole = std::string();
    myobj.comments = "Packer @600'";
    */

    myobj.print();

    return 0;
}
