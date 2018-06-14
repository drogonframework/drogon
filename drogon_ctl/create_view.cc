/**
 *
 *  @file
 *  @author An Tao
 *  @section LICENSE
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  @section DESCRIPTION
 *
 */

#include "create_view.h"
#include "cmd.h"
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
static const std::string cxx_include="<%inc";
static const std::string cxx_end="%>";
static const std::string cxx_lang="<%c++";
static const std::string cxx_view_data="@@";
static const std::string cxx_output="$$";

using namespace drogon_ctl;

static std::string&   replace_all(std::string&   str,const   std::string&   old_value,const   std::string&   new_value)
{
    std::string::size_type   pos(0);
    while(true)   {


        //std::cout<<str<<endl;
        //std::cout<<"pos="<<pos<<endl;
        if(   (pos=str.find(old_value,pos))!=std::string::npos   ) {
            str = str.replace(pos, old_value.length(), new_value);
            pos += new_value.length()-old_value.length();
            pos++;
        }
        else
            break;
    }
    return   str;
}
static void parseCxxLine(std::ofstream &oSrcFile,const std::string& line,const std::string& streamName,const std::string& viewDataName)
{

    if(line.length()>0)
    {	std::string tmp=line;
        replace_all(tmp,cxx_output,streamName);
        replace_all(tmp,cxx_view_data,viewDataName);
        oSrcFile<<tmp<<"\n";
    }

}

static void parseLine(std::ofstream &oSrcFile,std::string& line,const std::string& streamName,const std::string& viewDataName,int &cxx_flag,int returnFlag=1)
{
    std::string::size_type pos(0);
    if(line.length()==0)
        return;
    if(cxx_flag==0)
    {
        //find cxx lang begin
        if((pos=line.find(cxx_lang))!=std::string::npos)
        {
            std::string oldLine=line.substr(0,pos);
            parseLine(oSrcFile,oldLine,streamName,viewDataName,cxx_flag,0);
            std::string newLine=line.substr(pos+cxx_lang.length());
            cxx_flag=1;
            parseLine(oSrcFile,newLine,streamName,viewDataName,cxx_flag,returnFlag);
        }
        else
        {
            if(line.length()>0)
                oSrcFile<<"\t"<<streamName<<" << \""<<replace_all(line,"\"","\\\"");
            if(returnFlag)
                oSrcFile<<"\\n\";\n";
            else
                oSrcFile<<"\";\n";
        }
    }
    else
    {
        if((pos=line.find(cxx_end))!=std::string::npos)
        {
            std::string newLine=line.substr(0,pos);
            parseCxxLine(oSrcFile,newLine,streamName,viewDataName);
            std::string oldLine=line.substr(pos+cxx_end.length());
            cxx_flag=0;
            parseLine(oSrcFile,oldLine,streamName,viewDataName,cxx_flag,returnFlag);
        }
        else
        {
            parseCxxLine(oSrcFile,line,streamName,viewDataName);
        }
    }

}


void create_view::handleCommand(std::vector<std::string> &parameters)
{
    for(auto iter=parameters.begin();iter!=parameters.end();iter++)
    {
        auto file=*iter;
        if(file=="-o"||file=="--output")
        {
            iter=parameters.erase(iter);
            if(iter!=parameters.end())
            {
                _outputPath=*iter;
                iter=parameters.erase(iter);
            }
            break;
        }
        else if(file[0]=='-')
        {
            std::cout<<ARGS_ERROR_STR<<std::endl;
            return;
        }
    }
    createViewFiles(parameters);
}
void create_view::createViewFiles(std::vector<std::string> &cspFileNames)
{

    for(auto file:cspFileNames)
    {
        std::cout<<"create view:"<<file<<std::endl;
        createViewFile(file);
    }
}
int create_view::createViewFile(const std::string &script_filename)
{
    std::cout<<"create HttpView Class file by "<<script_filename<<std::endl;
    std::ifstream infile(script_filename.c_str(),std::ifstream::in);
    if(infile)
    {
        std::string::size_type pos = script_filename.rfind(".");
        if(pos!=std::string::npos)
        {
            std::string className=script_filename.substr(0,pos);
            if((pos=className.rfind("/"))!=std::string::npos)
            {
                className=className.substr(pos+1);
            }
            std::cout<<"className="<<className<<std::endl;
            std::string headFileName=_outputPath+"/"+className+".h";
            std::string sourceFilename=_outputPath+"/"+className+".cc";
            std::ofstream oHeadFile(headFileName.c_str(),std::ofstream::out);
            std::ofstream oSourceFile(sourceFilename.c_str(),std::ofstream::out);
            if(!oHeadFile||!oSourceFile)
                return -1;

            newViewHeaderFile(oHeadFile,className);
            newViewSourceFile(oSourceFile,className,infile);
        }
        else
            return -1;
    }
    else
    {
        std::cerr<<"can't open file "<<script_filename<<std::endl;
        return -1;
    }
    return 0;
}
void create_view::newViewHeaderFile(std::ofstream &file,const std::string &className)
{
    file<<"//this file is generated by program automatically,don't modify it!\n";
    file<<"#include <drogon/HttpView.h>\n";
    file<<"using namespace drogon;\n";
    file<<"class "<<className<<":public HttpView<"<<className<<">\n";
    file<<"{\npublic:\n\t"<<className<<"(){};\n\tvirtual ~"<<className<<"(){};\nprotected:\n\t"
			"virtual HttpResponsePtr genHttpResponse(const HttpViewData&) override;\n};";
}

