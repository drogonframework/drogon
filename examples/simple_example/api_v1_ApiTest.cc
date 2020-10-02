#include "api_v1_ApiTest.h"
using namespace api::v1;
// add definition of your processing function here
void ApiTest::rootGet(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto res = HttpResponse::newHttpResponse();
    res->setBody("ROOT Get!!!");
    callback(res);
}
void ApiTest::rootPost(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback)
{
    std::thread([callback = std::move(callback)]() {
        auto res = HttpResponse::newHttpResponse();
        res->setBody("ROOT Post!!!");
        callback(res);
    }).detach();
}
void ApiTest::get(const HttpRequestPtr &req,
                  std::function<void(const HttpResponsePtr &)> &&callback,
                  int p1,
                  std::string &&p2)
{
    HttpViewData data;
    data.insert("title", std::string("ApiTest::get"));
    std::unordered_map<std::string, std::string> para;
    para["p1"] = std::to_string(p1);
    para["p2"] = p2;
    data.insert("parameters", para);
    auto res = HttpResponse::newHttpViewResponse("ListParaView.csp", data);
    callback(res);
}

void ApiTest::your_method_name(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    double p1,
    int p2) const
{
    LOG_WARN << req->matchedPathPatternData();
    HttpViewData data;
    data.insert("title", std::string("ApiTest::get"));
    std::unordered_map<std::string, std::string> para;
    para["p1"] = std::to_string(p1);
    para["p2"] = std::to_string(p2);
    para["p3"] = HttpViewData::htmlTranslate(string_view(
        "<script>alert(\" This should not be displayed in a browser alert "
        "box.\");</script>"));
    data.insert("parameters", para);
    auto res = HttpResponse::newHttpViewResponse("ListParaView", data);
    callback(res);
}
void ApiTest::staticApi(const HttpRequestPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto resp = HttpResponse::newHttpResponse();
    resp->setBody("staticApi,hello!!");
    resp->setExpiredTime(0);  // cache the response forever;
    callback(resp);
}

