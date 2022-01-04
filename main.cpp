// Copyright Andrea Alberti, 2019
// All rights reserved

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <shlwapi.h>
#include <vector>
#include <regex>
#include <cmath>
#include <windows.h>
#include <winuser.h>
// #include <VersionHelpers.h>

#define ID_TOGGLE_LOGGING_BTN (100)
#define ID_TOGGLE_SHUTTER_BTN (120)
#define ID_TOGGLE_COMM_BTN (130)
#define ID_RESCAN_COM_BTN (140)

#define ID_POWER_LBL (110)
#define ID_RMSPOWER_LBL (111)
#define ID_RMSPOWERAVG_LBL (112)
#define ID_ERROR_LBL (113)

#define ID_PORT_COMBO (120)

#define ID_TEXT_LASERNAME (100)
#define ID_TEXT_LOGPERIOD (110)
#define ID_TEXT_logFilePath (120)
#define ID_TEXT_SERIALNUMBER (130)
#define ID_TEXT_CMDLINE (140)

#define IDT_QUERY_TIMER (100)
#define IDT_STORE_TIMER (101)

using namespace std;

INT resumeLogging();
INT setShutterDown(BOOL status);
INT connectToLaser();
INT queryLaserStatus();
void disconnectFromLaser();
INT executeCmd(unsigned char* cmdLine, int* numBytesWritten = NULL, int* numBytesRead = NULL, char* readStr = NULL, size_t len_ReadStr = 0);
void printMemory(char* mem);

char* comPort = NULL;
HANDLE hSerial  = INVALID_HANDLE_VALUE;

static const DWORD delay = 100;

HANDLE isCOMbusyMutex;

BOOL isLogging = false;
BOOL doRecord = false;
BOOL doStore = false;
BOOL loggingThreadActive = false;
BOOL detectedError = false;

BOOL isShutterDown = false;

HWND hwnd;
HWND hwndPowerLabel;
HWND hwndFakeLabel;
HWND hwndTempDiodesLabel;
HWND hwndTempCrystalLabel;
HWND hwndRMSpowerLabel;
HWND hwndRMSpowerAvgLabel;
HWND hwndToggleLoggingButton;
HWND hwndRescanCOMportsButton;
HWND hwndToggleShutterButton;
HWND hwndToggleCommButton;
HWND hWndCMDline;
HWND hWndCMDlineRet;
HWND hWndComboBoxPorts;
HWND hWndLaserName;
HWND hWndSerialNumber;
HWND hWndLoggingPeriod;
HWND hWndLogDir;
HWND hwndLaserHours;
HWND hwndDiodeHours;
HWND hwndHeadHours;
HWND hwndDiodeCurrent;
HWND hwndError;

HANDLE hThread;

WNDPROC origTextEditWndProc;

DWORD threadID;

void writeConfigFile();
void getTimestamp(char* str);
double computeRMSpower(float* power_vals, size_t num_vals_power, char* rmspower);
void rescanCOMports();

char configPath[MAX_PATH+1];
char logFilePath[MAX_PATH+1];
ofstream logFile;

float logPeriod;
DWORD64 logPeriodMilli;
char logPeriodStr_last_valid[1024];

int numPorts;
vector<char*> namePorts;

char COM_NONE[] = "NONE";

string power_lbl("Power: ");
string rmspower_lbl("RMS power (last 5 sec): ");
string temp_diodes_lbl("Temp. diodes: ");
string temp_crystal_lbl("Temp. cyrstal tower: ");
string diode_hours_lbl("Diode hours: ");
string head_hours_lbl("Laser head hours: ");
string laser_hours_lbl("Electronics hours: ");
string diode_current_lbl("Diode current: ");
string empty_str("");
string title_str("Log app for Millennia");

char laserName[1024];
char serialNumber[1024];
char errorStr[255];

void stopLogging()
{
    if(isLogging)
    {
        isLogging = false;
        WaitForSingleObject(hThread,INFINITE);  // Wait for thread to finish execution
        CloseHandle(hThread);

        SetWindowText(hwndPowerLabel, power_lbl.c_str());
        SetWindowText(hwndRMSpowerLabel, rmspower_lbl.c_str());
        SetWindowText(hwndRMSpowerAvgLabel, "");
        SetWindowText(hwndTempDiodesLabel, temp_diodes_lbl.c_str());
        SetWindowText(hwndTempCrystalLabel, temp_crystal_lbl.c_str());
        SetWindowText(hwndDiodeCurrent, diode_current_lbl.c_str());
        SetWindowText(hwndDiodeHours, diode_current_lbl.c_str());
        SetWindowText(hwndHeadHours, head_hours_lbl.c_str());
        SetWindowText(hwndLaserHours, laser_hours_lbl.c_str());
        
        // Configure windows' controls
        SetWindowText(hwndToggleLoggingButton,  "RESUME LOGGING");
        EnableWindow(hWndLoggingPeriod,  true);
        EnableWindow(hWndLogDir,         true);
    }
}

void queryTimerFnc( 
    HWND hwnd,        // handle to window for timer messages 
    UINT message,     // WM_TIMER message 
    UINT idTimer,     // timer identifier 
    DWORD dwTime)     // current system time 
{
    doRecord = true;
}

void storeTimerFnc( 
    HWND hwnd,        // handle to window for timer messages 
    UINT message,     // WM_TIMER message 
    UINT idTimer,     // timer identifier 
    DWORD dwTime)     // current system time 
{
    doStore = true;
}

