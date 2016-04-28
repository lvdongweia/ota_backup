package com.avatar.fota.service;

import android.app.ProgressDialog;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.net.ConnectivityManager;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.RecoverySystem;
import android.os.RemoteException;
import android.robot.scheduler.SchedulerManager;
import android.view.WindowManager;

import org.apache.http.HttpEntity;
import org.apache.http.HttpResponse;
import org.apache.http.HttpStatus;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.entity.StringEntity;
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.params.CoreConnectionPNames;
import org.apache.http.protocol.HTTP;
import org.apache.http.util.EntityUtils;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Timer;
import java.util.TimerTask;
import java.util.UUID;

import com.avatar.fota.R;

import com.avatar.fota.utils.ConstValue;
import com.avatar.fota.utils.Util;
import com.avatar.fota.utils.UpdateStatus;


public class OTAUpdateService extends Service {
    private static final String TAG = "OTAService";
    private Context mContext;
    private OTAUpdateStub mBinder;
    private OTADownloadMgr mDownload;
    private IOTAUpdateCallback mICallback;

    private String mLocalVersion = "";
    private String mNewVersion = "";
    private String mNewVerUrl;
    private SharedPreferences mSharedPrefrns;

    private Thread mQueryThrad;

    private static final int POP_ALERT_DIALOG = 0;
    private static final int DIMISS_DIALOG = 1;

    private Handler mOTAHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case POP_ALERT_DIALOG:
                    popAlertDialog();
                    break;

                case DIMISS_DIALOG:
                    break;