void ApiTest::get2(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback,
                   std::string &&p1)
{
    // test gzip feature
    auto res = HttpResponse::newHttpResponse();
    res->setBody(
        "Applications\n"
        "Developer\n"
        "Library\n"
        "Network\n"
        "System\n"
        "Users\n"
        "Volumes\n"
        "bin\n"
        "cores\n"
        "dev\n"
        "etc\n"
        "home\n"
        "installer.failurerequests\n"
        "net\n"
        "opt\n"
        "private\n"
        "sbin\n"
        "tmp\n"
        "usb\n"
        "usr\n"
        "var\n"
        "vm\n"
        "\n"
        "/Applications:\n"
        "Adobe\n"
        "Adobe Creative Cloud\n"
        "Adobe Photoshop CC\n"
        "AirPlayer Pro.app\n"
        "Android Studio.app\n"
        "App Store.app\n"
        "Autodesk\n"
        "Automator.app\n"
        "Axure RP Pro 7.0.app\n"
        "BaiduNetdisk_mac.app\n"
        "CLion.app\n"
        "Calculator.app\n"
        "Calendar.app\n"
        "Chess.app\n"
        "CleanApp.app\n"
        "Contacts.app\n"
        "DVD Player.app\n"
        "Dashboard.app\n"
        "Dictionary.app\n"
        "Docs for Xcode.app\n"
        "FaceTime.app\n"
        "FinalShell\n"
        "Firefox.app\n"
        "Folx.app\n"
        "Font Book.app\n"
        "GitHub.app\n"
        "Google Chrome.app\n"
        "Grammarly.app\n"
        "Image Capture.app\n"
        "Lantern.app\n"
        "Launchpad.app\n"
        "License.rtf\n"
        "MacPorts\n"
        "Mail.app\n"
        "Maps.app\n"
        "Messages.app\n"
        "Microsoft Excel.app\n"
        "Microsoft Office 2011\n"
        "Microsoft OneNote.app\n"
        "Microsoft Outlook.app\n"
        "Microsoft PowerPoint.app\n"
        "Microsoft Word.app\n"
        "Mindjet MindManager.app\n"
        "Mission Control.app\n"
        "Mockplus.app\n"
        "MyEclipse 2015\n"
        "Notes.app\n"
        "OmniGraffle.app\n"
        "Pages.app\n"
        "Photo Booth.app\n"
        "Photos.app\n"
        "Preview.app\n"
        "QJVPN.app\n"
        "QQ.app\n"
        "QuickTime Player.app\n"
        "RAR Extractor Lite.app\n"
        "Reminders.app\n"
        "Remote Desktop Connection.app\n"
        "Renee Undeleter.app\n"
        "Sabaki.app\n"
        "Safari.app\n"
        "ShadowsocksX.app\n"
        "Siri.app\n"
        "SogouInputPad.app\n"
        "Stickies.app\n"
        "System Preferences.app\n"
        "TeX\n"
        "Telegram.app\n"
        "Termius.app\n"
        "Tesumego - How to Make a Professional Go Player.app\n"
        "TextEdit.app\n"
        "Thunder.app\n"
        "Time Machine.app\n"
        "Tunnelblick.app\n"
        "Utilities\n"
        "VPN Shield.appdownload\n"
        "VirtualBox.app\n"
        "WeChat.app\n"
        "WinOnX2.app\n"
        "Wireshark.app\n"
        "Xcode.app\n"
        "Yose.app\n"
        "YoudaoNote.localized\n"
        "finalshelldata\n"
        "iBooks.app\n"
        "iPhoto.app\n"
        "iTools.app\n"
        "iTunes.app\n"
        "pgAdmin 4.app\n"
        "wechatwebdevtools.app\n"
        "\n"
        "/Applications/Adobe:\n"
        "Flash Player\n"
        "\n"
        "/Applications/Adobe/Flash Player:\n"
        "AddIns\n"
        "\n"
        "/Applications/Adobe/Flash Player/AddIns:\n"
        "airappinstaller\n"
        "\n"
        "/Applications/Adobe/Flash Player/AddIns/airappinstaller:\n"
        "airappinstaller\n"
        "digest.s\n"
        "\n"
        "/Applications/Adobe Creative Cloud:\n"
        "Adobe Creative Cloud\n"
        "Icon\n"
        "Uninstall Adobe Creative Cloud\n"
        "\n"
        "/Applications/Adobe Photoshop CC:\n"
        "Adobe Photoshop CC.app\n"
        "Configuration\n"
        "Icon\n"
        "Legal\n"
        "LegalNotices.pdf\n"
        "Locales\n"
        "Plug-ins\n"
        "Presets\n"
        "\n"
        "/Applications/Adobe Photoshop CC/Adobe Photoshop CC.app:\n"
        "Contents\n"
        "Linguistics\n"
        "\n"
        "/Applications/Adobe Photoshop CC/Adobe Photoshop CC.app/Contents:\n"
        "Application Data\n"
        "Frameworks\n"
        "Info.plist\n"
        "MacOS\n"
        "PkgInfo\n"
        "Required\n"
        "Resources\n"
        "_CodeSignature\n"
        "\n"
        "/Applications/Adobe Photoshop CC/Adobe Photoshop "
        "CC.app/Contents/Application Data:\n"
        "Custom File Info Panels\n"
        "\n"
        "/Applications/Adobe Photoshop CC/Adobe Photoshop "
        "CC.app/Contents/Application Data/Custom File Info Panels:\n"
        "4.0\n"
        "\n"
        "/Applications/Adobe Photoshop CC/Adobe Photoshop "
        "CC.app/Contents/Application Data/Custom File Info Panels/4.0:\n"
        "bin\n"
        "custom\n"
        "panels\n"
        "\n"
        "/Applications/Adobe Photoshop CC/Adobe Photoshop "
        "CC.app/Contents/Application Data/Custom File Info Panels/4.0/bin:\n"
        "FileInfoFoundation.swf\n"
        "FileInfoUI.swf\n"
        "framework.swf\n"
        "loc\n"
        "\n"
        "/Applications/Adobe Photoshop CC/Adobe Photoshop "
        "CC.app/Contents/Application Data/Custom File Info "
        "Panels/4.0/bin/loc:\n"
        "FileInfo_ar_AE.dat\n"
        "FileInfo_bg_BG.dat\n"
        "FileInfo_cs_CZ.dat\n"
        "FileInfo_da_DK.dat\n"
        "FileInfo_de_DE.dat\n"
        "FileInfo_el_GR.dat\n"
        "FileInfo_en_US.dat\n"
        "FileInfo_es_ES.dat\n"
        "FileInfo_et_EE.dat\n"
        "FileInfo_fi_FI.dat\n"
        "FileInfo_fr_FR.dat\n"
        "FileInfo_he_IL.dat\n"
        "FileInfo_hr_HR.dat\n"
        "FileInfo_hu_HU.dat\n"
        "FileInfo_it_IT.dat\n"
        "FileInfo_ja_JP.dat\n"
        "FileInfo_ko_KR.dat\n"
        "FileInfo_lt_LT.dat\n"
        "FileInfo_lv_LV.dat\n"
        "FileInfo_nb_NO.dat\n"
        "FileInfo_nl_NL.dat\n"
        "FileInfo_pl_PL.dat\n"
        "FileInfo_pt_BR.dat\n"
        "FileInfo_ro_RO.dat\n"
        "FileInfo_ru_RU.dat\n"
        "FileInfo_sk_SK.dat\n"
        "FileInfo_sl_SI.dat\n"
        "FileInfo_sv_SE.dat\n"
        "FileInfo_tr_TR.dat\n"
        "FileInfo_uk_UA.dat\n"
        "FileInfo_zh_CN.dat\n"
        "FileInfo_zh_TW.dat\n"
        "\n"
        "/Applications/Adobe Photoshop CC/Adobe Photoshop "
        "CC.app/Contents/Application Data/Custom File Info Panels/4.0/custom:\n"
        "DICOM.xml\n"
        "Mobile.xml\n"
        "loc\n"
        "\n"
        "/Applications/Adobe Photoshop CC/Adobe Photoshop "
        "CC.app/Contents/Application Data/Custom File Info "
        "Panels/4.0/custom/loc:\n"
        "DICOM_ar_AE.dat\n"
        "DICOM_bg_BG.dat\n"
        "DICOM_cs_CZ.dat\n"
        "DICOM_da_DK.dat\n"
        "DICOM_de_DE.dat\n"
        "DICOM_el_GR.dat\n"
        "DICOM_en_US.dat\n"
        "DICOM_es_ES.dat\n"
        "DICOM_et_EE.dat\n"
        "DICOM_fi_FI.dat\n"
        "DICOM_fr_FR.dat\n"
        "DICOM_he_IL.dat\n"
        "DICOM_hr_HR.dat\n"
        "DICOM_hu_HU.dat\n"
        "DICOM_it_IT.dat\n"
        "DICOM_ja_JP.dat\n"
        "DICOM_ko_KR.dat\n"
        "DICOM_lt_LT.dat\n"
        "DICOM_lv_LV.dat\n"
        "DICOM_nb_NO.dat\n"
        "DICOM_nl_NL.dat\n"
        "DICOM_pl_PL.dat\n"
        "DICOM_pt_BR.dat\n"
        "DICOM_ro_RO.dat\n"
        "DICOM_ru_RU.dat\n"
        "DICOM_sk_SK.dat\n"
        "DICOM_sl_SI.dat\n"
        "DICOM_sv_SE.dat\n"
        "DICOM_tr_TR.dat\n"
        "DICOM_uk_UA.dat\n"
        "DICOM_zh_CN.dat\n"
        "DICOM_zh_TW.dat\n"
        "Mobile_ar_AE.dat\n"
        "Mobile_bg_BG.dat\n"
        "Mobile_cs_CZ.dat\n"
        "Mobile_da_DK.dat\n"
        "Mobile_de_DE.dat\n"
        "Mobile_el_GR.dat\n"
        "Mobile_en_US.dat\n"
        "Mobile_es_ES.dat\n"
        "Mobile_et_EE.dat\n"
        "Mobile_fi_FI.dat\n"
        "Mobile_fr_FR.dat\n"
        "Mobile_he_IL.dat\n"
        "Mobile_hr_HR.dat\n"
        "Mobile_hu_HU.dat\n"
        "Mobile_it_IT.dat\n"
        "Mobile_ja_JP.dat\n"
        "Mobile_ko_KR.dat\n"
        "Mobile_lt_LT.dat\n"
        "Mobile_lv_LV.dat\n"
        "Mobile_nb_NO.dat\n"
        "Mobile_nl_NL.dat\n"
        "Mobile_pl_PL.dat\n"
        "Mobile_pt_BR.dat\n"
        "Mobile_ro_RO.dat\n"
        "Mobile_ru_RU.dat\n"
        "Mobile_sk_SK.dat\n"
        "Mobile_sl_SI.dat\n"
        "Mobile_sv_SE.dat\n"
        "Mobile_tr_TR.dat\n"
        "Mobile_uk_UA.dat\n"
        "Mobile_zh_CN.dat\n"
        "Mobile_zh_TW.dat\n"
        "\n"
        "/Applications/Adobe Photoshop CC/Adobe Photoshop "
        "CC.app/Contents/Application Data/Custom File Info Panels/4.0/panels:\n"
        "IPTC\n"
        "IPTCExt\n"
        "advanced\n"
        "audioData\n"
        "camera\n"
        "categories\n"
        "description\n"
        "dicom\n"
        "gpsData\n"
        "history\n"
        "mobile\n"
        "origin\n"
        "rawpacket");
    res->setExpiredTime(0);
    callback(res);
}