void create_view::newViewSourceFile(std::ofstream &file,const std::string &className,std::ifstream &infile)
{
    file << "//this file is generated by program automatically,don't modify it!\n";
    file << "#include \"" << className << ".h\"\n";
    file <<"#include <string>\n";
    file <<"#include <sstream>\n";
    file <<"#include <map>\n";
    file <<"#include <vector>\n";
    file <<"#include <set>\n";
    file <<"#include <iostream>\n";
    file <<"using namespace std;\n";
//    file <<"void __attribute__((constructor)) startup()\n";
//    file <<"{std::cout<<\"dynamic lib start to load!\"<<std::endl;}\n";
//    file <<"void __attribute__((destructor)) shutdown()\n";
//    file <<"{std::cout<<\"dynamic lib start to unload!\"<<std::endl;}\n";
    std::string buffer;
    char line[8192];
    int import_flag=0;


    while(infile.getline(line,sizeof(line)))
    {
        buffer=line;

        std::string::size_type pos(0);


        if(!import_flag)
        {
            std::string lowerBuffer=buffer;
            std::transform(lowerBuffer.begin(), lowerBuffer.end(), lowerBuffer.begin(), ::tolower);
            if((pos=lowerBuffer.find(cxx_include))!=std::string::npos)
            {
                //std::cout<<"haha find it!"<<endl;
                std::string newLine=buffer.substr(pos+cxx_include.length());
                import_flag=1;
                if((pos=newLine.find(cxx_end))!=std::string::npos)
                {
                    newLine=newLine.substr(0,pos);
                    file<<newLine<<"\n";
                    break;
                }else{
                    file<<newLine<<"\n";

                }

            }
        }
        else
        {
            //std::cout<<buffer<<endl;
            if((pos=buffer.find(cxx_end))!=std::string::npos)
            {
                std::string newLine=buffer.substr(0,pos);
                file<<newLine<<"\n";

                break;
            }
            else
            {
                //std::cout<<"to source file"<<buffer<<endl;
                file<<buffer<<"\n";
            }

        }



    }
    //std::cout<<"import_flag="<<import_flag<<std::endl;
    if(import_flag==0)
    {
        infile.clear();
        infile.seekg(0,std::ifstream::beg);
    }

    //std::cout<<"file pos:"<<infile.tellg()<<std::endl;

    std::string viewDataName=className+"_view_data";
    file <<"HttpResponsePtr "<<className<<"::genHttpResponse(const HttpViewData& "<<viewDataName<<")\n{\n";
    //std::string bodyName=className+"_bodystr";
    std::string streamName=className+"_tmp_stream";

    //oSrcFile <<"\tstd::string "<<bodyName<<";\n";
    file<<"\tstd::stringstream "<<streamName<<";\n";
    int cxx_flag=0;
    while(infile.getline(line,sizeof(line)))
    {
        buffer=line;
        parseLine(file,buffer,streamName,viewDataName,cxx_flag);
    }

    file<<"\tauto res = HttpResponse::newHttpResponse();\n";
    file<<"\tres->setStatusCode(HttpResponse::k200Ok);\n";
    file<<"#ifdef CONTENT_TYPE\n";
    file<<"\tres->setContentTypeCode(CONTENT_TYPE);\n";
    file<<"#else\n";
    file<<"\tres->setContentTypeCode(CT_TEXT_HTML);\n";
    file<<"#endif\n";
    file<<"\tres->setBody("<<streamName<<".str().c_str());\n\treturn res;\n}\n";
}