                default:
                    break;
            }
        }
    };

    private class OTAUpdateStub extends IOTAUpdateService.Stub {
        @Override
        public void queryVersion(IOTAUpdateCallback cb) {
            manualQueryNewVersion(cb);
        }

        @Override
        public String getCurrentVersion() {
            return mLocalVersion;
        }

        @Override
        public String getNewVersion() {
            return mNewVersion;
        }

        @Override
        public int progress() {
            return getDownloadProgress();
        }

        @Override
        public boolean download() {
            return downloadUpdate();
        }

        @Override
        public boolean install() {
            return installUpdate();
        }
    }

    private final Runnable mQueryRunnable = new Runnable() {
        @Override
        public void run() {
            // 查询版本
            queryNewVersion();

            mQueryThrad = null;
        }
    };

    @Override
    public void onCreate() {
        super.onCreate();
        Util.Logd(TAG, "Service onCreate");

        mContext = this;

        mSharedPrefrns = getSharedPreferences(
            ConstValue.OTA_DATA, Context.MODE_PRIVATE);

        mBinder = new OTAUpdateStub();
        mDownload = new OTADownloadMgr(this);

        // 获取rm版本信息
        mLocalVersion = Build.VERSION.INCREMENTAL;

        // 注册广播接收：网络变化
        registerReceiver(mNetWorkReceiver, new IntentFilter(
            ConnectivityManager.CONNECTIVITY_ACTION));

        // 检查更新
        autoQueryNewVersion();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return super.onStartCommand(intent, flags, startId);
    }

    @Override
    public void onDestroy() {
        //super.onDestroy();
        Util.Logd(TAG, "Service onDestroy");
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        Util.Logd(TAG, "Service onUnbind");
        mICallback = null;
        return super.onUnbind(intent);
    }

    private BroadcastReceiver mNetWorkReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {

        }
    };

    public void notifyClientStatus() {
        String status = UpdateStatus.getStatus().toString();
        Util.Logd(TAG, "Notify user status:" + status);

        if (mICallback != null) {
            try {
                mICallback.onStatusUpdate(status);
            } catch (RemoteException e) {
                Util.Loge(TAG, "Callback - onStatusUpdate exception.(" + e.getMessage() + ")");
            }
        }
    }

    private void putStringToSP(String key, String value) {
        SharedPreferences.Editor editor = mSharedPrefrns.edit();
        editor.putString(key, value);
        editor.commit();
    }

    private void autoQueryNewVersion() {
        // 获取上次更新时间
        String lastupdate = mSharedPrefrns.getString(
            ConstValue.LAST_QUERY, ConstValue.DEFAULT_TIME);

        long diffTime = Util.getTimeDiff(Util.getTimeNow(), lastupdate);
        // 距离上次查询超过４小时
        if (diffTime > ConstValue.SP_QUERY_TIME) {
            new Timer().schedule(new TimerTask() {
                @Override
                public void run() {
                    if (mQueryThrad == null) {
                        mQueryThrad = new Thread(mQueryRunnable);
                        mQueryThrad.start();
                    }
                }
            }, 2000);
        }
    }

    private void manualQueryNewVersion(IOTAUpdateCallback cb) {
        if (cb != null)
            mICallback = cb;
        else
            mICallback = null;

        if (mQueryThrad == null) {
            mQueryThrad = new Thread(mQueryRunnable);
            mQueryThrad.start();
        }
    }

    /**
     * 查询版本任务
     */
    private void queryNewVersion() {
        // 重置版本信息
        mNewVersion = "null";
        mNewVerUrl = "null";

        // wifi网络是否连接
        if (!mDownload.isNetWorkConnect()) {
            UpdateStatus.setStatus(UpdateStatus.SERVER_ERROR);

            /*if (!mDownload.isWifiConnected()) {
                // 离线安装
                mDownload.queryDownload();
            }*/
            notifyClientStatus();
            return;
        }

        // 设置状态并记录时间
        UpdateStatus.setStatus(UpdateStatus.REFRESH);
        putStringToSP(ConstValue.LAST_QUERY, Util.getTimeNow());

        // 连接服务器
        try {
            boolean ret = queryVerByHttp();
            if (ret) {
                // 检测到有新版本时，再判断新版本是否已经下载
                if (UpdateStatus.getStatus() == UpdateStatus.CATCH_NEW) {
                    mDownload.queryDownload();
                }
            }
        } catch (IOException e) {
            Util.Logd(TAG, e.getMessage());
            UpdateStatus.setStatus(UpdateStatus.SERVER_ERROR);
        }

        notifyClientStatus();
    }

    private boolean queryVerByHttp() throws IOException {
        /* 建立HTTP连接 */
        try {
            HttpClient httpclient = new DefaultHttpClient();
            HttpPost httppost = new HttpPost(ConstValue.VERSION_URL);

            // 设置请求信息
            JSONObject json = getVerJSON();

            // 设置超时
            httpclient.getParams().setIntParameter(
                    CoreConnectionPNames.CONNECTION_TIMEOUT, ConstValue.NET_TIMEOUT);
            httpclient.getParams().setIntParameter(
                    CoreConnectionPNames.SO_TIMEOUT, ConstValue.NET_TIMEOUT);

            // 设置HTTP请求头
            httppost.setEntity(new StringEntity(json.toString(), HTTP.UTF_8));
            httppost.setHeader(HTTP.CONTENT_TYPE, "application/json");

            // 取得HTTP response
            Util.Logd(TAG, "executing request " + httppost.getRequestLine());
            HttpResponse response = httpclient.execute(httppost);

            String respStr = "null";
            int statusCode = response.getStatusLine().getStatusCode();
            HttpEntity resEntity = response.getEntity();
            if (resEntity != null) {
                respStr = EntityUtils.toString(resEntity);
            }
            if (statusCode == HttpStatus.SC_OK) {
                Util.Logd(TAG, "http response:" + respStr + " StatusCode:" + statusCode);

                JSONObject jsResp = new JSONObject(respStr);
                String rtCode = jsResp.getString(ConstValue.JSON_RETCODE);
                if (rtCode.equals("000")) {
                    // 获取新版本信息
                    JSONObject jsonBody = jsResp.getJSONObject(ConstValue.JSON_BODY);
                    mNewVersion = "1.0.0";
                    mNewVerUrl = jsonBody.getString(ConstValue.JSON_DLURL);

                    UpdateStatus.setStatus(UpdateStatus.CATCH_NEW);
                } else if (rtCode.equals("104")) {
                    // 已经是最新版本
                    UpdateStatus.setStatus(UpdateStatus.CATCH_NONE);
                } else {
                    Util.Loge(TAG, "Server return error.(" + rtCode + ")");
                    UpdateStatus.setStatus(UpdateStatus.SERVER_ERROR);
                    return false;
                }
            }
            else {
                String eMsg = "http response failed:" + respStr + " StatusCode:" + statusCode;
                throw new IOException(eMsg);
            }

        } catch (JSONException e) {
            UpdateStatus.setStatus(UpdateStatus.SERVER_ERROR);
            Util.Loge(TAG, "JSONException-" + e.getMessage());
            return false;
        }
        
        return true;
    }

    private boolean downloadUpdate() {
        if (mNewVerUrl == null) {
            return false;
        }

        UpdateStatus status = UpdateStatus.getStatus();
        Util.Logd(TAG, "download status check:" + status.toString());
        if (status == UpdateStatus.DOWNLOADING) return true;
        if (status != UpdateStatus.CATCH_NEW && status != UpdateStatus.DOWNLOAD_FAILED)
            return false;

        if (!mDownload.isNetWorkConnect())
            return false;

        // set status
        UpdateStatus.setStatus(UpdateStatus.DOWNLOADING);

        // start download
        mDownload.download(mNewVersion, mNewVerUrl);

        return true;
    }

    private int getDownloadProgress() {
        return mDownload.getProgress();
    }

    private boolean installUpdate() {
        UpdateStatus status = UpdateStatus.getStatus();
        Util.Logd(TAG, "install status check:" + status.toString());

        if (status != UpdateStatus.DOWNLOAD_OK) return false;

        // 电量检测
        // TODO

        // 存储空间检测
        long size = Util.getInternalMemorySize();
        if (size < ConstValue.INSTALL_MEMORY_NEED) {
            return false;
        }

        // 检测文件是否存在
        File file = new File(ConstValue.DOWNLOAD_PATH, ConstValue.INSTALL_PACKAGE);
        if (!file.exists()) {
            Util.Loge(TAG, "Can't find update package.");
            UpdateStatus.setStatus(UpdateStatus.DOWNLOAD_FAILED);
            return false;
        }

        // 设置状态
        UpdateStatus.setStatus(UpdateStatus.INSTALLING);

        // 弹出提示框
        mOTAHandler.sendEmptyMessage(POP_ALERT_DIALOG);

        new Thread(new Runnable() {
            @Override
            public void run() {
                // 重启进行安装
                try {
                    // 解压缩文件并删除原文件，并检测
                    File updatePackage = new File(ConstValue.DOWNLOAD_PATH, ConstValue.INSTALL_PACKAGE);
                    File[] files = Util.unzipFile(updatePackage.getPath(), updatePackage.getParent());
                    //updatePackage.delete();

                    // 存储解压后文件信息，为了重启后删除这些文件
                    StringBuilder sigFile = new StringBuilder();
                    for (File fl : files) {
                        sigFile.append(fl.getName()).append(";");
                    }
                    mSharedPrefrns.edit().putString(ConstValue.UNZIP_TMP_FILES, sigFile.toString()).commit();

                    File[] packages = getUpdateFiles(files);
                    RecoverySystem.installPackages(mContext, packages);
                } catch (IOException e) {
                    Util.Loge(TAG, "Install package error.(" + e.getMessage() + ")");


                }
            }
        }).start();

        return true;
    }

    private String getRobotCode() {
        WifiManager wifi = (WifiManager) mContext.getSystemService(Context.WIFI_SERVICE);
        String macAddr = wifi.getConnectionInfo().getMacAddress();
        if (macAddr == null)
            return null;

        return UUID.nameUUIDFromBytes(macAddr.getBytes()).toString();
    }

    private File[] getUpdateFiles(File[] files) {
        List<File> packages = new ArrayList<File>();

        for (File file : files) {
            if (file.isFile()) {
                String lowname = file.getName().toLowerCase();
                if (lowname.contains(ConstValue.UPDATE_RM)
                    || lowname.contains(ConstValue.UPDATE_RC)
                    || lowname.contains(ConstValue.UPDATE_RP)
                    || lowname.contains(ConstValue.UPDATE_RF)
                    || lowname.contains(ConstValue.UPDATE_RB)) {
                    packages.add(file);
                }
            }
        }

        File[] total = new File[packages.size()];
        return packages.toArray(total);
    }

    private void popAlertDialog() {
        ProgressDialog pd = new ProgressDialog(mContext);
        pd.setTitle(R.string.app_name);
        pd.setMessage(mContext.getResources().getString(R.string.install_prepare));
        pd.setIndeterminate(true);
        pd.setCancelable(false);
        pd.getWindow().setType(WindowManager.LayoutParams.TYPE_KEYGUARD_DIALOG);

        pd.show();
    }

    private JSONObject getVerJSON() throws JSONException {
        // 获取设备型号
        String product = Build.PRODUCT;

        // 获取子系统版本
        SchedulerManager sm = (SchedulerManager)getSystemService(Context.SCHEDULER_SERVICE);
        String rcVer = sm.getSubSystemVersion(0x1);
        String rpVer = sm.getSubSystemVersion(0x2);
        String rfVer = sm.getSubSystemVersion(0x3);
        String rb_rVer = sm.getSubSystemVersion(0x4);
        Util.Logd(TAG, "rcVer:" + rcVer +
                ",rpVer:" + rpVer +
                ",rfVer:" + rfVer +
                ",rb_rVer:" + rb_rVer);

        JSONObject json = new JSONObject();
        json.put(ConstValue.JSON_ROBOTCODE, getRobotCode());
        json.put(ConstValue.JSON_ROBOTMODEL, product);
        json.put(ConstValue.JSON_RMVER, mLocalVersion);
        json.put(ConstValue.JSON_RCVER, rcVer);
        json.put(ConstValue.JSON_RPVER, rpVer);
        json.put(ConstValue.JSON_RFVER, rfVer);
        json.put(ConstValue.JSON_RBVER, rb_rVer);
        Util.Logd(TAG, "Ver json:" + json.toString());

        return json;
    }
}