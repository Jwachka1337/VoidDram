#pragma once

#include <windows.h>
#include <taskschd.h>
#include <comdef.h>
#include <string>
#include "raii.hpp"

#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

class TaskSchedulerManager {
public:
    static bool IsTaskRegistered(const std::wstring& taskName) {
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        bool uninit = (hr == S_OK || hr == S_FALSE);

        bool registered = false;
        {
            MyComPtr<ITaskService> pService;
            hr = CoCreateInstance(
                CLSID_TaskScheduler,
                NULL,
                CLSCTX_INPROC_SERVER,
                IID_ITaskService,
                pService.VoidPut()
            );

            if (SUCCEEDED(hr)) {
                _variant_t vEmpty;
                if (SUCCEEDED(pService->Connect(vEmpty, vEmpty, vEmpty, vEmpty))) {
                    MyComPtr<ITaskFolder> pRootFolder;
                    _bstr_t bstrRoot(L"\\");
                    if (SUCCEEDED(pService->GetFolder(bstrRoot, pRootFolder.Put()))) {
                        MyComPtr<IRegisteredTask> pTask;
                        _bstr_t bstrTask(taskName.c_str());
                        if (SUCCEEDED(pRootFolder->GetTask(bstrTask, pTask.Put()))) {
                            registered = true;
                        }
                    }
                }
            }
        }

        if (uninit) {
            CoUninitialize();
        }
        return registered;
    }

    static bool RegisterElevatedLogonTask(const std::wstring& taskName, const std::wstring& exePath, const std::wstring& args = L"--minimized") {
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        bool uninit = (hr == S_OK || hr == S_FALSE);

        bool success = false;
        {
            MyComPtr<ITaskService> pService;
            hr = CoCreateInstance(
                CLSID_TaskScheduler,
                NULL,
                CLSCTX_INPROC_SERVER,
                IID_ITaskService,
                pService.VoidPut()
            );

            if (SUCCEEDED(hr)) {
                _variant_t vEmpty;
                if (SUCCEEDED(pService->Connect(vEmpty, vEmpty, vEmpty, vEmpty))) {
                    MyComPtr<ITaskFolder> pRootFolder;
                    _bstr_t bstrRoot(L"\\");
                    if (SUCCEEDED(pService->GetFolder(bstrRoot, pRootFolder.Put()))) {
                        MyComPtr<ITaskDefinition> pTask;
                        if (SUCCEEDED(pService->NewTask(0, pTask.Put()))) {
                            MyComPtr<IRegistrationInfo> pRegInfo;
                            if (SUCCEEDED(pTask->get_RegistrationInfo(pRegInfo.Put()))) {
                                pRegInfo->put_Author(_bstr_t(L"VoidDRAM"));
                                pRegInfo->put_Description(_bstr_t(L"VoidDRAM AutoStart"));
                            }

                            MyComPtr<IPrincipal> pPrincipal;
                            if (SUCCEEDED(pTask->get_Principal(pPrincipal.Put()))) {
                                pPrincipal->put_Id(_bstr_t(L"Author"));
                                pPrincipal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
                                pPrincipal->put_RunLevel(TASK_RUNLEVEL_HIGHEST);
                            }

                            MyComPtr<ITaskSettings> pSettings;
                            if (SUCCEEDED(pTask->get_Settings(pSettings.Put()))) {
                                pSettings->put_StartWhenAvailable(VARIANT_TRUE);
                                pSettings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
                                pSettings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
                                pSettings->put_ExecutionTimeLimit(_bstr_t(L"PT0S"));
                                pSettings->put_Priority(4);
                            }

                            MyComPtr<ITriggerCollection> pTriggerCollection;
                            if (SUCCEEDED(pTask->get_Triggers(pTriggerCollection.Put()))) {
                                MyComPtr<ITrigger> pTrigger;
                                if (SUCCEEDED(pTriggerCollection->Create(TASK_TRIGGER_LOGON, pTrigger.Put()))) {
                                    pTrigger->put_Id(_bstr_t(L"LogonTrigger"));
                                }
                            }

                            MyComPtr<IActionCollection> pActionCollection;
                            if (SUCCEEDED(pTask->get_Actions(pActionCollection.Put()))) {
                                MyComPtr<IAction> pAction;
                                if (SUCCEEDED(pActionCollection->Create(TASK_ACTION_EXEC, pAction.Put()))) {
                                    MyComPtr<IExecAction> pExecAction;
                                    if (SUCCEEDED(pAction->QueryInterface(IID_IExecAction, pExecAction.VoidPut()))) {
                                        pExecAction->put_Path(_bstr_t(exePath.c_str()));
                                        if (!args.empty()) {
                                            pExecAction->put_Arguments(_bstr_t(args.c_str()));
                                        }
                                    }
                                }
                            }

                            MyComPtr<IRegisteredTask> pRegisteredTask;
                            _variant_t vEmptyReg;
                            hr = pRootFolder->RegisterTaskDefinition(
                                _bstr_t(taskName.c_str()),
                                pTask.Get(),
                                TASK_CREATE_OR_UPDATE,
                                vEmptyReg,
                                vEmptyReg,
                                TASK_LOGON_INTERACTIVE_TOKEN,
                                _variant_t(L""),
                                pRegisteredTask.Put()
                            );

                            if (SUCCEEDED(hr)) {
                                success = true;
                            }
                        }
                    }
                }
            }
        }

        if (uninit) {
            CoUninitialize();
        }
        return success;
    }

    static bool DeleteTask(const std::wstring& taskName) {
        HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        bool uninit = (hr == S_OK || hr == S_FALSE);

        bool success = false;
        {
            MyComPtr<ITaskService> pService;
            hr = CoCreateInstance(
                CLSID_TaskScheduler,
                NULL,
                CLSCTX_INPROC_SERVER,
                IID_ITaskService,
                pService.VoidPut()
            );

            if (SUCCEEDED(hr)) {
                _variant_t vEmpty;
                if (SUCCEEDED(pService->Connect(vEmpty, vEmpty, vEmpty, vEmpty))) {
                    MyComPtr<ITaskFolder> pRootFolder;
                    _bstr_t bstrRoot(L"\\");
                    if (SUCCEEDED(pService->GetFolder(bstrRoot, pRootFolder.Put()))) {
                        _bstr_t bstrTask(taskName.c_str());
                        hr = pRootFolder->DeleteTask(bstrTask, 0);
                        success = SUCCEEDED(hr);
                    }
                }
            }
        }

        if (uninit) {
            CoUninitialize();
        }
        return success;
    }
};