DWORD WINAPI ThreadLoggingFunc(void* data) {

    loggingThreadActive = true;

    cout << "Starting the logging thread function" << endl;

    DWORD64 counter;
    char power[255];
    char temperatureDiode[255];
    char temperatureCrystal[255];
    char rmspower[255];
    char rmspower_avg[255];
    char headHours[255];
    char laserHours[255];
    char diodeCurrent[255];
    char diodeHours[255];
    char timestamp[255];
    char error[255];

    size_t num_vals_rmspower = round((float)logPeriodMilli/(float)delay);
    double* rmspower_vals = (double*)malloc(sizeof(double)*(num_vals_rmspower));

    size_t num_vals_power = round(5.0f*1000.0f/delay); // 5 second per default for the GUI update
    float* power_vals = (float*)malloc(sizeof(float)*num_vals_power);
    
    size_t k;
    BOOL array_powers_filled, array_rmspowers_filled;

    for(k = 0; k<num_vals_power; k++)
    {
        power_vals[k] = 0;
    }
    
    for(k = 0; k<num_vals_rmspower; k++)
    {
        rmspower_vals[k] = 0;
    }
    
    k = 0;
    counter = 0;
    array_powers_filled = false;
    array_rmspowers_filled = false;
    strcpy(rmspower,"NaN");
    strcpy(rmspower_avg,"NaN");

    INT numBytesRead;
    
    int s = 0;

    doStore = true;
    doRecord = false;

    SetTimer(hwnd,             // handle to main window 
        IDT_STORE_TIMER,        // timer identifier 
        logPeriodMilli,                 // delay
        (TIMERPROC) storeTimerFnc);

    SetTimer(hwnd,             // handle to main window 
        IDT_QUERY_TIMER,        // timer identifier 
        delay,                 // delay
        (TIMERPROC) queryTimerFnc);
    
    while(isLogging)
    {
        if(doRecord)
        {            
            executeCmd((unsigned char*)"?p", NULL, &numBytesRead, power, 255);
            
            if(isLogging)
            {
                SetWindowText(hwndPowerLabel, (power_lbl + (const char*)power + " W").c_str());
            }

            getTimestamp((char*)timestamp);

            power_vals[k] = stof((const char*)power);

            if(array_powers_filled)
            {
                rmspower_vals[counter] = computeRMSpower(power_vals,num_vals_power,(char*)rmspower);
                s++;
            }
            
            if(doStore)
            {
                if(array_rmspowers_filled)
                {
                    double total_rmspower = 0;
                    for(int j = 0; j<num_vals_rmspower; j++)
                    {
                        total_rmspower += rmspower_vals[j];
                    }
                    total_rmspower /= (double)num_vals_rmspower;

                    sprintf(rmspower_avg,"%1.4f",total_rmspower);

                    if(isLogging)
                    {
                        SetWindowText(hwndRMSpowerAvgLabel, (empty_str + "(avg: " + rmspower_avg + "%)").c_str());
                    }
                }

                executeCmd((unsigned char*)"?T", NULL, &numBytesRead, (char*)temperatureDiode, 255);
                executeCmd((unsigned char*)"?TT", NULL, &numBytesRead, (char*)temperatureCrystal, 255);
                executeCmd((unsigned char*)"?C", NULL, &numBytesRead, (char*)diodeCurrent, 255);
                executeCmd((unsigned char*)"?PSHRS", NULL, &numBytesRead, (char*)laserHours, 255);
                executeCmd((unsigned char*)"?HEADHRS", NULL, &numBytesRead, (char*)headHours, 255);
                executeCmd((unsigned char*)"?DH", NULL, &numBytesRead, (char*)diodeHours, 255);
                executeCmd((unsigned char*)"?FH", NULL, &numBytesRead, (char*)error, 255);

                if(strcmp(errorStr,error)!=0)
                {
                    strcpy(errorStr,error);
                    detectedError = true;
                }
                error[3] = 0;

                if(isLogging)
                {
                    SetWindowText(hwndTempDiodesLabel, (temp_diodes_lbl + (const char*)temperatureDiode + " C").c_str());
                    SetWindowText(hwndTempCrystalLabel, (temp_crystal_lbl + (const char*)temperatureCrystal + " C").c_str());
                    SetWindowText(hwndLaserHours, (laser_hours_lbl + (const char*)laserHours).c_str());
                    SetWindowText(hwndHeadHours, (head_hours_lbl + (const char*)headHours).c_str());
                    SetWindowText(hwndDiodeCurrent, (diode_current_lbl + (const char*)diodeCurrent + " A").c_str());
                    SetWindowText(hwndDiodeHours, (diode_hours_lbl + (const char*)diodeHours).c_str());

                    if(detectedError)
                    {
                        SetWindowText(hwndError,"Warning! Detected an error in the laser.\nQuery the laser with ?FH and check the log file.");
                    }
                }

                logFile << timestamp << "\t" << power << "\t" << rmspower << "\t" << rmspower_avg << "\t" << temperatureDiode << "\t" << temperatureCrystal << "\t" << diodeCurrent << "\t" << diodeHours << "\t" << headHours << "\t" << laserHours << "\t" << error << endl;

                doStore = false;
            }

            counter++;
            if(counter==num_vals_rmspower)
            {   
                if(s == counter)
                {
                    array_rmspowers_filled = true;
                }

                counter = 0;     
                array_rmspowers_filled = true;
            }

            k++;
            if(k==num_vals_power)
            {
                k = 0;
                array_powers_filled = true;
            }

            doRecord = false;
        }

        // Do not remove this pause - it serves to avoid consuming all CPU resources
        Sleep(1);
    }

    logFile.close();

    free(power_vals);
    free(rmspower_vals);

    KillTimer(hwnd, IDT_QUERY_TIMER);
    KillTimer(hwnd, IDT_STORE_TIMER);
    
    cout << "Exiting the logging thread function" << endl;
    
    loggingThreadActive = false;

    return 0;
}

double computeRMSpower(float* power_vals, size_t num_vals_power, char* rmspower)
{
    double sum_power = 0, sum_power2 = 0;

    for(size_t k = 0; k<num_vals_power; k++)
    {
        sum_power += power_vals[k];
        sum_power2 += power_vals[k]*power_vals[k];
    }
    
    double rmspower_num = sum_power2/(sum_power*sum_power)*(double)num_vals_power-1;
    if(rmspower_num<=0)
        rmspower_num = 0;
    else
        rmspower_num = sqrt(rmspower_num)*(double)100;

    sprintf(rmspower,"%1.4f",rmspower_num);

    if(isLogging)
        SetWindowText(hwndRMSpowerLabel, (rmspower_lbl + rmspower + "%").c_str());

    return rmspower_num;
}

INT readFromSerialPort(uint8_t * buffer, int buffersize, DWORD& dwBytesRead)
{
    dwBytesRead = 0;

    if(!ReadFile(hSerial, buffer, buffersize-1, &dwBytesRead, NULL)){
        cerr << "Error: Reading from " << comPort << "." << endl;

        MessageBox(hwnd, (empty_str + "Reading from " + comPort + ".").c_str(), "Error!",
            MB_ICONEXCLAMATION | MB_OK | MB_APPLMODAL);

        return 1;
    }

    if(!dwBytesRead)
    {
        cerr << "Error: Could not read from " << comPort << "." << endl;

        MessageBox(hwnd, (empty_str + "Reading from " + comPort + ".").c_str(), "Error!",
            MB_ICONEXCLAMATION | MB_OK | MB_APPLMODAL);

        return 1;
    }

    return 0;
}

