/**
 *
 *  DrTemplateBase.h
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#pragma once

#include <drogon/DrObject.h>
#include <drogon/HttpViewData.h>
#include <string>
#include <memory>

namespace drogon
{

typedef HttpViewData DrTemplateData;

/// The templating engine class
/**
 * This class can generate a text string from the template file and template data.
 * For more details on the template file, see the wiki site (the 'View' section)
 */
class DrTemplateBase : public virtual DrObjectBase
{
  public:
    /// Create an object of the implementation class
    /**
     * The @param templateName represents the name of the template file.
     * A template file is a description file with a special format. Its extension is 
     * usually .csp. The user should preprocess the template file 
     * with the drogon_ctl tool to create c++ source files. 
     */
    static std::shared_ptr<DrTemplateBase> newTemplate(std::string templateName);

    /// Generate the text string
    /**
     * The @param data represents data rendered in the string in a format according to the template file.
     */
    virtual std::string genText(const DrTemplateData &data = DrTemplateData()) = 0;

    virtual ~DrTemplateBase(){};
    DrTemplateBase(){};
};

} // namespace drogon
