#include "Well.h"
#include <iostream>

// constructor definition
UnitedGas::Well::Well(int wellno, int dailyOil, int dailyWater, int dailyGas, int opPressureTubing, int opPressureCasing, int strokesPerMin, int strokeLength, int motorHp, float pumpingRatio, float unitGearRatio, std::string wellname, std::string dateOfRecentTest, std::string pumpingUnitSize, std::string casingSize, std::string depth, std::string tubingSize, std::string pumpSize, std::string firstCole, std::string secondCole, std::string thirdCole, std::string comments)
{
  m_wellno = wellno;
  m_dailyOil = dailyOil;
  m_dailyWater = dailyWater;
  m_dailyGas = dailyGas;
  m_opPressureTubing = opPressureTubing;
  m_opPressureCasing = opPressureCasing;
  m_strokesPerMin = strokesPerMin;
  m_strokeLength = strokeLength;
  m_motorHp = motorHp;
  m_pumpingRatio = pumpingRatio;
  m_unitGearRatio = unitGearRatio;
  m_wellname = wellname;
  m_dateOfRecentTest = dateOfRecentTest;
  m_pumpingUnitSize = pumpingUnitSize;
  m_casingSize = casingSize;
  m_depth = depth;
  m_tubingSize = tubingSize;
  m_pumpSize = pumpSize;
  m_firstCole = firstCole;
  m_secondCole = secondCole;
  m_thirdCole = thirdCole;
  m_comments = comments;
}

void UnitedGas::Well::print()
{
  std::cout << "wellno: " << m_wellno << std::endl;
  std::cout << "dailyOil: " << m_dailyOil << std::endl;
  std::cout << "dailyWater: " << m_dailyWater << std::endl;
  std::cout << "dailyGas: " << m_dailyGas << std::endl;
  std::cout << "opPressureTubing: " << m_opPressureTubing << std::endl;
  std::cout << "opPressureCasing: " << m_opPressureCasing << std::endl;
  std::cout << "strokesPerMin: " << m_strokesPerMin << std::endl;
  std::cout << "strokeLength: " << m_strokeLength << std::endl;
  std::cout << "motorHp: " << m_motorHp << std::endl;
  std::cout << "pumpingRatio: " << m_pumpingRatio << std::endl;
  std::cout << "unitGearRatio: " << m_unitGearRatio << std::endl;
  std::cout << "wellname: " << m_wellname << std::endl;
  std::cout << "dateOfRecentTest: " << m_dateOfRecentTest << std::endl;
  std::cout << "pumpingUnitSize: " << m_pumpingUnitSize << std::endl;
  std::cout << "casingSize: " << m_casingSize << std::endl;
  std::cout << "depth: " << m_depth << std::endl;
  std::cout << "tubingSize: " << m_tubingSize << std::endl;
  std::cout << "pumpSize: " << m_pumpSize << std::endl;
  std::cout << "firstCole: " << m_firstCole << std::endl;
  std::cout << "secondCole: " << m_secondCole << std::endl;
  std::cout << "thirdCole: " << m_thirdCole << std::endl;
  std::cout << "comments: " << m_comments << std::endl;
}

/*
  : wellno(0), dailyOil(0), dailyWater(0), dailyGas(0), opPressureTubing(0), opPressureCasing(0), strokesPerMin(0),
    strokeLength(0), motorHp(0),
    pumpingRatio(0),
    unitGearRatio(0),
    wellname{std::string()},
    dateOfRecentTest{std::string()},
    pumpingUnitSize{std::string()},
    casingSize{std::string()},
    depth{std::string()},
    tubingSize{std::string()},
    pumpSize{std::string()},
    firstCole{std::string()},
    secondCole{std::string()},
    thirdCole{std::string()},
    comments{std::string()}
{}
void Well::print() const // print function variables
{
  std::cout << "wellno: " << m_wellno << endl;
  std::cout << "dailyOil: " << m_dailyOil << endl;
  std::cout << "dailyWater: " << m_dailyWater << endl;
  std::cout << "dailyGas: " << m_dailyGas << endl;
  std::cout << "opPressureTubing: " << m_opPressureTubing << endl;
  std::cout << "opPressureCasing: " << m_opPressureCasing << endl;
  std::cout << "strokesPerMin: " << m_strokesPerMin << endl;
  std::cout << "strokeLength: " << m_strokeLength << endl;
  std::cout << "motorHp: " << m_motorHp << endl;
  std::cout << "pumpingRatio: " << m_pumpingRatio << endl;
  std::cout << "unitGearRatio: " << m_unitGearRatio << endl;
  std::cout << "wellname: " << m_wellname << endl;
  std::cout << "dateOfRecentTest: " << m_dateOfRecentTest << endl;
  std::cout << "pumpingUnitSize: " << m_pumpingUnitSize << endl;
  std::cout << "casingSize: " << m_casingSize << endl;
  std::cout << "depth: " << m_depth << endl;
  std::cout << "tubingSize: " << m_tubingSize << endl;
  std::cout << "pumpSize: " << m_pumpSize << endl;
  std::cout << "firstCole: " << m_firstCole << endl;
  std::cout << "secondCole: " << m_secondCole << endl;
  std::cout << "thirdCole: " << m_thirdCole << endl;
  std::cout << "comments: " << m_comments << endl;
};
*/
