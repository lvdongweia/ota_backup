package com.avatar.fota.utils;


public enum UpdateStatus {
    IDLE,             // idle
    REFRESH,          // updating
    CATCH_NEW,        // catch new version?
    CATCH_NONE,       // already new
    SERVER_ERROR,     // server error
    DOWNLOADING,      // downloading
    DOWNLOAD_FAILED,  // download failed
    DOWNLOAD_OK,      // download finish
    CHECKING,         // check after download
    CHECK_FAILED,     // check failed
    CHECK_OK,         // check finish ready to install
    INSTALLING,       // already install--latest

    STORAGE_INEQUACY; // storage inequacy


    private static UpdateStatus mStatus = IDLE;

    public static UpdateStatus getStatus() {
        return mStatus;
    }

    public static void setStatus(UpdateStatus status) {
        mStatus = status;
    }
}
