/* A function to detect a keyboard press on Linux */
#include "GenericMethods.h"

#include "windows.h"

#include <iostream>
#include <tuple>

#include <fstream>
#include <string>

uint16_t inputRanges[PS5000A_MAX_RANGES] = {
                        10,
                        20,
                        50,
                        100,
                        200,
                        500,
                        1000,
                        2000,
                        5000,
                        10000,
                        20000,
                        50000 };


/****************************************************************************
* adc_to_mv
*
* Convert an 16-bit ADC count into millivolts
****************************************************************************/
int32_t adc_to_mv(int32_t raw, int32_t rangeIndex, int16_t maxADCValue)
{
    return (raw * inputRanges[rangeIndex]) / maxADCValue;
}

/****************************************************************************
* mv_to_adc
*
* Convert a millivolt value into a 16-bit ADC count
*
*  (useful for setting trigger thresholds)
****************************************************************************/
int16_t mv_to_adc(int16_t mv, int16_t rangeIndex, int16_t maxADCValue)
{
    return (mv * maxADCValue) / inputRanges[rangeIndex];
}

bool g_ready = FALSE;


/****************************************************************************
* Callback
* used by ps5000a data block collection calls, on receipt of data.
* used to set global flags etc checked by user routines
****************************************************************************/
void PREF4 callBackBlock(int16_t handle, PICO_STATUS status, void* pParameter)
{
    if (status != PICO_CANCELLED)
    {
        g_ready = TRUE;
    }
}

void getStatusCode() {

    std::string myText;

    // Read from the text file
    std::ifstream MyReadFile("C:\\Program\ Files\\Pico\ Technology\\SDK\\inc\\PicoStatus.h");

    auto i = 0;
    auto cat = -1;
    std::string arr[2] = {
        "PICO_POINTER" ,
        "PICO_INFO   "
    };
    std::cout << std::endl;

    // Use a while loop together with the getline() function to read the file line by line
    std::cout << "    TYPE    |    HEX   |   DEC\t|      DESCRIPTION      |" << std::endl;
    while (getline(MyReadFile, myText)) {
        // Output the text from the file
        auto first = myText.find("0x");
        auto definePos = myText.find("#define");
        if (definePos != std::string::npos)
            if (first != std::string::npos) {
              std::string s = myText.substr(first, 10);
              std::string startText = myText.substr(8);
              auto endTextSpace = startText.find(" ");
              auto endTextTab = startText.find("\t");
              std::string text;
              if (endTextTab != std::string::npos)
                startText.substr(0, endTextTab);
              else
                text = startText.substr(0, endTextSpace);
              std::string num = s.substr(2);
              auto j = std::stoi(num, 0, 16);
              if (j == 0) {
                  cat++;
                  std::cout << std::endl;
              }
              std::cout << arr[cat] << " " << s << " " << j << " \t  " << text << std::endl;
            }
        i++;
    }
    std::cout << std::endl;

}

void getStatusCodeCSV() {

    std::string myText;

    // Read from the text file
    std::ifstream MyReadFile("C:\\Program\ Files\\Pico\ Technology\\SDK\\inc\\PicoStatus.h");

    auto i = 0;
    auto cat = -1;
    std::string arr[2] = {
        "PICO_POINTER" ,
        "PICO_INFO"
    };
    std::cout << std::endl;

    // Use a while loop together with the getline() function to read the file line by line
    std::cout << "    TYPE    |    HEX   |   DEC\t|      DESCRIPTION      |" << std::endl;
    std::string prevComment;
    auto commentCount = 0;
    while (getline(MyReadFile, myText)) {
        // Output the text from the file
        auto first = myText.find("0x");
        auto definePos = myText.find("#define");
        auto commentPos = myText.find("//");
        if (commentPos != std::string::npos) {
            if (myText.size() < 3)
                continue;
            if (prevComment.size() > 0) 
                prevComment.append("\n ");

            std::string myText2 = myText.substr(3);
            auto tabPos = myText2.find("\t");
            if( tabPos != std::string::npos )
                prevComment.append(myText2.substr(tabPos));
            else
                prevComment.append(myText2);

            prevComment.append(". ");
            commentCount++;
        }
        if (definePos != std::string::npos)
            if (first != std::string::npos) {
                std::string s = myText.substr(first, 10);
                std::string startText = myText.substr(8);
                auto endTextSpace = startText.find(" ");
                auto endTextTab = startText.find("\t");
                std::string text;
                if (endTextTab != std::string::npos)
                    text = startText.substr(0, endTextTab);
                else
                    text = startText.substr(0, endTextSpace);
                std::string num = s.substr(2);
                auto j = std::stoi(num, 0, 16);
                if (j == 0) {
                    cat++;
                    std::cout << std::endl;
                }
                if (commentCount > 1) {
                  std::cout << std::endl << prevComment << std::endl;
                  std::cout << arr[cat] << "," << s << "," << j << "," << text << std::endl;
                }
                else {
                  std::cout << arr[cat] << "," << s << "," << j << "," << text << "," << prevComment << std::endl;
                }
                prevComment.clear();
                commentCount = 0;
            }
        i++;
    }
    std::cout << std::endl;

}

void getStatusCode(int Code) {

    std::cout << Code << std::endl;

    std::string myText;

    // Read from the text file
    std::ifstream MyReadFile("C:\\Program\ Files\\Pico\ Technology\\SDK\\inc\\PicoStatus.h");

    auto i = 0;
    auto cat = -1;
    std::string arr[2] = {
        "PICO_POINTER" ,
        "PICO_INFO   "
    };
    std::cout << std::endl;

    // Use a while loop together with the getline() function to read the file line by line
    std::cout << "    TYPE    |    HEX   |   DEC\t|      DESCRIPTION      |" << std::endl;
    while (getline(MyReadFile, myText)) {
        // Output the text from the file
        auto first = myText.find("0x");
        auto definePos = myText.find("#define");
        if (definePos != std::string::npos)
            if (first != std::string::npos) {
                std::string s = myText.substr(first, 10);
                std::string startText = myText.substr(8);
                auto endText = startText.find(" ");
                std::string text = startText.substr(0, endText);
                std::string num = s.substr(2);
                auto j = stoi(num, 0, 16);
                if (j == 0)
                    cat++;
                if (j == Code)
                  std::cout << arr[cat] << " " << s << " " << j << " \t  " << text << std::endl;
            }
        i++;
    }
    std::cout << std::endl;

}