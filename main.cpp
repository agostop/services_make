#include <Windows.h>
#include <stdio.h>
#include <strsafe.h>
#include "getopt.h"
#include "go_curl.h"

#define APPLICATION_NAME    "FindMP"
#define SERVICE_NAME_LEN    64
#define MAX_CMD_LEN    (MAX_PATH * 2)
#define CONFIG_FILE NULL
#define EVENTLOG_REG_PATH TEXT("SYSTEM\\CurrentControlSet\\Services\\EventLog\\")
#define MAX(a, b)       (a) > (b) ? (a) : (b)
#define	SUCCEED		0
#define	FAIL		-1
#define _strlen wcslen
#define _free(ptr)		\
                \
do				\
{				\
    if (ptr)		\
    {			\
        free(ptr);	\
        ptr = NULL;	\
    }			\
}				\
while (0)


static SERVICE_STATUS          serviceStatus;
static SERVICE_STATUS_HANDLE   serviceHandle;
static int appstat=1;

char SERVICE_NAME[SERVICE_NAME_LEN] = APPLICATION_NAME;
char EVENT_SOURCE[SERVICE_NAME_LEN];


static int	svc_OpenSCManager(SC_HANDLE *mgr);
void  *_malloc(void *old, size_t size);
static LPTSTR utf8_to_unicode(unsigned int codepage, LPCSTR cp_string);
static int	svc_OpenService(SC_HANDLE mgr, SC_HANDLE *service, DWORD desired_access);
static int	svc_install_event_source(const char *path);
static void	get_fullpath(const char *path, LPTSTR fullpath, size_t max_fullpath);
int _CreateService(const char *path);
void service_start();
static VOID WINAPI	ServiceEntry(DWORD argc, LPTSTR *argv);
static VOID WINAPI	ServiceCtrlHandler(DWORD ctrlCode);
int MAIN_ENTRY();
void free_service_resources(void);

static int	svc_OpenSCManager(SC_HANDLE *mgr)
{
    if (NULL != (*mgr = OpenSCManager(NULL, NULL, GENERIC_WRITE)))
        return SUCCEED;

    printf("ERROR: cannot connect to Service Manager");

    return FAIL;
}


void  *_malloc(void *old, size_t size)
{
    int	max_attempts;
    void	*ptr = NULL;

    /* old pointer must be NULL */
    if (NULL != old)
    {
        printf("malloc: allocating already allocated memory. ");
    }

    for (
        max_attempts = 10, size = MAX(size, 1);
        0 < max_attempts && NULL == ptr;
        ptr = malloc(size), max_attempts--
    );

    if (NULL != ptr)
        return ptr;

    printf("out of memory. Requested " );

    exit(FAIL);
}

static LPTSTR utf8_to_unicode(unsigned int codepage, LPCSTR cp_string)
{
    LPTSTR	wide_string = NULL;
    int	wide_size;

    wide_size = MultiByteToWideChar(codepage, 0, cp_string, -1, NULL, 0);
    wide_string = (LPTSTR)_malloc(wide_string, (size_t)wide_size * sizeof(TCHAR));

    /* convert from cp_string to wide_string */
    MultiByteToWideChar(codepage, 0, cp_string, -1, wide_string, wide_size);

    return wide_string;
}

static int	svc_OpenService(SC_HANDLE mgr, SC_HANDLE *service, DWORD desired_access)
{
    LPTSTR	wservice_name;
    int	ret = SUCCEED;

    wservice_name = utf8_to_unicode(CP_UTF8,SERVICE_NAME);

    if (NULL == (*service = OpenService(mgr, wservice_name, desired_access)))
    {
        printf("ERROR: cannot open service ");
        ret = FAIL;
    }

    _free(wservice_name);

    return ret;
}

static int	svc_install_event_source(const char *path)
{
    HKEY	hKey;
    DWORD	dwTypes = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
    TCHAR	execName[MAX_PATH];
    TCHAR	regkey[256], *wevent_source;

    get_fullpath(path, execName, MAX_PATH);

    wevent_source = utf8_to_unicode(CP_UTF8,EVENT_SOURCE);
   // printf("System\\%s", wevent_source);
    StringCchPrintf(regkey, sizeof(regkey)/sizeof(TCHAR), EVENTLOG_REG_PATH TEXT("System\\%s"), wevent_source);
    _free(wevent_source);

    if (ERROR_SUCCESS != RegCreateKeyEx(HKEY_LOCAL_MACHINE, regkey, 0, NULL, REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE, NULL, &hKey, NULL))
    {
        printf("unable to create registry key " );
        return FAIL;
    }

    RegSetValueEx(hKey, TEXT("TypesSupported"), 0, REG_DWORD, (BYTE *)&dwTypes, sizeof(DWORD));
    RegSetValueEx(hKey, TEXT("EventMessageFile"), 0, REG_EXPAND_SZ, (BYTE *)execName,
            (DWORD)(_strlen(execName) + 1) * sizeof(TCHAR));
    RegCloseKey(hKey);

    printf("event source installed successfully");

    return SUCCEED;
}

