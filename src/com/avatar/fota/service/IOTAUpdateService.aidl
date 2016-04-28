package com.avatar.fota.service;

import com.avatar.fota.service.IOTAUpdateCallback;

interface IOTAUpdateService {
    void queryVersion(IOTAUpdateCallback cb);
    String getCurrentVersion();
    String getNewVersion();
    int progress();
    boolean download();
    boolean install();
}