void ApiTest::jsonTest(std::shared_ptr<Json::Value> &&json,
                       std::function<void(const HttpResponsePtr &)> &&callback)
{
    Json::Value ret;
    if (json)
    {
        ret["result"] = "ok";
    }
    else
    {
        ret["result"] = "bad";
    }
    auto resp = HttpResponse::newCustomHttpResponse(ret);
    callback(resp);
}

void ApiTest::formTest(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto parameters = req->getParameters();
    Json::Value ret;
    ret["k1"] = parameters["k1"];
    ret["k2"] = parameters["k2"];
    ret["k3"] = parameters["k3"];
    if (parameters["k1"] == "1" && parameters["k2"] == "安" &&
        parameters["k3"] == "test@example.com")
    {
        ret["result"] = "ok";
    }
    else
    {
        ret["result"] = "bad";
    }
    auto resp = HttpResponse::newHttpJsonResponse(ret);
    callback(resp);
}

void ApiTest::attributesTest(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    AttributesPtr attributes = req->getAttributes();
    const std::string key = "ATTR_ADDR";
    Json::Value ret;

    uint64_t data = (uint64_t)req.get();

    if (attributes->find(key))
    {
        ret["result"] = "bad";
        callback(HttpResponse::newHttpJsonResponse(ret));
        return;
    }

    attributes->insert(key, data);

    if (!attributes->find(key) || attributes->get<uint64_t>(key) != data)
    {
        ret["result"] = "bad";
    }
    else
    {
        ret["result"] = "ok";
    }

    callback(HttpResponse::newHttpJsonResponse(ret));
    return;
}

void ApiTest::regexTest(const HttpRequestPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback,
                        int p1,
                        std::string &&p2)
{
    Json::Value ret;
    ret["p1"] = p1;
    ret["p2"] = std::move(p2);
    auto resp = HttpResponse::newHttpJsonResponse(std::move(ret));
    callback(resp);
}