INT writeToSerialPort(uint8_t * data, DWORD &dwBytesRead)
{
    int buffersize = strlen((const char*) data);

    dwBytesRead = 0;
    if(!WriteFile(hSerial, data, buffersize, &dwBytesRead, NULL)){
        cerr << "Error: Writing to " << comPort << "." << endl;

        MessageBox(hwnd, (empty_str + "Writing to " + comPort + ".").c_str(), "Error!",
            MB_ICONEXCLAMATION | MB_OK | MB_APPLMODAL);

        return 1;
    }

    if(!dwBytesRead)
    {
        cerr << "Error: Writing to " << comPort << "." << endl;

        MessageBox(hwnd, (empty_str + "Writing to " + comPort + ".").c_str(), "Error!",
            MB_ICONEXCLAMATION | MB_OK | MB_APPLMODAL);

        return 1;
    }

    return 0;
}

void exiting()
{
    stopLogging();
    disconnectFromLaser();

    CloseHandle(isCOMbusyMutex);

    cout << "Exiting the program" << endl;
}

BOOL str_to_float(const char* str, float& number)
{
    static const regex re("^(\\d+)([.,](\\d*))?$");
    cmatch m;

    if(regex_search(str, m, re))
    {
        string s(m[1].str());

        if(!m[3].str().empty()) 
        {
            s += "." + m[3].str();
        }

        number = stof(s);
        
        return true;
    }
    else
    {
        return false;
    }

}

const char g_szClassName[] = "myWindowClass";

// Step 4: the Window Procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static HBRUSH hBrush = CreateSolidBrush(RGB(255,255,255));

    switch(msg)
    {
        case WM_CLOSE:
        {
            DestroyWindow(hwnd);
        }
        break;
        case WM_DESTROY:
        {
            PostQuitMessage(0);
        }
        break;
        case WM_CTLCOLORSTATIC:
        {
            if((HWND)lParam == hwndError) 
            {
                SetBkMode((HDC)wParam,TRANSPARENT);
                SetTextColor((HDC)wParam, RGB(255,0,0));
            }
            return (INT_PTR)hBrush;        
        }
        break;
        case WM_COMMAND:
        {
            int ID = LOWORD(wParam);
            if(((HWND)lParam) && (HIWORD(wParam) == BN_CLICKED))
            {
                switch(ID)
                {
                    case ID_TOGGLE_LOGGING_BTN:
                    {   
                        if(isLogging)   
                            stopLogging();
                        else
                            resumeLogging();                            
                    }
                    break;
                    case ID_TOGGLE_SHUTTER_BTN:
                    {
                        if(isShutterDown)
                        {
                            SetWindowText(hwndToggleShutterButton,  "SET THE SHUTTER DOWN");
                            isShutterDown = false;
                        }
                        else
                        {
                            SetWindowText(hwndToggleShutterButton,  "OPEN THE SHUTTER");
                            isShutterDown = true;
                        }

                        setShutterDown(isShutterDown);
                    }
                    break;
                    case ID_RESCAN_COM_BTN:
                    {
                        //char* current_COM = (char*)malloc(sizeof(char)*(strlen(comPort)+1));
                        //strcpy(current_COM, comPort);

                        string current_COM = string(comPort);

                        rescanCOMports();

                        comPort = NULL;

                        for(int k = 0; k<numPorts; k++)
                        {
                            if(current_COM.compare(namePorts[k])==0)
                            {
                                comPort = namePorts[k];
                            }
                        }

                        if(comPort == NULL)
                        {
                            if(numPorts>0)
                                comPort = namePorts[0];
                            else
                                comPort = COM_NONE;
                        }

                        SendMessage(hWndComboBoxPorts,(UINT) CB_RESETCONTENT,(WPARAM) 0,(LPARAM)0); 

                        for(int k = 0; k<numPorts; k++)
                        {
                            SendMessage(hWndComboBoxPorts,(UINT) CB_ADDSTRING,(WPARAM) 0,(LPARAM)namePorts[k]); 
                            if(namePorts[k]==comPort)
                            {
                                SendMessage(hWndComboBoxPorts, CB_SETCURSEL, (WPARAM)k, (LPARAM)0);
                            }
                        }
                    }
                    break;
                    case ID_TOGGLE_COMM_BTN:
                    {
                        if(hSerial==INVALID_HANDLE_VALUE)
                        {
                            connectToLaser();

                            if(hSerial!=INVALID_HANDLE_VALUE)
                            {
                                SetWindowText(hwndToggleCommButton,  "DISCONNECT FROM LASER");
                                EnableWindow(hWndComboBoxPorts,  false);
                                EnableWindow(hwndRescanCOMportsButton,  false);
                                EnableWindow(hWndLaserName,      false);
                                EnableWindow(hWndSerialNumber,   false);
                                EnableWindow(hwndToggleLoggingButton,  true);
                                EnableWindow(hwndToggleShutterButton,  true);
                                EnableWindow(hWndCMDline,              true);
                            }

                        }
                        else
                        {
                            if(isLogging)
                            {
                                // PostMessageA(hwnd,WM_COMMAND,ID_TOGGLE_LOGGING_BTN,(LPARAM)hwndToggleLoggingButton);
                                stopLogging();
                            }

                            disconnectFromLaser();

                            SetWindowText(hwndToggleCommButton,  "CONNECT TO LASER");
                            EnableWindow(hWndComboBoxPorts,  true);
                            EnableWindow(hwndRescanCOMportsButton,  true);
                            EnableWindow(hWndLaserName,      true);
                            EnableWindow(hWndSerialNumber,   true);
                            EnableWindow(hwndToggleLoggingButton,  false);
                            EnableWindow(hwndToggleShutterButton,  false);
                            EnableWindow(hWndCMDline,              false);
                        }
                    }
                    break;
                    default:
                    break;
                }
            }
            else if(((HWND)lParam) && (HIWORD(wParam) == CBN_SELENDOK))
            {
                switch(ID)
                {
                    case ID_PORT_COMBO:
                    {
                        int selection = SendMessage(hWndComboBoxPorts,CB_GETCURSEL,(WPARAM)NULL,(LPARAM)NULL);

                        if (selection!=CB_ERR)
                        {
                            comPort = namePorts[selection];
                            writeConfigFile();                      
                        }                    
                    }
                    break;
                    default:
                    break;
                }
            }
            else if(((HWND)lParam) && (HIWORD(wParam) == EN_KILLFOCUS))
            {   
                switch(ID)
                {
                    case ID_TEXT_LOGPERIOD:
                    {
                        char logPeriodStr[1024];
                        GetWindowText(hWndLoggingPeriod,logPeriodStr,sizeof(logPeriodStr));

                        if(str_to_float(logPeriodStr,logPeriod))
                        {
                            strncpy(logPeriodStr_last_valid,logPeriodStr,sizeof(logPeriodStr_last_valid));
                        }
                        else
                        {
                            SetWindowText(hWndLoggingPeriod,logPeriodStr_last_valid);
                        }
                        logPeriodMilli = llround(logPeriod*1000.0f);
                        writeConfigFile();                 
                    }
                    break;
                    default:
                    break;
                } 
            }
            else if(((HWND)lParam) && (HIWORD(wParam) == EN_CHANGE))
            {
                switch(ID)
                {
                    case ID_TEXT_LASERNAME:
                    {
                        GetWindowText(hWndLaserName,laserName,sizeof(laserName));

                        if(strlen(laserName)>0)
                        {
                            SetWindowText(hwnd,(title_str+" ("+laserName+")").c_str());    
                        }
                        else
                        {
                            SetWindowText(hwnd,title_str.c_str());    
                        }

                        writeConfigFile();                      
                    }
                    break;
                    case ID_TEXT_logFilePath:
                    {
                        GetWindowText(hWndLogDir,logFilePath,sizeof(logFilePath));
                        writeConfigFile();
                    }
                    break;
                    case ID_TEXT_SERIALNUMBER:
                    {
                        GetWindowText(hWndSerialNumber,serialNumber,sizeof(serialNumber));
                        writeConfigFile();     
                    }
                    break;
                    default:
                    break;
                } 
            }
        }
        break;
        default:        
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    return 0;
}

