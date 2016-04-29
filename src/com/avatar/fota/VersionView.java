package com.avatar.fota;

import android.content.Context;
import android.os.Handler;
import android.util.AttributeSet;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import com.avatar.fota.utils.Util;


public class VersionView extends LinearLayout{
    private static final String TAG = "VersionView";

    private Handler mHandler;

    private TextView mVerTitleView;
    private TextView mVersionView;
    private TextView mTipsView;
    private TextView mReleaseNoteView;
    private TextView mWarningTextView;
    private TextView mProgressTextView;
    private ProgressBar mProgressBar;
    private ImageView mDevideLineView;
    private LinearLayout mWarningView;
    private Button mInstallBtnView;

    public VersionView(Context context) {
        super(context);
    }

    public VersionView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        mVerTitleView = (TextView)findViewById(R.id.versionTitle);
        mVersionView = (TextView)findViewById(R.id.version);
        mTipsView = (TextView)findViewById(R.id.tips);
        mReleaseNoteView = (TextView)findViewById(R.id.release_note);
        mWarningTextView = (TextView)findViewById(R.id.warn_text);
        mProgressTextView = (TextView)findViewById(R.id.progress);
        mProgressBar = (ProgressBar)findViewById(R.id.download_bar);
        mDevideLineView = (ImageView)findViewById(R.id.line_devide);
        mWarningView = (LinearLayout)findViewById(R.id.warning);
        mInstallBtnView = (Button)findViewById(R.id.install_button);

        mInstallBtnView.setOnClickListener(mInstallClick);
    }

    public void setHandler(Handler handler) {
        mHandler = handler;
    }

    public void setCurentVersion(String ver) {
        mVerTitleView.setText(R.string.version_cur);
        mVersionView.setText(ver);
        setTipText(R.string.tip_ver_finish);
    }

    public void setNewVersion(String ver, String tip) {
        mVerTitleView.setText(R.string.version_new);
        mVersionView.setText(ver);
        setTipText(tip);
    }

    public void setWarning(String w) {
        if (w == null) {
            mWarningView.setVisibility(View.GONE);
            return;
        }

        mWarningView.setVisibility(View.VISIBLE);
        mWarningTextView.setText(w);
    }

    public void cancelWarning() {
        mWarningView.setVisibility(View.GONE);
    }

    public void setTipText(String t) {
        if (t != null)
            mTipsView.setText(t);
        else
            mTipsView.setText(" ");
    }

    public void setTipText(int resId) {
        mTipsView.setText(resId);
    }

    public void startDownload() {
        setTipText(R.string.tip_downloading);
        mDevideLineView.setVisibility(View.GONE);
        mProgressBar.setVisibility(View.VISIBLE);
        mProgressTextView.setVisibility(View.VISIBLE);
        mWarningView.setVisibility(View.GONE);
        mInstallBtnView.setVisibility(View.GONE);

        mProgressBar.setProgress(0);
        mProgressTextView.setText("0%");
    }

    public void setDownloadEnd() {
        mProgressBar.setVisibility(View.GONE);
        mProgressTextView.setVisibility(View.GONE);
        mDevideLineView.setVisibility(View.VISIBLE);
    }

    public void setProgress(int p) {
        mProgressBar.setProgress(p);
        mProgressTextView.setText(p + "%");
    }

    public void setInstallEnable(boolean enabled) {
        if (enabled) {
            mInstallBtnView.setVisibility(View.VISIBLE);
            mInstallBtnView.setEnabled(true);
        }
        else {
            mInstallBtnView.setEnabled(false);
        }
    }

    public void startInstall() {
        mInstallBtnView.setEnabled(false);
    }

    public void setInatallEnd() {
        mInstallBtnView.setEnabled(true);
        mInstallBtnView.setVisibility(View.GONE);
    }

    private OnClickListener mInstallClick = new OnClickListener() {
        @Override
        public void onClick(View v) {
            mHandler.sendEmptyMessage(OTAUpdateActivity.MSG_START_INSTALL);
        }
    };


}
