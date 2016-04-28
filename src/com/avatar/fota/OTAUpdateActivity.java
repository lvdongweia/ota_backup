package com.avatar.fota;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Message;
import android.os.RemoteException;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import com.avatar.fota.service.IOTAUpdateService;
import com.avatar.fota.service.IOTAUpdateCallback;
import com.avatar.fota.utils.UpdateStatus;
import com.avatar.fota.utils.Util;

public class OTAUpdateActivity extends Activity{
    private static final String TAG = "OTAActivity";

    private static final int MSG_QUERY_VERSION  = 0;
    private static final int MSG_STATUS_UPDATE  = 1;
    private static final int MSG_START_DOWNLOAD = 2;
    public static final int MSG_START_INSTALL   = 3;

    private IOTAUpdateService mIOTAService;
    private DownloadTask mDownloadTask = null;

    private FrameLayout mMainView;
    private QueryingView mQueryView;
    private VersionView mVersionView;
    private TextView mDownloadBtn;

    private boolean mClickFlag = false;
    private boolean mDwloadCanceled = false;

    public ServiceConnection mServiceConn = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            mIOTAService = IOTAUpdateService.Stub.asInterface(service);
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            mIOTAService = null;
        }
    };

    private IOTAUpdateCallback mCallback = new IOTAUpdateCallback.Stub() {
        @Override
        public void onStatusUpdate(String s) throws RemoteException {
            Util.Logd(TAG, "onStatusUpdate->" + s);
            mHandler.obtainMessage(MSG_STATUS_UPDATE, s).sendToTarget();
        }
    };

    private Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            try {

                switch (msg.what) {
                    case MSG_QUERY_VERSION:
                        loadQueryingView();
                        mIOTAService.queryVersion(mCallback);
                        break;

                    case MSG_START_DOWNLOAD:
                        mDownloadBtn.setVisibility(View.INVISIBLE);
                        mVersionView.startDownload();
                        mIOTAService.download();

                        createDownloadTask();
                        break;

                    case MSG_START_INSTALL:
                        mVersionView.startInstall();
                        mIOTAService.install();
                        break;

                    case MSG_STATUS_UPDATE:
                        handleResponse((String)msg.obj);
                        break;

                    default:
                        break;
                }
            } catch (RemoteException e) {
                Util.Loge(TAG, e.getMessage());
                e.printStackTrace();
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.ota_activity);

        mMainView = (FrameLayout)findViewById(R.id.main_view);
        mDownloadBtn = (TextView)findViewById(R.id.about_system_update_download);
    }

    @Override
    protected void onStart() {
        super.onStart();
        // bind service
        Intent intent = new Intent();
        intent.setAction("com.avatar.fota.OTAUPDATESERVICE");
        boolean bS = bindService(intent, mServiceConn, Context.BIND_AUTO_CREATE);
        if (!bS) {
            Util.Loge(TAG, "Bind ota service failed.");
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        hiddenNavigationBar();

        mHandler.sendEmptyMessageDelayed(MSG_QUERY_VERSION, 300);
    }

    @Override
    protected void onPause() {
        super.onPause();
        // cancel download
        cancelDownloadTask();
    }

    @Override
    protected void onStop() {
        super.onStop();
        Util.Logd(TAG, "onStop");
        // unbind service
        unbindService(mServiceConn);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }

    public void onClose(View v) {
        this.finish();
    }

    /**
     * Click query button
     * @param v
     */
    public void query(View v) {
        if (mClickFlag) return;

        mClickFlag = true;
        mHandler.sendEmptyMessage(MSG_QUERY_VERSION);
    }

    /**
     * Click download button
     * @param v
     */
    public void onDownload(View v) {
        mHandler.sendEmptyMessage(MSG_START_DOWNLOAD);
    }

    private void hiddenNavigationBar() {
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }

    private void loadQueryingView() {
        if (mQueryView == null) {
            mQueryView = (QueryingView)getLayoutInflater()
                    .inflate(R.layout.querying_view, mMainView, false);
        }
        mMainView.removeAllViews();
        mMainView.addView(mQueryView);
    }

    private void loadVersionView() {
        if (mVersionView == null) {
            mVersionView = (VersionView)getLayoutInflater()
                    .inflate(R.layout.version_view, mMainView, false);
            mVersionView.setHandler(mHandler);
        }
        mMainView.removeAllViews();
        mMainView.addView(mVersionView);
    }

    // run in ui thread
    private void handleResponse(String status) {
        Util.Logd(TAG, "handle response status:" + status);

        if (status.equals(UpdateStatus.SERVER_ERROR.toString())) {
            mQueryView.setErrorInfo(getString(R.string.detail_server_error));
            return;
        }

        // load version view
        loadVersionView();
        try {
            String curVer = mIOTAService.getCurrentVersion();
            String newVer = mIOTAService.getNewVersion();

            if (status.equals(UpdateStatus.CATCH_NONE.toString())) {
                mDownloadBtn.setVisibility(View.INVISIBLE);
                mVersionView.setCurentVersion(curVer);
            }
            else if (status.equals(UpdateStatus.CATCH_NEW.toString())) {
                Util.Logd(TAG, "catch_new");
                mDownloadBtn.setVisibility(View.VISIBLE);
                mVersionView.setNewVersion(newVer, null);
            }
            else if (status.equals(UpdateStatus.DOWNLOAD_FAILED.toString())) {
                mDownloadBtn.setVisibility(View.VISIBLE);
                mVersionView.setNewVersion(newVer, getString(R.string.tip_download_failed));
                mVersionView.setDownloadEnd();
                cancelDownloadTask();
            }
            else if (status.equals(UpdateStatus.DOWNLOAD_OK.toString())) {
                mDownloadBtn.setVisibility(View.INVISIBLE);
                mVersionView.setNewVersion(newVer, getString(R.string.tip_download_ok));
                mVersionView.setDownloadEnd();
                mVersionView.setInstallEnable(true);
                cancelDownloadTask();
            } else if (status.equals(UpdateStatus.DOWNLOADING.toString())) {
                mDownloadBtn.setVisibility(View.INVISIBLE);
                mVersionView.setNewVersion(newVer, getString(R.string.tip_downloading));
                mVersionView.startDownload();
                createDownloadTask();
            } else if (status.equals(UpdateStatus.STORAGE_INEQUACY.toString())) {

            }
        } catch (RemoteException e) {
            Util.Logd(TAG, e.getMessage());
            e.printStackTrace();
        }
    }

    private void createDownloadTask() {
        if (mDownloadTask == null) {
            mDwloadCanceled = false;

            mDownloadTask = new DownloadTask();
            mDownloadTask.execute();
        }
    }

    private void cancelDownloadTask() {
        if (mDownloadTask != null) {
            mDwloadCanceled = true;

            mDownloadTask.cancel(true);
            mDownloadTask = null;
        }
    }

    private class DownloadTask extends AsyncTask<Void, Integer, Void> {
        @Override
        protected Void doInBackground(Void... voids) {
            // 定时检测进度
            int prog = 0;
            do {
                try {
                    if (mIOTAService != null) {
                        prog = mIOTAService.progress();
                    }
                    // 刷新进度
                    Util.Logd(TAG, "get progress:" + prog);
                    publishProgress(prog);
                    Thread.sleep(1000);
                } catch (RemoteException e) {
                    Util.Loge(TAG, "Call ota service fun:progress exception.");
                } catch (InterruptedException e) {
                   //ignore
                }
            } while (prog < 100 && !mDwloadCanceled);

            return null;
        }

        @Override
        protected void onProgressUpdate(Integer... values) {
            super.onProgressUpdate(values);

            if (isCancelled()) return;

            mVersionView.setProgress(values[0]);
        }
    }

}
