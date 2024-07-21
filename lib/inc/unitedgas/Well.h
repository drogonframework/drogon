//
// Created by trist007 on 7/13/24.
//

#ifndef WELL_H
#define WELL_H

#include <string>

namespace UnitedGas {
class Well
    {
        public:
            Well(int wellno, int dailyOil, int dailyWater, int dailyGas, int opPressureTubing, int opPressureCasing, int strokesPerMin, int strokeLength, int motorHp, float pumpingRatio, float unitGearRatio, std::string wellname, std::string dateOfRecentTest, std::string pumpingUnitSize, std::string casingSize, std::string depth, std::string tubingSize, std::string pumpSize, std::string firstCole, std::string secondCole, std::string thirdCole, std::string comments);
            void print();
            std::string printResult();
        private:
            int m_wellno, m_dailyOil, m_dailyWater, m_dailyGas, m_opPressureTubing, m_opPressureCasing, m_strokesPerMin,
                    m_strokeLength, m_motorHp;
            float m_pumpingRatio, m_unitGearRatio;
            std::string m_wellname, m_dateOfRecentTest, m_pumpingUnitSize, m_casingSize, m_depth, m_tubingSize, m_pumpSize,
                    m_firstCole, m_secondCole, m_thirdCole, m_comments;
    };
} // UnitedGas

#endif //WELL_H