static void	get_fullpath(const char *path, LPTSTR fullpath, size_t max_fullpath)
{
    LPTSTR	wpath;

    wpath = utf8_to_unicode(CP_ACP,path);
    _wfullpath(fullpath, wpath, max_fullpath);
    _free(wpath);
}

static void	svc_get_command_line(const char *path, int multiple_agents, LPTSTR cmdLine, size_t max_cmdLine)
{
    TCHAR	path1[MAX_PATH], path2[MAX_PATH];

    get_fullpath(path, path2, MAX_PATH);

    if (NULL == wcsstr(path2, TEXT(".exe")))
        StringCchPrintf(path1, MAX_PATH, TEXT("%s.exe"), path2);
    else
        StringCchPrintf(path1, MAX_PATH, path2);

    if (NULL != CONFIG_FILE)
    {
        get_fullpath(CONFIG_FILE, path2, MAX_PATH);
        StringCchPrintf(cmdLine, max_cmdLine, TEXT("\"%s\" %s--config \"%s\""),
                path1,
                (0 == multiple_agents) ? TEXT("") : TEXT("--multiple-agents "),
                path2);
    }
    else
        StringCchPrintf(cmdLine, max_cmdLine, TEXT("\"%s\""), path1);
}

int	_CreateService(const char *path)
{
//#define MAX_CMD_LEN	(MAX_PATH * 2)

    int multiple_agents = 0;
    SC_HANDLE		mgr, service;
    SERVICE_DESCRIPTION	sd;
    TCHAR			cmdLine[MAX_CMD_LEN];
    LPTSTR			wservice_name;
    DWORD			code;
    int			ret = FAIL;

    if (FAIL == svc_OpenSCManager(&mgr))
        return ret;

    svc_get_command_line(path, multiple_agents, cmdLine, MAX_CMD_LEN);

    wservice_name = utf8_to_unicode(CP_UTF8, SERVICE_NAME);

    if (NULL == (service = CreateService(mgr, wservice_name, wservice_name, GENERIC_READ, SERVICE_WIN32_OWN_PROCESS,
            SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, cmdLine, NULL, NULL, NULL, NULL, NULL)))
    {
        if (ERROR_SERVICE_EXISTS == (code = GetLastError()))
            printf("ERROR: service already exists");
        else
            printf("ERROR: cannot create service ");
    }
    else
    {
        printf("service [%s] installed successfully", SERVICE_NAME);
        CloseServiceHandle(service);
        ret = SUCCEED;

        /* update the service description */
        if (SUCCEED == svc_OpenService(mgr, &service, SERVICE_CHANGE_CONFIG))
        {
            //sd.lpDescription = TEXT("Provides system monitoring");
            sd.lpDescription = TEXT("find the liwu mp");
            if (0 == ChangeServiceConfig2(service, SERVICE_CONFIG_DESCRIPTION, &sd))
                printf("service description update failed");
            CloseServiceHandle(service);
        }
    }

    _free(wservice_name);

    CloseServiceHandle(mgr);

    if (SUCCEED == ret)
        ret = svc_install_event_source(path);

    return ret;
}

int	_StartService()
{
    SC_HANDLE	mgr, service;
    int		ret = FAIL;

    if (FAIL == svc_OpenSCManager(&mgr))
        return ret;

    if (SUCCEED == svc_OpenService(mgr, &service, SERVICE_START))
    {
        if (0 != StartService(service, 0, NULL))
        {
            printf("service started successfully");
            ret = SUCCEED;
        }
        else
        {
            printf("ERROR: cannot start service ");
        }

        CloseServiceHandle(service);
    }

    CloseServiceHandle(mgr);

    return ret;
}