INT setShutterDown(BOOL status)
{
    unsigned char query[] = "shut:?\n";
    query[5] = status ? '0' : '1';

    INT ret = executeCmd(query);

    return ret;
}

void readLaserPower(char* power)
{
    INT numBytesRead;
    
    if(executeCmd((unsigned char*)"?p", NULL, &numBytesRead, power, 255))
        return;

    if(isLogging)
        SetWindowText(hwndPowerLabel, (power_lbl + (const char*)power).c_str());
}

void CenterWindow(HWND hwndWindow)
{
    HWND hwndParent;
    RECT rectWindow, rectParent;

   // make the window relative to its parent
    if ((hwndParent = GetParent(hwndWindow)) == NULL)
    {
        hwndParent = GetDesktopWindow();
    }

    GetWindowRect(hwndWindow, &rectWindow);
    GetWindowRect(hwndParent, &rectParent);

    int nWidth = rectWindow.right - rectWindow.left;
    int nHeight = rectWindow.bottom - rectWindow.top;

    int nX = ((rectParent.right - rectParent.left) - nWidth) / 2 + rectParent.left;
    int nY = ((rectParent.bottom - rectParent.top) - nHeight) / 2 + rectParent.top;

    int nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
    int nScreenHeight = GetSystemMetrics(SM_CYSCREEN);

         // make sure that the dialog box never moves outside of the screen
    if (nX < 0) nX = 0;
    if (nY < 0) nY = 0;
    if (nX + nWidth > nScreenWidth) nX = nScreenWidth - nWidth;
    if (nY + nHeight > nScreenHeight) nY = nScreenHeight - nHeight;

    MoveWindow(hwndWindow, nX, nY, nWidth, nHeight, FALSE);
}

string trim(const string& str)
{
    size_t first = str.find_first_not_of(' ');
    if (string::npos == first)
    {
        return string("");
    }
    size_t last = str.find_last_not_of(' ');
    return str.substr(first, (last - first + 1));
}

void writeConfigFile()
{
    ofstream configFile;
    configFile.open(configPath,ios::out);

    configFile << "logFilePath = " << logFilePath << endl;
    configFile << "logPeriod = " << logPeriodStr_last_valid << endl;
    configFile << "comPort = " << comPort << endl;
    configFile << "laserName = " << laserName << endl;
    configFile << "serialNumber = " << serialNumber << endl;

    configFile.close();
}

void printMemory(char* mem)
{
	for(int i=0; i<20; i++)
	{
		printf("0x%02x ",*((unsigned char*)mem+i));
	}
    printf("\n");
}

INT executeCmd(unsigned char* cmdLine, int* numBytesWritten, int* numBytesRead, char* readStr, size_t len_ReadStr)
{
    DWORD numBytes;
    INT ret = 0;

    unsigned char cmd[260];
    strcpy((char*)cmd, (char*)cmdLine);
    // Concatenate an end of line \n character
    strcat((char*)cmd, "\n");

    WaitForSingleObject(isCOMbusyMutex,INFINITE);

    if(strlen((char*)cmd)>0)
        if(ret = writeToSerialPort(cmd, numBytes))
        {
            ReleaseMutex(isCOMbusyMutex);
            return ret;
        }
    
    if(numBytesWritten!=NULL)
        *numBytesWritten = numBytes;

    if(cmdLine[0]=='?' || cmdLine[0]=='*')
    {
        if(ret = readFromSerialPort((uint8_t*)readStr, len_ReadStr, numBytes))
        {
            ReleaseMutex(isCOMbusyMutex);
            return ret;
        }

        // Terminate the string
        readStr[numBytes] = 0;

        // Strip end of lines at the end
        for(int k=numBytes-1; k>=0; k--)
        {
            if(readStr[k]=='\n' || readStr[k]=='\r')
            {
                readStr[k] = 0;
            }
            else
                break;
        }

        if(numBytesRead!=NULL)
        {
            *numBytesRead = numBytes;
            if(readStr[*numBytesRead-1] == '\n')
            {
                readStr[*numBytesRead-1] = 0;
            }
        }
    }

    ReleaseMutex(isCOMbusyMutex);
    return ret;
}

LRESULT CALLBACK TextEditWithOnlyNumbersProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CHAR:
        {
            if(!((wParam>=48 && wParam <=57) || wParam == 44 || wParam == 46 || wParam == 8 ) )
                return 0;
        }
        default:
            return CallWindowProc(origTextEditWndProc, wnd, msg, wParam, lParam);
    }
    return 0;
}


LRESULT CALLBACK TextEditWithEnterProc(HWND wnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CHAR:
        {
            switch (wParam)
            {
                case VK_RETURN:
                    
                    char cmdLine[255];
                    GetWindowText(hWndCMDline,cmdLine,sizeof(cmdLine));

                    char read_str[255];
                    int numBytesRead = 0;
                    executeCmd((unsigned char*)cmdLine,NULL,&numBytesRead,(char*)read_str,sizeof(read_str));

                    SetWindowText(hWndCMDlineRet,(numBytesRead > 0) ? (LPCSTR)read_str : "");

                    queryLaserStatus();

                    return 0;

                break;
            }
        }
        default:
            return CallWindowProc(origTextEditWndProc, wnd, msg, wParam, lParam);
    }
    return 0;
}

