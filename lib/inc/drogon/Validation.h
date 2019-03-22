#pragma once

#include <iostream>
#include <drogon/HttpRequest.h>
#include <drogon/utils/Utilities.h>

using namespace std;
using namespace drogon;
using namespace drogon::utils;

namespace drogon { 

    /** 校验器 **/
    enum class Validator: unsigned short {
        NotNull,
        Numeric,
        StringLength,
        //UserName,       
        //Mail,          
        //MobileNumber, 
        //TelNumber,   
        //Includes,   
        //Excludes,  
        //Between,  
        //Equals,  
    };

    class Validation { 

        public:
            /**
             * Constructor
             */
            explicit Validation() noexcept;
            virtual ~Validation() noexcept;

            //~Validation();
            virtual bool validate(const HttpRequestPtr& request) noexcept = 0;
            
            /**
             * Get all error messages
             */
            string getMessages() noexcept;

        protected:

            /**
             * The error message
             */
            string errorMessages;

            /**
             * Need a value which must not be empty
             */
            void notNull(const HttpRequestPtr& request, const string& field, const string& message) noexcept;
    
            /**
             * Need a number
             */
            void isNumeric(const HttpRequestPtr& request, const string& field, const string& message) noexcept;

            /**
             * A string whose length between min and max
             */
            void stringLength(const HttpRequestPtr& request, const string& field, const int min, const int max, const string& message) noexcept;

            /**
             * Validate all fields
             */
            bool validateAll() noexcept;

        private:
            Validation(const Validation&);
            Validation& operator=(const Validation&);
    };
}