void service_start()
{
    int				ret;
    static SERVICE_TABLE_ENTRY	serviceTable[2];

    serviceTable[0].lpServiceName = utf8_to_unicode(CP_UTF8,SERVICE_NAME);
    serviceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceEntry;
    serviceTable[1].lpServiceName = NULL;
    serviceTable[1].lpServiceProc = NULL;

    ret = StartServiceCtrlDispatcher(serviceTable);
    _free(serviceTable[0].lpServiceName);

    if (0 == ret)
    {
        if (ERROR_FAILED_SERVICE_CONTROLLER_CONNECT == GetLastError())
        {
            printf("\n\n\t!!!ATTENTION!!! Agent started as a console application. !!!ATTENTION!!!\n");
            MAIN_ENTRY();
        }
        else
            printf("StartServiceCtrlDispatcher() failed");
    }
}

static VOID WINAPI	ServiceEntry(DWORD argc, LPTSTR *argv)
{
    LPTSTR	wservice_name;

    wservice_name = utf8_to_unicode(CP_UTF8,SERVICE_NAME);
    serviceHandle = RegisterServiceCtrlHandler(wservice_name, ServiceCtrlHandler);
    _free(wservice_name);

    /* start service initialization */
    serviceStatus.dwServiceType		= SERVICE_WIN32_OWN_PROCESS;
    serviceStatus.dwCurrentState		= SERVICE_START_PENDING;
    serviceStatus.dwControlsAccepted	= SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    serviceStatus.dwWin32ExitCode		= 0;
    serviceStatus.dwServiceSpecificExitCode	= 0;
    serviceStatus.dwCheckPoint		= 0;
    serviceStatus.dwWaitHint		= 2000;

    SetServiceStatus(serviceHandle, &serviceStatus);

    /* service is running */
    serviceStatus.dwCurrentState	= SERVICE_RUNNING;
    serviceStatus.dwWaitHint	= 0;
    SetServiceStatus(serviceHandle, &serviceStatus);

    MAIN_ENTRY();
}


static VOID WINAPI	ServiceCtrlHandler(DWORD ctrlCode)
{
    serviceStatus.dwServiceType		= SERVICE_WIN32_OWN_PROCESS;
    serviceStatus.dwCurrentState		= SERVICE_RUNNING;
    serviceStatus.dwControlsAccepted	= SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    serviceStatus.dwWin32ExitCode		= 0;
    serviceStatus.dwServiceSpecificExitCode	= 0;
    serviceStatus.dwCheckPoint		= 0;
    serviceStatus.dwWaitHint		= 0;

    switch (ctrlCode)
    {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            //zabbix_log(LOG_LEVEL_INFORMATION, "Zabbix Agent shutdown requested");

            serviceStatus.dwCurrentState	= SERVICE_STOP_PENDING;
            serviceStatus.dwWaitHint	= 4000;
            SetServiceStatus(serviceHandle, &serviceStatus);

            /* notify other threads and allow them to terminate */
            //ZBX_DO_EXIT();
            free_service_resources();

            serviceStatus.dwCurrentState	= SERVICE_STOPPED;
            serviceStatus.dwWaitHint	= 0;
            serviceStatus.dwCheckPoint	= 0;
            serviceStatus.dwWin32ExitCode	= 0;

            break;
        default:
            break;
    }

    SetServiceStatus(serviceHandle, &serviceStatus);
}

int MAIN_ENTRY()
{

    go_curl();

    return 0;
    /*
    FILE *file=fopen("d:/test.log","w+");

    if (file ==NULL)
    {
        printf("error ");
       exit(1);
    }

    while (1)
    {
       if (appstat==0)
           break;
       fwrite("mytestlog\n",9,1,file);
       Sleep(1000);
    }

    fclose(file);
    return 0;
    */
}

void	free_service_resources(void)
{
    appstat=0;
}

void usage(char *appname)
{
    printf("Usage: %s [-i]|[-s]|[-d]\n",appname);
    printf("  -i\tinstall the services\n");
    printf("  -s\tstart the services\n");
    printf("  -d\tdebug mode\n");
}

int main(int argc,char *argv[])
{
    char ch;
    if (argc<=1)
    {
        usage(argv[0]);
       // exit(1);
    }

    while((ch=getopt(argc,argv,"isd"))!=-1)
    {
        switch(ch)
        {
            case 'i':
                    _CreateService(argv[0]);
                    exit(0);

            case 's':
                    _StartService();
                    exit(0);

            case 'd':
                    MAIN_ENTRY();
                    exit(0);

            default:
                    usage(argv[0]);
                    break;
        }
    }
    service_start();
}

