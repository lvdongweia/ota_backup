package com.avatar.fota.service;

import com.avatar.fota.service.IOTAUpdateCallback;

interface IOTAUpdateService {
    void queryVersion(IOTAUpdateCallback cb);
    String getCurrentVersion();
    String getNewVersion();
    int progress();
    int download();
    int install();
    int getError();
}