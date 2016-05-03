package com.avatar.fota.service;

import android.app.DownloadManager;
import android.content.Context;
import android.content.SharedPreferences;
import android.database.ContentObserver;
import android.database.Cursor;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.NetworkInfo.State;
import android.net.Uri;
import android.os.Environment;
import android.os.Handler;
import android.os.Message;

import java.io.File;

import com.avatar.fota.utils.ConstValue;
import com.avatar.fota.utils.Util;
import com.avatar.fota.utils.UpdateStatus;

import com.avatar.fota.R;


public class OTADownloadMgr {
    private static final String TAG = "OTADownloadMgr";
    private Context mContext;
    private DownloadManager mDlService;
    private DownloadChangeObserver mDlObserver;
    private SharedPreferences mSharePrefs;
    private long mDwloadId;
    private int mProgress = 0; // 0 - 100

    private Handler mDwloadHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            super.handleMessage(msg);

            int status = (Integer) msg.obj;
            if (status == DownloadManager.STATUS_SUCCESSFUL) {
                Util.Logd(TAG, "DownloadManager.STATUS_SUCCESSFUL");
                // 下载成功的处理
                downloadSuccess();
                // 设置状态
                UpdateStatus.setStatus(UpdateStatus.DOWNLOAD_OK);

                // 获取下载文件的md5
                ((OTAUpdateService)mContext).getUpdatePackageMD5();

                // 下载成功通知用户
                ((OTAUpdateService)mContext).notifyClientStatus();
            } else if (status == DownloadManager.STATUS_FAILED) {
                Util.Logd(TAG, "DownloadManager.STATUS_FAILED");
                // 下载失败的处理
                downloadFailed();
                // 设置状态
                UpdateStatus.setStatus(UpdateStatus.DOWNLOAD_FAILED);
                // 下载失败通知用户
                ((OTAUpdateService)mContext).notifyClientStatus();
            } else if (status == DownloadManager.STATUS_PAUSED) {
                Util.Logd(TAG, "DownloadManager.STATUS_PAUSED");

            } else if (status == DownloadManager.STATUS_PENDING) {
                Util.Logd(TAG, "DownloadManager.STATUS_PENDING");

            } else if (status == DownloadManager.STATUS_RUNNING) {
                Util.Logd(TAG, "DownloadManager.STATUS_RUNNING");

                // 显示下载进度
                long cur = msg.arg1;
                long tol = msg.arg2;

                double prog = cur / (tol * 0.01f);
                mProgress = (int)prog;
                Util.Logd(TAG, "Cur/Max: " + Util.getFileSize(cur) + "/ "
                        + Util.getFileSize(tol) + " Prog:" + mProgress);
            } else {
                Util.Logd(TAG, "DownloadManager unknow status(" + status + ")");
            }
        }
    };

    public OTADownloadMgr(Context context) {
        mContext = context;
        mSharePrefs = mContext.getSharedPreferences(
            ConstValue.OTA_DATA, Context.MODE_PRIVATE);

        mDlService = (DownloadManager)
            mContext.getSystemService(Context.DOWNLOAD_SERVICE);
        mDlObserver = new DownloadChangeObserver(mDwloadHandler);

        mDwloadId = mSharePrefs.getLong(ConstValue.DOWNLOAD_ID, 0);
        if (mDwloadId != 0) {
            // 注册观察下载进度变化
            mContext.getContentResolver().registerContentObserver(
                ConstValue.CONTENT_URI, true, mDlObserver);
        }

        // 清楚历史记录
        clearDownloadPackage();
    }

    public boolean isNetWorkConnect() {
        ConnectivityManager conn = (ConnectivityManager)
            mContext.getSystemService(Context.CONNECTIVITY_SERVICE);
        if (conn != null) {
            NetworkInfo netInfo = conn.getActiveNetworkInfo();
            if (netInfo != null && netInfo.isConnected()) {
                if (netInfo.getState() == NetworkInfo.State.CONNECTED) {
                    return true;
                }
            }
        }
        return false;
    }

    public boolean isWifiConnected() {
        ConnectivityManager conn = (ConnectivityManager)
                mContext.getSystemService(Context.CONNECTIVITY_SERVICE);

        State st = conn.getNetworkInfo(ConnectivityManager.TYPE_WIFI).getState();
        if (st == State.CONNECTING || st == State.CONNECTED)
            return true;
        else
            return false;
    }

    public int getProgress() {
        return mProgress;
    }

    public void queryDownload() {
        // 判断是否存在下载任务
        if (mDwloadId == 0) {
            // 检测下载目录下是否有安装包
            File file = new File(ConstValue.DOWNLOAD_PATH, ConstValue.INSTALL_PACKAGE);
            if (file.exists()) {
                Util.Logd(TAG, "Install package file(" + file.getPath() + ")exists.");
                UpdateStatus.setStatus(UpdateStatus.DOWNLOAD_OK);
            } else {
                Util.Logd(TAG, "Not find install package in (" + file.getPath() + ")path.");
            }
            return;
        }

        // 查询下载任务管理
        DownloadManager.Query query = new DownloadManager.Query();
        query.setFilterById(mDwloadId);
        Cursor cs = null;
        cs = mDlService.query(query);

        if (cs != null && cs.moveToFirst()) {
            int status = cs.getInt(cs.getColumnIndex(DownloadManager.COLUMN_STATUS));
            Util.Logd(TAG, "Download id " + mDwloadId + " status: " + status);

            switch (status) {
                case DownloadManager.STATUS_PENDING:
                case DownloadManager.STATUS_PAUSED:
                case DownloadManager.STATUS_RUNNING:
                    UpdateStatus.setStatus(UpdateStatus.DOWNLOADING);
                    break;

                case DownloadManager.STATUS_SUCCESSFUL:
                    // 下载成功处理
                    downloadSuccess();
                    // 设置状态
                    UpdateStatus.setStatus(UpdateStatus.DOWNLOAD_OK);
                    break;

                case DownloadManager.STATUS_FAILED:
                    // 下载失败处理
                    downloadFailed();
                    // 设置状态
                    UpdateStatus.setStatus(UpdateStatus.DOWNLOAD_FAILED);
                    break;
            }
        } else {
            // 在DownloadManager中找不到之前的下载任务ID
            Util.Loge(TAG, "DownloadManager can't find id:" + mDwloadId);
            mDwloadId = 0;
            mSharePrefs.edit().remove(ConstValue.DOWNLOAD_ID).commit();
        }
    }

    public void clearDownloadPackage() {
        File path = new File(ConstValue.DOWNLOAD_PATH);
        String tmpFiles = mSharePrefs.getString(ConstValue.UNZIP_TMP_FILES, null);
        if (tmpFiles != null) {
            // clear md5
            mSharePrefs.edit().remove(ConstValue.PACKAGE_MD5).commit();

            Util.Logd(TAG, "Tmp files is not null:");
            String[] file = tmpFiles.split(";");
            for (int i = 0; i < file.length; i++) {
                File delFile = new File(path, file[i]);
                boolean ret = delFile.delete();

                Util.Logd(TAG, "Delete:" + delFile + " ret=" + ret);
            }
            // clear unzip files
            mSharePrefs.edit().remove(ConstValue.UNZIP_TMP_FILES).commit();
        }
    }

    public void download(String ver, String url) {
        // 判断是否已经下载
        if (mDwloadId != 0) {
            Util.Logd(TAG, "Download task " + mDwloadId + " is running...");
            return;
        } else {
            if (url == null)
                return;

            // 删除旧文件
            File updatePackage = new File(ConstValue.DOWNLOAD_PATH, ConstValue.INSTALL_PACKAGE);
            if (updatePackage.exists()) {
                updatePackage.delete();
            }

            Uri resource = Uri.parse(url);
            DownloadManager.Request dwloadRequest =
                new DownloadManager.Request(resource);

            // 设置下载请求属性
            dwloadRequest.setAllowedNetworkTypes(DownloadManager.Request.NETWORK_WIFI);
            dwloadRequest.setAllowedOverRoaming(false);
            dwloadRequest.setNotificationVisibility(DownloadManager.Request.VISIBILITY_HIDDEN);
            dwloadRequest.setDestinationInExternalPublicDir(Environment.DIRECTORY_DOWNLOADS,
                ConstValue.DOWNLOAD_TMP_NAME);

            // 执行下载
            mDwloadId = mDlService.enqueue(dwloadRequest);
            // 存储下载任务ID
            mSharePrefs.edit().putLong(ConstValue.DOWNLOAD_ID, mDwloadId).commit();

            // 注册观察下载进度变化
            mContext.getContentResolver().registerContentObserver(
                ConstValue.CONTENT_URI, true, mDlObserver);
        }
    }

    private class DownloadChangeObserver extends ContentObserver {

        public DownloadChangeObserver(Handler handler) {
            super(handler);
        }

        @Override
        public void onChange(boolean selfChange) {
            // 查询下载信息
            int dwBytes[] = getDownloadStatus(mDwloadId);

            Message msg = mDwloadHandler.obtainMessage(
                0, dwBytes[1], dwBytes[2], dwBytes[0]);
            msg.sendToTarget();
        }
    }

    private int[] getDownloadStatus(long id) {
        int[] ret = new int[]{0, 0, 0};
        DownloadManager.Query query = new DownloadManager.Query();
        query.setFilterById(id);

        Cursor dw = null;
        try {
            dw = mDlService.query(query);
            if (dw != null && dw.moveToFirst()) {
                // get status
                ret[0] = dw.getInt(dw.getColumnIndex(DownloadManager.COLUMN_STATUS));
                // get bytes so far
                ret[1] = dw.getInt(dw.getColumnIndex(DownloadManager.COLUMN_BYTES_DOWNLOADED_SO_FAR));
                // get bytes total
                ret[2] = dw.getInt(dw.getColumnIndex(DownloadManager.COLUMN_TOTAL_SIZE_BYTES));
            }
        } finally {
            if (dw != null) {
                //dw.close();
            }
        }

        return ret;
    }

    private void downloadSuccess() {
        if (mDwloadId != 0) {
            // 注销观察
            mContext.getContentResolver().unregisterContentObserver(mDlObserver);

            // 更改文件名，防止删除下载任务时删除原文件
            File srcFile = new File(ConstValue.DOWNLOAD_PATH, ConstValue.DOWNLOAD_TMP_NAME);
            File dstFile = new File(ConstValue.DOWNLOAD_PATH, ConstValue.INSTALL_PACKAGE);
            File path = new File(ConstValue.DOWNLOAD_PATH);

            boolean pathExist = true;
            if (!path.exists()) {
                pathExist = path.mkdirs();
                Util.Loge(TAG, "Path " + path.getPath() + " is not exist and mkdirs " + pathExist);
            }
            boolean cp = srcFile.renameTo(dstFile);
            if (!cp) {
                Util.Loge(TAG, "Copy download file to path " + path.getPath() + " failed.");
            }

            // 删除下载任务，会删除原文件
            mDlService.remove(mDwloadId);
            mSharePrefs.edit().remove(ConstValue.DOWNLOAD_ID).commit();
            mDwloadId = 0;
        }
    }

    private void downloadFailed() {
        if (mDwloadId != 0) {
            // 删除下载进程ID
            mDlService.remove(mDwloadId);
            mSharePrefs.edit().remove(ConstValue.DOWNLOAD_ID).commit();
            mDwloadId = 0;
            mProgress = 0;
        }
    }

}