void rescanCOMports()
{
    char physical[65536];
    char logical[65536];

    QueryDosDevice(NULL, physical, sizeof(physical));

    numPorts = 0;
    namePorts.clear();
    for (char *pos = physical; *pos; pos+=strlen(pos)+1) {
        QueryDosDevice(pos, logical, sizeof(logical));
        if(strncmp(pos,"COM",3)==0)
        {
            namePorts.push_back(pos);
            numPorts++;
        }
    }
}

// Solution from https://stackoverflow.com/a/36545162/4216175
RTL_OSVERSIONINFOW GetRealOSVersion() {
    typedef LONG NTSTATUS, * PNTSTATUS;
    typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE hMod = ::GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        RtlGetVersionPtr fxPtr = (RtlGetVersionPtr)::GetProcAddress(hMod, "RtlGetVersion");
        if (fxPtr != nullptr) {
            RTL_OSVERSIONINFOW version_info = { 0 };
            version_info.dwOSVersionInfoSize = sizeof(version_info);
            if (0x00000000 == fxPtr(&version_info)) { // in case of success
                return version_info;
            }
        }
    }
    RTL_OSVERSIONINFOW version_info = { 0 };
    return version_info;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	LPWSTR *szArgList;
	int argCount;

	szArgList = CommandLineToArgvW(GetCommandLineW(), &argCount);
	if (szArgList == NULL)
	{
		MessageBox(NULL, "Unable to parse command line", "Error!", MB_ICONEXCLAMATION | MB_OK | MB_APPLMODAL);
		return 1;
	}

	BOOL start_at_launch = false;
	BOOL configPath_provided = false;
	for(int i = 1; i < argCount; i++)
	{
		if(StrCmpW(szArgList[i],L"-start")==0)
			start_at_launch = true;
		else if(StrCmpW(szArgList[i],L"-config")==0 && (i+1)<argCount)
		{
			size_t numBytesConverted;
			wcstombs_s(&numBytesConverted, configPath, sizeof(configPath), szArgList[i+1], sizeof(configPath) );
			cout << configPath << endl;
			configPath_provided = true;
			i++;
		}
	}

	LocalFree(szArgList);

    HWND hwndLabel;

    cout << "Starting the program" << endl;
    atexit(exiting);

    strcpy(laserName,"");
    strcpy(serialNumber,"");

    strcpy(logPeriodStr_last_valid,"30");
    str_to_float(logPeriodStr_last_valid,logPeriod);
    logPeriodMilli = llround(logPeriod*1000.0f);

    if(!configPath_provided)
    {
    	char programPath[MAX_PATH+1];

    	int bytes = GetModuleFileName(NULL, programPath, MAX_PATH);

    	if(!bytes)
    	{
    		MessageBox(NULL, "Could not find program path!", "Error!",
    			MB_ICONEXCLAMATION | MB_OK | MB_APPLMODAL);
    		return 1;
    	}

    	strcpy(configPath,programPath);
    	PathRemoveFileSpecA(configPath);
    	strcpy(logFilePath,configPath);
	    PathCombineA(configPath,logFilePath,"config.txt");
    }
    else
    {
    	strcpy(logFilePath,configPath);
    	PathRemoveFileSpecA(logFilePath);
    	PathCombineA(logFilePath,logFilePath,"log.txt");
    }

    rescanCOMports();

    ifstream configFile;
    configFile.open(configPath,ios::in);

    if(configFile.good())
    {
        string line;
        while(getline(configFile, line) )
        {
            istringstream is_line(line);
            string key;
            if(getline(is_line, key, '=') )
            {
                string value;
                if(getline(is_line, value) )
                {
                    key = trim(key);
                    value = trim(value);
                    
                    if(key.compare("logFilePath") == 0)
                    {   
                        strcpy(logFilePath,value.c_str());
                    }
                    else if(key.compare("logPeriod") == 0)
                    {
                        if(str_to_float(value.c_str(),logPeriod))
                        {
                            strcpy(logPeriodStr_last_valid,value.c_str());
                        }                        
                    }
                    else if(key.compare("comPort") == 0)
                    {
                        for(int k = 0; k<numPorts; k++)
                        {
                            if(value.compare(namePorts[k])==0)
                            {
                                comPort = namePorts[k];
                            }
                        }
                    }
                    else if(key.compare("laserName") == 0)
                    {
                        strncpy(laserName,value.c_str(),sizeof(laserName));
                    }
                    else if(key.compare("serialNumber") == 0)
                    {
                        strncpy(serialNumber,value.c_str(),sizeof(serialNumber));
                    }
                }
            }
        }
    }

    configFile.close();

    if(comPort == NULL)
    {
        if(numPorts>0)
            comPort = namePorts[0];
        else
            comPort = COM_NONE;
    }

    WNDCLASSEX wc;
    MSG Msg;

    //Step 1: Registering the Window Class
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = 0;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = g_szClassName;
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

    if(!RegisterClassEx(&wc))
    {
        MessageBox(hwnd, "Window Registration Failed!", "Error!",
            MB_ICONEXCLAMATION | MB_OK | MB_APPLMODAL);
        return 1;
    }

    SIZE main_size;


    RTL_OSVERSIONINFOW version_info = GetRealOSVersion();
    ULONG winMajorVersion = version_info.dwMajorVersion;

    if(winMajorVersion>=10)
    {
        main_size.cx = 580;
        main_size.cy = 530;
    }
    else
    {
        main_size.cx = 571;
        main_size.cy = 518;
    }

    /* Main window */
    hwnd = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        g_szClassName,
        "",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        main_size.cx,
        main_size.cy,
        NULL, NULL, hInstance, NULL);

    if(hwnd == NULL)
    {
        MessageBox(NULL, "Window Creation Failed!", "Error!",
            MB_ICONEXCLAMATION | MB_OK | MB_APPLMODAL);
        return 1;
    }
    
    CenterWindow(hwnd);
    
    LONG_PTR windowStyle = GetWindowLongPtrA(hwnd, GWL_STYLE);

    windowStyle &= ~(WS_SIZEBOX | WS_MAXIMIZEBOX);

    SetWindowLongPtr(hwnd, GWL_STYLE, windowStyle);


    /* Connect/disconnect */
    hwndToggleCommButton = CreateWindow( 
    "BUTTON",  // Predefined class; Unicode assumed 
    "CONNECT TO LASER",      // Button text 
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
    350,         // x position 
    150,         // y position 
    200,        // Button width
    70,        // Button height
    hwnd,       // Parent window
    (HMENU)ID_TOGGLE_COMM_BTN,
    (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), 
    NULL);      // Pointer not needed.



    /* Stop/resume logging button */
    hwndToggleLoggingButton = CreateWindow( 
    "BUTTON",  // Predefined class; Unicode assumed 
    "RESUME LOGGING",      // Button text 
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
    350,         // x position 
    235,         // y position 
    200,        // Button width
    70,        // Button height
    hwnd,       // Parent window
    (HMENU)ID_TOGGLE_LOGGING_BTN,
    (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), 
    NULL);      // Pointer not needed.

    EnableWindow(hwndToggleLoggingButton,  false);


    /* Shutter button */
    hwndToggleShutterButton = CreateWindow( 
    "BUTTON",  // Predefined class; Unicode assumed 
    "TOGGLE THE SHUTTER",      // Button text 
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
    350,         // x position 
    320,         // y position 
    200,        // Button width
    70,        // Button height
    hwnd,       // Parent window
    (HMENU)ID_TOGGLE_SHUTTER_BTN,
    (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), 
    NULL);      // Pointer not needed.

    EnableWindow(hwndToggleShutterButton,  false);
    

    /* Rescan COM ports */
    hwndRescanCOMportsButton = CreateWindow( 
    "BUTTON",  // Predefined class; Unicode assumed 
    "RESCAN",      // Button text 
    WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
    220,         // x position 
    20,         // y position 
    70,        // Button width
    30,        // Button height
    hwnd,       // Parent window
    (HMENU)ID_RESCAN_COM_BTN,
    (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), 
    NULL);      // Pointer not needed.

    EnableWindow(hwndRescanCOMportsButton,  true);
    


    /* Fake */
    /*
    hwndFakeLabel = CreateWindow("STATIC",
        "ST_U",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP ,
        10,
        300,
        150,
        18,
        hwnd,
        (HMENU)(ID_POWER_LBL),
        (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);

    SetWindowText(hwndFakeLabel, "");
    */

    /* Laser power */
    hwndPowerLabel = CreateWindow("STATIC",
        "ST_U",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP ,
        10,
        175,
        150,
        18,
        hwnd,
        (HMENU)(ID_POWER_LBL),
        (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);

    SetWindowText(hwndPowerLabel, power_lbl.c_str());

    /* RMS power */
    hwndRMSpowerLabel = CreateWindow("STATIC",
        "ST_U",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP ,
        10,
        195,
        218,
        18,
        hwnd,
        (HMENU)(ID_RMSPOWER_LBL),
        (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);

    SetWindowText(hwndRMSpowerLabel, rmspower_lbl.c_str());


    /* Avg of RMS power */
    hwndRMSpowerAvgLabel = CreateWindow("STATIC",
        "ST_U",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP ,
        230,
        195,
        110,
        18,
        hwnd,
        (HMENU)(ID_RMSPOWERAVG_LBL),
        (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);

    SetWindowText(hwndRMSpowerAvgLabel, "");


    /* Tempereature diodes */
    hwndTempDiodesLabel = CreateWindow("STATIC",
        "ST_U",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP ,
        10,
        215,
        300,
        18,
        hwnd,
        (HMENU)(NULL),
        (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);

    SetWindowText(hwndTempDiodesLabel, temp_diodes_lbl.c_str());
    

    /* Tempereature crystal */
    hwndTempCrystalLabel = CreateWindow("STATIC",
        "ST_U",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP ,
        10,
        235,
        300,
        18,
        hwnd,
        (HMENU)(NULL),
        (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);

    SetWindowText(hwndTempCrystalLabel, temp_crystal_lbl.c_str());
    


    /* Laser diode current */
    hwndDiodeCurrent = CreateWindow("STATIC",
        "ST_U",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP ,
        10,
        255,
        250,
        18,
        hwnd,
        (HMENU)(NULL),
        (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);

    SetWindowText(hwndDiodeCurrent, diode_current_lbl.c_str());
    

    /* Diode work hours */
    hwndDiodeHours = CreateWindow("STATIC",
        "ST_U",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP ,
        10,
        275,
        300,
        18,
        hwnd,
        (HMENU)(NULL),
        (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);

    SetWindowText(hwndDiodeHours, diode_hours_lbl.c_str());


    /* Head work hours */
    hwndHeadHours = CreateWindow("STATIC",
        "ST_U",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP ,
        10,
        295,
        300,
        18,
        hwnd,
        (HMENU)(NULL),
        (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);

    SetWindowText(hwndHeadHours, head_hours_lbl.c_str());
    

    /* Laser work hours */
    hwndLaserHours = CreateWindow("STATIC",
        "ST_U",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP ,
        10,
        315,
        300,
        18,
        hwnd,
        (HMENU)(NULL),
        (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);

    SetWindowText(hwndLaserHours, laser_hours_lbl.c_str());
    


    /* Laser work hours */
    hwndError = CreateWindow("STATIC",
        "ST_U",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP & ~SS_SIMPLE,
        10,
        335,
        330,
        18*3,
        hwnd,
        (HMENU)ID_ERROR_LBL,
        (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);

    SetWindowText(hwndError, "");
    


    /* Serial number */
    hwndLabel = CreateWindow("STATIC",
        "ST_U",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        350,
        60,
        200,
        18,
        hwnd,
        (HMENU)NULL,
        (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);

    SetWindowText(hwndLabel, "Laser's serial number:");

    hWndSerialNumber = CreateWindow("EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_LEFT | WS_EX_CLIENTEDGE | WS_BORDER, 
        350, 80, 200, 25,
        hwnd, (HMENU) ID_TEXT_SERIALNUMBER, (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    SetWindowText(hWndSerialNumber,serialNumber);



    /* Log path */
    hwndLabel = CreateWindow("STATIC",
        "ST_U",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        10,
        60,
        200,
        18,
        hwnd,
        (HMENU)NULL,
        (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);

    SetWindowText(hwndLabel, "Log path:");

    hWndLogDir = CreateWindow("EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_LEFT | WS_EX_CLIENTEDGE | WS_BORDER | ES_AUTOHSCROLL, 
        10, 80, 300, 25,
        hwnd, (HMENU) ID_TEXT_logFilePath, (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    SetWindowText(hWndLogDir, logFilePath);




    /* Laser name */
    hwndLabel = CreateWindow("STATIC",
        "ST_U",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        350,
        5,
        200,
        18,
        hwnd,
        (HMENU)NULL,
        (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);

    SetWindowText(hwndLabel, "Display name:");

    hWndLaserName = CreateWindow("EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_LEFT | WS_EX_CLIENTEDGE | WS_BORDER, 
        350, 25, 200, 25,
        hwnd, (HMENU) ID_TEXT_LASERNAME, (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    SetWindowText(hWndLaserName, laserName);



    /* Logging period */
    hwndLabel = CreateWindow("STATIC",
        "ST_U",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        10,
        115,
        200,
        18,
        hwnd,
        (HMENU)NULL,
        (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);

    SetWindowText(hwndLabel, "Logging period (s):");

    hWndLoggingPeriod = CreateWindow("EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_LEFT | WS_EX_CLIENTEDGE | WS_BORDER, 
        10, 135, 200, 25,
        hwnd, (HMENU) ID_TEXT_LOGPERIOD, (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    SetWindowText(hWndLoggingPeriod,logPeriodStr_last_valid);

    origTextEditWndProc = (WNDPROC)SetWindowLongPtr(hWndLoggingPeriod,GWLP_WNDPROC, (LONG_PTR)TextEditWithOnlyNumbersProc);


    /* Copyright */
    hwndLabel = CreateWindow("STATIC",
        "ST_U",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | SS_RIGHT,
        295,
        470,
        250,
        18,
        hwnd,
        (HMENU)NULL,
        (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);

    SetWindowText(hwndLabel, "Copyright (c) Andrea Alberti, 2019");
   
    HFONT hFont = CreateFont (12, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET, 
        OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
        DEFAULT_PITCH | FF_DONTCARE, TEXT("Tahoma"));

    SendMessage(hwndLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

    /* COM port */
    hwndLabel = CreateWindow("STATIC",
        "ST_U",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        10,
        5,
        200,
        18,
        hwnd,
        (HMENU)NULL,
        (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);

    SetWindowText(hwndLabel, "COM port:");

    hWndComboBoxPorts = CreateWindow("COMBOBOX", TEXT(""), 
        CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_CHILD | WS_OVERLAPPED | WS_VISIBLE,
        10, 25, 200, 350,
        hwnd,
        (HMENU)(ID_PORT_COMBO),
        (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);

    for(int k = 0; k<numPorts; k++)
    {
        SendMessage(hWndComboBoxPorts,(UINT) CB_ADDSTRING,(WPARAM) 0,(LPARAM)namePorts[k]); 
        if(namePorts[k]==comPort)
        {
            SendMessage(hWndComboBoxPorts, CB_SETCURSEL, (WPARAM)k, (LPARAM)0);
        }
    }


    /* Query command line */
    hwndLabel = CreateWindow("STATIC",
        "ST_U",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        10,
        400,
        200,
        18,
        hwnd,
        (HMENU)NULL,
        (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);

    SetWindowText(hwndLabel, "Command line:");

    hWndCMDline = CreateWindow("EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_LEFT | WS_EX_CLIENTEDGE | WS_BORDER, 
        10, 420, 540, 25,
        hwnd, (HMENU) ID_TEXT_CMDLINE, (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

    SetWindowText(hWndCMDline,"");
    EnableWindow(hWndCMDline,false);

    hWndCMDlineRet = CreateWindow("STATIC",
        "ST_U",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
        10,
        450,
        540,
        18,
        hwnd,
        (HMENU)NULL,
        (HINSTANCE) GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);

    SetWindowText(hWndCMDlineRet, "");

    origTextEditWndProc = (WNDPROC)SetWindowLongPtr(hWndCMDline,GWLP_WNDPROC, (LONG_PTR)TextEditWithEnterProc);


    writeConfigFile();


    isCOMbusyMutex = CreateMutex( 
        NULL,              // default security attributes
        FALSE,             // initially not owned
        NULL);             // unnamed mutex


    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    if(start_at_launch)
    {
    	PostMessageA(hwnd,WM_COMMAND,ID_TOGGLE_COMM_BTN,(LPARAM)hwndToggleCommButton);
    	PostMessageA(hwnd,WM_COMMAND,ID_TOGGLE_LOGGING_BTN,(LPARAM)hwndToggleLoggingButton);
    }
                                

    // Message Loop
    while(GetMessage(&Msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }
    return Msg.wParam;
}

void getTimestamp(char* str)
{
    FILETIME ft = {0};
    GetSystemTimeAsFileTime(&ft);

    SYSTEMTIME st;
    char szLocalDate[255], szLocalTime[255];

    FileTimeToLocalFileTime( &ft, &ft );
    FileTimeToSystemTime( &ft, &st );
    GetDateFormat( LOCALE_SYSTEM_DEFAULT, 0, &st, "yyyy-MM-dd", szLocalDate, 255 );
    GetTimeFormat( LOCALE_SYSTEM_DEFAULT, 0, &st, "HH:mm:ss", szLocalTime, 255 );

    strcat(szLocalDate," ");
    strcat(szLocalDate,szLocalTime);
    strcpy(str,szLocalDate);
}


void disconnectFromLaser()
{
    CloseHandle(hSerial);
    hSerial = INVALID_HANDLE_VALUE;

    cout << "Disconnected from laser" << endl;
}


INT queryShutterStatus()
{
    char read_str[10];
    int numBytesRead;
    
    INT ret = executeCmd((unsigned char*)"?sht",NULL,&numBytesRead,(char*)read_str,sizeof(read_str));

    isShutterDown = (read_str[0] == '0');
    
    SetWindowText(hwndToggleShutterButton,  isShutterDown ? "OPEN THE SHUTTER" : "SET THE SHUTTER DOWN");

    return ret;
}

INT queryLaserStatus()
{
    INT ret = 0;

    if((ret |= queryShutterStatus()))
    {
        return ret;
    }

    return ret;
}



INT connectToLaser()
{
    INT ret = 0;

    if(hSerial!=INVALID_HANDLE_VALUE)
        return 0;

    // See also https://123a321.wordpress.com/2010/02/01/serial-port-with-mingw/
    DWORD  accessdirection = GENERIC_READ | GENERIC_WRITE;

    DWORD numBytes;
    unsigned char output[255];

    char lpFileName[255];
    strcpy(lpFileName,"\\\\.\\");
    strncat(lpFileName,comPort,250);

    hSerial = CreateFile(lpFileName,
        accessdirection,
        0,
        0,
        OPEN_EXISTING,
        0,
        0);


    if (hSerial == INVALID_HANDLE_VALUE) {
        // Call GetLastError(); to gain more information#
        cerr << "Error: The port " << comPort << " could not be opened! Check that the port has not already been opened by some other program." << endl;

        MessageBox(hwnd, (empty_str + "The port " + comPort + " could not be opened! Check that the port has not already been opened by some other program.").c_str(), "Error!",
            MB_ICONEXCLAMATION | MB_OK | MB_APPLMODAL);

        return 1;
    }

    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        cerr << "Error: Could not get the state of the port " << comPort << "." << endl;

        MessageBox(hwnd, (empty_str + "Could not get the state of the port " + comPort + ".").c_str(), "Error!",
            MB_ICONEXCLAMATION | MB_OK | MB_APPLMODAL);

        disconnectFromLaser(); 
        return 1;
    }

    dcbSerialParams.BaudRate=9600;
    dcbSerialParams.ByteSize=8;
    dcbSerialParams.StopBits=ONESTOPBIT;
    dcbSerialParams.Parity=NOPARITY;

    if(!SetCommState(hSerial, &dcbSerialParams)){
        cerr << "Error: Could not configure the port " << comPort << "." << endl;

        MessageBox(hwnd, (empty_str + "Could not configure the port " + comPort + ".").c_str(), "Error!",
            MB_ICONEXCLAMATION | MB_OK | MB_APPLMODAL);

        disconnectFromLaser();
        return 1;
    }

    // Finally timeouts need to be set so that the program does not hang up when receiving nothing

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if(!SetCommTimeouts(hSerial, &timeouts)){
        cerr << "Error: Could not set timeout for the port " << comPort << "." << endl;

        MessageBox(hwnd, (empty_str + "Could not set timeout for the port " + comPort + ".").c_str(), "Error!",
            MB_ICONEXCLAMATION | MB_OK | MB_APPLMODAL);

        disconnectFromLaser();
        return 1;
    }

    if(ret = executeCmd((unsigned char*)"*idn?", NULL, NULL, (char*)output, sizeof(output)))
    {
        disconnectFromLaser();
        return ret;
    }
    
    if(!strstr((const char*) output,"Spectra_Physics,Millennia eV"))
    {
        cerr << "Error: No Millennia found connected to " << comPort << "." << endl;

        MessageBox(hwnd, (empty_str + "No Millennia found connected to " + comPort + "." ).c_str(), "Error!",
            MB_ICONEXCLAMATION | MB_OK | MB_APPLMODAL);

        disconnectFromLaser();
        return 1;
    }

    int numCommas = 0;
    int start_SN_pos,end_SN_pos;
    for(int i=0; i<strlen((const char*)output); i++)
    {
        if(output[i]==',')
        {
            numCommas++;
            
            if(numCommas==2)
            {
                start_SN_pos = i+1;
            }
            else if(numCommas==3)
            {
                end_SN_pos = i;
                break;
            }
        }
    }
    
    if(numCommas<2)
    {
        cerr << "Error: Wrong handshaking with Millennia on " << comPort << "." << endl;

        MessageBox(hwnd, (empty_str + "Wrong handshaking with Millennia on " + comPort + "." ).c_str(), "Error!",
            MB_ICONEXCLAMATION | MB_OK | MB_APPLMODAL);

        disconnectFromLaser();
        return 1;
    }

    char serialNumberFromLaser[255];
    output[end_SN_pos] = 0;
    strncpy(serialNumberFromLaser,(const char*)(output+start_SN_pos),sizeof(serialNumberFromLaser));

    if(strncmp(serialNumber,serialNumberFromLaser,sizeof(serialNumber)))
    {
        cerr << "Error: Found serial number \"" << serialNumberFromLaser << "\" of Millennia on " << comPort << ". This does not match the provided serial number \"" << serialNumber << "\". Please check the COM port of your laser." << endl;

        MessageBox(hwnd, (empty_str + "Found serial number \"" + serialNumberFromLaser + "\" of Millennia on " + comPort + ". This does not match the provided serial number \"" + serialNumber + "\". Please check the COM port of your laser.").c_str(), "Error!",
            MB_ICONEXCLAMATION | MB_OK | MB_APPLMODAL);

        disconnectFromLaser();
        return 1;
    }


    if(ret = queryLaserStatus())
    {
        cerr << "Error: Could not query the status of the laser on " << comPort << "." << endl;

        MessageBox(hwnd, (empty_str + "Could not query the status of the laser on " + comPort + ".").c_str(), "Error!",
            MB_ICONEXCLAMATION | MB_OK | MB_APPLMODAL);

        disconnectFromLaser();
        return ret;
    }

    if(ret = executeCmd((unsigned char*)"?FH", NULL, NULL, (char*)errorStr, sizeof(errorStr)))
    {
        cerr << "Error: Could not query the error status on " << comPort << "." << endl;

        MessageBox(hwnd, (empty_str + "Could not query the error status on " + comPort + ".").c_str(), "Error!",
            MB_ICONEXCLAMATION | MB_OK | MB_APPLMODAL);

        disconnectFromLaser();
        return ret;
    }

    cout << "Connected to laser" << endl;

    return 0;
}


INT resumeLogging() 
{	
    INT ret = 0;
    
    logFile.open(logFilePath,ios::out | ios_base::app);
    if(ios_base::iostate() != ios_base::goodbit)
    {
        cerr << "Error: The file \"" << logFilePath << "\" could not be opened." << endl;

        MessageBox(hwnd, (empty_str + "The file \"" + logFilePath + "\" could not be opened.").c_str(), "Error!",
            MB_ICONEXCLAMATION | MB_OK | MB_APPLMODAL);

        return 1;
    }

    char timestamp[255];
    getTimestamp((char*)timestamp);

    logFile << "# " << timestamp << " - Started logging Millenia laser";
    if(strlen(laserName)>0)
    {
        logFile << " (" << laserName << ")";
    }
    logFile << endl;
    logFile << "# Timestamp\t\tPower\tRMS\tAvg.RMS\tT-dio.\tT.cry.\tDio.c.\tDio.h.\tHead-h.\tLas.-h.\tError" << endl;

    
    // Start the logger thread
    isLogging = true;

    hThread = CreateThread(NULL, // security attributes ( default if NULL )
        0, // stack SIZE default if 0
        ThreadLoggingFunc, // Start Address
        NULL, // input data
        0, // creational flag ( start if  0 )
        &threadID); // thread ID


    // Configure windows' controls
    SetWindowText(hwndToggleLoggingButton,  "STOP LOGGING");
    EnableWindow(hWndLoggingPeriod,  false);
    EnableWindow(hWndLogDir,         false);
    
    return 0;
}
