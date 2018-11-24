#include <drogon/utils/Utilities.h>
#include <drogon/FileUpload.h>
#ifdef USE_OPENSSL
#include <openssl/md5.h>
#else
#include "ssl_funcs/Md5.h"
#endif
#include <iostream>
#include <fstream>
#include<sys/stat.h>
#include<unistd.h>
#include<fcntl.h>
#include <algorithm>

using namespace drogon;

const std::vector<HttpFile> FileUpload::getFiles()
{
    return files_;
}
const std::map<std::string, std::string> &FileUpload::getPremeter() const
{
    return premeter_;
};
int FileUpload::parse(const HttpRequestPtr &req)
{
    if (req->method() != Post)
        return -1;
    std::string contentType = req->getHeader("Content-Type");
    if (contentType.empty())
    {
        return -1;
    }
    std::string::size_type pos = contentType.find(";");
    if (pos == std::string::npos)
        return -1;

    std::string type = contentType.substr(0, pos);
    std::transform(type.begin(), type.end(), type.begin(), tolower);
    if (type != "multipart/form-data")
        return -1;
    pos = contentType.find("boundary=");
    if (pos == std::string::npos)
        return -1;
    std::string boundary = contentType.substr(pos + 9);
    //std::cout << "boundary[" << boundary << "]" << std::endl;
    std::string content = req->query();
    std::string::size_type pos1, pos2;
    pos1 = 0;
    pos2 = content.find(boundary);
    while (1)
    {
        pos1 = pos2;
        if (pos1 == std::string::npos)
            break;
        pos1 += boundary.length();
        if (content[pos1] == '\r' && content[pos1 + 1] == '\n')
            pos1 += 2;
        pos2 = content.find(boundary, pos1);
        if (pos2 == std::string::npos)
            break;
        //    std::cout<<"pos1="<<pos1<<" pos2="<<pos2<<std::endl;
        if (content[pos2 - 4] == '\r' && content[pos2 - 3] == '\n' && content[pos2 - 2] == '-' && content[pos2 - 1] == '-')
            pos2 -= 4;
        if (parseEntity(content.substr(pos1, pos2 - pos1)) != 0)
            return -1;
        //pos2+=boundary.length();
    }
    return 0;
}
int FileUpload::parseEntity(const std::string &str)
{
    //    parseEntity:[Content-Disposition: form-data; name="userfile1"; filename="appID.txt"
    //    Content-Type: text/plain
    //
    //    AA004MV7QI]
    //    pos1=190 pos2=251
    //    parseEntity:[YvBu
    //    Content-Disposition: form-data; name="text"
    //
    //    text]

    // std::cout<<"parseEntity:["<<str<<"]"<<std::endl;
    std::string::size_type pos = str.find("name=\"");
    if (pos == std::string::npos)
        return -1;
    pos += 6;
    std::string::size_type pos1 = str.find("\"", pos);
    if (pos1 == std::string::npos)
        return -1;
    std::string name = str.substr(pos, pos1 - pos);
    // std::cout<<"name=["<<name<<"]\n";
    pos = str.find("filename=\"");
    if (pos == std::string::npos)
    {
        pos1 = str.find("\r\n\r\n");
        if (pos1 == std::string::npos)
            return -1;
        premeter_[name] = str.substr(pos1 + 4);
        return 0;
    }
    else
    {
        pos += 10;
        std::string::size_type pos1 = str.find("\"", pos);
        if (pos1 == std::string::npos)
            return -1;
        HttpFile file;
        file.setFileName(str.substr(pos, pos1 - pos));
        pos1 = str.find("\r\n\r\n");
        if (pos1 == std::string::npos)
            return -1;
        file.setFile(str.substr(pos1 + 4));
        files_.push_back(file);
        return 0;
    }
}

int HttpFile::save(const std::string &path)
{
    if (fileName_ == "")
        return -1;
    std::string filename;
	std::string path_("./"+path);
	//replace_all(path_, "../", "");
	//to prevent to create dir on the top of current dir
	if(path_.find("../")!=std::string::npos){
		return -1;
	}
	if(access(path_.c_str(), F_OK)!=0){
		if (mkdir(path_.c_str(), 0667)==-1){
			return -1;
		}
	}

    if (path_[path_.length()] != '/')
    {
        filename = path_ + "/" + fileName_;
    }
    else
        filename = path_ + fileName_;

    return saveAs(filename);
}

int HttpFile::saveAs(const std::string &filename)
{
    std::ofstream file(filename);
    if (file.is_open())
    {
        file << fileContent_;
        file.close();
        return 0;
    }
    else
        return -1;
}
const std::string HttpFile::getMd5() const
{
#ifdef USE_OPENSSL
    MD5_CTX c;
    unsigned char md5[16] = {0};
    MD5_Init(&c);
    MD5_Update(&c, fileContent_.c_str(), fileContent_.size());
    MD5_Final(md5, &c);
    return binaryStringToHex(md5, 16);
#else
    Md5Encode encode;
    return encode.Encode(fileContent_);
#endif
}
