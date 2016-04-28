package com.avatar.fota.utils;


import android.net.Uri;
import android.os.Environment;

public class ConstValue {
    public static final String VERSION_URL = "http://10.50.10.78:8182/FotaServer/queryVersion";
    public static final String DEFAULT_TIME = "2000-01-01 00:00:00";
    public static final long SP_QUERY_TIME  = 4 * 60 * 60 * 1000;   // 4小时查询一次

    public static final String OTA_DATA = "ota_data";
    public static final String LAST_QUERY = "last_query";
    public static final String LAST_STATUS = "last_status";

    public static final Uri CONTENT_URI = Uri.parse("content://downloads/my_downloads");

    /**
     * OTADownload
     */
    public static final int NET_TIMEOUT = 5000;

    public static final String DOWNLOAD_PATH =
        Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS).getPath();
    public static final String DOWNLOAD_TMP_NAME = "update.zip.tmp";
    public static final String INSTALL_PACKAGE = "robot_update.zip";
    public static final String DOWNLOAD_ID  = "download_id";
    public static final String UNZIP_TMP_FILES = "unzip_tmp_files";

    public static final String JSON_ROBOTCODE = "robotCode";
    public static final String JSON_ROBOTMODEL = "type";
    public static final String JSON_RMVER  = "rmVer";
    public static final String JSON_RCVER  = "rcVer";
    public static final String JSON_RPVER  = "rpVer";
    public static final String JSON_RFVER  = "rfVer";
    public static final String JSON_RBVER  = "rbVer";
    public static final String JSON_RETCODE = "returnCode";
    public static final String JSON_RETMSG  = "returnMsg";
    public static final String JSON_BODY    = "body";

    public static final String JSON_DLURL   = "downloadPath";

    /**
     * Update file name
     */
    public static final String UPDATE_RM = "rm";
    public static final String UPDATE_RC = "rc";
    public static final String UPDATE_RP = "rp";
    public static final String UPDATE_RF = "rf";
    public static final String UPDATE_RB = "rb";

    public static final long INSTALL_MEMORY_NEED = 1050 * 1024 * 1024;  // byte
}
